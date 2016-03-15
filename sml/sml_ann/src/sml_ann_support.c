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
