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
