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

#pragma once

#define ON_NULL_RETURN_VAL(var, val) do { \
        if (var == NULL) { \
            return val; \
        } \
} while (0)

#define ON_NULL_RETURN_NULL(var) ON_NULL_RETURN_VAL(var, NULL)

#define ON_NULL_RETURN(var) do { \
        if (var == NULL) { \
            return; \
        } \
} while (0)

#define API_EXPORT __attribute__((visibility("default")))
#define VARIABLE_MEMBERSHIP_THRESHOLD (0.05)
