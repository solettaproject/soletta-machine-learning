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
