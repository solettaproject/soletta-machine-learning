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

#include <stdlib.h>
#include "sml_ann_variable_list.h"
#include <macros.h>
#include <float.h>
#include <math.h>
#include <errno.h>
#include <sol-vector.h>
#include <string.h>
#include <sml_log.h>

struct sml_variable_impl {
    char *name;
    unsigned int observations_idx;
    /* Keep this as plain C-Array, it's easier to resize
       and preallocate the data */
    float *observations;

    float current_value;
    float previous_value;
    float last_stable_value;
    float min_value;
    float max_value;

    bool enabled;
    bool input;
};

struct sml_variables_list_impl {
    struct sol_ptr_vector variables;
};

void
sml_ann_variable_fill_with_random_values(struct sml_variable *var,
    unsigned int total)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;
    unsigned int i;

    for (i = 0; i < total; i++) {
        impl->observations[impl->observations_idx++] =
            impl->min_value + rand() /
            (RAND_MAX / (impl->max_value - impl->min_value));
    }
}

void
sml_ann_variable_set_value_by_index(struct sml_variable *var, float value,
    unsigned int idx)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;

    impl->observations[idx] = value;
}

void
sml_ann_variables_list_add_last_value_to_observation(
    struct sml_variables_list *list)
{
    struct sml_variables_list_impl *impl =
        (struct sml_variables_list_impl *)list;
    struct sml_variable_impl *var;
    uint16_t i;

    SOL_PTR_VECTOR_FOREACH_IDX (&impl->variables, var, i)
        var->observations[var->observations_idx++] = var->current_value;
}

void
sml_ann_variables_list_reset_observations(struct sml_variables_list *list,
    bool reset_control_varaibles)
{
    struct sml_variables_list_impl *impl =
        (struct sml_variables_list_impl *)list;
    struct sml_variable_impl *var;
    uint16_t i;

    SOL_PTR_VECTOR_FOREACH_IDX (&impl->variables, var, i) {
        var->observations_idx = 0;
        if (reset_control_varaibles) {
            var->current_value = var->previous_value =
                    var->last_stable_value = NAN;
        }
    }
}

void
sml_ann_variables_list_set_current_value_as_stable(
    struct sml_variables_list *list)
{
    struct sml_variables_list_impl *impl =
        (struct sml_variables_list_impl *)list;
    struct sml_variable_impl *var;
    uint16_t i;

    SOL_PTR_VECTOR_FOREACH_IDX (&impl->variables, var, i)
        var->last_stable_value = var->current_value;
}

int
sml_ann_variable_realloc_observation_array(struct sml_variable *var,
    unsigned int size)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;
    float *observations;
    unsigned int i;

    observations = realloc(impl->observations, sizeof(float) * size);
    if (!observations) {
        sml_critical("Could not realloc the variable %s observations array" \
            " to %d", impl->name, size);
        return -errno;
    }

    impl->observations = observations;

    if (size < impl->observations_idx)
        impl->observations_idx = size;
    else {
        for (i = impl->observations_idx; i < size; i++)
            impl->observations[i] = NAN;
    }
    return 0;
}

int
sml_ann_variables_list_realloc_observations_array(
    struct sml_variables_list *list, unsigned int size)
{
    struct sml_variables_list_impl *impl =
        (struct sml_variables_list_impl *)list;
    struct sml_variable_impl *var;
    uint16_t i;
    int r;

    SOL_PTR_VECTOR_FOREACH_IDX (&impl->variables, var, i) {
        r = sml_ann_variable_realloc_observation_array(
            (struct sml_variable *)var, size);
        if (r < 0)
            return r;
    }
    return 0;
}

void
sml_ann_variable_free(struct sml_variable *var)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;

    free(impl->observations);
    free(impl->name);
    free(impl);
}

struct sml_variable *
sml_ann_variable_new(const char *name, bool input)
{
    struct sml_variable_impl *var = calloc(1, sizeof(struct sml_variable_impl));

    ON_NULL_RETURN_VAL(var, NULL);
    var->name = strdup(name);
    if (!var->name) {
        free(var);
        return NULL;
    }

    var->current_value = NAN;
    var->previous_value = NAN;
    var->last_stable_value = NAN;
    var->min_value = -FLT_MAX;
    var->max_value = FLT_MAX;
    var->enabled = true;
    var->input = input;

    return (struct sml_variable *)var;
}

void
sml_ann_variable_set_observations_array_index(struct sml_variable *var,
    unsigned int idx)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;

    impl->observations_idx = idx;
}

float
sml_ann_variable_get_last_stable_value(struct sml_variable *var)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;

    return impl->last_stable_value;
}

struct sml_variables_list *
sml_ann_variable_list_new()
{
    struct sml_variables_list_impl *list =
        malloc(sizeof(struct sml_variables_list_impl));

    ON_NULL_RETURN_VAL(list, NULL);
    sol_ptr_vector_init(&list->variables);
    return (struct sml_variables_list *)list;
}

float
sml_ann_variable_scale_value(struct sml_variable *var, float value)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;
    float midrange = (impl->max_value + impl->min_value) / 2.0;

    if (value > impl->max_value)
        value = impl->max_value;
    else if (value < impl->min_value)
        value = impl->min_value;
    return (value - midrange) / ((impl->max_value - impl->min_value) / 2.0);
}

float
sml_ann_variable_descale_value(struct sml_variable *var, float value)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;
    float midrange = (impl->max_value + impl->min_value) / 2.0;

    if (value > 1.0)
        value = 1.0;
    else if (value < -1.0)
        value = -1.0;
    return (((impl->max_value - impl->min_value) / 2.0) * value) + midrange;
}

void
sml_ann_variable_list_free(struct sml_variables_list *list, bool free_var)
{
    struct sml_variables_list_impl *impl =
        (struct sml_variables_list_impl *)list;
    struct sml_variable *var;
    uint16_t i;

    if (free_var) {
        SOL_PTR_VECTOR_FOREACH_IDX (&impl->variables, var, i)
            sml_ann_variable_free(var);
    }
    sol_ptr_vector_clear(&impl->variables);
    free(impl);
}

int
sml_ann_variable_list_add_variable(struct sml_variables_list *list,
    struct sml_variable *var)
{
    struct sml_variables_list_impl *impl =
        (struct sml_variables_list_impl *)list;

    return sol_ptr_vector_append(&impl->variables, var);
}

float
sml_ann_variable_get_value_by_index(struct sml_variable *var,
    unsigned int index)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;

    return impl->observations[index];
}

unsigned int
sml_ann_variable_get_observations_length(struct sml_variable *var)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;

    return impl->observations_idx;
}

bool
sml_ann_variable_list_remove(struct sml_variables_list *list, uint16_t index)
{
    struct sml_variables_list_impl *impl =
        (struct sml_variables_list_impl *)list;
    struct sml_variable *var = sol_ptr_vector_steal(&impl->variables, index);

    if (!var) {
        sml_critical("Could not remove the index %d", index);
        return false;
    }
    sml_ann_variable_free(var);
    return true;
}

float
sml_ann_variable_get_previous_value(struct sml_variable *var)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;

    return impl->previous_value;
}

bool
sml_ann_variable_is_input(struct sml_variable *var)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;

    return impl->input;
}

float
sml_ann_variable_get_value(struct sml_variable *var)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;

    ON_NULL_RETURN_VAL(var, NAN);
    return impl->current_value;
}

bool
sml_ann_variable_set_value(struct sml_variable *var, float value)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;

    ON_NULL_RETURN_VAL(var, false);
    impl->previous_value = impl->current_value;
    impl->current_value  = value;
    return true;
}

struct sml_variable *
sml_ann_variables_list_index(struct sml_variables_list *list,
    unsigned int index)
{
    struct sml_variables_list_impl *impl =
        (struct sml_variables_list_impl *)list;

    ON_NULL_RETURN_VAL(list, NULL);
    return sol_ptr_vector_get(&impl->variables, index);
}

uint16_t
sml_ann_variables_list_get_length(struct sml_variables_list *list)
{
    struct sml_variables_list_impl *impl =
        (struct sml_variables_list_impl *)list;

    ON_NULL_RETURN_VAL(list, 0);
    return sol_ptr_vector_get_len(&impl->variables);
}

int
sml_ann_variable_get_name(struct sml_variable *var, char *var_name, size_t var_name_size)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;
    size_t name_len;

    ON_NULL_RETURN_VAL(var, -EINVAL);

    if ((!var_name) || var_name_size == 0) {
        sml_warning("Invalid parameters");
        return -EINVAL;
    }

    name_len = strlen(impl->name);

    if (var_name_size <= name_len) {
        sml_warning("Not enough space to get name %s(%d)",
            impl->name, name_len);
        return -EINVAL;
    }

    strcpy(var_name, impl->name);
    return 0;
}

bool
sml_ann_variable_set_range(struct sml_engine *engine, struct sml_variable *var,
    float min, float max)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;

    ON_NULL_RETURN_VAL(var, false);
    impl->min_value = min;
    impl->max_value = max;
    return true;
}

bool
sml_ann_variable_get_range(struct sml_variable *var, float *min, float *max)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;

    ON_NULL_RETURN_VAL(var, false);
    if (min)
        *min = impl->min_value;
    if (max)
        *max = impl->max_value;
    return true;
}

int
sml_ann_variable_set_enabled(struct sml_variable *var, bool enabled)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;

    impl->enabled = enabled;

    return 0;
}

bool
sml_ann_variable_is_enabled(struct sml_variable *var)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;

    ON_NULL_RETURN_VAL(var, false);
    return impl->enabled;
}
