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

#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include "sml_bit_array.h"
#include "sml_rule_group.h"
#include "sml_fuzzy_bridge.h"

#define ALL_SET 0xff

//Observation Group is a class to group all observation that have exactly same
//inputs for enabled inputs. i.e, if all inputs are enabled, only a single
//observation will belong to a group.

//We need to group rules by outputs
struct sml_rule_group {
    struct sol_ptr_vector observations;
    struct sol_ptr_vector rules;
    struct sml_bit_array relevant_inputs;
};

typedef struct _Tmp_Rule_Fuzzy {
    struct sml_rule_group *rule_group;
    struct sml_fuzzy *fuzzy;
    bool error;
} Tmp_Rule_Fuzzy;

static int _rule_group_list_observation_append(struct sml_fuzzy *fuzzy,
    struct sol_ptr_vector *rule_group_list,
    struct sml_observation_group *obs_group,
    float weight_threshold,
    uint16_t output_number, bool hard);

static int
_ptr_vector_del_obj(struct sol_ptr_vector *pv, void *ptr)
{
    uint16_t i;
    void *tmp_ptr;

    SOL_PTR_VECTOR_FOREACH_IDX (pv, tmp_ptr, i)
        if (tmp_ptr == ptr)
            return sol_ptr_vector_del(pv, i);
    return -ENODATA;
}

static void
_rule_append(const char *str, void *data)
{
    Tmp_Rule_Fuzzy *tmp = data;
    void *rule = sml_fuzzy_rule_add(tmp->fuzzy, str);

    if (!rule) {
        tmp->error = true;
        return;
    }

    sol_ptr_vector_append(&tmp->rule_group->rules, rule);
}

static uint16_t
_observation_belong_in_group(struct sml_fuzzy *fuzzy,
    struct sml_rule_group *rule_group, struct sml_observation_group *obs_group)
{
    uint16_t level = 0;
    uint16_t i, len, j;
    struct sml_variable *v;

    if (sol_ptr_vector_get_len(&rule_group->observations) == 0)
        return 0;

    struct sml_observation_group *first_group =
        sol_ptr_vector_get(&rule_group->observations, 0);

    struct sml_observation *first_observation1 =
        sml_observation_group_get_first_observation(first_group);
    struct sml_observation *first_observation2 =
        sml_observation_group_get_first_observation(obs_group);

    if (first_observation1 == NULL || first_observation2 == NULL)
        return 0;

    len = sml_fuzzy_variables_list_get_length(fuzzy->input_list);
    for (i = 0; i < len; i++) {
        v = sml_fuzzy_variables_list_index(fuzzy->input_list, i);
        uint16_t terms_len = sml_fuzzy_variable_terms_count(v);
        uint16_t found = 0;
        if (sml_fuzzy_variable_is_enabled(v) &&
            sml_bit_array_get(&rule_group->relevant_inputs, i)) {
            for (j = 0; j < terms_len; j++) {
                if (sml_observation_input_term_get(first_observation1, i, j) ==
                    sml_observation_input_term_get(first_observation2, i, j))
                    found++;
            }

            if (found == terms_len)
                level++;
        }
    }

    return level;
}

static void
_insert_in_rule_group(struct sml_fuzzy *fuzzy,
    struct sml_rule_group *rule_group, struct sml_observation_group *obs_group)
{
    uint16_t i, len, j;
    struct sml_variable *v;

    struct sml_observation_group *first_group =
        sol_ptr_vector_get(&rule_group->observations, 0);

    struct sml_observation *first_observation1 =
        sml_observation_group_get_first_observation(first_group);
    struct sml_observation *first_observation2 =
        sml_observation_group_get_first_observation(obs_group);

    len = sml_fuzzy_variables_list_get_length(fuzzy->input_list);
    for (i = 0; i < len; i++) {
        v = sml_fuzzy_variables_list_index(fuzzy->input_list, i);
        uint16_t terms_len = sml_fuzzy_variable_terms_count(v);
        uint16_t found = 0;

        if (sml_fuzzy_variable_is_enabled(v) &&
            sml_bit_array_get(&rule_group->relevant_inputs, i)) {
            for (j = 0; j < terms_len; j++) {
                if (sml_observation_input_term_get(first_observation1, i, j) ==
                    sml_observation_input_term_get(first_observation2, i, j))
                    found++;
            }

            if (found != terms_len)
                sml_bit_array_set(&rule_group->relevant_inputs, i, UNSET);
        }
    }

    sol_ptr_vector_append(&rule_group->observations, obs_group);
}

static bool
_observation_conflicts_in_group(struct sml_fuzzy *fuzzy,
    struct sml_rule_group *rule_group, struct sml_observation_group *obs_group,
    uint16_t output_number)
{
    struct sml_observation_group *first_group =
        sol_ptr_vector_get(&rule_group->observations, 0);

    if (first_group == obs_group) {
        if (sol_ptr_vector_get_len(&rule_group->observations) == 1)
            return false;
        first_group = sol_ptr_vector_get(&rule_group->observations, 1);
    }

    struct sml_observation *first_observation1 =
        sml_observation_group_get_first_observation(first_group);
    struct sml_observation *first_observation2 =
        sml_observation_group_get_first_observation(obs_group);

    return !sml_observation_output_equals(fuzzy, first_observation1,
        first_observation2,
        output_number);
}

static int
_rule_group_rule_refresh(struct sml_fuzzy *fuzzy,
    struct sml_rule_group *rule_group, float weight_threshold,
    uint16_t output_number)
{
    uint16_t i;
    void *rule;
    Tmp_Rule_Fuzzy tmp;
    int error;

    SOL_PTR_VECTOR_FOREACH_IDX (&rule_group->rules, rule, i)
        sml_fuzzy_rule_free(fuzzy, rule);
    sol_ptr_vector_clear(&rule_group->rules);

    tmp.rule_group = rule_group;
    tmp.fuzzy = fuzzy;
    tmp.error = false;

    if ((error = sml_rule_group_rule_generate(fuzzy, rule_group,
            weight_threshold, output_number, _rule_append, &tmp)))
        return error;

    if (tmp.error)
        return SML_INTERNAL_ERROR;
    return 0;
}

static int
_break_groups_hard(struct sml_fuzzy *fuzzy,
    struct sol_ptr_vector *conflict_rule_group,
    struct sol_ptr_vector *rule_group_list, float weight_threshold,
    uint16_t output_number)
{
    uint16_t i, j;
    struct sml_rule_group *rule_group;
    struct sml_observation_group *obs_group;

    SOL_PTR_VECTOR_FOREACH_IDX (conflict_rule_group, rule_group, i) {
        _ptr_vector_del_obj(rule_group_list, rule_group);
        SOL_PTR_VECTOR_FOREACH_IDX (&rule_group->observations, obs_group, j) {
            struct sml_rule_group *new_rule_group = sml_rule_group_new();
            if (!new_rule_group)
                return -errno;

            sol_ptr_vector_append(&new_rule_group->observations, obs_group);
            sol_ptr_vector_append(rule_group_list, new_rule_group);
            sml_bit_array_size_set(&new_rule_group->relevant_inputs,
                sml_fuzzy_variables_list_get_length(fuzzy->input_list),
                ALL_SET);
            _rule_group_rule_refresh(fuzzy, new_rule_group, weight_threshold,
                output_number);
        }
        sml_rule_group_free(fuzzy, rule_group);
    }

    return 0;
}

static int
_break_groups(struct sml_fuzzy *fuzzy,
    struct sol_ptr_vector *conflict_rule_group,
    struct sol_ptr_vector *rule_group_list, float weight_threshold,
    uint16_t output_number)
{
    int error;
    uint16_t i, j;
    struct sml_rule_group *rule_group;
    struct sml_observation_group *obs_group;

    SOL_PTR_VECTOR_FOREACH_IDX (conflict_rule_group, rule_group, i)
        _ptr_vector_del_obj(rule_group_list, rule_group);

    SOL_PTR_VECTOR_FOREACH_IDX (conflict_rule_group, rule_group, i) {
        SOL_PTR_VECTOR_FOREACH_IDX (&rule_group->observations, obs_group, j) {
            error = _rule_group_list_observation_append(fuzzy, rule_group_list,
                obs_group, weight_threshold, output_number, true);
            if (error)
                return error;
        }
        sml_rule_group_free(fuzzy, rule_group);
    }
    return 0;
}

static bool
_remove_from_rule_group_list(struct sml_fuzzy *fuzzy,
    struct sol_ptr_vector *rule_group_list,
    struct sml_observation_group *obs_group_obj,
    float weight_threshold, uint16_t output_number)
{
    uint16_t i, j;
    struct sml_rule_group *rule_group;
    struct sml_observation_group *obs_group;

    SOL_PTR_VECTOR_FOREACH_IDX (rule_group_list, rule_group, i) {
        SOL_PTR_VECTOR_FOREACH_IDX (&rule_group->observations, obs_group, j)
            if (obs_group == obs_group_obj) {
                if (sol_ptr_vector_get_len(&rule_group->observations) == 1) {
                    sol_ptr_vector_del(&rule_group->observations, j);
                    sol_ptr_vector_del(rule_group_list, i);
                    sml_rule_group_free(fuzzy, rule_group);
                    return true;
                }

                if (_observation_conflicts_in_group(fuzzy, rule_group,
                    obs_group, output_number)) {
                    sol_ptr_vector_del(&rule_group->observations, j);
                    return true;
                }

                _rule_group_rule_refresh(fuzzy, rule_group, weight_threshold,
                    output_number);
                return false;
            }
    }

    return false;
}

struct sml_rule_group *
sml_rule_group_new()
{
    struct sml_rule_group *rule_group = malloc(sizeof(struct sml_rule_group));

    if (!rule_group)
        return NULL;

    sol_ptr_vector_init(&rule_group->rules);
    sol_ptr_vector_init(&rule_group->observations);
    sml_bit_array_init(&rule_group->relevant_inputs);
    return rule_group;
}

void
sml_rule_group_free(struct sml_fuzzy *fuzzy, struct sml_rule_group *rule_group)
{
    uint16_t i;
    void *rule;

    sol_ptr_vector_clear(&rule_group->observations);
    SOL_PTR_VECTOR_FOREACH_IDX (&rule_group->rules, rule, i)
        sml_fuzzy_rule_free(fuzzy, rule);
    sol_ptr_vector_clear(&rule_group->rules);
    sml_bit_array_clear(&rule_group->relevant_inputs);
    free(rule_group);
}

static int
_rule_group_list_observation_append_no_simplification(struct sml_fuzzy *fuzzy,
    struct sol_ptr_vector *rule_group_list,
    struct sml_observation_group *obs_group,
    float weight_threshold,
    uint16_t output_number)
{
    struct sml_rule_group *rule_group;

    rule_group = sml_rule_group_new();
    if (!rule_group)
        return -errno;

    sol_ptr_vector_append(&rule_group->observations, obs_group);
    sol_ptr_vector_append(rule_group_list, rule_group);
    sml_bit_array_size_set(&rule_group->relevant_inputs,
        sml_fuzzy_variables_list_get_length(fuzzy->input_list),
        ALL_SET);
    _rule_group_rule_refresh(fuzzy, rule_group, weight_threshold,
        output_number);

    return 0;
}
static int
_rule_group_list_observation_append(struct sml_fuzzy *fuzzy,
    struct sol_ptr_vector *rule_group_list,
    struct sml_observation_group *obs_group,
    float weight_threshold,
    uint16_t output_number, bool hard)
{
    uint16_t i, insert_len, conflict_len;
    struct sml_rule_group *rule_group;
    struct sol_ptr_vector insert_rule_group = SOL_PTR_VECTOR_INIT;
    struct sol_ptr_vector conflict_rule_group = SOL_PTR_VECTOR_INIT;
    uint16_t max_level = 0;
    int error = 0;

    SOL_PTR_VECTOR_FOREACH_IDX (rule_group_list, rule_group, i) {

        uint16_t cur_level = _observation_belong_in_group(fuzzy, rule_group,
            obs_group);
        if (cur_level < max_level || cur_level == 0)
            continue;

        if (cur_level > max_level) {
            max_level = cur_level;
            sol_ptr_vector_clear(&insert_rule_group);
            sol_ptr_vector_clear(&conflict_rule_group);
        }

        if (_observation_conflicts_in_group(fuzzy, rule_group, obs_group,
            output_number))
            sol_ptr_vector_append(&conflict_rule_group, rule_group);
        else
            sol_ptr_vector_append(&insert_rule_group, rule_group);
    }


    insert_len = sol_ptr_vector_get_len(&insert_rule_group);
    conflict_len = sol_ptr_vector_get_len(&conflict_rule_group);
    if (insert_len == 0 || conflict_len > 0) {
        //Create new rule group
        rule_group = sml_rule_group_new();
        if (!rule_group) {
            error = -errno;
            goto end;
        }

        sol_ptr_vector_append(&rule_group->observations, obs_group);
        sol_ptr_vector_append(rule_group_list, rule_group);
        sml_bit_array_size_set(&rule_group->relevant_inputs,
            sml_fuzzy_variables_list_get_length(fuzzy->input_list),
            ALL_SET);
        _rule_group_rule_refresh(fuzzy, rule_group, weight_threshold,
            output_number);
    } else if (insert_len > 0) {
        //Insert in existing group
        rule_group = sol_ptr_vector_get(&insert_rule_group, 0);
        _insert_in_rule_group(fuzzy, rule_group, obs_group);
        _rule_group_rule_refresh(fuzzy, rule_group, weight_threshold,
            output_number);
    }

    if (conflict_len > 0) {
        if (hard)
            error = _break_groups_hard(fuzzy, &conflict_rule_group,
                rule_group_list, weight_threshold,
                output_number);
        else
            error = _break_groups(fuzzy, &conflict_rule_group, rule_group_list,
                weight_threshold, output_number);
    }

end:
    sol_ptr_vector_clear(&insert_rule_group);
    sol_ptr_vector_clear(&conflict_rule_group);

    return error;
}

int
sml_rule_group_list_observation_append(struct sml_fuzzy *fuzzy,
    struct sol_ptr_vector *rule_group_list,
    struct sml_observation_group *obs_group,
    float weight_threshold,
    bool simplification_disabled,
    uint16_t output_number)
{
    if (simplification_disabled)
        return _rule_group_list_observation_append_no_simplification(fuzzy,
            rule_group_list, obs_group, weight_threshold, output_number);
    return _rule_group_list_observation_append(fuzzy, rule_group_list,
        obs_group, weight_threshold, output_number, false);
}

int
sml_rule_group_rule_generate(struct sml_fuzzy *fuzzy,
    struct sml_rule_group *rule_group,
    float weight_threshold, uint16_t output_number,
    sml_process_str_cb process_cb, void *data)
{
    struct sml_observation_group *obs_group;
    struct sml_variable *var;
    uint16_t *output_weights, len;
    float *output_weights_float;
    uint16_t c;
    uint16_t i, j, index;
    int error = 0;

    output_weights = calloc(fuzzy->output_terms_count, sizeof(uint16_t));
    if (!output_weights)
        return -errno;

    SOL_PTR_VECTOR_FOREACH_IDX (&rule_group->observations, obs_group, c)
        sml_observation_group_fill_output_weights(fuzzy, obs_group,
            output_weights);

    output_weights_float = calloc(fuzzy->output_terms_count, sizeof(float));
    if (!output_weights_float) {
        error = -errno;
        goto rule_generate_end;
    }

    index = 0;
    len = sml_fuzzy_variables_list_get_length(fuzzy->output_list);
    for (i = 0; i < len; i++) {
        var = sml_fuzzy_variables_list_index(fuzzy->output_list, i);
        uint16_t terms_len = sml_fuzzy_variable_terms_count(var);
        uint16_t total_weight = 0;
        for (j = 0; j < terms_len; j++)
            total_weight += output_weights[index + j];

        for (j = 0; j < terms_len; j++) {
            output_weights_float[index] = output_weights[index] /
                (float)total_weight;
            index++;
        }
    }

    obs_group = sol_ptr_vector_get(&rule_group->observations, 0);
    sml_observation_group_rule_generate(fuzzy, obs_group, weight_threshold,
        &rule_group->relevant_inputs,
        output_weights_float,
        output_number,
        process_cb, data);
    free(output_weights_float);

rule_generate_end:
    free(output_weights);
    return error;
}

int
sml_rule_group_list_rebuild(struct sml_fuzzy *fuzzy,
    struct sol_ptr_vector *rule_group_list,
    struct sol_ptr_vector *obs_group_list,
    float weight_threshold,
    bool simplification_disabled,
    uint16_t output_number)
{
    uint16_t i;
    struct sml_rule_group *rule_group;
    struct sml_observation_group *obs_group;
    int error;

    SOL_PTR_VECTOR_FOREACH_IDX (rule_group_list, rule_group, i)
        sml_rule_group_free(fuzzy, rule_group);
    sol_ptr_vector_clear(rule_group_list);

    SOL_PTR_VECTOR_FOREACH_IDX (obs_group_list, obs_group, i)
        if ((error = sml_rule_group_list_observation_append(fuzzy,
                rule_group_list, obs_group, weight_threshold,
                simplification_disabled, output_number)))
            return error;

    return 0;
}

int
sml_rule_group_list_rebalance(struct sml_fuzzy *fuzzy,
    struct sol_ptr_vector *rule_group_list,
    struct sml_observation_group *obs_group,
    float weight_threshold,
    uint16_t output_number)
{
    if (!_remove_from_rule_group_list(fuzzy, rule_group_list, obs_group,
        weight_threshold, output_number))
        return 0;

    return sml_rule_group_list_observation_append(fuzzy, rule_group_list,
        obs_group, weight_threshold,
        false, output_number);
}

bool
sml_rule_group_list_observation_remove(struct sml_fuzzy *fuzzy,
    struct sol_ptr_vector *rule_group_list,
    struct sml_observation_group *obs_group_obj,
    float weight_threshold,
    uint16_t output_number)
{
    uint16_t i, j;
    struct sml_rule_group *rule_group;
    struct sml_observation_group *obs_group;

    SOL_PTR_VECTOR_FOREACH_IDX (rule_group_list, rule_group, i) {
        SOL_PTR_VECTOR_FOREACH_IDX (&rule_group->observations, obs_group, j)
            if (obs_group == obs_group_obj) {
                sol_ptr_vector_del(&rule_group->observations, j);
                if (sol_ptr_vector_get_len(&rule_group->observations) == 0) {
                    sol_ptr_vector_del(rule_group_list, i);
                    sml_rule_group_free(fuzzy, rule_group);
                }
                return true;
            }
    }
    return false;
}
