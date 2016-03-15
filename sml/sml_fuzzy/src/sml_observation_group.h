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

#pragma once

#include "sml_fuzzy_bridge.h"
#include "sml_observation.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sml_observation_group;

struct sml_observation_group *sml_observation_group_new();
int sml_observation_group_observation_hit(struct sml_fuzzy *fuzzy, struct sml_observation_group *obs_group, struct sml_measure *measure, bool *hit);
void sml_observation_group_free(struct sml_observation_group *obs_group);
void sml_observation_group_rule_generate(struct sml_fuzzy *fuzzy, struct sml_observation_group *obs_group, float weight_threshold, struct sml_bit_array *relevant_inputs, float *output_weights, uint16_t output_number, sml_process_str_cb process_cb, void *data);
void sml_observation_group_debug(struct sml_observation_group *obs_group);
int sml_observation_group_merge(struct sml_fuzzy *fuzzy, struct sml_observation_group *obs_group1, struct sml_observation_group *obs_group2);
bool sml_observation_group_enabled_input_equals(struct sml_fuzzy *fuzzy, struct sml_observation_group *obs_group1, struct sml_observation_group *obs_group2);
void sml_observation_group_split(struct sml_fuzzy *fuzzy, struct sml_observation_group *obs_group, struct sol_ptr_vector *split);
int sml_observation_group_remove_variables(struct sml_observation_group *obs_group, bool *inputs_to_remove, bool *outputs_to_remove);
struct sml_observation *sml_observation_group_get_first_observation(struct sml_observation_group *obs_group);
void sml_observation_group_fill_output_weights(struct sml_fuzzy *fuzzy, struct sml_observation_group *obs_group, uint16_t *output_weights);
bool sml_observation_group_is_empty(struct sml_observation_group *obs_group);
int sml_observation_group_remove_terms(struct sml_observation_group *obs_group, uint16_t var_num, uint16_t term_num, bool input);
int sml_observation_group_merge_terms(struct sml_observation_group *obs_group, uint16_t var_num, uint16_t term1, uint16_t term2, bool input);
int sml_observation_group_split_terms(struct sml_fuzzy *fuzzy, struct sml_observation_group *obs_group, uint16_t var_num, uint16_t term_num, int16_t term1, uint16_t term2, bool input);
int sml_observation_group_observation_append(struct sml_fuzzy *fuzzy, struct sml_observation_group *obs_group, struct sml_observation *obs, bool *appended);

#ifdef __cplusplus
}
#endif
