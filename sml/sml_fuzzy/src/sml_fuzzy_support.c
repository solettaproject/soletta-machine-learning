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
