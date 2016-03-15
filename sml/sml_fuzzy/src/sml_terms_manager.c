/*
 * This file is part of the Soletta Project
 *
 * Copyright (C) 2015 Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sml_terms_manager.h"
#include "macros.h"
#include "sml_log.h"
#include "sml_string.h"
#include <errno.h>
#include <math.h>

#define START_TERMS_COUNT (16)
#define OVERLAP_RATIO (0.1)
#define TERM_PREFIX "%s_TERM_%d"
#define TERM_SPLIT_PREFIX "TERM_SPLIT_%s_%d"
#define TERM_LEN (150)
#define MAX_HIT (500)

#define MAX_CAP (MAX_HIT / 1.5)
#define MIN_CAP (MAX_CAP / 20 + 1)
#define FLOAT_THREASHOLD 0.01

static bool
_create_term(struct sml_fuzzy *fuzzy, struct sml_variable *var,
    const char *name, float min, float max)
{
    float var_min, var_max;

    sml_fuzzy_variable_get_range(var, &var_min, &var_max);

    if (fabs(var_min - min) < FLOAT_THREASHOLD)
        return sml_fuzzy_variable_add_term_ramp((struct sml_object *)fuzzy,
            var, name, min, max);
    if (fabs(var_max - max) < FLOAT_THREASHOLD)
        return sml_fuzzy_variable_add_term_ramp((struct sml_object *)fuzzy,
            var, name, max, min);

    return sml_fuzzy_variable_add_term_triangle((struct sml_object *)fuzzy,
        var, name, min, min + (max - min) / 2, max);
}

static bool
_split(struct sml_fuzzy *fuzzy,
    struct sml_observation_controller *obs_controller,
    struct sml_variables_list *list, struct sml_matrix *variable_hits,
    uint16_t var_num, uint16_t term_num, int *error)
{
    char buf[TERM_LEN];
    float min, max, step, overlap;
    struct sml_fuzzy_term *term;
    struct sml_variable *var;
    uint16_t cur_hits, new_hits, terms_len, i, *val;
    char term_name[SML_TERM_NAME_MAX_LEN + 1];
    void *tmp;

    *error = 0;
    var = sml_fuzzy_variables_list_index(list, var_num);
    term = sml_fuzzy_variable_get_term(var, term_num);
    if (!sml_fuzzy_term_get_range(term, &min, &max))
        return false;

    if (sml_fuzzy_term_get_name(term, term_name, sizeof(term_name)))
        return false;

    cur_hits = sml_matrix_cast_get(variable_hits, var_num, term_num, tmp,
        uint16_t);
    new_hits = cur_hits / 2;
    step = ((max - min) / 2);
    overlap = step * OVERLAP_RATIO;

    //create new terms in fuzzy
    snprintf(buf, TERM_LEN, TERM_SPLIT_PREFIX, term_name, 0);
    if (!_create_term(fuzzy, var, buf, min, min + step + overlap)) {
        *error = SML_INTERNAL_ERROR;
        return false;
    }

    snprintf(buf, TERM_LEN, TERM_SPLIT_PREFIX, term_name, 1);
    if (!_create_term(fuzzy, var, buf, max - step - overlap, max)) {
        *error = SML_INTERNAL_ERROR;
        return false;
    }

    //Add hits for terms just created
    terms_len = sml_fuzzy_variable_terms_count(var);
    for (i = 0; i < 2; i++) {
        val = sml_matrix_insert(variable_hits, var_num, terms_len - i - 1);
        if (!val) {
            *error = -ENOMEM;
            return false;
        }
        *val = new_hits;
    }

    //Update controller
    if ((*error = sml_observation_controller_split_terms(fuzzy, obs_controller,
            var_num, term_num, terms_len - 1, terms_len - 2,
            list == fuzzy->input_list)))
        return false;

    if ((*error = sml_fuzzy_bridge_variable_remove_term(var, term_num)))
        return false;

    sml_matrix_remove_col(variable_hits, var_num, term_num);
    return true;
}

static bool
_merge(struct sml_fuzzy *fuzzy,
    struct sml_observation_controller *obs_controller,
    struct sml_variables_list *list, struct sml_matrix *variable_hits,
    uint16_t var_num, uint16_t term_num, int *error)
{
    float min, max, cur_min, cur_max;
    uint16_t len, i, val, found_term_num, *cur_hits, *found_hits = NULL;
    void *tmp;
    struct sml_fuzzy_term *term, *cur_term, *found_term = NULL;
    struct sml_variable *var;

    *error = 0;
    var = sml_fuzzy_variables_list_index(list, var_num);
    term = sml_fuzzy_variable_get_term(var, term_num);
    if (!sml_fuzzy_term_get_range(term, &min, &max))
        return false;

    //get the overlaping term with less hits
    len = sml_fuzzy_variable_terms_count(var);
    for (i = 0; i < len; i++) {
        cur_term = sml_fuzzy_variable_get_term(var, i);
        cur_hits = sml_matrix_get(variable_hits, var_num, i);
        if (cur_term == term ||
            (found_hits != NULL && *cur_hits >= *found_hits))
            continue;

        if (!sml_fuzzy_term_get_range(cur_term, &cur_min, &cur_max))
            continue;

        if ((cur_min >= min && cur_min <= max) ||
            (min >= cur_min && min <= cur_max)) {
            found_hits = cur_hits;
            found_term = cur_term;
            found_term_num = i;
        }
    }

    if (found_term) {
        if (!sml_fuzzy_term_get_range(found_term, &cur_min, &cur_max))
            return false;
        sml_fuzzy_term_set_range(found_term, fmin(min, cur_min),
            fmax(max, cur_max));
        if ((*error = sml_observation_controller_merge_terms(obs_controller,
                var_num, found_term_num, term_num,
                list == fuzzy->input_list)))
            return false;
        if ((*error = sml_fuzzy_bridge_variable_remove_term(var, term_num)))
            return false;

        val = sml_matrix_cast_get(variable_hits, var_num, term_num, tmp,
            uint16_t);
        *found_hits = *found_hits + val;
        sml_matrix_remove_col(variable_hits, var_num, term_num);
    }

    return true;
}

static bool
_should_split(uint16_t val)
{
    //TODO: Improve
    return val > MAX_CAP;
}

static bool
_should_merge(uint16_t val)
{
    //TODO: Improve
    return val < MIN_CAP;
}

static int
_hit(struct sml_fuzzy *fuzzy, struct sml_variables_list *list,
    struct sml_observation_controller *obs_controller,
    struct sml_matrix *values, struct sml_matrix *variable_hits, bool rebuild)
{
    float measured_val;
    uint16_t i, j, len, *val, terms_len;
    int error;
    void *tmp;
    struct sml_variable *var;
    bool changed = false;

    len = sml_fuzzy_variables_list_get_length(list);
    for (i = 0; i < len; i++) {
        var = sml_fuzzy_variables_list_index(list, i);
        terms_len = sml_fuzzy_variable_terms_count(var);
        for (j = 0; j < terms_len; j++) {
            measured_val = sml_matrix_cast_get(values, i, j, tmp, float);
            struct sml_fuzzy_term *term = sml_fuzzy_variable_get_term(var, j);
            float min = NAN, max = NAN;

            if (!sml_fuzzy_term_get_range(term, &min, &max))
                return -EINVAL;

            val = sml_matrix_insert(variable_hits, i, j);
            if (measured_val >= VARIABLE_MEMBERSHIP_THRESHOLD)
                *val = *val + 1;
            if (rebuild) {
                if (_should_split(*val)) {
                    if (_split(fuzzy, obs_controller, list, variable_hits, i, j,
                        &error)) {
                        j--;
                        terms_len--;
                        changed = true;
                    } else if (error)
                        return error;
                } else if (_should_merge(*val)) {
                    if (_merge(fuzzy, obs_controller, list, variable_hits, i, j,
                        &error)) {
                        j--;
                        terms_len--;
                        changed = true;
                    } else if (error)
                        return error;
                }
            }
        }
    }
    if (rebuild)
        SML_MATRIX_FOREACH_IDX(variable_hits, tmp, val, i, j)
        * val = *val / 2;

    if (changed)
        sml_observation_controller_post_remove_variables(obs_controller);

    return 0;
}

static void
_remove(struct sml_matrix *variable_hits, bool *to_remove)
{
    uint16_t i, len, removed = 0;


    if (!to_remove)
        return;

    len = sml_matrix_lines(variable_hits);
    for (i = 0; i < len; i++) {
        if (to_remove[i]) {
            sml_matrix_remove_line(variable_hits, i - removed);
            removed++;
        }
    }
}

void
sml_terms_manager_init(struct sml_terms_manager *terms_manager)
{
    sml_measure_init(&terms_manager->hits, sizeof(uint16_t), sizeof(uint16_t));
    terms_manager->total = 0;
}

void
sml_terms_manager_clear(struct sml_terms_manager *terms_manager)
{
    sml_measure_clear(&terms_manager->hits);
    terms_manager->total = 0;
}

int
sml_terms_manager_remove_variables(struct sml_terms_manager *terms_manager,
    bool *inputs_to_remove,
    bool *outputs_to_remove)
{
    _remove(&terms_manager->hits.inputs, inputs_to_remove);
    _remove(&terms_manager->hits.outputs, outputs_to_remove);

    return 0;
}

int
sml_terms_manager_hit(struct sml_terms_manager *terms_manager,
    struct sml_fuzzy *fuzzy, struct sml_observation_controller *obs_controller,
    struct sml_measure *measure)
{
    int error;

    terms_manager->total++;
    if (terms_manager->total == MAX_HIT)
        terms_manager->total = 0;

    if ((error = _hit(fuzzy, fuzzy->input_list, obs_controller,
            &measure->inputs, &terms_manager->hits.inputs,
            terms_manager->total == 0)))
        return error;

    if ((error = _hit(fuzzy, fuzzy->output_list, obs_controller,
            &measure->outputs, &terms_manager->hits.outputs,
            terms_manager->total == 0)))
        return error;

    return 0;
}

void
sml_terms_manager_debug(struct sml_terms_manager *terms_manager)
{
    sml_debug("Terms_Manager {");
    sml_debug("\tInputs (%d) {", sml_matrix_lines(&terms_manager->hits.inputs));
    sml_matrix_debug_uint16_t(&terms_manager->hits.inputs);
    sml_debug("\t}");
    sml_debug("\tOutputs (%d) {",
        sml_matrix_lines(&terms_manager->hits.outputs));
    sml_matrix_debug_uint16_t(&terms_manager->hits.inputs);
    sml_matrix_debug_uint16_t(&terms_manager->hits.outputs);
    sml_debug("\t}");
    sml_debug("}");
}

int
sml_terms_manager_initialize_variable(struct sml_fuzzy *fuzzy,
    struct sml_variable *var)
{
    //TODO: Improve terms creation
    float min, max, range, step, overlap;
    char var_name[SML_VARIABLE_NAME_MAX_LEN + 1];
    char buf[TERM_LEN];
    uint16_t i;

    sml_fuzzy_variable_get_range(var, &min, &max);

    if (sml_fuzzy_variable_get_name(var, var_name, sizeof(var_name)))
        return -ENODATA;

    range = max - min;
    step = range / START_TERMS_COUNT;
    overlap = step * OVERLAP_RATIO;

    snprintf(buf, TERM_LEN, TERM_PREFIX, var_name, 0);
    if (!sml_fuzzy_variable_add_term_ramp((struct sml_object *)fuzzy,
        var, buf, min, min + step + overlap))
        return SML_INTERNAL_ERROR;

    for (i = 1; i < START_TERMS_COUNT - 1; i++) {
        snprintf(buf, TERM_LEN, TERM_PREFIX, var_name, i);
        if (!sml_fuzzy_variable_add_term_triangle((struct sml_object *)fuzzy,
            var, buf, min + step * i - overlap, min + step / 2,
            min + step * (i + 1) + overlap))
            return SML_INTERNAL_ERROR;
    }
    snprintf(buf, TERM_LEN, TERM_PREFIX, var_name, START_TERMS_COUNT - 1);
    if (!sml_fuzzy_variable_add_term_ramp((struct sml_object *)fuzzy,
        var, buf, max, max - step - overlap))
        return SML_INTERNAL_ERROR;

    return 0;
}

void
sml_terms_manager_remove_term(struct sml_terms_manager *terms_manager,
    uint16_t var_num, uint16_t term_num, bool is_input)
{
    if (is_input)
        sml_matrix_remove_col(&terms_manager->hits.inputs, var_num, term_num);
    else
        sml_matrix_remove_col(&terms_manager->hits.outputs, var_num, term_num);
}
