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

#include <sol-vector.h>
#include "sml_observation_group.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sml_rule_group;

struct sml_rule_group *sml_rule_group_new();
void sml_rule_group_free(struct sml_fuzzy *fuzzy, struct sml_rule_group *rule_group);
int sml_rule_group_list_observation_append(struct sml_fuzzy *fuzzy, struct sol_ptr_vector *rule_group_list, struct sml_observation_group *obs_group, float weight_threshold, bool simplification_disabled, uint16_t output_number);
int sml_rule_group_rule_generate(struct sml_fuzzy *fuzzy, struct sml_rule_group *rule_group, float weight_threshold, uint16_t output_number, sml_process_str_cb process_cb, void *data);
int sml_rule_group_list_rebuild(struct sml_fuzzy *fuzzy, struct sol_ptr_vector *rule_group_list, struct sol_ptr_vector *obs_group_list, float weight_threshold, bool simplification_disabled, uint16_t output_number);
int sml_rule_group_list_rebalance(struct sml_fuzzy *fuzzy, struct sol_ptr_vector *rule_group_list, struct sml_observation_group *obs_group, float weight_threshold, uint16_t output_number);
bool sml_rule_group_list_observation_remove(struct sml_fuzzy *fuzzy, struct sol_ptr_vector *rule_group_list, struct sml_observation_group *obs_group_obj, float weight_threshold, uint16_t output_number);

#ifdef __cplusplus
}
#endif
