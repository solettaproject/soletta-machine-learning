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

#include <stdbool.h>
#include <stdint.h>

#define UNSET 0
#define SET 1

#ifdef __cplusplus
extern "C" {
#endif

struct sml_bit_array {
    uint16_t size;
    uint8_t *data;
};

bool sml_bit_array_set(struct sml_bit_array *array, uint16_t pos, uint8_t value);
uint8_t sml_bit_array_get(struct sml_bit_array *array, uint16_t pos);
void sml_bit_array_clear(struct sml_bit_array *array);
int sml_bit_array_size_set(struct sml_bit_array *array, uint16_t new_size, uint8_t initial_value);
uint16_t sml_bit_array_size_get(struct sml_bit_array *array);
void sml_bit_array_init(struct sml_bit_array *array);
unsigned int sml_bit_array_byte_size_get(struct sml_bit_array *array);
int sml_bit_array_remove(struct sml_bit_array *array, uint16_t pos);

#ifdef __cplusplus
}
#endif
