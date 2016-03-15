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

#include "sml_observation.h"
#include "sml_fuzzy_bridge.h"
#include "sml_bit_array.h"
#include <macros.h>
#include <sml_log.h>
#include <sml_string.h>

#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>

#define RULE_WEIGHT 1
#define WEIGHT_THRESHOLD (0.1)
#define FLOAT_THRESHOLD (0.01)

struct sml_observation {
    //Replace this structure with a struct sml_matrix
    struct sol_vector output_weights;
    struct sol_vector input_membership;
};

static void
_input_set(struct sml_observation *observation, uint16_t input, uint16_t term,
    uint8_t value)
{
    struct sml_bit_array *array;

    array = sol_vector_get(&observation->input_membership, input);
    if (!array)
        return;

    sml_bit_array_set(array, term, value);
}


static int
_input_membership_initialize(struct sml_fuzzy *fuzzy,
    struct sml_observation *obs)
{
    uint16_t len, i, terms_len;
    struct sml_bit_array *array;
    struct sml_variable *var;
    int error;

    len = sml_fuzzy_variables_list_get_length(fuzzy->input_list);
    for (i = 0; i < len; i++) {
        if (i >= obs->input_membership.len) {
            array = sol_vector_append(&obs->input_membership);
            if (!array)
                return -ENOMEM;
            sml_bit_array_init(array);
        } else
            array = sol_vector_get(&obs->input_membership, i);

        var = sml_fuzzy_variables_list_index(fuzzy->input_list, i);
        terms_len = sml_fuzzy_variable_terms_count(var);
        error = sml_bit_array_size_set(array, terms_len, 0);
        if (error != 0)
            return error;
    }

    return 0;
}

static int
_output_weights_initialize(struct sml_fuzzy *fuzzy, struct sml_observation *obs)
{
    uint16_t len, i, j, terms_len;
    uint8_t *val;
    struct sol_vector *vector;
    struct sml_variable *var;

    len = sml_fuzzy_variables_list_get_length(fuzzy->output_list);
    for (i = 0; i < len; i++) {
        if (i >= obs->output_weights.len) {
            vector = sol_vector_append(&obs->output_weights);
            if (!vector)
                return -ENOMEM;
            sol_vector_init(vector, sizeof(uint8_t));
        } else
            vector = sol_vector_get(&obs->output_weights, i);

        var = sml_fuzzy_variables_list_index(fuzzy->output_list, i);
        terms_len = sml_fuzzy_variable_terms_count(var);
        for (j = vector->len; j < terms_len; j++) {
            val = sol_vector_append(vector);
            *val = 0;
        }
    }
    return 0;
}

static void
_input_membership_free(struct sml_observation *observation)
{
    uint16_t i;

    for (i = 0; i < observation->input_membership.len; i++)
        sml_bit_array_clear(sol_vector_get(&observation->input_membership, i));
    sol_vector_clear(&observation->input_membership);
}

static void
_output_weights_free(struct sml_observation *observation)
{
    uint16_t i;

    for (i = 0; i < observation->output_weights.len; i++)
        sol_vector_clear(sol_vector_get(&observation->output_weights, i));
    sol_vector_clear(&observation->output_weights);
}

static int
_output_set(struct sml_fuzzy *fuzzy, struct sml_observation *obs, uint16_t
    output,
    uint16_t term, uint8_t data)
{
    struct sol_vector *vector;
    uint8_t *val;
    int error;

    if (output >= obs->output_weights.len ||
        term >= *((uint8_t *)sol_vector_get(&obs->output_weights, output))) {
        if ((error = _output_weights_initialize(fuzzy, obs)))
            return error;
    }

    vector = sol_vector_get(&obs->output_weights, output);
    if (!vector)
        return -ENODATA;

    val = sol_vector_get(vector, term);
    if (!val)
        return -ENODATA;

    *val = data;
    return 0;
}

static uint8_t
_output_get(struct sml_observation *observation, uint16_t output, uint16_t term)
{
    struct sol_vector *terms;
    uint8_t *val;

    if (output >= observation->output_weights.len)
        return 0;

    terms = sol_vector_get(&observation->output_weights, output);
    if (term >= terms->len)
        return 0;

    val = sol_vector_get(terms, term);
    if (!val)
        return 0;
    return *val;
}

static struct sml_observation *
_observation_alloc(struct sml_fuzzy *fuzzy)
{
    struct sml_observation *observation = malloc(
        sizeof(struct sml_observation));

    if (!observation)
        return NULL;

    sol_vector_init(&observation->output_weights, sizeof(struct sol_vector));
    if (_output_weights_initialize(fuzzy, observation))
        goto alloc_error;

    sol_vector_init(&observation->input_membership,
        sizeof(struct sml_bit_array));
    if (_input_membership_initialize(fuzzy, observation) != 0)
        goto alloc_error;

    return observation;

alloc_error:
    _output_weights_free(observation);
    _input_membership_free(observation);
    free(observation);
    return NULL;
}

static bool
_set_input_observations_values(struct sml_observation *observation,
    struct sml_variables_list *list, struct sml_matrix *values)
{
    uint16_t len, i, j;
    float *tmp, val;
    bool set = false;

    len = sml_fuzzy_variables_list_get_length(list);
    for (i = 0; i < len; i++) {
        struct sml_variable *variable = sml_fuzzy_variables_list_index(list, i);
        uint16_t terms_len = sml_fuzzy_variable_terms_count(variable);
        if (sml_fuzzy_variable_is_enabled(variable)) {
            for (j = 0; j < terms_len; j++) {
                val = sml_matrix_cast_get(values, i, j, tmp, float);
                if (val > VARIABLE_MEMBERSHIP_THRESHOLD) {
                    _input_set(observation, i, j, SET);
                    set = true;
                } else
                    _input_set(observation, i, j, UNSET);
            }
        }
    }

    return set;
}

uint8_t
sml_observation_input_term_get(struct sml_observation *obs, uint16_t input,
    uint16_t term)
{
    struct sml_bit_array *array;

    array = sol_vector_get(&obs->input_membership, input);
    if (!array)
        return 0;

    return sml_bit_array_get(array, term);
}

void
sml_observation_free(struct sml_observation *observation)
{
    _output_weights_free(observation);
    _input_membership_free(observation);
    free(observation);
}

int
sml_observation_new(struct sml_fuzzy *fuzzy, struct sml_measure *measure,
    struct sml_observation **observation)
{
    struct sml_observation *tmp_obs;

    if (!observation)
        return -EINVAL;

    if (!fuzzy->output_terms_count || !fuzzy->input_terms_count) {
        *observation = NULL;
        return 0;
    }

    tmp_obs = _observation_alloc(fuzzy);
    if (!tmp_obs) {
        sml_critical("Failed to create new tmp_obs");
        return -errno;
    }

    if (!_set_input_observations_values(tmp_obs, fuzzy->input_list,
        &measure->inputs)) {
        sml_observation_free(tmp_obs);
        *observation = NULL;
        return 0;
    }

    *observation = tmp_obs;
    return 0;
}

bool
sml_observation_enabled_input_values_equals(struct sml_fuzzy *fuzzy,
    struct sml_observation *observation,
    struct sml_measure *measure)
{
    uint16_t i, j, len;
    float val, *tmp;
    struct sml_variable *var;

    len = sml_fuzzy_variables_list_get_length(fuzzy->input_list);
    for (i = 0; i < len; i++) {
        var = sml_fuzzy_variables_list_index(fuzzy->input_list, i);
        uint16_t terms_len = sml_fuzzy_variable_terms_count(var);
        if (sml_fuzzy_variable_is_enabled(var)) {
            for (j = 0; j < terms_len; j++) {
                val = sml_matrix_cast_get(&measure->inputs, i, j, tmp, float);
                uint8_t set = val > VARIABLE_MEMBERSHIP_THRESHOLD;
                if (sml_observation_input_term_get(observation, i, j) != set)
                    return false;
            }
        }
    }

    return true;
}

bool
sml_observation_enabled_input_equals(struct sml_fuzzy *fuzzy,
    struct sml_observation *obs1,
    struct sml_observation *obs2)
{
    uint16_t i, j, len;
    struct sml_variable *var;

    len = sml_fuzzy_variables_list_get_length(fuzzy->input_list);
    for (i = 0; i < len; i++) {
        var = sml_fuzzy_variables_list_index(fuzzy->input_list, i);
        uint16_t terms_len = sml_fuzzy_variable_terms_count(var);
        if (sml_fuzzy_variable_is_enabled(var)) {
            for (j = 0; j < terms_len; j++) {
                if (sml_observation_input_term_get(obs1, i, j) !=
                    sml_observation_input_term_get(obs2, i, j))
                    return false;
            }
        }
    }

    return true;
}

bool
sml_observation_output_equals(struct sml_fuzzy *fuzzy,
    struct sml_observation *obs1, struct sml_observation *obs2,
    uint16_t output_number)
{
    uint16_t i, terms_len, total_weight1 = 0, total_weight2 = 0;
    struct sml_variable *var;

    var = sml_fuzzy_variables_list_index(fuzzy->output_list, output_number);
    terms_len = sml_fuzzy_variable_terms_count(var);
    for (i = 0; i < terms_len; i++) {
        total_weight1 += _output_get(obs1, output_number, i);
        total_weight2 += _output_get(obs2, output_number, i);
    }
    if (total_weight1 == 0 && total_weight2 == 0)
        return true;

    if (total_weight1 == 0 || total_weight2 == 0)
        return false;

    for (i = 0; i < terms_len; i++)
        if (fabs((_output_get(obs1, output_number, i) / total_weight1) -
            (_output_get(obs2, output_number, i) / total_weight2)) >
            WEIGHT_THRESHOLD)
            return false;

    return true;
}

bool
sml_observation_input_equals(struct sml_fuzzy *fuzzy,
    struct sml_observation *obs1, struct sml_observation *obs2)
{
    uint16_t i, j, len;
    struct sml_variable *var;

    len = sml_fuzzy_variables_list_get_length(fuzzy->input_list);
    for (i = 0; i < len; i++) {
        var = sml_fuzzy_variables_list_index(fuzzy->input_list, i);
        uint16_t terms_len = sml_fuzzy_variable_terms_count(var);
        for (j = 0; j < terms_len; j++) {
            if (sml_observation_input_term_get(obs1, i, j) !=
                sml_observation_input_term_get(obs2, i, j))
                return false;
        }
    }

    return true;
}

int
sml_observation_hit(struct sml_fuzzy *fuzzy,
    struct sml_observation *observation, struct sml_measure *measure, bool *hit)
{
    uint16_t len, i, j, index = 0;
    int error;
    float val, *tmp;
    bool tmp_hit = false;
    struct sml_variable *variable;

    len = sml_fuzzy_variables_list_get_length(fuzzy->output_list);
    for (i = 0; i < len; i++) {
        variable = sml_fuzzy_variables_list_index(fuzzy->output_list, i);
        uint16_t terms_len = sml_fuzzy_variable_terms_count(variable);
        bool reduce_weight = false;

        for (j = 0; j < terms_len; j++) {
            val = sml_matrix_cast_get(&measure->outputs, i, j, tmp, float);
            uint16_t output_weight = _output_get(observation, i, j);

            if (val >= VARIABLE_MEMBERSHIP_THRESHOLD) {
                if (output_weight >= UINT8_MAX - RULE_WEIGHT) {
                    output_weight = UINT8_MAX;
                    reduce_weight = true;
                } else
                    output_weight += RULE_WEIGHT;
                if ((error = _output_set(fuzzy, observation, i, j,
                        output_weight)))
                    return error;
                tmp_hit = true;
            } else if (output_weight > 0 &&
                val < VARIABLE_MEMBERSHIP_THRESHOLD) {
                output_weight -= RULE_WEIGHT;
                if ((error = _output_set(fuzzy, observation, i, j,
                        output_weight)))
                    return error;
                tmp_hit = true;
            }
            index++;
        }

        if (reduce_weight)
            for (j = 0; j < terms_len; j++) {
                if ((error = _output_set(fuzzy, observation, i, j,
                        _output_get(observation, i, j))))
                    return error;
            }
    }

    if (hit)
        *hit = tmp_hit;
    return 0;
}

static bool
_is_input_relevant(struct sml_bit_array *relevant_inputs, uint16_t i)
{
    if (!relevant_inputs)
        return true;
    return sml_bit_array_get(relevant_inputs, i) == SET;
}

static bool
_observation_input_rule_generate(struct sml_variables_list *list,
    struct sml_bit_array *relevant_inputs,
    struct sml_observation *observation, struct sml_string *str)
{
    uint16_t len, i, j;
    bool is_first = true, r;
    struct sml_variable *variable;
    struct sml_fuzzy_term *term;
    char var_name[SML_VARIABLE_NAME_MAX_LEN + 1];
    char term_name[SML_TERM_NAME_MAX_LEN + 1];

    len = sml_fuzzy_variables_list_get_length(list);
    for (i = 0; i < len; i++) {
        variable = sml_fuzzy_variables_list_index(list, i);
        uint16_t terms_len = sml_fuzzy_variable_terms_count(variable);
        if (sml_fuzzy_variable_is_enabled(variable) &&
            _is_input_relevant(relevant_inputs, i)) {
            for (j = 0; j < terms_len; j++) {
                term = sml_fuzzy_variable_get_term(variable, j);
                uint8_t val = sml_observation_input_term_get(observation,
                    i, j);
                if (val == SET) {
                    if (is_first)
                        is_first = false;
                    else {
                        r = sml_string_append(str, " and ");
                        if (!r) {
                            sml_critical("Could not append \"and\" to the " \
                                "string");
                            return false;
                        }
                    }

                    if (sml_fuzzy_variable_get_name(variable, var_name,
                        sizeof(var_name))) {
                        sml_critical("Failed to get variable name %p",
                            variable);
                        return false;
                    }
                    if (sml_fuzzy_term_get_name(term, term_name,
                        sizeof(term_name))) {
                        sml_critical("Failed to get term name %p", term);
                        return false;
                    }

                    r = sml_string_append_printf(str, "%s is %s", var_name,
                        term_name);
                    if (!r) {
                        sml_critical("Could not append the variable name " \
                            "and term to the string");
                        return false;
                    }
                }
            }
        }
    }
    return !is_first;
}

static void
_observation_output_rule_generate(struct sml_variables_list *list,
    float *output_weights,
    struct sml_string *prefix,
    float weight_threshold,
    uint16_t output_number,
    sml_process_str_cb process_cb, void *data)
{
    char var_name[SML_VARIABLE_NAME_MAX_LEN + 1];
    char term_name[SML_TERM_NAME_MAX_LEN + 1];
    uint16_t i, j, len, index = 0;
    bool r;

    len = sml_fuzzy_variables_list_get_length(list);
    for (i = 0; i < len; i++) {
        struct sml_variable *v = sml_fuzzy_variables_list_index(list, i);
        uint16_t terms_len = sml_fuzzy_variable_terms_count(v);
        if (i != output_number) {
            index += terms_len;
            continue;
        }

        if (sml_fuzzy_variable_get_name(v, var_name, sizeof(var_name))) {
            sml_critical("Failed to get variable name %p", v);
            index += terms_len;
            continue;
        }

        for (j = 0; j < terms_len; j++) {
            struct sml_fuzzy_term *term = sml_fuzzy_variable_get_term(v, j);
            float output_weight = output_weights[index];
            if (output_weight > weight_threshold) {
                struct sml_string *str_observation =
                    sml_string_new(sml_string_get_string(prefix));
                if (!str_observation) {
                    sml_critical("Could not alloc the sml string");
                    index++;
                    continue;
                }

                if (sml_fuzzy_term_get_name(term, term_name,
                    sizeof(term_name))) {
                    sml_critical("Failed to get term name %p", term);
                    sml_string_free(str_observation);
                    index++;
                    continue;
                }

                r = sml_string_append_printf(str_observation, "%s is %s",
                    var_name, term_name);
                if (r) {
                    if (output_weight < (1 - FLOAT_THRESHOLD))
                        r = sml_string_append_printf(
                            str_observation, " with %f", output_weight);
                    if (r)
                        process_cb(sml_string_get_string(str_observation),
                            data);
                    else
                        sml_critical("Couild not append the rule weight");
                } else
                    sml_critical("Could not append the variable name and term");

                sml_string_free(str_observation);
            }
            index++;
        }
        break;
    }
}

void
sml_observation_rule_generate(struct sml_fuzzy *fuzzy,
    struct sol_ptr_vector *observation_list,
    float weight_threshold,
    struct sml_bit_array *relevant_inputs,
    float *output_weights,
    uint16_t output_number,
    sml_process_str_cb process_cb, void *data)
{
    struct sml_observation *first_observation =
        sol_ptr_vector_get(observation_list, 0);
    bool success;
    struct sml_string *str = sml_string_new("if ");

    if (!str) {
        sml_critical("Could not alloc the sml string");
        return;
    }

    success = _observation_input_rule_generate(fuzzy->input_list,
        relevant_inputs,
        first_observation, str);
    if (!success) {
        sml_critical("Generating observation string failed");
        goto error;
    }

    if (!sml_string_append(str, " then ")) {
        sml_critical("Could not append to \"then\" to the string");
        goto error;
    }

    _observation_output_rule_generate(fuzzy->output_list,
        output_weights, str, weight_threshold,
        output_number,
        process_cb, data);
error:
    sml_string_free(str);
}

void
sml_observation_debug(struct sml_observation *observation)
{
    uint16_t i, j;
    struct sml_string *str = sml_string_new("\t");
    struct sol_vector *term_weights;
    struct sml_bit_array *term_membership;
    uint8_t *val;

    sml_string_append_printf(str, "Observation {");
    sml_string_append_printf(str, "Inputs (%d) {",
        observation->input_membership.len);
    SOL_VECTOR_FOREACH_IDX (&observation->input_membership, term_membership,
        i) {
        if (i > 0)
            sml_string_append(str, ", ");
        sml_string_append_printf(str, "{");
        for (j = 0; j < term_membership->size; j++) {
            if ( j > 0)
                sml_string_append(str, ", ");
            sml_string_append_printf(str, "%d",
                sml_bit_array_get(term_membership, j));
        }
        sml_string_append(str, "}");
    }
    sml_string_append(str, "}");

    sml_string_append_printf(str, ", Outputs (%d) {",
        observation->output_weights.len);

    SOL_VECTOR_FOREACH_IDX (&observation->output_weights, term_weights, i) {
        if (i > 0)
            sml_string_append(str, ", ");
        sml_string_append_printf(str, "{");
        SOL_VECTOR_FOREACH_IDX (term_weights, val, j) {
            if ( j > 0)
                sml_string_append(str, ", ");
            sml_string_append_printf(str, "%d", *val);
        }
        sml_string_append(str, "}");
    }

    sml_string_append(str, "}");
    sml_string_append(str, "}");
    sml_debug("%s", sml_string_get_string(str));

    sml_string_free(str);
}

int
sml_observation_remove_variables(struct sml_observation *observation,
    bool *inputs_to_remove,
    bool *outputs_to_remove)
{
    uint16_t i, removed, len;

    if (inputs_to_remove) {
        removed = 0;
        len = observation->input_membership.len;
        for (i = 0; i < len; i++) {
            if (inputs_to_remove[i]) {
                sml_bit_array_clear(sol_vector_get(
                    &observation->input_membership, i - removed));
                sol_vector_del(&observation->input_membership, i - removed);
                removed++;
            }
        }
    }

    if (outputs_to_remove) {
        removed = 0;
        len = observation->output_weights.len;
        for (i = 0; i < len; i++) {
            if (outputs_to_remove[i]) {
                sol_vector_clear(sol_vector_get(&observation->output_weights,
                    i - removed));
                sol_vector_del(&observation->output_weights, i - removed);
                removed++;
            }
        }
    }

    return 0;
}

bool
sml_observation_is_empty(struct sml_observation *observation)
{
    return !observation->output_weights.len ||
           !observation->input_membership.len;
}

int
sml_observation_merge_output(struct sml_fuzzy *fuzzy,
    struct sml_observation *observation1, struct sml_observation *observation2)
{
    uint16_t i, j;
    struct sol_vector *term_weights;
    uint8_t *val;
    int error;

    if ((error = _output_weights_initialize(fuzzy, observation1)))
        return error;

    SOL_VECTOR_FOREACH_IDX (&observation1->output_weights, term_weights, i) {
        SOL_VECTOR_FOREACH_IDX (term_weights, val, j)
            *val = *val + _output_get(observation2, i, j);
    }

    return 0;
}

bool
sml_observation_is_base(struct sml_fuzzy *fuzzy,
    struct sml_observation *observation)
{
    uint16_t i, j, len;
    struct sml_variable *var;

    len = sml_fuzzy_variables_list_get_length(fuzzy->input_list);
    for (i = 0; i < len; i++) {
        var = sml_fuzzy_variables_list_index(fuzzy->input_list, i);
        uint16_t terms_len = sml_fuzzy_variable_terms_count(var);
        if (!sml_fuzzy_variable_is_enabled(var)) {
            for (j = 0; j < terms_len; j++) {
                if (sml_observation_input_term_get(observation, i, j))
                    return false;
            }
        }
    }

    return true;
}

void
sml_observation_fill_output_weights(struct sml_fuzzy *fuzzy,
    struct sml_observation *obs, uint16_t *output_weights)
{
    uint16_t index, len, i, j, terms_len;
    struct sml_variables_list *list;

    index = 0;
    list = fuzzy->output_list;
    len = sml_fuzzy_variables_list_get_length(list);
    for (i = 0; i < len; i++) {
        struct sml_variable *variable = sml_fuzzy_variables_list_index(list, i);
        terms_len = sml_fuzzy_variable_terms_count(variable);
        for (j = 0; j < terms_len; j++)
            output_weights[index++] += _output_get(obs, i, j);
    }
}

bool
sml_observation_save(struct sml_observation *obs, FILE *f)
{
    uint16_t input_byte_size, i;
    struct sol_vector *vector;
    struct sml_bit_array *array;

    if (fwrite(&obs->output_weights.len, sizeof(uint16_t), 1, f) < 1)
        return false;

    SOL_VECTOR_FOREACH_IDX (&obs->output_weights, vector, i) {
        if (fwrite(&vector->len, sizeof(uint16_t), 1, f) < 1)
            return false;

        if (fwrite(vector->data, sizeof(uint8_t), vector->len, f) < vector->len)
            return false;
    }

    if (fwrite(&obs->input_membership.len, sizeof(uint16_t), 1, f) < 1)
        return false;

    SOL_VECTOR_FOREACH_IDX (&obs->input_membership, array, i) {
        if (fwrite(&array->size, sizeof(uint16_t), 1, f) < 1)
            return false;

        input_byte_size = sml_bit_array_byte_size_get(array);
        if (fwrite(array->data, sizeof(uint8_t), input_byte_size, f)
            < input_byte_size)
            return false;

    }

    return true;
}

struct sml_observation *
sml_observation_load(FILE *f)
{
    struct sml_observation *obs;
    uint16_t i, j, input_size, output_size;

    obs = malloc(sizeof(struct sml_observation));

    if (fread(&output_size, sizeof(uint16_t), 1, f) < 1)
        goto error;
    sol_vector_init(&obs->output_weights, sizeof(struct sol_vector));
    for (i = 0; i < output_size; i++) {
        struct sol_vector *vector;
        uint16_t terms_size;

        vector = sol_vector_append(&obs->output_weights);
        sol_vector_init(vector, sizeof(uint8_t));

        if (fread(&terms_size, sizeof(uint16_t), 1, f) < 1)
            goto error;
        for (j = 0; j < terms_size; j++) {
            uint8_t *val = sol_vector_append(vector);
            if (fread(val, sizeof(uint8_t), 1, f) < 1)
                goto error;
        }
    }

    if (fread(&input_size, sizeof(uint16_t), 1, f) < 1)
        goto error;
    sol_vector_init(&obs->input_membership, sizeof(struct sml_bit_array));
    for (i = 0; i < input_size; i++) {
        struct sml_bit_array *array;
        uint16_t terms_size, input_byte_size;

        array = sol_vector_append(&obs->input_membership);
        sml_bit_array_init(array);

        if (fread(&terms_size, sizeof(uint16_t), 1, f) < 1)
            goto error;
        if (sml_bit_array_size_set(array, terms_size, 0) != 0)
            goto error;
        input_byte_size = sml_bit_array_byte_size_get(array);
        if (fread(array->data, sizeof(uint8_t), input_byte_size, f)
            < input_byte_size)
            goto error;
    }
    return obs;

error:
    sml_observation_free(obs);
    return NULL;
}

int
sml_observation_remove_term(struct sml_observation *obs, uint16_t var_num,
    uint16_t term_num, bool input)
{
    struct sol_vector *vector;
    struct sml_bit_array *array;

    if (input) {
        array = sol_vector_get(&obs->input_membership, var_num);
        return sml_bit_array_remove(array, term_num);
    }

    vector = sol_vector_get(&obs->output_weights, var_num);
    return sol_vector_del(vector, term_num);
}

int
sml_observation_merge_terms(struct sml_observation *obs, uint16_t var_num,
    uint16_t term1, uint16_t term2, bool input)
{
    struct sol_vector *vector;
    struct sml_bit_array *array;
    uint8_t val;
    uint16_t *weight1, *weight2;

    if (input) {
        array = sol_vector_get(&obs->input_membership, var_num);
        val = sml_bit_array_get(array, term1) == SET ||
            sml_bit_array_get(array, term2) == SET ? SET : UNSET;
        sml_bit_array_set(array, term1, val);
        return sml_bit_array_remove(array, term2);
    }

    vector = sol_vector_get(&obs->output_weights, var_num);
    weight1 = sol_vector_get(vector, term1);
    weight2 = sol_vector_get(vector, term2);
    *weight1 = *weight1 + *weight2;
    return sol_vector_del(vector, term2);
}

int
sml_observation_split_terms(struct sml_fuzzy *fuzzy,
    struct sml_observation *obs, uint16_t var_num, uint16_t term_num,
    uint16_t term1, uint16_t term2, bool input)
{
    struct sol_vector *vector;
    struct sml_bit_array *array;
    struct sml_variable *var;
    uint8_t val;
    uint16_t weight, terms_len;
    int error;

    if (input) {
        array = sol_vector_get(&obs->input_membership, var_num);
        var = sml_fuzzy_variables_list_index(fuzzy->input_list, var_num);
        terms_len = sml_fuzzy_variable_terms_count(var);
        if ((error = sml_bit_array_size_set(array, terms_len, 0)))
            return error;

        val = sml_bit_array_get(array, term_num) == SET;
        sml_bit_array_set(array, term1, val);
        sml_bit_array_set(array, term2, val);
        return sml_bit_array_remove(array, term_num);
    }

    weight = _output_get(obs, var_num, term_num);

    if ((error = _output_set(fuzzy, obs, var_num, term1, weight)))
        return error;

    if ((error = _output_set(fuzzy, obs, var_num, term2, weight)))
        return error;

    vector = sol_vector_get(&obs->output_weights, var_num);
    return sol_vector_del(vector, term_num);
}

unsigned int
sml_observation_estimate_size(struct sml_fuzzy *fuzzy)
{
    unsigned int size = sizeof(struct sml_observation);

    size += (sml_fuzzy_variables_list_get_length(fuzzy->input_list) +
        sml_fuzzy_variables_list_get_length(fuzzy->output_list)) *
        sizeof(struct sol_vector);
    size += (fuzzy->output_terms_count * sizeof(uint8_t)) +
        (fuzzy->input_terms_count / 8 + 1);
    return size;
}

