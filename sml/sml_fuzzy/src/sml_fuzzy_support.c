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

#include <sml_fuzzy.h>
#include <stdlib.h>
#include "macros.h"
#include "sml_log.h"

API_EXPORT bool
sml_fuzzy_set_variable_terms_auto_balance(struct sml_object *sml,
    bool variable_terms_auto_balance)
{
    sml_critical("Fuzzy engine not supported.");
    return false;
}

API_EXPORT bool
sml_fuzzy_set_rule_weight_threshold(struct sml_object *sml, float
    weight_threshold)
{
    sml_critical("Fuzzy engine not supported.");
    return false;
}

API_EXPORT bool
sml_fuzzy_conjunction_set(struct sml_object *sml, enum sml_fuzzy_tnorm norm)
{
    sml_critical("Fuzzy engine not supported.");
    return false;
}

API_EXPORT bool
sml_fuzzy_disjunction_set(struct sml_object *sml, enum sml_fuzzy_snorm norm)
{
    sml_critical("Fuzzy engine not supported.");
    return false;
}

API_EXPORT bool
sml_fuzzy_variable_remove_term(struct sml_object *sml, struct sml_variable *var,
    struct sml_fuzzy_term *term)
{
    sml_critical("Fuzzy engine not supported.");
    return false;
}

API_EXPORT struct sml_fuzzy_term *
sml_fuzzy_variable_add_term_rectangle(struct sml_object *sml, struct
    sml_variable *variable,
    const char *name, float start, float end,
    float height)
{
    sml_critical("Fuzzy engine not supported.");
    return NULL;
}

API_EXPORT struct sml_fuzzy_term *
sml_fuzzy_variable_add_term_triangle(struct sml_object *sml, struct
    sml_variable *variable,
    const char *name, float vertex_a,
    float vertex_b, float vertex_c,
    float height)
{
    sml_critical("Fuzzy engine not supported.");
    return NULL;
}

API_EXPORT struct sml_fuzzy_term *
sml_fuzzy_variable_add_term_cosine(struct sml_object *sml, struct
    sml_variable *variable,
    const char *name, float center, float width,
    float height)
{
    sml_critical("Fuzzy engine not supported.");
    return NULL;
}

API_EXPORT struct sml_fuzzy_term *
sml_fuzzy_variable_add_term_gaussian(struct sml_object *sml, struct
    sml_variable *variable,
    const char *name, float mean,
    float standard_deviation, float height)
{
    sml_critical("Fuzzy engine not supported.");
    return NULL;
}

API_EXPORT struct sml_fuzzy_term *
sml_fuzzy_variable_add_term_ramp(struct sml_object *sml, struct
    sml_variable *variable,
    const char *name, float start, float end,
    float height)
{
    sml_critical("Fuzzy engine not supported.");
    return NULL;
}

API_EXPORT bool
sml_fuzzy_set_simplification_disabled(struct sml_object *sml, bool disabled)
{
    sml_critical("Fuzzy engine not supported.");
    return false;
}

API_EXPORT struct sml_object *
sml_fuzzy_new(void)
{
    sml_critical("Fuzzy engine not supported.");
    return NULL;
}

API_EXPORT bool
sml_is_fuzzy(struct sml_object *sml)
{
    sml_critical("Fuzzy engine not supported.");
    return false;
}

API_EXPORT bool
sml_fuzzy_output_set_defuzzifier(struct sml_object *sml, struct
    sml_variable *var,
    enum sml_fuzzy_defuzzifier defuzzifier,
    int defuzzifier_resolution)
{
    sml_critical("Fuzzy engine not supported.");
    return false;
}

API_EXPORT bool
sml_fuzzy_output_set_default_value(struct sml_object *sml, struct
    sml_variable *var,
    float default_value)
{
    sml_critical("Fuzzy engine not supported.");
    return false;
}

API_EXPORT bool
sml_fuzzy_output_set_accumulation(struct sml_object *sml, struct
    sml_variable *var,
    enum sml_fuzzy_snorm accumulation)
{
    sml_critical("Fuzzy engine not supported.");
    return false;
}

API_EXPORT bool
sml_fuzzy_supported(void)
{
    return false;
}
