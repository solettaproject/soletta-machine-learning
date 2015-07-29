/*
 * This file is part of the Soletta Project
 *
 * Copyright (C) 2015 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <sml.h>
#include <stdint.h>
#include <stdbool.h>

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
bool sml_ann_variable_set_range(struct sml_variable *var, float min, float max);
bool sml_ann_variable_get_range(struct sml_variable *var, float *min, float *max);
int sml_ann_variable_set_enabled(struct sml_variable *var, bool enabled);
bool sml_ann_variable_is_enabled(struct sml_variable *var);

void sml_ann_variables_list_add_last_value_to_observation(struct sml_variables_list *list);
void sml_ann_variables_list_reset_observations(struct sml_variables_list *list);
void sml_ann_variables_list_set_current_value_as_stable(struct sml_variables_list *list);
int sml_ann_variables_list_realloc_observations_array(struct sml_variables_list *list, unsigned int size);
void sml_ann_variable_fill_with_random_values(struct sml_variable *var, unsigned int total);

#ifdef __cplusplus
}
#endif
