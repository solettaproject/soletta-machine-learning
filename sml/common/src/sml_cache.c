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

#include "sml_cache.h"
#include <config.h>
#include <stdlib.h>
#include <sml_log.h>

struct sml_cache {
#ifdef Debug
    unsigned int total;
#endif
    uint16_t max_elements;
    struct sol_ptr_vector elements;
    sml_cache_element_free_cb free_cb;
    void *free_cb_data;
};

unsigned int
sml_cache_get_total_elements_inserted(struct sml_cache *cache)
{
#ifdef Debug
    return cache->total;
#else
    sml_warning("sml_cache_get_total_elements_inserted is not available under "
        "releasemode");
    return 0;
#endif
}

static bool
_sml_cache_find_index_by_data(struct sol_ptr_vector *elements, void *data,
    uint16_t *idx)
{
    uint16_t i;
    void *v;

    SOL_PTR_VECTOR_FOREACH_IDX (elements, v, i) {
        if (v == data) {
            *idx = i;
            return true;
        }
    }
    return false;
}

bool
sml_cache_set_max_size(struct sml_cache *cache, uint16_t max_elements)
{
    void *to_del;

    if (!max_elements || cache->max_elements == max_elements)
        return true;

    cache->max_elements = max_elements;
    while (sol_ptr_vector_get_len(&cache->elements) > max_elements) {
        to_del = sol_ptr_vector_steal(&cache->elements, 0);
        if (!to_del) {
            sml_critical("Could not remove an element from the cache!");
            return false;
        }
        cache->free_cb(to_del, cache->free_cb_data);
    }
    return true;
}

struct sml_cache *
sml_cache_new(uint16_t max_elements, sml_cache_element_free_cb free_cb,
    void *free_cb_data)
{
    struct sml_cache *cache;

    cache = calloc(1, sizeof(struct sml_cache));
    if (!cache) {
        sml_critical("Could not create the cache");
        return NULL;
    }
    sol_ptr_vector_init(&cache->elements);
    cache->max_elements = max_elements;
    cache->free_cb = free_cb;
    cache->free_cb_data = free_cb_data;
    return cache;
}

bool
sml_cache_put(struct sml_cache *cache, void *data)
{
    uint16_t count = sol_ptr_vector_get_len(&cache->elements);
    void *to_del;

    if (cache->max_elements && count == cache->max_elements) {
        to_del = sol_ptr_vector_steal(&cache->elements, 0);
        if (!to_del) {
            sml_critical("Could not remove the oldest element in the cache");
            return false;
        }
        cache->free_cb(to_del, cache->free_cb_data);
    }

    if (sol_ptr_vector_append(&cache->elements, data)) {
        sml_critical("Could not add element to the cache");
        return false;
    }
#ifdef Debug
    cache->total++;
#endif
    return true;
}

bool
sml_cache_hit(struct sml_cache *cache, void *data)
{
    uint16_t i;

    if (!_sml_cache_find_index_by_data(&cache->elements, data, &i))
        return false;

    if (sol_ptr_vector_del(&cache->elements, i) ||
        sol_ptr_vector_append(&cache->elements, data)) {
        sml_critical("Could not relocate the element in the cache");
        return false;
    }
    return true;
}

bool
sml_cache_remove(struct sml_cache *cache, void *data)
{
    uint16_t i;

    if (!_sml_cache_find_index_by_data(&cache->elements, data, &i)) {
        sml_critical("Could not find the index for data: %p", data);
        return false;
    }

    sol_ptr_vector_del(&cache->elements, i);
    cache->free_cb(data, cache->free_cb_data);
    return true;
}

struct sol_ptr_vector *
sml_cache_get_elements(struct sml_cache *cache)
{
    return &cache->elements;
}

uint16_t
sml_cache_get_size(struct sml_cache *cache)
{
    return sol_ptr_vector_get_len(&cache->elements);
}

void
sml_cache_free(struct sml_cache *cache)
{
    sml_cache_clear(cache);
    free(cache);
}

void
sml_cache_clear(struct sml_cache *cache)
{
    uint16_t i;
    void *v;

    SOL_PTR_VECTOR_FOREACH_IDX (&cache->elements, v, i)
        cache->free_cb(v, cache->free_cb_data);
    sol_ptr_vector_clear(&cache->elements);
}

bool
sml_cache_remove_by_id(struct sml_cache *cache, uint16_t elem)
{
    void *to_del;

    to_del = sol_ptr_vector_steal(&cache->elements, elem);
    if (!to_del) {
        sml_critical("Could not remove the oldest element in the cache");
        return false;
    }
    cache->free_cb(to_del, cache->free_cb_data);
    return true;
}

void *
sml_cache_get_element(struct sml_cache *cache, uint16_t elem)
{
    return sol_ptr_vector_get(&cache->elements, elem);
}
