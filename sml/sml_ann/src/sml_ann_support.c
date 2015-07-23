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

#include <sml_ann.h>
#include <stdlib.h>
#include "macros.h"
#include "sml_log.h"

API_EXPORT bool
sml_ann_set_training_algorithm(struct sml_object *sml,
    enum sml_ann_training_algorithm algorithm)
{
    sml_critical("Ann engine not supported.");
    return false;
}

API_EXPORT bool
sml_ann_set_training_epochs(struct sml_object *sml,
    unsigned int training_epochs)
{
    sml_critical("Ann engine not supported.");
    return false;
}

API_EXPORT bool
sml_ann_set_desired_error(struct sml_object *sml, float desired_error)
{
    sml_critical("Ann engine not supported.");
    return false;
}

API_EXPORT bool
sml_ann_set_max_neurons(struct sml_object *sml, unsigned int max_neurons)
{
    sml_critical("Ann engine not supported.");
    return false;
}

API_EXPORT bool
sml_ann_set_candidate_groups(struct sml_object *sml,
    unsigned int candidate_groups)
{
    sml_critical("Ann engine not supported.");
    return false;
}

API_EXPORT bool
sml_ann_set_max_memory_for_observations(struct sml_object *sml,
    unsigned int max_size)
{
    sml_critical("Ann engine not supported.");
    return false;
}

API_EXPORT struct sml_object *
sml_ann_new(void)
{
    sml_critical("Ann engine not supported.");
    return NULL;
}

API_EXPORT bool
sml_is_ann(struct sml_object *sml)
{
    sml_critical("Ann engine not supported.");
    return false;
}

API_EXPORT bool
sml_ann_supported(void)
{
    return false;
}

API_EXPORT bool
sml_ann_set_cache_max_size(struct sml_object *sml, unsigned int max_size)
{
    return false;
}

API_EXPORT bool
sml_ann_use_pseudorehearsal_strategy(struct sml_object *sml,
    bool use_pseudorehearsal)
{
    return false;
}

API_EXPORT bool
sml_ann_set_initial_required_observations(struct sml_object *sml,
    unsigned int required_observations)
{
    return false;
}
