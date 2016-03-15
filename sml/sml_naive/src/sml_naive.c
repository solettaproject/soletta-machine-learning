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

#include "macros.h"
#include "sml_engine.h"
#include "sml_log.h"
#include <sol-vector.h>
#include "config.h"
#include <sml_naive.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#define NAIVE_MAGIC (0x2a17e)

typedef struct _Variable {
    char *name;
    bool enabled;
    float min, max, val;
} Variable;

struct sml_naive_engine {
    struct sml_engine engine;
    struct sol_ptr_vector input_list;
    struct sol_ptr_vector output_list;
};

static int
_sml_process(struct sml_engine *engine)
{
    if (!engine->read_state_cb) {
        sml_critical("There is not read callback registered");
        return -EINVAL;
    }

    if (!engine->read_state_cb((struct sml_object *)engine,
        engine->read_state_cb_data)) {
        sml_debug("Read cb returned false");
        return -EAGAIN;
    }
    return 0;
}

static bool
_sml_predict(struct sml_engine *engine)
{
    return true;
}

static bool
_sml_save(struct sml_engine *engine, const char *path)
{
    sml_debug("Save not needed for naive engine");
    return true;
}

static bool
_sml_load(struct sml_engine *engine, const char *path)
{
    sml_debug("Load not needed for naive engine");
    return true;
}

static void
_sml_free(struct sml_engine *engine)
{
    struct sml_naive_engine *naive_engine = (struct sml_naive_engine *)engine;
    uint16_t i;
    Variable *var;

    SOL_PTR_VECTOR_FOREACH_IDX (&naive_engine->input_list, var, i) {
        free(var->name);
        free(var);
    }
    sol_ptr_vector_clear(&naive_engine->input_list);

    SOL_PTR_VECTOR_FOREACH_IDX (&naive_engine->output_list, var, i) {
        free(var->name);
        free(var);
    }
    sol_ptr_vector_clear(&naive_engine->output_list);
    free(naive_engine);
}

static void
_print_list(struct sol_ptr_vector *list)
{
    uint16_t i;
    Variable *var;

    SOL_PTR_VECTOR_FOREACH_IDX (list, var, i)
        sml_debug("\t%s: %f (%f - %f)", var->name, var->val, var->min,
            var->max);
}

static void
_sml_print_debug(struct sml_engine *engine, bool full)
{
    struct sml_naive_engine *naive_engine = (struct sml_naive_engine *)engine;

    sml_debug("Inputs(%d) {",
        sol_ptr_vector_get_len(&naive_engine->input_list));
    _print_list(&naive_engine->input_list);
    sml_debug("}");
    sml_debug("Outputs(%d) {",
        sol_ptr_vector_get_len(&naive_engine->output_list));
    _print_list(&naive_engine->output_list);
    sml_debug("}");
}

static struct sml_variables_list *
_sml_get_input_list(struct sml_engine *engine)
{
    struct sml_naive_engine *naive_engine = (struct sml_naive_engine *)engine;

    return (struct sml_variables_list *)&naive_engine->input_list;
}

static struct sml_variables_list *
_sml_get_output_list(struct sml_engine *engine)
{
    struct sml_naive_engine *naive_engine = (struct sml_naive_engine *)engine;

    return (struct sml_variables_list *)&naive_engine->output_list;
}

static bool
_sml_remove_variable(struct sml_engine *engine, struct sml_variable *variable)
{
    struct sml_naive_engine *naive_engine = (struct sml_naive_engine *)engine;
    uint16_t i;
    Variable *var;

    SOL_PTR_VECTOR_FOREACH_IDX (&naive_engine->input_list, var, i)
        if ((struct sml_variable *)var == variable) {
            if (sol_ptr_vector_del(&naive_engine->input_list, i)) {
                sml_critical("Could not remove input variable");
                return false;
            }
            return true;
        }

    SOL_PTR_VECTOR_FOREACH_IDX (&naive_engine->output_list, var, i)
        if ((struct sml_variable *)var == variable) {
            if (sol_ptr_vector_del(&naive_engine->output_list, i)) {
                sml_critical("Could not remove output variable");
                return false;
            }
            return true;
        }

    sml_critical("Failed to remove. Variable not in naive engine.");
    return false;
}

static int
_sml_variable_set_enabled(struct sml_engine *engine,
    struct sml_variable *variable, bool enabled)
{
    Variable *var = (Variable *)variable;

    var->enabled = enabled;

    return true;
}

static struct sml_variable *
_sml_get_input(struct sml_engine *engine, const char *name)
{
    struct sml_naive_engine *naive_engine = (struct sml_naive_engine *)engine;
    uint16_t i;
    Variable *var;

    SOL_PTR_VECTOR_FOREACH_IDX (&naive_engine->input_list, var, i)
        if (!strcmp(var->name, name))
            return (struct sml_variable *)var;
    return NULL;
}

static struct sml_variable *
_sml_get_output(struct sml_engine *engine, const char *name)
{
    struct sml_naive_engine *naive_engine = (struct sml_naive_engine *)engine;
    uint16_t i;
    Variable *var;

    SOL_PTR_VECTOR_FOREACH_IDX (&naive_engine->output_list, var, i)
        if (!strcmp(var->name, name))
            return (struct sml_variable *)var;
    return NULL;
}

static Variable *
_create_var(const char *name)
{
    Variable *var = malloc(sizeof(Variable));

    if (!var)
        return NULL;

    var->name = strdup(name);
    if (!var->name) {
        free(var);
        return NULL;
    }
    var->enabled = true;
    var->min = NAN;
    var->max = NAN;
    var->val = NAN;

    return var;
}

static struct sml_variable *
_sml_new_input(struct sml_engine *engine, const char *name)
{
    struct sml_naive_engine *naive_engine = (struct sml_naive_engine *)engine;
    Variable *var = _create_var(name);

    if (!var)
        return NULL;

    if (sol_ptr_vector_append(&naive_engine->input_list, var)) {
        free(var);
        return NULL;
    }

    return (struct sml_variable *)var;
}

static struct sml_variable *
_sml_new_output(struct sml_engine *engine, const char *name)
{
    struct sml_naive_engine *naive_engine = (struct sml_naive_engine *)engine;
    Variable *var = _create_var(name);

    if (!var)
        return NULL;

    if (sol_ptr_vector_append(&naive_engine->output_list, var)) {
        free(var);
        return NULL;
    }

    return (struct sml_variable *)var;
}

static int
_sml_variable_get_name(struct sml_variable *sml_variable, char *var_name, size_t var_name_size)
{
    Variable *var = (Variable *)sml_variable;
    size_t name_len;

    if ((!var_name) || var_name_size == 0) {
        sml_warning("Invalid parameters");
        return -EINVAL;
    }

    name_len = strlen(var->name);
    if (var_name_size <= name_len) {
        sml_warning("Not enough space to get name %s(%d)",
            var->name, name_len);
        return -EINVAL;
    }

    strcpy(var_name, var->name);
    return 0;
}

static float
_sml_variable_get_value(struct sml_variable *sml_variable)
{
    Variable *var = (Variable *)sml_variable;

    return var->val;
}

static bool
_sml_variable_set_value(struct sml_variable *sml_variable, float val)
{
    Variable *var = (Variable *)sml_variable;

    var->val = val;
    return true;
}

static bool
_sml_variable_is_enabled(struct sml_variable *sml_variable)
{
    Variable *var = (Variable *)sml_variable;

    return var->enabled;
}

static struct sml_variable *
_variables_list_index(struct sml_variables_list *list, unsigned int index)
{
    if (index > UINT16_MAX)
        return NULL;

    return (struct sml_variable *)sol_ptr_vector_get(
        (struct sol_ptr_vector *)list, index);
}

static uint16_t
_sml_variables_list_get_length(struct sml_variables_list *list)
{
    return sol_ptr_vector_get_len((struct sol_ptr_vector *)list);
}

static bool
_sml_variable_set_range(struct sml_engine *engine,
    struct sml_variable *variable, float min, float max)
{
    Variable *var = (Variable *)variable;

    var->min = min;
    var->max = max;

    return true;
}

static bool
_sml_variable_get_range(struct sml_variable *variable, float *min, float *max)
{
    Variable *var = (Variable *)variable;

    if (min)
        *min = var->min;
    if (max)
        *max = var->max;
    return true;
}

API_EXPORT struct sml_object *
sml_naive_new(void)
{
    struct sml_naive_engine *naive_engine = calloc(1,
        sizeof(struct sml_naive_engine));

    if (!naive_engine)
        goto error_sml;

    sol_ptr_vector_init(&naive_engine->input_list);
    sol_ptr_vector_init(&naive_engine->output_list);

    naive_engine->engine.free = _sml_free;
    naive_engine->engine.process = _sml_process;
    naive_engine->engine.predict = _sml_predict;
    naive_engine->engine.save = _sml_save;
    naive_engine->engine.load = _sml_load;
    naive_engine->engine.get_input_list = _sml_get_input_list;
    naive_engine->engine.get_output_list = _sml_get_output_list;
    naive_engine->engine.new_input = _sml_new_input;
    naive_engine->engine.new_output = _sml_new_output;
    naive_engine->engine.get_input = _sml_get_input;
    naive_engine->engine.get_output = _sml_get_output;
    naive_engine->engine.get_name = _sml_variable_get_name;
    naive_engine->engine.set_value = _sml_variable_set_value;
    naive_engine->engine.get_value = _sml_variable_get_value;
    naive_engine->engine.variable_set_enabled = _sml_variable_set_enabled;
    naive_engine->engine.variable_is_enabled = _sml_variable_is_enabled;
    naive_engine->engine.remove_variable = _sml_remove_variable;
    naive_engine->engine.variables_list_get_length =
        _sml_variables_list_get_length;
    naive_engine->engine.variables_list_index = _variables_list_index;
    naive_engine->engine.variable_set_range = _sml_variable_set_range;
    naive_engine->engine.variable_get_range = _sml_variable_get_range;
    naive_engine->engine.print_debug = _sml_print_debug;
    naive_engine->engine.magic_number = NAIVE_MAGIC;

    return (struct sml_object *)&naive_engine->engine;

error_sml:
    sml_critical("Failed to create struct sml");
    return NULL;
}

API_EXPORT bool
sml_is_naive(struct sml_object *sml)
{
    ON_NULL_RETURN_VAL(sml, false);
    struct sml_naive_engine *naive_engine = (struct sml_naive_engine *)sml;
    return naive_engine->engine.magic_number == NAIVE_MAGIC;
}

API_EXPORT bool
sml_naive_supported(void)
{
    return true;
}
