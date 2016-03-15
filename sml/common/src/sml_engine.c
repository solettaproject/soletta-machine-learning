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
#include <config.h>
#include <stdarg.h>

#define LINE_SIZE (256)
#define STR_FORMAT(WIDTH) "%" #WIDTH "s"
#define STR_FMT(W) STR_FORMAT(W)

#ifdef Debug
#define SML_LOG_DEBUG_DATA(engine, ...)                     \
    do {                                                    \
        if (engine->debug_file) {                           \
            fprintf(engine->debug_file, __VA_ARGS__);       \
            fflush(engine->debug_file);                     \
        }                                                   \
    } while (0)

#define SML_LOG_DEBUG_DATA_VAR(engine, prefix, fmt, var, ...)                 \
    do {                                                                      \
        if (engine->debug_file)                                               \
            sml_log_debug_data_var(engine, prefix, fmt, var, ## __VA_ARGS__); \
    } while (0)

#define SML_LOG_DEBUG_DATA_LIST(engine, prefix, list, ...)                   \
    do {                                                                     \
        if (engine->debug_file)                                              \
            sml_log_debug_data_list(engine, prefix, list, ## __VA_ARGS__);   \
    } while (0)

#else
#define SML_LOG_DEBUG_DATA(...)
#define SML_LOG_DEBUG_DATA_VAR(...)
#define SML_LOG_DEBUG_DATA_LIST(...)
#endif

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

#ifdef Debug
void
sml_log_debug_data_var(struct sml_engine *engine, const char *prefix,
    const char *fmt, struct sml_variable *var, ...)
{
    int r;
    va_list args;
    char name[SML_VARIABLE_NAME_MAX_LEN + 1];

    va_start(args, var);
    r = sml_variable_get_name((struct sml_object *)engine, var, name,
        sizeof(name));
    if (r == 0) {
        fprintf(engine->debug_file, "%s %s ", prefix, name);
        vfprintf(engine->debug_file, fmt, args);
        fflush(engine->debug_file);
    }
    va_end(args);
}

void
sml_log_debug_data_list(struct sml_engine *engine, const char *prefix,
    struct sml_variables_list *list)
{
    int r;
    uint16_t len, i;
    struct sml_variable *var;
    char name[SML_VARIABLE_NAME_MAX_LEN + 1];

    SML_VARIABLES_LIST_FOREACH((struct sml_object *)engine, list, len, var, i) {
        r = sml_variable_get_name((struct sml_object *)engine, var, name,
            sizeof(name));
        if (r == 0)
            fprintf(engine->debug_file, "%s %s %f\n", prefix, name,
                sml_variable_get_value((struct sml_object *)engine, var));
    }
    fflush(engine->debug_file);
}
#endif

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
#ifdef Debug
    if (engine->debug_file)
        fclose(engine->debug_file);
#endif
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
sml_set_debug_log_file(struct sml_object *sml, const char *str)
{
#ifdef Debug
    ON_NULL_RETURN_VAL(sml, false);
    struct sml_engine *engine = (struct sml_engine *)sml;

    if (engine->debug_file)
        fclose(engine->debug_file);

    if (!str || str[0] == 0) {
        engine->debug_file = NULL;
        return true;
    }

    engine->debug_file = fopen(str, "a");
    return engine->debug_file != NULL;
#else
    return false;
#endif
}

#ifdef Debug
struct sml_variable *
variable_find_by_name(struct sml_object *sml, const char *name)
{
    struct sml_variable *var;

    var = sml_get_input(sml, name);
    if (var)
        return var;

    return sml_get_output(sml, name);
}

static bool
empty_read_state_cb(struct sml_object *sml, void *data)
{
    return true;
}

static void
empty_output_state_changed_cb(struct sml_object *sml,
    struct sml_variables_list *changed, void *data)
{
}

#endif

API_EXPORT bool
sml_load_debug_log_file(struct sml_object *sml, const char *str)
{
#ifdef Debug
    ON_NULL_RETURN_VAL(sml, false);
    ON_NULL_RETURN_VAL(str, false);
    FILE *file;
    char line[LINE_SIZE];
    char name[SML_VARIABLE_NAME_MAX_LEN + 1];
    int int_val, ret;
    float float_val, float_val2;
    struct sml_variable *var;
    struct sml_engine *engine = (struct sml_engine *)sml;
    sml_read_state_cb read_state_cb;
    sml_change_cb output_state_changed_cb;
    bool r;

    file = fopen(str, "r");
    ON_NULL_RETURN_VAL(file, false);

    read_state_cb = engine->read_state_cb;
    output_state_changed_cb = engine->output_state_changed_cb;
    engine->read_state_cb = empty_read_state_cb;
    engine->output_state_changed_cb = empty_output_state_changed_cb;
    r = false;
    while (fgets(line, LINE_SIZE, file)) {
        ret = strncmp(line, "sml_process", 11);
        if (ret == 0) {
            if (sml_process(sml)) {
                sml_error("Could not execute process");
                goto exit;
            }
            continue;
        }
        ret = strncmp(line, "sml_predict", 11);
        if (ret == 0) {
            sml_predict(sml);
            continue;
        }
        ret = sscanf(line, "sml_set_learn_disabled %d\n", &int_val);
        if (ret > 0) {
            sml_set_learn_disabled(sml, !!int_val);
            continue;
        }
        ret = sscanf(line, "sml_new_input %127s\n", name);
        if (ret > 0) {
            if (!sml_new_input(sml, name)) {
                sml_error("Could not create the input %s", name);
                goto exit;
            }
            continue;
        }
        ret = sscanf(line, "sml_new_output %127s\n", name);
        if (ret > 0) {
            if (!sml_new_output(sml, name)) {
                sml_error("Could not create the output %s", name);
                goto exit;
            }
            continue;
        }
        ret = sscanf(line, "sml_variable_set_value %127s %f\n", name,
            &float_val);
        if (ret > 1) {
            var = variable_find_by_name(sml, name);
            if (var)
                sml_variable_set_value(sml, var, float_val);
            continue;
        }
        ret = sscanf(line, "sml_variable_set_enabled %127s %d\n", name,
            &int_val);
        if (ret > 1) {
            var = variable_find_by_name(sml, name);
            if (var)
                sml_variable_set_enabled(sml, var, !!int_val);
            continue;
        }
        ret = sscanf(line, "sml_remove_variable %127s\n", name);
        if (ret > 0) {
            var = variable_find_by_name(sml, name);
            if (var)
                sml_remove_variable(sml, var);
            continue;
        }
        ret = sscanf(line, "sml_variable_set_range %127s %f %f\n", name,
            &float_val, &float_val2);
        if (ret > 2) {
            var = variable_find_by_name(sml, name);
            if (var)
                sml_variable_set_range(sml, var, float_val, float_val2);
            continue;
        }
        ret = strncmp(line, "sml_erase_knowledge", 19);
        if (ret == 0) {
            sml_erase_knowledge(sml);
            continue;
        }
    }
    r = true;
exit:
    fclose(file);
    engine->read_state_cb = read_state_cb;
    engine->output_state_changed_cb = output_state_changed_cb;

    return r;
#else
    return false;
#endif
}

API_EXPORT bool
sml_set_learn_disabled(struct sml_object *sml, bool disable)
{
    ON_NULL_RETURN_VAL(sml, false);
    struct sml_engine *engine = (struct sml_engine *)sml;
    engine->learn_disabled = disable;
    SML_LOG_DEBUG_DATA(engine, "sml_set_learn_disabled %d\n", !!disable);
    return true;
}

API_EXPORT int
sml_process(struct sml_object *sml)
{
    struct sml_engine *engine = (struct sml_engine *)sml;
    int r;

    ON_NULL_RETURN_VAL(sml, -EINVAL);
    if (!engine->process) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_process is mandatory for engines.");
        return -EINVAL;
    }
    r = engine->process(engine);
    SML_LOG_DEBUG_DATA(engine, "sml_process\n");
    return r;
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
    SML_LOG_DEBUG_DATA(engine, "sml_predict\n");
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

API_EXPORT bool
sml_erase_knowledge(struct sml_object *sml)
{
    struct sml_engine *engine = (struct sml_engine *)sml;

    SML_LOG_DEBUG_DATA(engine, "sml_erase_knowledge\n");
    ON_NULL_RETURN_VAL(sml, false);
    if (!engine->erase_knowledge) {
        sml_critical("Unexpected error. Implementation of function "
            "sml_load is mandatory for engines.");
        return false;
    }
    return engine->erase_knowledge(engine);
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

    SML_LOG_DEBUG_DATA(engine, "sml_new_input %s\n", name);
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

    SML_LOG_DEBUG_DATA(engine, "sml_new_output %s\n", name);
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

    SML_LOG_DEBUG_DATA_VAR(engine, "sml_variable_set_value", "%f\n",
        sml_variable, value);
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

    SML_LOG_DEBUG_DATA_VAR(engine, "sml_variable_set_enabled", "%d\n",
        sml_variable, !!enabled);
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
    SML_LOG_DEBUG_DATA_VAR(engine, "sml_remove_variable", "", sml_variable);
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

    SML_LOG_DEBUG_DATA_VAR(engine, "sml_variable_set_range", "%f %f\n",
        sml_variable, min, max);

    if (max < min) {
        sml_warning("Max value (%f) is lower than min value (%f). Inverting.",
            max, min);
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

int
sml_call_read_state_cb(struct sml_engine *engine)
{
    if (!engine->read_state_cb) {
        sml_critical("It's required to set a read_state_cb to read");
        return -EINVAL;
    }

    if (!engine->read_state_cb((struct sml_object *)engine,
        engine->read_state_cb_data))
        return -EAGAIN;

    SML_LOG_DEBUG_DATA(engine, "sml_call_read_state_cb\n");
    SML_LOG_DEBUG_DATA_LIST(engine, "sml_call_read_state_cb input",
        sml_get_input_list((struct sml_object *)engine));
    SML_LOG_DEBUG_DATA_LIST(engine, "sml_call_read_state_cb output",
        sml_get_output_list((struct sml_object *)engine));
    return 0;
}

void
sml_call_output_state_changed_cb(struct sml_engine *engine,
    struct sml_variables_list *changed)
{
    if (!engine->output_state_changed_cb) {
        sml_warning("output_state_changed called, but there is no callback registered.");
        return;
    }

    engine->output_state_changed_cb((struct sml_object *)engine, changed,
        engine->output_state_changed_cb_data);

    SML_LOG_DEBUG_DATA(engine, "sml_call_output_state_changed_cb\n");
    if (changed)
        SML_LOG_DEBUG_DATA_LIST(engine,
            "sml_call_output_state_changed_cb changed", changed);
}
