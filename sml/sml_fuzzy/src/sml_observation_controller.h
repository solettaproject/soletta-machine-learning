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

#include "sml_observation.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sml_observation_controller;

struct sml_observation_controller *sml_observation_controller_new(struct sml_fuzzy *fuzzy);
void sml_observation_controller_free(struct sml_observation_controller *observation);
void sml_observation_controller_clear(struct sml_observation_controller *observation);
int sml_observation_controller_observation_hit(struct sml_observation_controller *obs_controller, struct sml_measure *measure);
void sml_observation_controller_rule_generate(struct sml_observation_controller *obs_controller, sml_process_str_cb process_cb, void *data);
void sml_observation_controller_set_weight_threshold(struct sml_observation_controller *list, float weight_threshold);
void sml_observation_controller_debug(struct sml_observation_controller *obs_controller);
int sml_observation_controller_remove_variables(struct sml_observation_controller *obs_controller, bool *inputs_to_remove, bool *outputs_to_remove);
int sml_observation_controller_variable_set_enabled(struct sml_observation_controller *obs_controller, bool enabled);
bool sml_observation_controller_save_state(struct sml_observation_controller *obs_controller, const char *path);
bool sml_observation_controller_load_state(struct sml_observation_controller *obs_controller, const char *path);
int sml_observation_controller_post_remove_variables(struct sml_observation_controller *obs_controller);
int sml_observation_controller_rules_rebuild(struct sml_observation_controller *obs_controller);
void sml_observation_controller_set_simplification_disabled(struct sml_observation_controller *obs_controller, bool disabled);
int sml_observation_controller_remove_term(struct sml_observation_controller *obs_controller, uint16_t var_num, uint16_t term_num, bool input);
int sml_observation_controller_merge_terms(struct sml_observation_controller *obs_controller, uint16_t var_num, uint16_t term1, uint16_t term2, bool input);
int sml_observation_controller_split_terms(struct sml_fuzzy *fuzzy, struct sml_observation_controller *obs_controller, uint16_t var_num, uint16_t term_num, uint16_t term1, uint16_t term2, bool input);
bool sml_observation_controller_update_cache_size(struct sml_observation_controller *obs_controller, unsigned int max_memory_for_observation);

#ifdef __cplusplus
}
#endif
