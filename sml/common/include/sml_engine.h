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
#include <sml_ann.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <config.h>

#ifdef __cplusplus
extern "C" {
#endif
struct sml_engine;
typedef bool (*sml_engine_load_file)(struct sml_engine *engine, const char *filename);
typedef void (*sml_engine_free)(struct sml_engine *engine);
typedef int (*sml_engine_process)(struct sml_engine *engine);
typedef bool (*sml_engine_predict)(struct sml_engine *engine);
typedef bool (*sml_engine_save)(struct sml_engine *engine, const char *path);
typedef bool (*sml_engine_load)(struct sml_engine *engine, const char *path);
typedef struct sml_variables_list *(*sml_engine_get_input_list)(struct sml_engine *engine);
typedef struct sml_variables_list *(*sml_engine_get_output_list)(struct sml_engine *engine);
typedef struct sml_variable *(*sml_engine_new_input)(struct sml_engine *engine, const char *name);
typedef struct sml_variable *(*sml_engine_new_output)(struct sml_engine *engine, const char *name);
typedef struct sml_variable *(*sml_engine_get_input)(struct sml_engine *engine, const char *name);
typedef struct sml_variable *(*sml_engine_get_output)(struct sml_engine *engine, const char *name);
typedef bool (*sml_engine_variable_set_value)(struct sml_variable *sml_variable, float value);
typedef float (*sml_engine_variable_get_value)(struct sml_variable *sml_variable);
typedef int (*sml_engine_variable_get_name)(struct sml_variable *sml_variable, char *var_name, size_t var_name_size);
typedef int (*sml_engine_variable_set_enabled)(struct sml_engine *engine, struct sml_variable *variable, bool enabled);
typedef bool (*sml_engine_variable_is_enabled)(struct sml_variable *variable);
typedef bool (*sml_engine_remove_variable)(struct sml_engine *engine, struct sml_variable *variable);
typedef uint16_t (*sml_engine_variables_list_get_length)(struct sml_variables_list *list);
typedef struct sml_variable *(*sml_engine_variables_list_index)(struct sml_variables_list *list, unsigned int index);
typedef bool (*sml_engine_variable_set_range)(struct sml_engine *engine, struct sml_variable *sml_variable, float min, float max);
typedef bool (*sml_engine_variable_get_range)(struct sml_variable *sml_variable, float *min, float *max);
typedef void (*sml_engine_print_debug)(struct sml_engine *engine, bool full);
typedef bool (*sml_engine_erase_knowledge)(struct sml_engine *engine);

int sml_call_read_state_cb(struct sml_engine *engine);
void sml_call_output_state_changed_cb(struct sml_engine *engine, struct sml_variables_list *changed);

struct sml_engine {
    /* General API */
    sml_engine_load_file load_file;
    sml_engine_free free;
    sml_engine_process process;
    sml_engine_predict predict;
    sml_engine_save save;
    sml_engine_load load;
    sml_engine_erase_knowledge erase_knowledge;

    /* Variables API */
    sml_engine_get_input_list get_input_list;
    sml_engine_get_output_list get_output_list;
    sml_engine_new_input new_input;
    sml_engine_new_output new_output;
    sml_engine_get_input get_input;
    sml_engine_get_output get_output;
    sml_engine_variable_get_name get_name;
    sml_engine_variable_set_value set_value;
    sml_engine_variable_get_value get_value;
    sml_engine_variable_set_enabled variable_set_enabled;
    sml_engine_variable_is_enabled variable_is_enabled;
    sml_engine_remove_variable remove_variable;
    sml_engine_variables_list_get_length variables_list_get_length;
    sml_engine_variables_list_index variables_list_index;
    sml_engine_variable_set_range variable_set_range;
    sml_engine_variable_get_range variable_get_range;

    /* Debug API */
    sml_engine_print_debug print_debug;
#ifdef Debug
    FILE *debug_file;
#endif

    uint32_t magic_number;
    sml_read_state_cb read_state_cb;
    void *read_state_cb_data;
    sml_change_cb output_state_changed_cb;
    void *output_state_changed_cb_data;
    bool learn_disabled;
    bool output_state_changed_called;
    uint16_t stabilization_hits;
    uint16_t hits;
    unsigned int obs_max_size;
};

#ifdef __cplusplus
}
#endif
