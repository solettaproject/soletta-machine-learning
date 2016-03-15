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
#include "sml_observation_controller.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sml_terms_manager {
    uint16_t total;
    struct sml_measure hits;
};

void sml_terms_manager_init(struct sml_terms_manager *terms_manager);
int sml_terms_manager_remove_variables(struct sml_terms_manager *terms_manager, bool *inputs_to_remove, bool *outputs_to_remove);
int sml_terms_manager_hit(struct sml_terms_manager *terms_manager, struct sml_fuzzy *fuzzy, struct sml_observation_controller *obs_controller, struct sml_measure *measure);
void sml_terms_manager_clear(struct sml_terms_manager *terms_manager);
void sml_terms_manager_debug(struct sml_terms_manager *terms_manager);
int sml_terms_manager_initialize_variable(struct sml_fuzzy *fuzzy, struct sml_variable *var);
void sml_terms_manager_remove_term(struct sml_terms_manager *terms_manager, uint16_t var_num, uint16_t term_num, bool is_input);

#ifdef __cplusplus
}
#endif
