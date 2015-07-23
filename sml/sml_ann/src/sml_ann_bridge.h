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

#include <sml_ann.h>
#include <sol-vector.h>

#ifdef __cplusplus
extern "C" {
#endif
struct sml_ann_bridge;

struct sml_ann_bridge *sml_ann_bridge_new(unsigned int inputs,
    unsigned int outputs,
    unsigned int candidate_groups,
    unsigned int epochs,
    enum sml_ann_training_algorithm train_algorithm,
    struct sol_vector *activation_functions,
    int *error_code);

void sml_ann_bridge_free(struct sml_ann_bridge *iann);
bool sml_ann_bridge_predict_output(struct sml_ann_bridge *iann, struct sml_variables_list *inputs,
    struct sml_variables_list *outputs);
bool sml_ann_bridge_is_trained(struct sml_ann_bridge *iann);
int sml_ann_bridge_train(struct sml_ann_bridge *iann, struct sml_variables_list *inputs,
    struct sml_variables_list *outputs, float desired_train_error,
    unsigned int required_observations,
    unsigned int max_neurons,
    unsigned int *required_observations_suggestion,
    bool use_pseudorehearsal);
bool sml_ann_bridge_save(struct sml_ann_bridge *iann, const char *ann_path, const char *cfg_path);
struct sml_ann_bridge *sml_ann_bridge_load_from_file(const char *ann_path, const char *cfg_path);
int sml_ann_bridge_consider_trained(struct sml_ann_bridge *iann, struct sml_variables_list *inputs,
    unsigned int observations, bool use_pseudorehearsal);
unsigned int sml_ann_bridge_inputs_in_confidence_interval_hits(struct sml_ann_bridge *iann,
    struct sml_variables_list *inputs);
void sml_ann_bridge_add_observation(struct sml_ann_bridge *iann,
    struct sml_variables_list *inputs,
    struct sml_variables_list *outputs);
void sml_ann_bridge_print_debug(struct sml_ann_bridge *ann);
float sml_ann_bridge_get_confidence_interval_sum(struct sml_ann_bridge *iann);
float sml_ann_bridge_confidence_intervals_distance_sum(struct sml_ann_bridge *iann,
    struct sml_variables_list *inputs);
bool sml_ann_bridge_predict_output_by_index(struct sml_ann_bridge *iann,
    struct sml_variables_list *inputs,
    struct sml_variables_list *outputs,
    unsigned int idx);
float sml_ann_bridge_get_error(struct sml_ann_bridge *iann, struct sml_variables_list *input,
    struct sml_variables_list *output,
    unsigned int observations);
bool sml_ann_bridge_save_with_no_cfg(struct sml_ann_bridge *iann, const char *ann_path);
struct sml_ann_bridge *sml_ann_bridge_load_from_file_with_no_cfg(const char *ann_path);
#ifdef __cplusplus
}
#endif
