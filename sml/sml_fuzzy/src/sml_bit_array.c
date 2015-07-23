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

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "sml_bit_array.h"
#include "sml.h"

//Change this constants to change the size of the bit array element.
#define BITS 1
#define ITEMS_IN_BYTE 8 //(8/BITS)
#define MASK 1 //(11b)

static uint16_t
_calc_data_size(uint16_t size)
{
    uint16_t input_data_size = size / ITEMS_IN_BYTE;

    if (size % ITEMS_IN_BYTE)
        input_data_size++;
    return input_data_size;
}

void
sml_bit_array_init(struct sml_bit_array *array)
{
    array->size = 0;
    array->data = NULL;
}

bool
sml_bit_array_set(struct sml_bit_array *array, uint16_t pos, uint8_t value)
{
    uint16_t real_pos;
    uint8_t mask, shift;

    real_pos = pos / ITEMS_IN_BYTE;
    if (real_pos >= array->size)
        return false;

    shift = (pos % ITEMS_IN_BYTE) * BITS;
    mask = MASK << shift;
    value = value << shift;
    array->data[real_pos] = (array->data[real_pos] & ~mask) | value;

    return true;
}

uint8_t
sml_bit_array_get(struct sml_bit_array *array, uint16_t pos)
{
    uint16_t real_pos;
    uint8_t input_terms_membership, shift;

    if (pos >= array->size)
        return 0;

    real_pos = pos / ITEMS_IN_BYTE;
    shift = (pos % ITEMS_IN_BYTE) * BITS;
    input_terms_membership = array->data[real_pos];
    input_terms_membership = input_terms_membership & (MASK << shift);
    return input_terms_membership >> shift;
}

void
sml_bit_array_clear(struct sml_bit_array *array)
{
    array->size = 0;
    free(array->data);
    array->data = NULL;
}

int
sml_bit_array_size_set(struct sml_bit_array *array, uint16_t new_size,
    uint8_t initial_value)
{
    uint8_t *tmp_output;
    uint16_t new_real_size, old_size;

    if (new_size == 0) {
        sml_bit_array_clear(array);
        return 0;
    }

    new_real_size = _calc_data_size(new_size);
    tmp_output = realloc(array->data, new_real_size);
    if (tmp_output == NULL)
        return -errno;
    array->data = tmp_output;
    old_size = array->size;
    array->size = new_size;

    if (new_size > old_size) {
        uint16_t i;
        for (i = old_size; i < new_size; i++)
            sml_bit_array_set(array, i, initial_value);
    }

    return 0;
}

uint16_t
sml_bit_array_size_get(struct sml_bit_array *array)
{
    return array->size;
}

unsigned int
sml_bit_array_byte_size_get(struct sml_bit_array *array)
{
    return _calc_data_size(array->size);
}

int
sml_bit_array_remove(struct sml_bit_array *array, uint16_t pos)
{
    uint16_t i;

    if (pos >= array->size)
        return -EINVAL;
    for (i = pos; i < array->size - 1; i++)
        sml_bit_array_set(array, i, sml_bit_array_get(array, i + 1));

    return sml_bit_array_size_set(array, array->size - 1, 0);
}
