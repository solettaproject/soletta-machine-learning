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

#include <config.h>
#include <sml.h>
#include <sml_log.h>
#include <macros.h>
#include <sml_engine.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define LINE_SIZE (256)

static bool
_default_load_fll_file(struct sml_object *engine, const char *filename)
{
    char line[LINE_SIZE];
    char *token, *saveptr;
    FILE *f = fopen(filename, "r");
    uint16_t len;
    struct sml_variable *last_var = NULL;
    float min, max;

    if (!f)
        return false;

    while (fgets(line, LINE_SIZE, f)) {
        len = strlen(line);
        if (len <= 1 || line[0] == '#')
            continue;
        line[len - 1] = '\0';

        token = strtok_r(line, " \t", &saveptr);
        if (!strncmp(token, "InputVariable:", 14))
            last_var = sml_new_input(engine, token + 15);
        else if (!strncmp(token, "OutputVariable:", 15))
            last_var = sml_new_output(engine, token + 16);
        else if (last_var) {
            if (!strncmp(token, "enabled:", 8)) {
                token = strtok_r(NULL, " \t", &saveptr);
                if (!strncmp(token, "false", 5))
                    sml_variable_set_enabled(engine, last_var, false);
                else
                    sml_variable_set_enabled(engine, last_var, true);
            } else if (!strncmp(token, "range:", 6)) {
                token = strtok_r(NULL, " \t", &saveptr);
                if (!token)
                    continue;
                min = atof(token);

                token = strtok_r(NULL, " \t", &saveptr);
                if (!token)
                    continue;
                max = atof(token);
                sml_variable_set_range(engine, last_var, min, max);
            }
        }
    }

    fclose(f);
    return true;
}

API_EXPORT bool
sml_load_fll_file(struct sml_object *sml, const char *filename)
{
    ON_NULL_RETURN_VAL(sml, false);
    ON_NULL_RETURN_VAL(filename, false);
    struct sml_engine *engine = (struct sml_engine *)sml;
    if (!engine->load_file) {
        return _default_load_fll_file(sml, filename);
    }
    return engine->load_file(engine, filename);
}

API_EXPORT void
sml_free(struct sml_object *sml)
{
    ON_NULL_RETURN(sml);
    struct sml_engine *engine = (struct sml_engine *)sml;
    if (!engine->free) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_free is mandatory for engines.");
        return;
    }
    engine->free(engine);
}

API_EXPORT bool
sml_set_read_state_callback(struct sml_object *sml,
    sml_read_state_cb read_state_cb, void *data)
{
    ON_NULL_RETURN_VAL(sml, false);
    struct sml_engine *engine = (struct sml_engine *)sml;

    engine->read_state_cb = read_state_cb;
    engine->read_state_cb_data = data;

    return true;
}

API_EXPORT bool
sml_set_output_state_changed_callback(struct sml_object *sml,
    sml_change_cb output_state_changed_cb, void *data)
{
    ON_NULL_RETURN_VAL(sml, false);
    struct sml_engine *engine = (struct sml_engine *)sml;

    engine->output_state_changed_cb = output_state_changed_cb;
    engine->output_state_changed_cb_data = data;
    return true;
}

API_EXPORT bool
sml_set_stabilization_hits(struct sml_object *sml, uint16_t hits)
{
    ON_NULL_RETURN_VAL(sml, false);
    struct sml_engine *engine = (struct sml_engine *)sml;
    engine->stabilization_hits = hits;
    return true;
}

API_EXPORT bool
sml_set_learn_disabled(struct sml_object *sml, bool disable)
{
    ON_NULL_RETURN_VAL(sml, false);
    struct sml_engine *engine = (struct sml_engine *)sml;
    engine->learn_disabled = disable;
    return true;
}

API_EXPORT int
sml_process(struct sml_object *sml)
{
    struct sml_engine *engine = (struct sml_engine *)sml;

    ON_NULL_RETURN_VAL(sml, -EINVAL);
    if (!engine->process) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_process is mandatory for engines.");
        return -EINVAL;
    }
    return engine->process(engine);
}

API_EXPORT bool
sml_predict(struct sml_object *sml)
{
    struct sml_engine *engine = (struct sml_engine *)sml;

    ON_NULL_RETURN_VAL(sml, false);
    if (!engine->predict) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_predict is mandatory for engines.");
        return false;
    }
    return engine->predict(engine);
}

API_EXPORT bool
sml_save(struct sml_object *sml, const char *path)
{
    struct sml_engine *engine = (struct sml_engine *)sml;

    ON_NULL_RETURN_VAL(sml, false);
    ON_NULL_RETURN_VAL(path, false);
    if (!engine->save) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_save is mandatory for engines.");
        return false;
    }
    return engine->save(engine, path);
}

API_EXPORT bool
sml_load(struct sml_object *sml, const char *path)
{
    struct sml_engine *engine = (struct sml_engine *)sml;

    ON_NULL_RETURN_VAL(sml, false);
    ON_NULL_RETURN_VAL(path, false);
    if (!engine->load) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_load is mandatory for engines.");
        return false;
    }
    return engine->load(engine, path);
}

API_EXPORT struct sml_variables_list *
sml_get_input_list(struct sml_object *sml)
{
    struct sml_engine *engine = (struct sml_engine *)sml;

    ON_NULL_RETURN_VAL(sml, NULL);
    if (!engine->get_input_list) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_get_input_list is mandatory for engines.");
        return NULL;
    }
    return engine->get_input_list(engine);
}

API_EXPORT struct sml_variables_list *
sml_get_output_list(struct sml_object *sml)
{
    struct sml_engine *engine = (struct sml_engine *)sml;

    ON_NULL_RETURN_VAL(sml, NULL);
    if (!engine->get_output_list) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_get_output_list is mandatory for engines.");
        return NULL;
    }
    return engine->get_output_list(engine);
}

API_EXPORT struct sml_variable *
sml_new_input(struct sml_object *sml, const char *name)
{
    struct sml_engine *engine = (struct sml_engine *)sml;
    size_t name_len;

    ON_NULL_RETURN_VAL(sml, NULL);
    ON_NULL_RETURN_VAL(name, NULL);

    name_len = strlen(name);
    if (name_len == 0 || name_len >= SML_VARIABLE_NAME_MAX_LEN) {
        sml_warning("Invalid name size (%d) for variable %s", name_len,
            name);
        return NULL;
    }

    if (!engine->new_input) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_new_input is mandatory for engines.");
        return NULL;
    }

    return engine->new_input(engine, name);
}

API_EXPORT struct sml_variable *
sml_new_output(struct sml_object *sml, const char *name)
{
    struct sml_engine *engine = (struct sml_engine *)sml;
    size_t name_len;

    ON_NULL_RETURN_VAL(sml, NULL);
    ON_NULL_RETURN_VAL(name, NULL);

    name_len = strlen(name);
    if (name_len == 0 || name_len >= SML_VARIABLE_NAME_MAX_LEN) {
        sml_warning("Invalid name size (%d) for variable %s", name_len,
            name);
        return NULL;
    }

    if (!engine->new_output) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_new_output is mandatory for engines.");
        return NULL;
    }

    return engine->new_output(engine, name);
}

API_EXPORT struct sml_variable *
sml_get_input(struct sml_object *sml, const char *name)
{
    struct sml_engine *engine = (struct sml_engine *)sml;

    ON_NULL_RETURN_VAL(sml, NULL);
    ON_NULL_RETURN_VAL(name, NULL);
    if (!engine->get_input) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_get_input is mandatory for engines.");
        return NULL;
    }
    return engine->get_input(engine, name);
}

API_EXPORT struct sml_variable *
sml_get_output(struct sml_object *sml, const char *name)
{
    struct sml_engine *engine = (struct sml_engine *)sml;

    ON_NULL_RETURN_VAL(sml, NULL);
    ON_NULL_RETURN_VAL(name, NULL);
    if (!engine->get_output) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_get_output is mandatory for engines.");
        return NULL;
    }
    return engine->get_output(engine, name);
}

API_EXPORT bool
sml_variable_set_value(struct sml_object *sml,
    struct sml_variable *sml_variable, float value)
{
    struct sml_engine *engine = (struct sml_engine *)sml;

    ON_NULL_RETURN_VAL(sml_variable, false);
    ON_NULL_RETURN_VAL(sml, false);
    if (!engine->set_value) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_variable_set_value is mandatory for engines.");
        return false;
    }
    return engine->set_value(sml_variable, value);
}

API_EXPORT float
sml_variable_get_value(struct sml_object *sml,
    struct sml_variable *sml_variable)
{
    struct sml_engine *engine = (struct sml_engine *)sml;

    ON_NULL_RETURN_VAL(sml_variable, NAN);
    ON_NULL_RETURN_VAL(sml, NAN);
    if (!engine->get_value) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_variable_get_value is mandatory for engines.");
        return NAN;
    }
    return engine->get_value(sml_variable);
}

API_EXPORT int
sml_variable_get_name(struct sml_object *sml, struct sml_variable *sml_variable, char *var_name, size_t var_name_size)
{
    struct sml_engine *engine = (struct sml_engine *)sml;

    ON_NULL_RETURN_VAL(sml_variable, -EINVAL);
    ON_NULL_RETURN_VAL(sml, -EINVAL);
    ON_NULL_RETURN_VAL(var_name, -EINVAL);

    if (var_name_size == 0)
        return -EINVAL;

    if (!engine->get_name) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_variable_get_name is mandatory for engines.");
        return -EINVAL;
    }

    return engine->get_name(sml_variable, var_name, var_name_size);
}

API_EXPORT int
sml_variable_set_enabled(struct sml_object *sml,
    struct sml_variable *sml_variable, bool enabled)
{
    struct sml_engine *engine = (struct sml_engine *)sml;

    ON_NULL_RETURN_VAL(sml_variable, -EINVAL);
    ON_NULL_RETURN_VAL(sml, -EINVAL);
    if (!engine->variable_set_enabled) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_variable_set_enabled is mandatory for engines.");
        return -EINVAL;
    }
    return engine->variable_set_enabled(engine,
        sml_variable, enabled);
}

API_EXPORT bool
sml_variable_is_enabled(struct sml_object *sml,
    struct sml_variable *sml_variable)
{
    struct sml_engine *engine = (struct sml_engine *)sml;

    ON_NULL_RETURN_VAL(sml_variable, false);
    ON_NULL_RETURN_VAL(sml, false);
    if (!engine->variable_is_enabled) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_variable_is_enabled is mandatory for engines.");
        return false;
    }
    return engine->variable_is_enabled(sml_variable);
}

API_EXPORT bool
sml_remove_variable(struct sml_object *sml, struct sml_variable *sml_variable)
{
    struct sml_engine *engine = (struct sml_engine *)sml;

    ON_NULL_RETURN_VAL(sml_variable, false);
    ON_NULL_RETURN_VAL(sml, false);
    if (!engine->remove_variable) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_remove_variable is mandatory for engines.");
        return false;
    }
    return engine->remove_variable(engine, sml_variable);
}

API_EXPORT uint16_t
sml_variables_list_get_length(struct sml_object *sml,
    struct sml_variables_list *list)
{
    struct sml_engine *engine = (struct sml_engine *)sml;

    ON_NULL_RETURN_VAL(list, 0);
    ON_NULL_RETURN_VAL(sml, 0);
    if (!engine->variables_list_get_length) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_variables_list_get_length is mandatory for engines.");
        return 0;
    }
    return engine->variables_list_get_length(list);
}

API_EXPORT bool
sml_variables_list_contains(struct sml_object *sml,
    struct sml_variables_list *list, struct sml_variable *var)
{
    uint16_t i, len;

    ON_NULL_RETURN_VAL(sml, false);
    ON_NULL_RETURN_VAL(list, false);
    ON_NULL_RETURN_VAL(var, false);

    len = sml_variables_list_get_length(sml, list);
    for (i = 0; i < len; i++)
        if (sml_variables_list_index(sml, list, i) == var)
            return true;
    return false;

}

API_EXPORT struct sml_variable *
sml_variables_list_index(struct sml_object *sml,
    struct sml_variables_list *list, uint16_t index)
{
    struct sml_engine *engine = (struct sml_engine *)sml;

    ON_NULL_RETURN_VAL(list, NULL);
    ON_NULL_RETURN_VAL(sml, NULL);
    if (!engine->variables_list_index) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_variables_list_index is mandatory for engines.");
        return NULL;
    }
    return engine->variables_list_index(list, index);
}

API_EXPORT bool
sml_variable_set_range(struct sml_object *sml,
    struct sml_variable *sml_variable, float min, float max)
{
    struct sml_engine *engine = (struct sml_engine *)sml;

    ON_NULL_RETURN_VAL(sml_variable, false);
    ON_NULL_RETURN_VAL(sml, false);

    if (!engine->variable_set_range) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_variable_set_range is mandatory for engines.");
        return false;
    }

    if (isnan(min) && !sml_variable_get_range(sml, sml_variable, &min, NULL))
        return false;

    if (isnan(max) && !sml_variable_get_range(sml, sml_variable, NULL, &max))
        return false;

    if (max < min) {
        sml_warning("Max value (%f) is lower than min value (%f). Inverting.",
            min, max);
        return engine->variable_set_range(engine, sml_variable, max, min);
    }

    return engine->variable_set_range(engine, sml_variable, min, max);
}

API_EXPORT bool
sml_variable_get_range(struct sml_object *sml,
    struct sml_variable *sml_variable, float *min, float *max)
{
    struct sml_engine *engine = (struct sml_engine *)sml;

    ON_NULL_RETURN_VAL(sml_variable, false);
    if (!engine->variable_get_range) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_variable_get_range is mandatory for engines.");
        return false;
    }
    return engine->variable_get_range(sml_variable, min, max);
}

API_EXPORT void
sml_print_debug(struct sml_object *sml, bool full)
{
    struct sml_engine *engine = (struct sml_engine *)sml;

    ON_NULL_RETURN(sml);
    if (!engine->print_debug)
        return;
    engine->print_debug(engine, full);
}

API_EXPORT bool
sml_set_max_memory_for_observations(struct sml_object *sml,
    unsigned int obs_max_size)
{
    ON_NULL_RETURN_VAL(sml, false);
    struct sml_engine *engine = (struct sml_engine *)sml;

    engine->obs_max_size = obs_max_size;
    return true;
}
