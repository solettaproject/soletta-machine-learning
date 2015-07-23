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
