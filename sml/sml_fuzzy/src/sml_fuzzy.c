/*
 * This file is part of the Soletta Project
 *
 * Copyright (C) 2015 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sml.h>
#include <config.h>
#include "sml_fuzzy_bridge.h"
#include "sml_observation_controller.h"
#include "sml_util.h"
#include "sml_matrix.h"
#include "sml_terms_manager.h"
#include <macros.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sml_engine.h>
#include <sml_log.h>
#include <sol-vector.h>
#include <errno.h>
#include <float.h>

#define FUZZY_FILE_PREFIX "fuzzy"
#define DEFAULT_FLL "fuzzy_description.fll"
#define FUZZY_MAGIC (0xf0022ee)
#define DEFAULT_NUM_TERMS (10)
#define DEFAULT_OVERLAP_PERCENTAGE (0.1)
#define TERM_NAME_FMT "%.100s_%d_%d"

struct sml_term_to_remove {
    struct sml_variable *var;
    struct sml_fuzzy_term *term;
    bool is_input;
};

struct sml_fuzzy_engine {
    struct sml_engine engine;

    bool output_state_changed_called;
    bool variable_terms_auto_balance;

    struct sml_measure *last_stable_measure;

    struct sml_fuzzy *fuzzy;
    struct sml_observation_controller *observation_controller;
    struct sml_terms_manager terms_manager;
    struct sol_ptr_vector inputs_to_be_removed;
    struct sol_ptr_vector outputs_to_be_removed;
    struct sol_vector terms_to_be_removed;
};

struct sml_fuzzy *
sml_get_fuzzy(struct sml_engine *engine)
{
    struct sml_fuzzy_engine *fuzzy_engine = (struct sml_fuzzy_engine *)engine;

    return fuzzy_engine->fuzzy;
}

static void
_fill_variables(struct sml_variables_list *list,
    struct sol_ptr_vector *remove_list, bool *to_remove)
{
    struct sml_variable *var;
    uint16_t i, pos;

    SOL_PTR_VECTOR_FOREACH_IDX (remove_list, var, i) {
        if (sml_fuzzy_find_variable(list, var, &pos))
            to_remove[pos] = 1;
    }
}

static bool
_sml_load_fll_file(struct sml_engine *engine, const char *filename)
{
    struct sml_fuzzy_engine *fuzzy_engine = (struct sml_fuzzy_engine *)engine;

    if (!sml_fuzzy_load_file(fuzzy_engine->fuzzy, filename))
        return false;

    sml_observation_controller_clear(fuzzy_engine->observation_controller);
    sml_terms_manager_clear(&fuzzy_engine->terms_manager);
    sol_ptr_vector_clear(&fuzzy_engine->inputs_to_be_removed);
    sol_vector_clear(&fuzzy_engine->terms_to_be_removed);
    sol_ptr_vector_clear(&fuzzy_engine->outputs_to_be_removed);

    return true;
}

static bool
_float_equals(void *v1, void *v2)
{
    float a = 0, b = 0;

    if (v1)
        a = *((float *)v1);
    if (v2)
        b = *((float *)v2);

    if (isnan(a) && isnan(b))
        return true;

    if (isnan(a) || isnan(b))
        return false;

    return fabs(a - b) <= VARIABLE_MEMBERSHIP_THRESHOLD;
}

static bool
_matrix_has_significant_changes(struct sml_matrix *old, struct sml_matrix *new,
    struct sol_vector *changed)
{
    return sml_matrix_equals(old, new, changed, _float_equals);
}

static bool
_measure_has_significant_changes(struct sml_measure *old_measure,
    struct sml_measure *new_measure, bool *input_changed)
{
    //First run.
    if (!old_measure) {
        if (input_changed)
            *input_changed = true;
        return true;
    }

    if (_matrix_has_significant_changes(&old_measure->inputs,
        &new_measure->inputs, NULL)) {
        if (input_changed)
            *input_changed = true;
        return true;
    }

    if (input_changed)
        *input_changed = false;

    if (_matrix_has_significant_changes(&old_measure->outputs,
        &new_measure->outputs, NULL))
        return true;

    return false;
}

API_EXPORT bool
sml_fuzzy_set_variable_terms_auto_balance(struct sml_object *sml,
    bool variable_terms_auto_balance)
{
    if (!sml_is_fuzzy(sml))
        return false;
    struct sml_fuzzy_engine *fuzzy_engine = (struct sml_fuzzy_engine *)sml;

    if (!variable_terms_auto_balance &&
        fuzzy_engine->variable_terms_auto_balance)
        sml_terms_manager_clear(&fuzzy_engine->terms_manager);
    fuzzy_engine->variable_terms_auto_balance = variable_terms_auto_balance;

    return true;
}

static int
_get_next_term_id(struct sml_fuzzy *fuzzy, struct sml_variable *var)
{
    uint16_t terms_len, i;
    struct sml_fuzzy_term *term;
    char term_name[SML_TERM_NAME_MAX_LEN];
    char var_name[SML_VARIABLE_NAME_MAX_LEN + 1];
    char *token, *saveptr;
    size_t var_name_len;

    terms_len = sml_fuzzy_variable_terms_count(var);
    if (!terms_len)
        return 0;

    if (sml_fuzzy_variable_get_name(var, var_name, sizeof(var_name)))
        return 0;
    var_name_len = strlen(var_name);

    for (i = terms_len - 1; i != 0; i--) {
        term = sml_fuzzy_variable_get_term(var, i);
        if (sml_fuzzy_term_get_name(term, term_name, sizeof(term_name)))
            continue;
        if (strlen(term_name) <= var_name_len + 1)
            continue;

        token = strtok_r(term_name + var_name_len + 1, "_", &saveptr);
        if (!token)
            continue;

        return atoi(token) + 1;
    }

    return 0;
}

static int
_create_fuzzy_terms(struct sml_fuzzy_engine *fuzzy_engine,
    struct sml_variable *var, float min, float max, bool real_min,
    bool real_max)
{
    struct sml_fuzzy *fuzzy;
    float width, range, overlap, first_width, last_stop, cur;
    bool is_id;
    uint16_t num_terms, i;
    char term_name[SML_TERM_NAME_MAX_LEN + 1];
    char var_name[SML_VARIABLE_NAME_MAX_LEN + 1];
    uint16_t term_name_id;
    struct sml_fuzzy_term *term;

    fuzzy = fuzzy_engine->fuzzy;
    width = sml_fuzzy_bridge_variable_get_default_term_width(fuzzy, var);
    is_id = sml_fuzzy_bridge_variable_get_is_id(fuzzy, var);
    range = max - min;

    if (isnan(width)) {
        width = range / DEFAULT_NUM_TERMS;
        sml_fuzzy_bridge_variable_set_default_term_width(fuzzy, var, width);
    }

    if (width < FLT_EPSILON)
        num_terms = 1;
    else {
        if (is_id) {
            num_terms = floorf(range / width) + 1;
            first_width = (range - width * (num_terms - 2)) / 2;
        } else {
            num_terms = ceilf(range / width);
            first_width = width;
        }
    }

    overlap = width * DEFAULT_OVERLAP_PERCENTAGE;
    last_stop = min;
    term_name_id = _get_next_term_id(fuzzy, var);
    if (sml_fuzzy_variable_get_name(var, var_name, sizeof(var_name)))
        return -ENODATA;

    if (num_terms <= 1) {
        snprintf(term_name, sizeof(term_name), TERM_NAME_FMT, var_name,
            term_name_id, 0);
        if (real_min && real_max)
            term = sml_fuzzy_bridge_variable_add_term_triangle(fuzzy, var,
                term_name, min, min + (max - min) / 2, max);
        else if (real_min)
            term = sml_fuzzy_bridge_variable_add_term_triangle(fuzzy, var,
                term_name, min, min, max + overlap);
        else if (real_max)
            term = sml_fuzzy_bridge_variable_add_term_triangle(fuzzy, var,
                term_name, min - overlap, max, max);
        else
            term = sml_fuzzy_bridge_variable_add_term_triangle(fuzzy, var,
                term_name, min - overlap, min + (max - min) / 2, max + overlap);

        if (term == NULL)
            return -ENOMEM;
        else
            return 0;
    }

    for (i = 0; i < num_terms; i++) {
        snprintf(term_name, sizeof(term_name), TERM_NAME_FMT, var_name,
            term_name_id, i);
        if (real_min && i == 0) {
            last_stop = min + first_width;
            term = sml_fuzzy_bridge_variable_add_term_triangle(fuzzy, var,
                term_name, min, min, last_stop + overlap);
        } else if (real_max && i == num_terms - 1)
            term = sml_fuzzy_bridge_variable_add_term_triangle(fuzzy, var,
                term_name, last_stop - overlap, max, max);
        else {
            cur = last_stop + width;
            term = sml_fuzzy_bridge_variable_add_term_triangle(fuzzy, var,
                term_name, last_stop - overlap,
                last_stop + (cur - last_stop) / 2, cur + overlap);
            last_stop = cur;
        }

        if (term == NULL)
            return -ENOMEM;
    }

    return 0;
}

static int
_create_fuzzy_terms_variable(struct sml_fuzzy_engine *fuzzy_engine,
    struct sml_variable *var)
{
    float min, max;

    if (!sml_fuzzy_variable_get_range(var, &min, &max))
        return -ENODATA;

    return _create_fuzzy_terms(fuzzy_engine, var, min, max, true, true);
}

static int
_fuzzy_initialize_terms_list(struct sml_fuzzy_engine *fuzzy_engine,
    struct sml_variables_list *list)
{
    uint16_t i, len;
    struct sml_variable *var;
    int error;

    len = sml_fuzzy_variables_list_get_length(list);
    for (i = 0; i < len; i++) {
        var = sml_fuzzy_variables_list_index(list, i);
        if (sml_fuzzy_variable_terms_count(var) == 0) {
            if ((error = _create_fuzzy_terms_variable(fuzzy_engine, var)))
                return error;
        }
    }

    return 0;
}

static int
_fuzzy_initialize_terms(struct sml_fuzzy_engine *fuzzy_engine)
{
    int error;

    if ((error = _fuzzy_initialize_terms_list(fuzzy_engine,
            fuzzy_engine->fuzzy->input_list)))
        return error;
    if ((error = _fuzzy_initialize_terms_list(fuzzy_engine,
            fuzzy_engine->fuzzy->output_list)))
        return error;
    return 0;
}

static int
_pre_process(struct sml_fuzzy_engine *fuzzy_engine, bool *should_act,
    bool *should_learn)
{
    struct sml_measure *new_measure;
    bool input_changed;
    int error;

    if ((error = _fuzzy_initialize_terms(fuzzy_engine))) {
        sml_debug("Initialization of fuzzy terms failed.");
        return error;
    }

    if (!fuzzy_engine->fuzzy->input_terms_count ||
        !fuzzy_engine->fuzzy->output_terms_count)
        return 0;

    new_measure = sml_fuzzy_get_membership_values(fuzzy_engine->fuzzy);
    if (!new_measure)
        return -ENOMEM;

    if (_measure_has_significant_changes(fuzzy_engine->last_stable_measure,
        new_measure, &input_changed)) {
        sml_measure_free(fuzzy_engine->last_stable_measure);
        fuzzy_engine->last_stable_measure = new_measure;
        fuzzy_engine->engine.hits = 0;
        new_measure = NULL;

        //if input has changed
        if (input_changed)
            fuzzy_engine->engine.output_state_changed_called = false;
    }

    //measures didn't have significant changes in the defined number of hits
    if (fuzzy_engine->engine.hits == fuzzy_engine->engine.stabilization_hits) {
        sml_debug("Input is stable, saving state");
        if (new_measure) {
            sml_measure_free(fuzzy_engine->last_stable_measure);
            fuzzy_engine->last_stable_measure = new_measure;
            fuzzy_engine->engine.hits = 0;
            new_measure = NULL;
        }

        //If we called output_state_changed_cb before and there are no significative
        //changes in inputs, or we don't have enough rules, then
        //We won't act and we will learn this scenario
        if (fuzzy_engine->engine.output_state_changed_called ||
            sml_fuzzy_is_rule_block_empty(fuzzy_engine->fuzzy))
            *should_learn = true;
        else
            *should_act = true;
    } else {
        sml_measure_free(new_measure);
        fuzzy_engine->engine.hits++;
    }
    return 0;
}

static int
_act(struct sml_fuzzy_engine *fuzzy_engine, bool *should_learn)
{
    struct sml_matrix output_membership;
    struct sol_vector changed_idx = SOL_VECTOR_INIT(uint16_t);
    struct sml_variables_list *changed;
    int error;

#ifdef Debug
    struct sml_variable *variable;
#endif

    //predict outputs
    if ((error = sml_fuzzy_process_output(fuzzy_engine->fuzzy)))
        return error;

#ifdef Debug
    struct sml_variables_list *outputs;
    uint16_t i, len;
    char var_name[SML_VARIABLE_NAME_MAX_LEN + 1];

    sml_debug("Fuzzy output values");

    outputs = fuzzy_engine->fuzzy->output_list;
    len = sml_fuzzy_variables_list_get_length(outputs);
    for (i = 0; i < len; i++) {
        variable = sml_fuzzy_variables_list_index(outputs, i);
        if (sml_fuzzy_variable_get_name(variable, var_name, sizeof(var_name))) {
            sml_warning("Failed to get variable %p name", variable);
            continue;
        }
        sml_debug("%s\t%g", var_name,
            sml_fuzzy_variable_get_value(variable));
    }
#endif

    if (!fuzzy_engine->engine.output_state_changed_cb) {
        *should_learn = true;
        return 0;
    }

    sml_matrix_init(&output_membership, sizeof(float));
    if ((error = sml_fuzzy_get_membership_values_output(fuzzy_engine->fuzzy,
            &output_membership)))
        return error;

    //If output_has_significative_changes
    //or we are forced to call output_state_changed_cb
    if (_matrix_has_significant_changes(
        &fuzzy_engine->last_stable_measure->outputs,
        &output_membership, &changed_idx)) {
        changed = sml_fuzzy_variables_list_new(fuzzy_engine->fuzzy,
            &changed_idx);
        if (!changed) {
            sml_critical("Could not create the changed variables list");
        } else {
            sml_debug("Calling user's change state callback!");
            //If variable is not present in the changed list, the read values
            //should be set as its value.
            sml_fuzzy_set_read_values(fuzzy_engine->fuzzy, changed);
            sml_call_output_state_changed_cb(&fuzzy_engine->engine, changed);
            fuzzy_engine->engine.output_state_changed_called = true;
            sml_fuzzy_variables_list_free(changed);
        }
    }

    sol_vector_clear(&changed_idx);
    sml_matrix_clear(&output_membership);
    *should_learn = true;
    return 0;
}

static int
_remove_variables(struct sml_fuzzy_engine *fuzzy_engine, bool *removed)
{
    int error;
    struct sml_variable *var;
    uint16_t i, pos;
    bool *inputs_to_remove_bool = NULL;
    bool *outputs_to_remove_bool = NULL;

    *removed = false;
    if (sol_ptr_vector_get_len(&fuzzy_engine->inputs_to_be_removed)) {
        inputs_to_remove_bool = calloc(sml_fuzzy_variables_list_get_length(
            fuzzy_engine->fuzzy->input_list), sizeof(bool));
        _fill_variables(fuzzy_engine->fuzzy->input_list,
            &fuzzy_engine->inputs_to_be_removed,
            inputs_to_remove_bool);
    }

    if (sol_ptr_vector_get_len(&fuzzy_engine->outputs_to_be_removed)) {
        outputs_to_remove_bool = calloc(sml_fuzzy_variables_list_get_length(
            fuzzy_engine->fuzzy->output_list), sizeof(bool));
        _fill_variables(fuzzy_engine->fuzzy->output_list,
            &fuzzy_engine->outputs_to_be_removed,
            outputs_to_remove_bool);
    }

    if (!inputs_to_remove_bool && !outputs_to_remove_bool)
        return 0;

    if ((error = sml_terms_manager_remove_variables(
            &fuzzy_engine->terms_manager,
            inputs_to_remove_bool, outputs_to_remove_bool)))
        return error;

    if ((error = sml_observation_controller_remove_variables(
            fuzzy_engine->observation_controller,
            inputs_to_remove_bool, outputs_to_remove_bool)))
        return error;

    SOL_PTR_VECTOR_FOREACH_IDX (&fuzzy_engine->inputs_to_be_removed, var, i) {
        if (!sml_fuzzy_find_variable(fuzzy_engine->fuzzy->input_list, var,
            &pos))
            continue;
        if (!sml_fuzzy_remove_variable(fuzzy_engine->fuzzy, var))
            continue;
        if (fuzzy_engine->last_stable_measure)
            sml_measure_remove_input_variable(
                fuzzy_engine->last_stable_measure, pos);
    }

    SOL_PTR_VECTOR_FOREACH_IDX (&fuzzy_engine->outputs_to_be_removed, var, i) {
        if (!sml_fuzzy_find_variable(fuzzy_engine->fuzzy->output_list, var,
            &pos))
            continue;
        if (!sml_fuzzy_remove_variable(fuzzy_engine->fuzzy, var))
            continue;
        if (fuzzy_engine->last_stable_measure)
            sml_measure_remove_output_variable(
                fuzzy_engine->last_stable_measure, pos);
    }

    sol_ptr_vector_clear(&fuzzy_engine->inputs_to_be_removed);
    sol_ptr_vector_clear(&fuzzy_engine->outputs_to_be_removed);

    *removed = true;
    return 0;
}

static int
_remove_terms(struct sml_fuzzy_engine *fuzzy_engine, bool *removed)
{
    int error = 0;
    uint16_t i, var_num, term_num;
    bool terms_removed = false;
    struct sml_term_to_remove *to_remove;

    SOL_VECTOR_FOREACH_IDX (&fuzzy_engine->terms_to_be_removed, to_remove, i) {
        if (to_remove->is_input) {
            if (!sml_fuzzy_find_variable(fuzzy_engine->fuzzy->input_list,
                to_remove->var, &var_num))
                continue;
        } else {
            if (!sml_fuzzy_find_variable(fuzzy_engine->fuzzy->output_list,
                to_remove->var, &var_num))
                continue;
        }

        if (!sml_fuzzy_variable_find_term(to_remove->var, to_remove->term,
            &term_num))
            continue;

        //Will remove term
        if ((error = sml_observation_controller_remove_term(
                fuzzy_engine->observation_controller,
                var_num, term_num, to_remove->is_input)))
            goto _remove_terms_end;

        if ((error = sml_fuzzy_bridge_variable_remove_term(to_remove->var,
                term_num)))
            goto _remove_terms_end;

        sml_terms_manager_remove_term(&fuzzy_engine->terms_manager, var_num,
            term_num, to_remove->is_input);

        terms_removed = true;
    }
    sol_vector_clear(&fuzzy_engine->terms_to_be_removed);

_remove_terms_end:
    *removed = terms_removed;
    return error;
}

static int
_handle_removals(struct sml_fuzzy_engine *fuzzy_engine)
{
    int error;
    bool removed_terms = false, removed_variables = false;

    if ((error = _remove_variables(fuzzy_engine, &removed_variables)))
        return error;

    if ((error = _remove_terms(fuzzy_engine, &removed_terms)))
        return error;

    if (removed_terms || removed_variables)
        error = sml_observation_controller_post_remove_variables(
            fuzzy_engine->observation_controller);

    return error;
}

static void
_print_rule(const char *str, void *data)
{
    uint16_t *counter = (uint16_t *)data;

    *counter = *counter + 1;
    sml_debug("\t%s", str);
}

static int
_initialize_terms_list(struct sml_fuzzy *fuzzy, struct sml_variables_list *list)
{
    int error;
    uint16_t len, i;

    len = sml_fuzzy_variables_list_get_length(list);
    for (i = 0; i < len; i++) {
        struct sml_variable *var = sml_fuzzy_variables_list_index(list, i);
        if (!sml_fuzzy_variable_terms_count(var))
            if ((error = sml_terms_manager_initialize_variable(fuzzy, var)))
                return error;
    }

    return 0;
}

static int
_sml_process(struct sml_engine *engine)
{
    struct sml_fuzzy_engine *fuzzy_engine = (struct sml_fuzzy_engine *)engine;
    bool should_learn = false, should_act = false;
    int error;

    if (!sml_observation_controller_update_cache_size(
        fuzzy_engine->observation_controller,
        fuzzy_engine->engine.obs_max_size)) {
        sml_error("Failed to update observation cache size.");
        return -ENOMEM;
    }

    if ((error = _handle_removals(fuzzy_engine))) {
        sml_error("Failed to remove variables or terms.");
        return error;
    }

    if (fuzzy_engine->variable_terms_auto_balance) {
        error = _initialize_terms_list(fuzzy_engine->fuzzy,
            fuzzy_engine->fuzzy->input_list);
        if (error) {
            sml_error("Failed to initialize input list.");
            return error;
        }

        error = _initialize_terms_list(fuzzy_engine->fuzzy,
            fuzzy_engine->fuzzy->output_list);
        if (error) {
            sml_error("Failed to initialize output list.");
            return error;
        }
    }

    if ((error = sml_call_read_state_cb(&fuzzy_engine->engine))) {
        sml_error("Failed to read variables.");
        return error;
    }

    if ((error = _pre_process(fuzzy_engine, &should_act, &should_learn))) {
        sml_error("Failed to pre process.");
        return error;
    }

    if (should_act && (error = _act(fuzzy_engine, &should_learn))) {
        sml_error("Failed to process output.");
        return error;
    }

    if (should_learn && !fuzzy_engine->engine.learn_disabled &&
        (error = sml_observation_controller_observation_hit(
            fuzzy_engine->observation_controller,
            fuzzy_engine->last_stable_measure))) {
        sml_error("Failed to log observation.");
        return error;
    }

    if (fuzzy_engine->variable_terms_auto_balance &&
        (error = sml_terms_manager_hit(&fuzzy_engine->terms_manager,
            fuzzy_engine->fuzzy,
            fuzzy_engine->observation_controller,
            fuzzy_engine->last_stable_measure))) {
        sml_error("Failed to auto balance.");
        return error;
    }

    return 0;
}

static bool
_sml_predict(struct sml_engine *engine)
{
    struct sml_fuzzy_engine *fuzzy_engine = (struct sml_fuzzy_engine *)engine;

    if (!fuzzy_engine->fuzzy->input_terms_count ||
        !fuzzy_engine->fuzzy->output_terms_count)
        return false;

    return sml_fuzzy_process_output(fuzzy_engine->fuzzy) == 0;
}

static bool
_sml_save(struct sml_engine *engine, const char *path)
{
    char buf[SML_PATH_MAX];
    struct sml_fuzzy_engine *fuzzy_engine = (struct sml_fuzzy_engine *)engine;
    bool exists = file_exists(path);

    if (exists && !is_dir(path)) {
        sml_critical("Failed to save sml: %s is not a directory\n", path);
        return false;
    } else if (!exists && !create_dir(path)) {
        sml_critical("Could not create the directory:%s", path);
        return false;
    }

    if (!clean_dir(path, FUZZY_FILE_PREFIX)) {
        sml_critical("Failed to clear %s to save sml\n", path);
        return false;
    }

    snprintf(buf, sizeof(buf), "%s/%s", path, DEFAULT_FLL);
    if (!sml_fuzzy_save_file(fuzzy_engine->fuzzy, buf))
        return false;

    return sml_observation_controller_save_state(
        fuzzy_engine->observation_controller, path);
}

static bool
_sml_load(struct sml_engine *engine, const char *path)
{
    char buf[SML_PATH_MAX];
    struct sml_fuzzy_engine *fuzzy_engine = (struct sml_fuzzy_engine *)engine;

    if (!is_dir(path)) {
        sml_critical("Failed to load sml in directory %s\n", path);
        return false;
    }

    snprintf(buf, sizeof(buf), "%s/%s", path, DEFAULT_FLL);
    if (!_sml_load_fll_file(engine, buf))
        return false;

    return sml_observation_controller_load_state(
        fuzzy_engine->observation_controller, path);
}

static void
_sml_free(struct sml_engine *engine)
{
    struct sml_fuzzy_engine *fuzzy_engine = (struct sml_fuzzy_engine *)engine;

    sml_measure_free(fuzzy_engine->last_stable_measure);
    sml_observation_controller_free(fuzzy_engine->observation_controller);
    sml_terms_manager_clear(&fuzzy_engine->terms_manager);
    sol_ptr_vector_clear(&fuzzy_engine->inputs_to_be_removed);
    sol_vector_clear(&fuzzy_engine->terms_to_be_removed);
    sol_ptr_vector_clear(&fuzzy_engine->outputs_to_be_removed);
    sml_fuzzy_destroy(fuzzy_engine->fuzzy);
    free(fuzzy_engine);
}

API_EXPORT bool
sml_fuzzy_set_rule_weight_threshold(struct sml_object *sml,
    float weight_threshold)
{
    if (!sml_is_fuzzy(sml))
        return false;

    if (weight_threshold < 0 || weight_threshold > 1) {
        sml_warning("Invalid weight_threshold value. weight_threshold should "
            "be a value between 0 and 1.");
        return false;
    }
    struct sml_fuzzy_engine *fuzzy_engine = (struct sml_fuzzy_engine *)sml;

    sml_observation_controller_set_weight_threshold(
        fuzzy_engine->observation_controller,
        weight_threshold);
    return true;
}

API_EXPORT bool
sml_fuzzy_conjunction_set(struct sml_object *sml, enum sml_fuzzy_tnorm norm)
{
    if (!sml_is_fuzzy(sml))
        return false;
    struct sml_fuzzy_engine *fuzzy_engine = (struct sml_fuzzy_engine *)sml;

    return sml_fuzzy_bridge_conjunction_set(fuzzy_engine->fuzzy, norm);
}

static void
_sml_print_debug(struct sml_engine *engine, bool full)
{
    struct sml_fuzzy_engine *fuzzy_engine = (struct sml_fuzzy_engine *)engine;
    uint16_t counter = 0;

    if (full) {
        sml_observation_controller_debug(fuzzy_engine->observation_controller);
        sml_terms_manager_debug(&fuzzy_engine->terms_manager);
        sml_debug("Last Stable Measure:");
        if (fuzzy_engine->last_stable_measure)
            sml_measure_debug(fuzzy_engine->last_stable_measure,
                sml_matrix_float_convert);
        else
            sml_debug("\tNULL");
        sml_fuzzy_debug(fuzzy_engine->fuzzy);
    }

    sml_debug("Rules:");
    sml_observation_controller_rule_generate(
        fuzzy_engine->observation_controller, _print_rule, &counter);
    sml_debug("Total: %d\n", counter);
}

static struct sml_variables_list *
_sml_get_input_list(struct sml_engine *engine)
{
    struct sml_fuzzy_engine *fuzzy_engine = (struct sml_fuzzy_engine *)engine;

    return (struct sml_variables_list *)fuzzy_engine->fuzzy->input_list;
}

static struct sml_variables_list *
_sml_get_output_list(struct sml_engine *engine)
{
    struct sml_fuzzy_engine *fuzzy_engine = (struct sml_fuzzy_engine *)engine;

    return (struct sml_variables_list *)fuzzy_engine->fuzzy->output_list;
}

static bool
_sml_remove_variable(struct sml_engine *engine, struct sml_variable *variable)
{
    struct sml_fuzzy_engine *fuzzy_engine = (struct sml_fuzzy_engine *)engine;

    if (sml_fuzzy_is_input(fuzzy_engine->fuzzy, variable, NULL)) {
        if (sol_ptr_vector_append(&fuzzy_engine->inputs_to_be_removed,
            variable)) {
            sml_critical("Could not add input variable to the remove list");
            return false;
        }
        return true;
    }
    if (sml_fuzzy_is_output(fuzzy_engine->fuzzy, variable, NULL)) {
        if (sol_ptr_vector_append(&fuzzy_engine->outputs_to_be_removed,
            variable)) {
            sml_critical("Could not add output variable to the remove list");
            return false;
        }
        return true;
    }

    sml_critical("Failed to remove. Variable not in fuzzy engine.");
    return false;
}

API_EXPORT bool
sml_fuzzy_variable_remove_term(struct sml_object *sml, struct sml_variable *var,
    struct sml_fuzzy_term *term)
{
    if (!sml_is_fuzzy(sml))
        return false;

    struct sml_fuzzy_engine *fuzzy_engine = (struct sml_fuzzy_engine *)sml;
    bool is_input;
    struct sml_term_to_remove *to_remove;

    if (sml_fuzzy_is_input(fuzzy_engine->fuzzy, var, NULL))
        is_input = true;
    else if (sml_fuzzy_is_output(fuzzy_engine->fuzzy, var, NULL))
        is_input = false;
    else {
        sml_critical("Failed to remove term. Variable not in fuzzy engine.");
        return false;
    }

    if (!sml_fuzzy_variable_find_term(var, term, NULL)) {
        sml_critical("Failed to remove term. Term not in Variable.");
        return false;
    }

    to_remove = sol_vector_append(&fuzzy_engine->terms_to_be_removed);
    to_remove->is_input = is_input;
    to_remove->term = term;
    to_remove->var = var;

    return true;
}


static int
_sml_variable_set_enabled(struct sml_engine *engine,
    struct sml_variable *variable, bool enabled)
{
    struct sml_fuzzy_engine *fuzzy_engine = (struct sml_fuzzy_engine *)engine;

    if (enabled == sml_fuzzy_variable_is_enabled(variable))
        return 0;

    sml_fuzzy_variable_set_enabled(variable, enabled);
    return sml_observation_controller_variable_set_enabled(
        fuzzy_engine->observation_controller, enabled);
}


static struct sml_variable *
_sml_get_input(struct sml_engine *engine, const char *name)
{
    return sml_fuzzy_get_input(sml_get_fuzzy(engine), name);
}

static struct sml_variable *
_sml_get_output(struct sml_engine *engine, const char *name)
{
    return sml_fuzzy_get_output(sml_get_fuzzy(engine), name);
}

static struct sml_variable *
_sml_new_input(struct sml_engine *engine, const char *name)
{
    return sml_fuzzy_new_input(sml_get_fuzzy(engine), name);
}

static struct sml_variable *
_sml_new_output(struct sml_engine *engine, const char *name)
{
    struct sml_variable *variable;

    variable = sml_fuzzy_new_output(sml_get_fuzzy(engine), name);
    return variable;
}

static bool
_sml_variable_set_value(struct sml_variable *sml_variable, float value)
{
    if (!sml_fuzzy_variable_is_enabled(sml_variable)) {
        char var_name[SML_VARIABLE_NAME_MAX_LEN + 1];

        if (sml_fuzzy_variable_get_name(sml_variable, var_name,
            sizeof(var_name))) {
            sml_warning("Trying to set a value in a disabled variable: %p.",
                sml_variable);
        } else
            sml_warning("Trying to set a value in a disabled variable: %s.",
                var_name);
    }

    sml_fuzzy_variable_set_value(sml_variable, value);
    return true;
}

static bool
_rearrange_fuzzy_terms(struct sml_engine *engine,
    struct sml_variable *variable, float min, float max)
{
    struct sml_fuzzy_term *term, *first_term = NULL, *last_term = NULL;
    struct sml_fuzzy_engine *fuzzy_engine = (struct sml_fuzzy_engine *)engine;
    struct sml_fuzzy *fuzzy = fuzzy_engine->fuzzy;
    float width, term_min, term_max, first_min, first_max, last_max, last_min;
    float overlap;
    uint16_t i, num_terms;
    bool is_id;
    int error;

    width = sml_fuzzy_bridge_variable_get_default_term_width(fuzzy, variable);
    if (isnan(width))
        return true;

    is_id = sml_fuzzy_bridge_variable_get_is_id(fuzzy, variable);
    overlap = width * DEFAULT_OVERLAP_PERCENTAGE;
    first_min = max;
    first_max = max;
    last_max = min;
    last_min = min;
    num_terms = sml_fuzzy_variable_terms_count(variable);
    for (i = 0; i < num_terms; i++) {
        term = sml_fuzzy_variable_get_term(variable, i);
        if (!sml_fuzzy_term_get_range(term, &term_min, &term_max))
            continue; //Not a term created by fuzzy engine

        //if term out of range
        if (term_max < min || term_min > max)
            sml_fuzzy_variable_remove_term((struct sml_object *)engine,
                variable, term);
        else {
            if (term_min <= first_min) {
                first_min = term_min;
                first_max = term_max;
                first_term = term;
            }
            if (term_max >= last_max) {
                last_max = term_max;
                last_min = term_min;
                last_term = term;
            }
        }
    }

    if (first_term && (min < first_min)) {
        if (first_max - min <= width) {
            if (!sml_fuzzy_bridge_variable_term_triangle_update(first_term, min,
                min, first_max))
                return false;
        } else {
            first_max = first_max - overlap;
            first_min = first_max - width;
            if (!sml_fuzzy_bridge_variable_term_triangle_update(first_term,
                first_min - overlap, first_min + (first_max - first_min) / 2,
                first_max + overlap))
                return false;
            if (is_id)
                first_min -= width / 2;
            if ((error = _create_fuzzy_terms(fuzzy_engine, variable, min,
                    first_min, true, false)))
                return false;
        }
    }

    if (last_term && (max > last_max)) {
        if (max - last_min <= width) {
            if (!sml_fuzzy_bridge_variable_term_triangle_update(last_term,
                last_min, max, max))
                return false;
        } else {
            last_min = last_min + overlap;
            last_max = last_min + width;
            if (!sml_fuzzy_bridge_variable_term_triangle_update(last_term,
                last_min - overlap, last_min + (last_max - last_min) / 2,
                last_max + overlap))
                return false;
            if ((error = _create_fuzzy_terms(fuzzy_engine, variable, last_max,
                    max, false, true)))
                return false;
        }
    }

    if (!(first_term || last_term))
        if ((error = _create_fuzzy_terms(fuzzy_engine, variable, min, max,
                true, true)))
            return false;

    return true;
}

static bool
_fuzzy_variable_set_range(struct sml_engine *engine,
    struct sml_variable *variable, float min, float max)
{
    sml_fuzzy_bridge_variable_set_range(variable, min, max);
    return _rearrange_fuzzy_terms(engine, variable, min, max);
}

API_EXPORT struct sml_fuzzy_term *
sml_fuzzy_variable_add_term_rectangle(struct sml_object *sml,
    struct sml_variable *variable, const char *name, float start, float end)
{
    ON_NULL_RETURN_VAL(variable, false);
    if (!sml_is_fuzzy(sml))
        return false;

    return sml_fuzzy_bridge_variable_add_term_rectangle(
        sml_get_fuzzy((struct sml_engine *)sml),
        variable, name, start, end);
}

API_EXPORT struct sml_fuzzy_term *
sml_fuzzy_variable_add_term_triangle(struct sml_object *sml,
    struct sml_variable *variable, const char *name, float vertex_a,
    float vertex_b, float vertex_c)
{
    ON_NULL_RETURN_VAL(variable, false);
    if (!sml_is_fuzzy(sml))
        return false;

    return sml_fuzzy_bridge_variable_add_term_triangle(
        sml_get_fuzzy((struct sml_engine *)sml),
        variable, name, vertex_a, vertex_b, vertex_c);
}

API_EXPORT struct sml_fuzzy_term *
sml_fuzzy_variable_add_term_cosine(struct sml_object *sml,
    struct sml_variable *variable, const char *name, float center, float width)
{
    ON_NULL_RETURN_VAL(variable, false);
    if (!sml_is_fuzzy(sml))
        return false;

    return sml_fuzzy_bridge_variable_add_term_cosine(
        sml_get_fuzzy((struct sml_engine *)sml),
        variable, name, center, width);
}

API_EXPORT struct sml_fuzzy_term *
sml_fuzzy_variable_add_term_gaussian(struct sml_object *sml,
    struct sml_variable *variable, const char *name, float mean,
    float standard_deviation)
{
    ON_NULL_RETURN_VAL(variable, false);
    if (!sml_is_fuzzy(sml))
        return false;

    return sml_fuzzy_bridge_variable_add_term_gaussian(
        sml_get_fuzzy((struct sml_engine *)sml),
        variable, name, mean, standard_deviation);
}

API_EXPORT struct sml_fuzzy_term *
sml_fuzzy_variable_add_term_ramp(struct sml_object *sml,
    struct sml_variable *variable, const char *name, float start, float end)
{
    ON_NULL_RETURN_VAL(variable, false);
    if (!sml_is_fuzzy(sml))
        return false;

    return sml_fuzzy_bridge_variable_add_term_ramp(
        sml_get_fuzzy((struct sml_engine *)sml), variable,
        name, start, end);
}

static struct sml_variable *
_fuzzy_variables_list_index(struct sml_variables_list *list, unsigned int index)
{
    if (index > UINT16_MAX)
        return NULL;

    return sml_fuzzy_variables_list_index(list, index);
}

API_EXPORT bool
sml_fuzzy_set_simplification_disabled(struct sml_object *sml, bool disabled)
{
    if (!sml_is_fuzzy(sml))
        return false;
    struct sml_fuzzy_engine *fuzzy_engine = (struct sml_fuzzy_engine *)sml;

    sml_observation_controller_set_simplification_disabled(
        fuzzy_engine->observation_controller, disabled);
    return true;
}


static bool
_sml_fuzzy_erase_knowledge(struct sml_engine *engine)
{
    struct sml_fuzzy_engine *fuzzy_engine = (struct sml_fuzzy_engine *)engine;

    sml_fuzzy_erase_rules(fuzzy_engine->fuzzy);
    sml_measure_free(fuzzy_engine->last_stable_measure);
    sml_observation_controller_clear(fuzzy_engine->observation_controller);
    sml_terms_manager_clear(&fuzzy_engine->terms_manager);
    fuzzy_engine->last_stable_measure = NULL;
    fuzzy_engine->engine.hits = 0;
    return true;
}

API_EXPORT struct sml_object *
sml_fuzzy_new(void)
{
    struct sml_fuzzy_engine *fuzzy_engine = calloc(1,
        sizeof(struct sml_fuzzy_engine));

    if (!fuzzy_engine)
        goto error_sml;

    fuzzy_engine->fuzzy = sml_fuzzy_bridge_new();
    if (!fuzzy_engine->fuzzy)
        goto error_fuzzy;

    fuzzy_engine->observation_controller =
        sml_observation_controller_new(fuzzy_engine->fuzzy);
    if (!fuzzy_engine->observation_controller)
        goto error_observation_controller;

    sml_terms_manager_init(&fuzzy_engine->terms_manager);
    sol_ptr_vector_init(&fuzzy_engine->inputs_to_be_removed);
    sol_vector_init(&fuzzy_engine->terms_to_be_removed,
        sizeof(struct sml_term_to_remove));
    sol_ptr_vector_init(&fuzzy_engine->outputs_to_be_removed);
    fuzzy_engine->last_stable_measure = NULL;

    fuzzy_engine->engine.load_file = _sml_load_fll_file;
    fuzzy_engine->engine.free = _sml_free;
    fuzzy_engine->engine.process = _sml_process;
    fuzzy_engine->engine.predict = _sml_predict;
    fuzzy_engine->engine.save = _sml_save;
    fuzzy_engine->engine.load = _sml_load;
    fuzzy_engine->engine.get_input_list = _sml_get_input_list;
    fuzzy_engine->engine.get_output_list = _sml_get_output_list;
    fuzzy_engine->engine.new_input = _sml_new_input;
    fuzzy_engine->engine.new_output = _sml_new_output;
    fuzzy_engine->engine.get_input = _sml_get_input;
    fuzzy_engine->engine.get_output = _sml_get_output;
    fuzzy_engine->engine.get_name = sml_fuzzy_variable_get_name;
    fuzzy_engine->engine.set_value = _sml_variable_set_value;
    fuzzy_engine->engine.get_value = sml_fuzzy_variable_get_value;
    fuzzy_engine->engine.variable_set_enabled = _sml_variable_set_enabled;
    fuzzy_engine->engine.variable_is_enabled = sml_fuzzy_variable_is_enabled;
    fuzzy_engine->engine.remove_variable = _sml_remove_variable;
    fuzzy_engine->engine.variables_list_get_length =
        sml_fuzzy_variables_list_get_length;
    fuzzy_engine->engine.variables_list_index = _fuzzy_variables_list_index;
    fuzzy_engine->engine.variable_set_range = _fuzzy_variable_set_range;
    fuzzy_engine->engine.variable_get_range = sml_fuzzy_variable_get_range;
    fuzzy_engine->engine.print_debug = _sml_print_debug;
    fuzzy_engine->engine.erase_knowledge = _sml_fuzzy_erase_knowledge;
    fuzzy_engine->engine.magic_number = FUZZY_MAGIC;

    return (struct sml_object *)&fuzzy_engine->engine;

error_observation_controller:
    sml_fuzzy_destroy(fuzzy_engine->fuzzy);
error_fuzzy:
    free(fuzzy_engine);
error_sml:
    sml_critical("Failed to create struct sml");
    return NULL;
}

API_EXPORT bool
sml_is_fuzzy(struct sml_object *sml)
{
    struct sml_fuzzy_engine *fuzzy_engine = (struct sml_fuzzy_engine *)sml;

    ON_NULL_RETURN_VAL(sml, false);
    return fuzzy_engine->engine.magic_number == FUZZY_MAGIC;
}

API_EXPORT bool
sml_fuzzy_output_set_defuzzifier(struct sml_object *sml,
    struct sml_variable *var, enum sml_fuzzy_defuzzifier defuzzifier,
    int defuzzifier_resolution)
{
    ON_NULL_RETURN_VAL(var, false);
    if (!sml_is_fuzzy(sml))
        return false;

    return sml_fuzzy_bridge_output_set_defuzzifier(var, defuzzifier,
        defuzzifier_resolution);
}

API_EXPORT bool
sml_fuzzy_output_set_accumulation(struct sml_object *sml,
    struct sml_variable *var, enum sml_fuzzy_snorm accumulation)
{
    ON_NULL_RETURN_VAL(var, false);
    if (!sml_is_fuzzy(sml))
        return false;

    return sml_fuzzy_bridge_output_set_accumulation(var, accumulation);
}

API_EXPORT bool
sml_fuzzy_supported(void)
{
    return true;
}

API_EXPORT bool
sml_fuzzy_variable_set_default_term_width(struct sml_object *sml,
    struct sml_variable *var, float width)
{
    ON_NULL_RETURN_VAL(var, false);
    if (!sml_is_fuzzy(sml))
        return false;

    return sml_fuzzy_bridge_variable_set_default_term_width(
        sml_get_fuzzy((struct sml_engine *)sml), var, width);
}

API_EXPORT float
sml_fuzzy_variable_get_default_term_width(struct sml_object *sml,
    struct sml_variable *var)
{
    ON_NULL_RETURN_VAL(var, NAN);
    if (!sml_is_fuzzy(sml))
        return NAN;

    return sml_fuzzy_bridge_variable_get_default_term_width(
        sml_get_fuzzy((struct sml_engine *)sml), var);
}

API_EXPORT bool
sml_fuzzy_variable_set_is_id(struct sml_object *sml,
    struct sml_variable *var, bool is_id)
{
    ON_NULL_RETURN_VAL(var, NAN);
    if (!sml_is_fuzzy(sml))
        return NAN;

    return sml_fuzzy_bridge_variable_set_is_id(
        sml_get_fuzzy((struct sml_engine *)sml), var, is_id);
}

API_EXPORT bool
sml_fuzzy_variable_get_is_id(struct sml_object *sml,
    struct sml_variable *var)
{
    ON_NULL_RETURN_VAL(var, NAN);
    if (!sml_is_fuzzy(sml))
        return NAN;

    return sml_fuzzy_bridge_variable_get_is_id(
        sml_get_fuzzy((struct sml_engine *)sml), var);
}
