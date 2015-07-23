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

#include <stdint.h>
#include <stdbool.h>
#include <sol-vector.h>

#ifdef __cplusplus
extern "C" {
#endif
struct sml_cache;
typedef void (*sml_cache_element_free_cb)(void *element, void *data);
struct sml_cache *sml_cache_new(uint16_t max_elements, sml_cache_element_free_cb free_cb, void *free_cb_data);
bool sml_cache_put(struct sml_cache *cache, void *data);
struct sol_ptr_vector *sml_cache_get_elements(struct sml_cache *cache);
bool sml_cache_hit(struct sml_cache *cache, void *data);
bool sml_cache_remove(struct sml_cache *cache, void *data);
uint16_t sml_cache_get_size(struct sml_cache *cache);
void sml_cache_free(struct sml_cache *cache);
void sml_cache_clear(struct sml_cache *cache);
unsigned int sml_cache_get_total_elements_inserted(struct sml_cache *cache);
bool sml_cache_set_max_size(struct sml_cache *cache, uint16_t max_elements);
bool sml_cache_remove_by_id(struct sml_cache *cache, uint16_t elem);
void *sml_cache_get_element(struct sml_cache *cache, uint16_t elem);

#ifdef __cplusplus
}
#endif
