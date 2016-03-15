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
#include <stdbool.h>
#include <sol-vector.h>
#include "sml_measure.h"
#include <sml_fuzzy.h>
#include <sml_engine.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sml_fuzzy_term;
struct sml_fuzzy_rule;

struct sml_fuzzy {
    void *engine;
    struct sml_variables_list *input_list;
    struct sml_variables_list *output_list;
    uint16_t input_terms_count;
    uint16_t output_terms_count;
    struct sol_vector input_terms_width;
    struct sol_vector output_terms_width;
};

struct sml_fuzzy *sml_get_fuzzy(struct sml_engine *sml);
struct sml_fuzzy *sml_fuzzy_bridge_new(void);

struct sml_measure *sml_fuzzy_get_membership_values(struct sml_fuzzy *fuzzy);
int sml_fuzzy_get_membership_values_output(struct sml_fuzzy *fuzzy, struct sml_matrix *output_membership);
int sml_fuzzy_process_output(struct sml_fuzzy *fuzzy);
bool sml_fuzzy_load_file(struct sml_fuzzy *fuzzy, const char *filename);
bool sml_fuzzy_save_file(struct sml_fuzzy *fuzzy, const char *filename);
void sml_fuzzy_destroy(struct sml_fuzzy *fuzzy);
void sml_fuzzy_erase_rules(struct sml_fuzzy *fuzzy);

bool sml_fuzzy_bridge_conjunction_set(struct sml_fuzzy *fuzzy, enum sml_fuzzy_tnorm norm);
bool sml_fuzzy_bridge_disjunction_set(struct sml_fuzzy *fuzzy, enum sml_fuzzy_snorm norm);

uint16_t sml_fuzzy_variables_list_get_length(struct sml_variables_list *list);
struct sml_variable *sml_fuzzy_variables_list_index(struct sml_variables_list *list, uint16_t index);

struct sml_variable *sml_fuzzy_new_input(struct sml_fuzzy *fuzzy, const char *name);
struct sml_variable *sml_fuzzy_new_output(struct sml_fuzzy *fuzzy, const char *name);
struct sml_variable *sml_fuzzy_get_input(struct sml_fuzzy *fuzzy, const char *name);
struct sml_variable *sml_fuzzy_get_output(struct sml_fuzzy *fuzzy, const char *name);
int sml_fuzzy_variable_get_name(struct sml_variable *variable, char *var_name, size_t var_name_size);
float sml_fuzzy_variable_get_value(struct sml_variable *variable);
void sml_fuzzy_variable_set_value(struct sml_variable *variable, float value);
uint16_t sml_fuzzy_variable_terms_count(struct sml_variable *variable);
void sml_fuzzy_variable_set_enabled(struct sml_variable *variable, bool enabled);
bool sml_fuzzy_variable_is_enabled(struct sml_variable *variable);
bool sml_fuzzy_remove_variable(struct sml_fuzzy *fuzzy, struct sml_variable *variable);

void sml_fuzzy_bridge_variable_set_range(struct sml_variable *variable, float min, float max);
bool sml_fuzzy_bridge_output_set_defuzzifier(struct sml_variable *variable, enum sml_fuzzy_defuzzifier defuzzifier, int defuzzifier_resolution);
bool sml_fuzzy_bridge_output_set_accumulation(struct sml_variable *variable, enum sml_fuzzy_snorm accumulation);
bool sml_fuzzy_bridge_output_set_default_value(struct sml_variable *variable, float default_value);

bool sml_fuzzy_variable_get_range(struct sml_variable *variable, float *min, float *max);
struct sml_fuzzy_term *sml_fuzzy_bridge_variable_add_term_rectangle(struct sml_fuzzy *fuzzy, struct sml_variable *variable, const char *name, float start, float end);
struct sml_fuzzy_term *sml_fuzzy_bridge_variable_add_term_triangle(struct sml_fuzzy *fuzzy, struct sml_variable *variable, const char *name, float vertex_a, float vertex_b, float vertex_c);
struct sml_fuzzy_term *sml_fuzzy_bridge_variable_add_term_cosine(struct sml_fuzzy *fuzzy, struct sml_variable *variable, const char *name, float center, float width);
struct sml_fuzzy_term *sml_fuzzy_bridge_variable_add_term_gaussian(struct sml_fuzzy *fuzzy, struct sml_variable *variable, const char *name, float mean, float standard_deviation);
struct sml_fuzzy_term *sml_fuzzy_bridge_variable_add_term_ramp(struct sml_fuzzy *fuzzy, struct sml_variable *variable, const char *name, float start, float end);
int sml_fuzzy_bridge_variable_remove_term(struct sml_variable *variable, uint16_t term_num);
struct sml_fuzzy_term *sml_fuzzy_variable_get_term(struct sml_variable *variable, uint16_t index);
int sml_fuzzy_term_get_name(struct sml_fuzzy_term *term, char *term_name, size_t term_name_size);
bool sml_fuzzy_term_get_range(struct sml_fuzzy_term *term, float *min, float *max);
bool sml_fuzzy_term_set_range(struct sml_fuzzy_term *term, float min, float max);

bool sml_fuzzy_is_input(struct sml_fuzzy *fuzzy, struct sml_variable *variable, uint16_t *index);
bool sml_fuzzy_is_output(struct sml_fuzzy *fuzzy, struct sml_variable *variable, uint16_t *index);
bool sml_fuzzy_is_rule_block_empty(struct sml_fuzzy *fuzzy);
struct sml_fuzzy_rule *sml_fuzzy_rule_add(struct sml_fuzzy *fuzzy, const char *rule);
bool sml_fuzzy_rule_free(struct sml_fuzzy *fuzzy, struct sml_fuzzy_rule *rule);
bool sml_fuzzy_find_variable(struct sml_variables_list *list, struct sml_variable *var, uint16_t *index);
void sml_fuzzy_update_terms_count(struct sml_fuzzy *fuzzy);
bool sml_fuzzy_variable_find_term(struct sml_variable *var, struct sml_fuzzy_term *term, uint16_t *index);
void sml_fuzzy_debug(struct sml_fuzzy *fuzzy);
struct sml_variables_list *sml_fuzzy_variables_list_new(struct sml_fuzzy *fuzzy, struct sol_vector *indexes);
void sml_fuzzy_variables_list_free(struct sml_variables_list *list);
bool sml_fuzzy_set_read_values(struct sml_fuzzy *fuzzy, struct sml_variables_list *outputs);
bool sml_fuzzy_bridge_variable_set_default_term_width(struct sml_fuzzy *fuzzy, struct sml_variable *var, float width);
float sml_fuzzy_bridge_variable_get_default_term_width(struct sml_fuzzy *fuzzy, struct sml_variable *var);
bool sml_fuzzy_bridge_variable_set_is_id(struct sml_fuzzy *fuzzy, struct sml_variable *var, bool is_id);
bool sml_fuzzy_bridge_variable_get_is_id(struct sml_fuzzy *fuzzy, struct sml_variable *var);
bool sml_fuzzy_bridge_variable_term_triangle_update(struct sml_fuzzy_term *term, float vertex_a, float vertex_b, float vertex_c);

#ifdef __cplusplus
}
#endif
