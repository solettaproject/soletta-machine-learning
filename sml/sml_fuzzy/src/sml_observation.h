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
#include "sml_bit_array.h"
#include "sml_measure.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sml_observation;
typedef void (*sml_process_str_cb) (const char *str, void *data);

int sml_observation_new(struct sml_fuzzy *fuzzy, struct sml_measure *measure, struct sml_observation **observation);
void sml_observation_free(struct sml_observation *observation);
bool sml_observation_enabled_input_equals(struct sml_fuzzy *fuzzy, struct sml_observation *obs1, struct sml_observation *obs2);
bool sml_observation_input_equals(struct sml_fuzzy *fuzzy, struct sml_observation *obs1, struct sml_observation *obs2);
bool sml_observation_enabled_input_values_equals(struct sml_fuzzy *fuzzy, struct sml_observation *observation, struct sml_measure *measure);
void sml_observation_rule_generate(struct sml_fuzzy *fuzzy, struct sol_ptr_vector *observation_list, float weight_threshold, struct sml_bit_array *relevant_inputs, float *output_weights, uint16_t output_number, sml_process_str_cb process_cb, void *data);
int sml_observation_hit(struct sml_fuzzy *fuzzy, struct sml_observation *observation, struct sml_measure *measure, bool *hit);
void sml_observation_debug(struct sml_observation *observation);
int sml_observation_remove_variables(struct sml_observation *observation, bool *inputs_to_remove, bool *outputs_to_remove);
bool sml_observation_is_empty(struct sml_observation *observation);
int sml_observation_merge_output(struct sml_fuzzy *fuzzy, struct sml_observation *observation1, struct sml_observation *observation2);
bool sml_observation_is_base(struct sml_fuzzy *fuzzy, struct sml_observation *observation);
uint8_t sml_observation_input_term_get(struct sml_observation *observation, uint16_t input, uint16_t term);
bool sml_observation_output_equals(struct sml_fuzzy *fuzzy, struct sml_observation *obs1, struct sml_observation *obs2, uint16_t output_number);
void sml_observation_fill_output_weights(struct sml_fuzzy *fuzzy, struct sml_observation *obs, uint16_t *output_weights);
bool sml_observation_save(struct sml_observation *obs, FILE *f);
struct sml_observation *sml_observation_load(FILE *f);
int sml_observation_remove_term(struct sml_observation *obs, uint16_t var_num, uint16_t term_num, bool input);
int sml_observation_merge_terms(struct sml_observation *obs, uint16_t var_num, uint16_t term1, uint16_t term2, bool input);
int sml_observation_split_terms(struct sml_fuzzy *fuzzy, struct sml_observation *obs, uint16_t var_num, uint16_t term_num, uint16_t term1, uint16_t term2, bool input);
unsigned int sml_observation_estimate_size(struct sml_fuzzy *fuzzy);

#ifdef __cplusplus
}
#endif
