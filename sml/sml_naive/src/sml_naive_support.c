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

#include <sml_naive.h>
#include <stdlib.h>
#include "macros.h"
#include "sml_log.h"

API_EXPORT struct sml_object *
sml_naive_new(void)
{
    sml_critical("Naive engine not supported.");
    return NULL;
}

API_EXPORT bool
sml_is_naive(struct sml_object *sml)
{
    sml_critical("Naive engine not supported.");
    return false;
}

API_EXPORT bool
sml_naive_supported(void)
{
    return false;
}

