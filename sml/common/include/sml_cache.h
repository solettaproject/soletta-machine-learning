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
