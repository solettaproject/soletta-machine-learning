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

#include "sml_observation_controller.h"
#include "sml_observation_group.h"
#include <sml_log.h>
#include <sml_cache.h>
#include "sml_rule_group.h"
#include "sml_util.h"
#include <macros.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#define DEFAULT_CACHE_SIZE (0)
#define WEIGHT_THRESHOLD (0.05)
#define DEFAULT_OBS_CONTROLLER_FILE "controller.dat"
#define VERSION 0x1

struct sml_observation_controller {
    struct sml_cache *obs_group_cache;
    //vector of ptr_vector of struct sml_rule_group
    //The index of this vector indicates which output the rules are related to
    struct sol_vector rule_group_map; //TODO: User Matrix
    struct sml_fuzzy *fuzzy;
    float weight_threshold;
    bool simplification_disabled;
};

static int
_merge_obs_groups(struct sml_observation_controller *obs_controller)

{
    int error;
    uint16_t i, j;
    uint16_t len;
    struct sml_observation_group *item_i, *item_j;
    struct sol_ptr_vector *obs_group_list;

    obs_group_list = sml_cache_get_elements(obs_controller->obs_group_cache);
    len  = sol_ptr_vector_get_len(obs_group_list);
    for (i = 0; i < len; i++)
        for (j = i + 1; j < len; j++) {
            item_i = sol_ptr_vector_get(obs_group_list, i);
            item_j = sol_ptr_vector_get(obs_group_list, j);

            if (sml_observation_group_enabled_input_equals(
                obs_controller->fuzzy, item_i, item_j)) {
                if ((error = sml_observation_group_merge(obs_controller->fuzzy,
                        item_i, item_j)))
                    return error;
                sml_observation_group_free(item_j);
                sol_ptr_vector_del(obs_group_list, j);
                len--;
                j--;
            }
        }

    return 0;
}

static void
_initialize_rule_group_map(struct sml_observation_controller *obs_controller)
{
    uint16_t i, l;

    l = sml_fuzzy_variables_list_get_length(obs_controller->fuzzy->output_list);
    for (i = obs_controller->rule_group_map.len; i < l; i++)
        sol_ptr_vector_init(sol_vector_append(&obs_controller->rule_group_map));

}

static void
_rule_group_map_clear(struct sml_observation_controller *obs_controller)
{
    uint16_t i, j;
    struct sol_ptr_vector *rule_group_list;
    struct sml_rule_group *rule_group;

    SOL_VECTOR_FOREACH_IDX (&obs_controller->rule_group_map, rule_group_list,
        i) {
        SOL_PTR_VECTOR_FOREACH_IDX (rule_group_list, rule_group, j)
            sml_rule_group_free(obs_controller->fuzzy, rule_group);
        sol_ptr_vector_clear(rule_group_list);
    }
    sol_vector_clear(&obs_controller->rule_group_map);
}

static bool
_controller_append_observation(
    struct sml_observation_controller *obs_controller,
    struct sml_observation *obs)
{
    int error;
    uint16_t i;
    struct sml_observation_group *obs_group;
    struct sol_ptr_vector *obs_group_list;
    bool appended = false;

    obs_group_list = sml_cache_get_elements(obs_controller->obs_group_cache);
    SOL_PTR_VECTOR_FOREACH_IDX (obs_group_list, obs_group, i) {
        error = sml_observation_group_observation_append(obs_controller->fuzzy,
            obs_group, obs,
            &appended);
        if (error || appended)
            return error;
    }

    //New observation
    obs_group = sml_observation_group_new();
    if (!obs_group)
        return -errno;

    error = sml_observation_group_observation_append(obs_controller->fuzzy,
        obs_group, obs, &appended);
    if (error || !appended)
        goto append_error;

    sml_cache_put(obs_controller->obs_group_cache, obs_group);
    return 0;

append_error:
    sml_observation_group_free(obs_group);
    return error;
}

static void
_cache_element_free(void *element, void *data)
{
    struct sml_observation_controller *obs_controller = data;
    uint16_t i;
    struct sol_ptr_vector *rule_group_list;

    SOL_VECTOR_FOREACH_IDX (&obs_controller->rule_group_map,
        rule_group_list, i)
        sml_rule_group_list_observation_remove(obs_controller->fuzzy,
            rule_group_list, element, obs_controller->weight_threshold, i);

    sml_observation_group_free(element);
}

struct sml_observation_controller *
sml_observation_controller_new(struct sml_fuzzy *fuzzy)
{
    struct sml_observation_controller *obs_controller =
        malloc(sizeof(struct sml_observation_controller));

    obs_controller->obs_group_cache = sml_cache_new(DEFAULT_CACHE_SIZE,
        _cache_element_free, obs_controller);
    sol_vector_init(&obs_controller->rule_group_map,
        sizeof(struct sol_ptr_vector));
    obs_controller->fuzzy = fuzzy;
    obs_controller->weight_threshold = WEIGHT_THRESHOLD;
    obs_controller->simplification_disabled = false;
    _initialize_rule_group_map(obs_controller);
    return obs_controller;
}

void
sml_observation_controller_clear(
    struct sml_observation_controller *obs_controller)
{
    sml_cache_clear(obs_controller->obs_group_cache);
    _rule_group_map_clear(obs_controller);
}

void
sml_observation_controller_free(
    struct sml_observation_controller *obs_controller)
{
    sml_observation_controller_clear(obs_controller);
    sml_cache_free(obs_controller->obs_group_cache);
    free(obs_controller);
}

int
sml_observation_controller_observation_hit(struct sml_observation_controller *
    obs_controller, struct sml_measure *measure)
{
    int error;
    uint16_t i, j;
    struct sml_observation_group *obs_group;
    struct sol_ptr_vector *rule_group_list;
    struct sol_ptr_vector *obs_group_list;
    bool found;

    _initialize_rule_group_map(obs_controller);

    obs_group_list = sml_cache_get_elements(obs_controller->obs_group_cache);
    SOL_PTR_VECTOR_FOREACH_IDX (obs_group_list, obs_group, i) {
        error = sml_observation_group_observation_hit(obs_controller->fuzzy,
            obs_group, measure, &found);
        if (error)
            return error;

        if (found) {
            if (!obs_controller->simplification_disabled)
                SOL_VECTOR_FOREACH_IDX (&obs_controller->rule_group_map,
                    rule_group_list, j) {
                    error = sml_rule_group_list_rebalance(obs_controller->fuzzy,
                        rule_group_list, obs_group,
                        obs_controller->weight_threshold, j);
                    if (error)
                        return error;
                }
            sml_cache_hit(obs_controller->obs_group_cache, obs_group);
            return 0;
        }
    }

    //New observation
    obs_group = sml_observation_group_new();
    if (!obs_group)
        return -errno;

    if ((error = sml_observation_group_observation_hit(obs_controller->fuzzy,
            obs_group, measure, &found)))
        goto error_end;

    if (found) {
        SOL_VECTOR_FOREACH_IDX (&obs_controller->rule_group_map,
            rule_group_list, j) {
            if ((error = sml_rule_group_list_observation_append(
                    obs_controller->fuzzy, rule_group_list,
                    obs_group, obs_controller->weight_threshold,
                    obs_controller->simplification_disabled, j)))
                goto error_end;
        }

        sml_cache_put(obs_controller->obs_group_cache, obs_group);
    } else
        sml_observation_group_free(obs_group);
    return 0;

error_end:
    sml_observation_group_free(obs_group);
    return error;
}

void
sml_observation_controller_rule_generate(struct sml_observation_controller
    *obs_controller,
    sml_process_str_cb process_cb,
    void *data)
{
    uint16_t i, j;
    struct sml_rule_group *rule_group;
    struct sol_ptr_vector *rule_group_list;

    _initialize_rule_group_map(obs_controller);

    SOL_VECTOR_FOREACH_IDX (&obs_controller->rule_group_map,
        rule_group_list, i) {
        SOL_PTR_VECTOR_FOREACH_IDX (rule_group_list, rule_group, j)
            sml_rule_group_rule_generate(obs_controller->fuzzy, rule_group,
                obs_controller->weight_threshold, i,
                process_cb, data);
    }
}

void
sml_observation_controller_set_weight_threshold(
    struct sml_observation_controller *obs_controller, float weight_threshold)
{
    obs_controller->weight_threshold = weight_threshold;
}

void
sml_observation_controller_debug(
    struct sml_observation_controller *obs_controller)
{
    uint16_t i;
    struct sml_observation_group *obs_group;
    struct sol_ptr_vector *obs_group_list;

    obs_group_list = sml_cache_get_elements(obs_controller->obs_group_cache);
    sml_debug("Observation Controller (%d) {",
        sol_ptr_vector_get_len(obs_group_list));
    SOL_PTR_VECTOR_FOREACH_IDX (obs_group_list, obs_group, i)
        sml_observation_group_debug(obs_group);
    sml_debug("}");
}

int
sml_observation_controller_variable_set_enabled(
    struct sml_observation_controller *obs_controller, bool enabled)
{
    int error;
    struct sol_ptr_vector *obs_group_list;

    _initialize_rule_group_map(obs_controller);

    if (enabled) {
        uint16_t i;
        struct sml_observation_group *item;
        struct sol_ptr_vector split = SOL_PTR_VECTOR_INIT;

        //Split groups
        obs_group_list =
            sml_cache_get_elements(obs_controller->obs_group_cache);
        SOL_PTR_VECTOR_FOREACH_IDX (obs_group_list, item, i)
            sml_observation_group_split(obs_controller->fuzzy, item, &split);
        sml_cache_clear(obs_controller->obs_group_cache);
        SOL_PTR_VECTOR_FOREACH_IDX (&split, item, i)
            sml_cache_put(obs_controller->obs_group_cache, item);
        sol_ptr_vector_clear(&split);
    } else {
        if ((error = _merge_obs_groups(obs_controller)))
            return error;
    }

    if ((error = sml_observation_controller_rules_rebuild(obs_controller)))
        return error;

    return 0;
}

int
sml_observation_controller_remove_variables(struct sml_observation_controller
    *obs_controller,
    bool *inputs_to_remove,
    bool *outputs_to_remove)
{
    struct sml_observation_group *obs_group;
    uint16_t i;
    int error = 0;
    struct sol_ptr_vector *obs_group_list;

    obs_group_list = sml_cache_get_elements(obs_controller->obs_group_cache);
    SOL_PTR_VECTOR_FOREACH_IDX (obs_group_list, obs_group, i) {
        if ((error = sml_observation_group_remove_variables(obs_group,
                inputs_to_remove, outputs_to_remove)))
            goto exit;

        if (sml_observation_group_is_empty(obs_group)) {
            if (!sml_cache_remove_by_id(obs_controller->obs_group_cache, i)) {
                sml_critical("Could not remove the observation group");
                goto exit;
            }
            i--;
        }
    }

exit:
    free(inputs_to_remove);
    free(outputs_to_remove);
    return error;
}

int
sml_observation_controller_post_remove_variables(
    struct sml_observation_controller *obs_controller)
{
    int error;

    if ((error = _merge_obs_groups(obs_controller)))
        return error;

    _rule_group_map_clear(obs_controller);

    if ((error = sml_observation_controller_rules_rebuild(obs_controller)))
        return error;

    return 0;
}

bool
sml_observation_controller_save_state(struct sml_observation_controller
    *obs_controller, const char *path)
{
    char buf[SML_PATH_MAX];
    FILE *f;
    uint16_t i, j, count = 0;
    uint8_t version = VERSION;
    struct sol_ptr_vector *obs_group;
    struct sml_observation *obs;
    struct sol_ptr_vector *obs_group_list;

    snprintf(buf, sizeof(buf), "%s/%s", path, DEFAULT_OBS_CONTROLLER_FILE);
    if (!(f = fopen(buf, "wb"))) {
        sml_critical("Failed to open file %s\n", buf);
        return false;
    }

    if (fwrite(&version, sizeof(uint8_t), 1, f) < 1)
        goto save_state_false_end;

    obs_group_list = sml_cache_get_elements(obs_controller->obs_group_cache);
    SOL_PTR_VECTOR_FOREACH_IDX (obs_group_list, obs_group, i)
        count += sol_ptr_vector_get_len(obs_group);

    if (fwrite(&count, sizeof(uint16_t), 1, f) < 1)
        goto save_state_false_end;

    SOL_PTR_VECTOR_FOREACH_IDX (obs_group_list, obs_group, i) {
        SOL_PTR_VECTOR_FOREACH_IDX (obs_group, obs, j) {
            if (!sml_observation_save(obs, f))
                goto save_state_false_end;
        }
    }

    return fclose(f) == 0;

save_state_false_end:
    fclose(f);
    return false;
}

bool
sml_observation_controller_load_state(struct sml_observation_controller
    *obs_controller, const char *path)
{
    char buf[SML_PATH_MAX];
    FILE *f;
    uint8_t version;
    uint16_t count;
    int error;
    struct sml_observation *obs;

    snprintf(buf, sizeof(buf), "%s/%s", path, DEFAULT_OBS_CONTROLLER_FILE);
    if (!(f = fopen(buf, "rb"))) {
        sml_critical("Failed to open file %s\n", buf);
        return false;
    }

    if (fread(&version, sizeof(uint8_t), 1, f) < 1 || version != VERSION)
        goto load_state_false_end;

    if (fread(&count, sizeof(uint16_t), 1, f) < 1)
        goto load_state_false_end;

    while (count) {
        obs = sml_observation_load(f);
        if (!obs)
            goto load_state_false_end;

        if (_controller_append_observation(obs_controller, obs)
            != 0) {
            free(obs);
            goto load_state_false_end;
        }

        count--;
    }

    if ((error = sml_observation_controller_rules_rebuild(obs_controller)))
        goto load_state_false_end;

    return fclose(f) == 0;

load_state_false_end:
    fclose(f);
    return false;
}

int
sml_observation_controller_rules_rebuild(struct sml_observation_controller
    *obs_controller)
{
    int error;
    struct sol_ptr_vector *rule_group_list;
    uint16_t i;
    struct sol_ptr_vector *obs_group_list;

    obs_group_list = sml_cache_get_elements(obs_controller->obs_group_cache);
    _initialize_rule_group_map(obs_controller);
    SOL_VECTOR_FOREACH_IDX (&obs_controller->rule_group_map,
        rule_group_list, i) {
        error = sml_rule_group_list_rebuild(obs_controller->fuzzy,
            rule_group_list, obs_group_list,
            obs_controller->weight_threshold,
            obs_controller->simplification_disabled, i);
        if (error)
            return error;
    }

    return 0;
}

void
sml_observation_controller_set_simplification_disabled(
    struct sml_observation_controller *obs_controller, bool disabled)
{
    if (obs_controller->simplification_disabled == disabled)
        return;

    obs_controller->simplification_disabled = disabled;
    sml_observation_controller_rules_rebuild(obs_controller);
}

int
sml_observation_controller_remove_term(struct sml_observation_controller
    *obs_controller, uint16_t var_num,
    uint16_t term_num, bool input)
{
    struct sml_observation_group *obs_group;
    uint16_t i;
    int error = 0;
    struct sol_ptr_vector *obs_group_list;

    obs_group_list = sml_cache_get_elements(obs_controller->obs_group_cache);
    SOL_PTR_VECTOR_FOREACH_IDX (obs_group_list, obs_group, i) {
        if ((error = sml_observation_group_remove_terms(obs_group, var_num,
                term_num, input)))
            return error;

        if (sml_observation_group_is_empty(obs_group)) {
            if (!sml_cache_remove_by_id(obs_controller->obs_group_cache, i)) {
                sml_critical("Could not remove the observation group");
                return error;
            }
            i--;
        }
    }

    return error;
}

int
sml_observation_controller_merge_terms(struct sml_observation_controller
    *obs_controller, uint16_t var_num,
    uint16_t term1, uint16_t term2,
    bool input)
{
    struct sml_observation_group *obs_group;
    uint16_t i;
    int error = 0;
    struct sol_ptr_vector *obs_group_list;

    obs_group_list = sml_cache_get_elements(obs_controller->obs_group_cache);
    SOL_PTR_VECTOR_FOREACH_IDX (obs_group_list, obs_group, i) {
        if ((error = sml_observation_group_merge_terms(obs_group, var_num,
                term1, term2, input)))
            return error;

        if (sml_observation_group_is_empty(obs_group)) {
            if (!sml_cache_remove_by_id(obs_controller->obs_group_cache, i)) {
                sml_critical("Could not remove the observation group");
                return error;
            }
            i--;
        }
    }

    return error;
}

int
sml_observation_controller_split_terms(struct sml_fuzzy *fuzzy,
    struct sml_observation_controller
    *obs_controller, uint16_t var_num,
    uint16_t term_num, uint16_t term1,
    uint16_t term2, bool input)
{
    struct sml_observation_group *obs_group;
    uint16_t i;
    int error = 0;
    struct sol_ptr_vector *obs_group_list;

    obs_group_list = sml_cache_get_elements(obs_controller->obs_group_cache);
    SOL_PTR_VECTOR_FOREACH_IDX (obs_group_list, obs_group, i) {
        if ((error = sml_observation_group_split_terms(fuzzy, obs_group,
                var_num, term_num, term1, term2, input)))
            return error;

        if (sml_observation_group_is_empty(obs_group)) {
            if (!sml_cache_remove_by_id(obs_controller->obs_group_cache, i)) {
                sml_critical("Could not remove the observation group");
                return error;
            }
            i--;
        }
    }

    return error;
}

bool
sml_observation_controller_update_cache_size(struct sml_observation_controller
    *obs_controller, unsigned int max_memory_for_observation)
{
    unsigned int cache_size;

    if (!max_memory_for_observation)
        return true;

    cache_size = max_memory_for_observation /
        sml_observation_estimate_size(obs_controller->fuzzy);
    if (!cache_size)
        cache_size = 1;

    return sml_cache_set_max_size(obs_controller->obs_group_cache, cache_size);
}
