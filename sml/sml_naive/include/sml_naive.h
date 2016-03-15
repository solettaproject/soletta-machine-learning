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
#include <sml.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * @defgroup Naive_Engine Naive engine
 * @brief The SML naive testing engine.
 *
 * This engine is only used to for testing, it will never try to predict
 * an output. It will only call the ::sml_read_state_cb
 * @{
 */

/**
 * @brief Creates a SML naive engine.
 *
 * @remark Free with ::sml_free
 *
 * @return A ::sml_object object on success.
 * @return @c NULL on failure.
 *
 * @see ::sml_free
 */
struct sml_object *sml_naive_new(void);

/**
 * @brief Check if this SML object is a naive engine.
 *
 * @param sml A ::sml_object object.
 * @return @c true If it is naive.
 * @return @c false If it is not naive.
 */
bool sml_is_naive(struct sml_object *sml);

/**
 * @brief Check if SML was built with naive support.
 *
 * @return @c true If it is supported.
 * @return @c false If not supported.
 */
bool sml_naive_supported(void);

/**
 * @}
 */
#ifdef __cplusplus
}
#endif
