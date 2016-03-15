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

#include <sml.h>
#include <stdint.h>
#include <stdbool.h>

struct sml_engine;

#ifdef __cplusplus
extern "C" {
#endif
int sml_ann_variable_list_add_variable(struct sml_variables_list *list, struct sml_variable *var);
float sml_ann_variable_get_value_by_index(struct sml_variable *var, unsigned int index);

struct sml_variable *sml_ann_variable_new(const char *name, bool input);
void sml_ann_variable_set_observations_array_index(struct sml_variable *var, unsigned int idx);
bool sml_ann_variable_is_input(struct sml_variable *var);
void sml_ann_variable_free(struct sml_variable *var);
struct sml_variables_list *sml_ann_variable_list_new();
void sml_ann_variable_list_free(struct sml_variables_list *list, bool free_var);
bool sml_ann_variable_list_remove(struct sml_variables_list *list, uint16_t index);

unsigned int sml_ann_variable_get_observations_length(struct sml_variable *var);

float sml_ann_variable_scale_value(struct sml_variable *var, float value);
float sml_ann_variable_descale_value(struct sml_variable *var, float value);

int sml_ann_variable_realloc_observation_array(struct sml_variable *var, unsigned int size);
float sml_ann_variable_get_previous_value(struct sml_variable *var);

float sml_ann_variable_get_last_stable_value(struct sml_variable *var);
void sml_ann_variable_set_last_stable_value(struct sml_variable *var, float value);

float sml_ann_variable_get_value(struct sml_variable *var);
bool sml_ann_variable_set_value(struct sml_variable *var, float value);
void sml_ann_variable_set_value_by_index(struct sml_variable *var, float value, unsigned int idx);

struct sml_variable *sml_ann_variables_list_index(struct sml_variables_list *list, unsigned int index);
uint16_t sml_ann_variables_list_get_length(struct sml_variables_list *list);
int sml_ann_variable_get_name(struct sml_variable *var, char *var_name, size_t var_name_size);
bool sml_ann_variable_set_range(struct sml_engine *engine, struct sml_variable *var, float min, float max);
bool sml_ann_variable_get_range(struct sml_variable *var, float *min, float *max);
int sml_ann_variable_set_enabled(struct sml_variable *var, bool enabled);
bool sml_ann_variable_is_enabled(struct sml_variable *var);

void sml_ann_variables_list_add_last_value_to_observation(struct sml_variables_list *list);
void sml_ann_variables_list_reset_observations(struct sml_variables_list *list, bool reset_control_variables);
void sml_ann_variables_list_set_current_value_as_stable(struct sml_variables_list *list);
int sml_ann_variables_list_realloc_observations_array(struct sml_variables_list *list, unsigned int size);
void sml_ann_variable_fill_with_random_values(struct sml_variable *var, unsigned int total);

#ifdef __cplusplus
}
#endif
