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
