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
sml_ann_variables_list_reset_observations(struct sml_variables_list *list)
{
    struct sml_variables_list_impl *impl =
        (struct sml_variables_list_impl *)list;
    struct sml_variable_impl *var;
    uint16_t i;

    SOL_PTR_VECTOR_FOREACH_IDX (&impl->variables, var, i)
        var->observations_idx = 0;
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
    var->min_value = FLT_MIN;
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

    return (value - midrange) / ((impl->max_value - impl->min_value) / 2.0);
}

float
sml_ann_variable_descale_value(struct sml_variable *var, float value)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;
    float midrange = (impl->max_value + impl->min_value) / 2.0;

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
    struct sml_variable *var = sol_ptr_vector_take(&impl->variables, index);

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

const char *
sml_ann_variable_get_name(struct sml_variable *var)
{
    struct sml_variable_impl *impl = (struct sml_variable_impl *)var;

    ON_NULL_RETURN_VAL(var, NULL);
    return impl->name;
}

bool
sml_ann_variable_set_range(struct sml_variable *var, float min, float max)
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
