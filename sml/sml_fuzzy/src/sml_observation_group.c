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
#include <stdint.h>
#include <sml_log.h>
#include "sml_observation_group.h"

//Observation Group is a class to group all observation that have exactly same
//inputs for enabled inputs. i.e, if all inputs are enabled, only a single
//observation will belong to a group.

struct sml_observation_group *
sml_observation_group_new()
{
    struct sml_observation_group *obs_group = calloc(1,
        sizeof(struct sol_ptr_vector));

    if (!obs_group)
        return NULL;

    sol_ptr_vector_init((struct sol_ptr_vector *)obs_group);
    return obs_group;
}

struct sml_observation *
sml_observation_group_get_first_observation(
    struct sml_observation_group *obs_group)
{
    if (!sol_ptr_vector_get_len((struct sol_ptr_vector *)obs_group))
        return NULL;

    return sol_ptr_vector_get((struct sol_ptr_vector *)obs_group, 0);
}

int
sml_observation_group_observation_hit(struct sml_fuzzy *fuzzy,
    struct sml_observation_group *obs_group,
    struct sml_measure *measure, bool *found)
{
    int error;
    bool obs_hit;

    if (found)
        *found = false;
    if (sol_ptr_vector_get_len((struct sol_ptr_vector *)obs_group)) {
        uint16_t i;
        struct sml_observation *obs;
        struct sml_observation *first_observation =
            sol_ptr_vector_get((struct sol_ptr_vector *)obs_group, 0);

        if (!sml_observation_enabled_input_values_equals(fuzzy,
            first_observation, measure))
            return 0;

        SOL_PTR_VECTOR_FOREACH_IDX ((struct sol_ptr_vector *)obs_group, obs, i)
            if (sml_observation_is_base(fuzzy, obs)) {
                if (found)
                    *found = true;
                return sml_observation_hit(fuzzy, obs, measure, NULL);
            }

    }

    //New observation in this group
    struct sml_observation *observation;
    error = sml_observation_new(fuzzy, measure, &observation);
    if (error || !observation)
        return error;

    error = sml_observation_hit(fuzzy, observation, measure, &obs_hit);
    if (error || !obs_hit) {
        sml_observation_free(observation);
        return error;
    }
    sol_ptr_vector_append((struct sol_ptr_vector *)obs_group, observation);

    if (found)
        *found = true;
    return 0;
}

int
sml_observation_group_observation_append(struct sml_fuzzy *fuzzy,
    struct sml_observation_group *obs_group,
    struct sml_observation *obs, bool *appended)
{
    if (sol_ptr_vector_get_len((struct sol_ptr_vector *)obs_group)) {
        struct sml_observation *first_observation =
            sol_ptr_vector_get((struct sol_ptr_vector *)obs_group, 0);

        if (!sml_observation_enabled_input_equals(fuzzy, first_observation,
            obs)) {
            if (appended)
                *appended = false;
            return 0;
        }
    }

    sol_ptr_vector_append((struct sol_ptr_vector *)obs_group, obs);

    if (appended)
        *appended = true;
    return 0;
}

int
sml_observation_group_merge(struct sml_fuzzy *fuzzy,
    struct sml_observation_group *obs_group1,
    struct sml_observation_group *obs_group2)
{
    int error;
    uint16_t i, j;
    struct sml_observation *obs, *obs_aux;

    SOL_PTR_VECTOR_FOREACH_IDX ((struct sol_ptr_vector *)obs_group2, obs, i)
        sol_ptr_vector_append((struct sol_ptr_vector *)obs_group1, obs);
    sol_ptr_vector_clear((struct sol_ptr_vector *)obs_group2);

    for (i = 0; i < sol_ptr_vector_get_len((struct sol_ptr_vector *)obs_group1);
        i++) {
        obs = sol_ptr_vector_get((struct sol_ptr_vector *)obs_group1, i);
        for (j = i + 1; j < sol_ptr_vector_get_len(
            (struct sol_ptr_vector *)obs_group1); j++) {
            obs_aux = sol_ptr_vector_get((struct sol_ptr_vector *)obs_group1,
                j);
            if (sml_observation_input_equals(fuzzy, obs, obs_aux)) {
                if ((error = sml_observation_merge_output(fuzzy,
                        obs, obs_aux)))
                    return error;
                sml_observation_free(obs_aux);
                sol_ptr_vector_del((struct sol_ptr_vector *)obs_group1, j);
                j--;
            }
        }
    }

    return 0;
}

bool
sml_observation_group_enabled_input_equals(struct sml_fuzzy *fuzzy,
    struct sml_observation_group *obs_group1,
    struct sml_observation_group *obs_group2)
{
    struct sml_observation *f1, *f2;

    if (!sol_ptr_vector_get_len((struct sol_ptr_vector *)obs_group1) ||
        !sol_ptr_vector_get_len((struct sol_ptr_vector *)obs_group2))
        return false;

    f1 = sol_ptr_vector_get((struct sol_ptr_vector *)obs_group1, 0),
    f2 = sol_ptr_vector_get((struct sol_ptr_vector *)obs_group2, 0);
    return sml_observation_enabled_input_equals(fuzzy, f1, f2);
}

void
sml_observation_group_free(struct sml_observation_group *obs_group)
{
    uint16_t i;
    struct sml_observation *obs;

    SOL_PTR_VECTOR_FOREACH_IDX ((struct sol_ptr_vector *)obs_group, obs, i)
        sml_observation_free(obs);
    sol_ptr_vector_clear((struct sol_ptr_vector *)obs_group);
    free(obs_group);
}

void
sml_observation_group_rule_generate(struct sml_fuzzy *fuzzy,
    struct sml_observation_group *obs_group,
    float weight_threshold,
    struct sml_bit_array *relevant_inputs,
    float *output_weights,
    uint16_t output_number,
    sml_process_str_cb process_cb,
    void *data)
{
    sml_observation_rule_generate(fuzzy, (struct sol_ptr_vector *)obs_group,
        weight_threshold, relevant_inputs, output_weights,
        output_number, process_cb, data);
}

void
sml_observation_group_debug(struct sml_observation_group *obs_group)
{
    uint16_t i;
    struct sml_observation *obs;

    sml_debug("Observation Group (%d) {",
        sol_ptr_vector_get_len((struct sol_ptr_vector *)obs_group));
    SOL_PTR_VECTOR_FOREACH_IDX ((struct sol_ptr_vector *)obs_group, obs, i)
        sml_observation_debug(obs);
    sml_debug("}");
}

void
sml_observation_group_split(struct sml_fuzzy *fuzzy,
    struct sml_observation_group *obs_group, struct sol_ptr_vector *split)
{
    uint16_t i, j, start;
    struct sml_observation *item_i;
    struct sml_observation_group *item_j;
    struct sml_observation *first;

    start =  sol_ptr_vector_get_len((struct sol_ptr_vector *)split);
    SOL_PTR_VECTOR_FOREACH_IDX ((struct sol_ptr_vector *)obs_group, item_i, i) {
        bool added = false;
        for (j = start; j < sol_ptr_vector_get_len(
            (struct sol_ptr_vector *)split); j++) {
            item_j = sol_ptr_vector_get((struct sol_ptr_vector *)split, j);
            first = sol_ptr_vector_get((struct sol_ptr_vector *)item_j, 0);
            if (sml_observation_enabled_input_equals(fuzzy, item_i, first)) {
                sol_ptr_vector_append((struct sol_ptr_vector *)item_j, item_i);
                added = true;
                break;
            }
        }

        if (!added) {
            item_j = sml_observation_group_new();
            sol_ptr_vector_append((struct sol_ptr_vector *)item_j, item_i);
            sol_ptr_vector_append((struct sol_ptr_vector *)split, item_j);
        }
    }

    sol_ptr_vector_clear((struct sol_ptr_vector *)obs_group);
}

int
sml_observation_group_remove_variables(struct sml_observation_group *obs_group,
    bool *inputs_to_remove,
    bool *outputs_to_remove)
{
    struct sml_observation *obs;
    uint16_t i;
    int error;

    //Remove variables from observation structure and remove empty structs
    SOL_PTR_VECTOR_FOREACH_IDX ((struct sol_ptr_vector *)obs_group, obs, i) {
        if ((error = sml_observation_remove_variables(obs, inputs_to_remove,
                outputs_to_remove)))
            return error;

        if (sml_observation_is_empty(obs)) {
            sml_observation_free(obs);
            if ((error = sol_ptr_vector_del((struct sol_ptr_vector *)obs_group,
                    i))) {
                sml_critical("Could not remove the observation from group");
                return error;
            }
            i--;
        }
    }

    return 0;
}

bool
sml_observation_group_is_empty(struct sml_observation_group *obs_group)
{
    return !sol_ptr_vector_get_len((struct sol_ptr_vector *)obs_group);
}

void
sml_observation_group_fill_output_weights(struct sml_fuzzy *fuzzy,
    struct sml_observation_group *obs_group,
    uint16_t *output_weights)
{
    uint16_t i;
    struct sml_observation *obs;

    SOL_PTR_VECTOR_FOREACH_IDX ((struct sol_ptr_vector *)obs_group, obs, i)
        sml_observation_fill_output_weights(fuzzy, obs, output_weights);
}

int
sml_observation_group_remove_terms(struct sml_observation_group *obs_group,
    uint16_t var_num, uint16_t term_num,
    bool input)
{
    struct sml_observation *obs;
    uint16_t i;
    int error;

    SOL_PTR_VECTOR_FOREACH_IDX ((struct sol_ptr_vector *)obs_group, obs, i) {
        if ((error = sml_observation_remove_term(obs, var_num, term_num,
                input)))
            return error;

        if (sml_observation_is_empty(obs)) {
            sml_observation_free(obs);
            if ((error = sol_ptr_vector_del((struct sol_ptr_vector *)obs_group,
                    i))) {
                sml_critical("Could not remove the observation from group");
                return error;
            }
            i--;
        }
    }

    return 0;
}

int
sml_observation_group_merge_terms(struct sml_observation_group *obs_group,
    uint16_t var_num, uint16_t term1,
    uint16_t term2, bool input)
{
    struct sml_observation *obs;
    uint16_t i;
    int error;

    SOL_PTR_VECTOR_FOREACH_IDX ((struct sol_ptr_vector *)obs_group, obs, i) {
        if ((error = sml_observation_merge_terms(obs, var_num, term1, term2,
                input)))
            return error;

        if (sml_observation_is_empty(obs)) {
            sml_observation_free(obs);
            if ((error = sol_ptr_vector_del((struct sol_ptr_vector *)obs_group,
                    i))) {
                sml_critical("Could not remove the observation from group");
                return error;
            }
            i--;
        }
    }

    return 0;
}

int
sml_observation_group_split_terms(struct sml_fuzzy *fuzzy,
    struct sml_observation_group *obs_group,
    uint16_t var_num, uint16_t term_num,
    int16_t term1, uint16_t term2, bool input)
{
    struct sml_observation *obs;
    uint16_t i;
    int error;

    SOL_PTR_VECTOR_FOREACH_IDX ((struct sol_ptr_vector *)obs_group, obs, i) {
        if ((error = sml_observation_split_terms(fuzzy, obs, var_num, term_num,
                term1, term2, input)))
            return error;

        if (sml_observation_is_empty(obs)) {
            sml_observation_free(obs);
            if ((error = sol_ptr_vector_del((struct sol_ptr_vector *)obs_group,
                    i))) {
                sml_critical("Could not remove the observation from group");
                return error;
            }
            i--;
        }
    }

    return 0;
}
