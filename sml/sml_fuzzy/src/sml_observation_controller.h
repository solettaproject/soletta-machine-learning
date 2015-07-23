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
