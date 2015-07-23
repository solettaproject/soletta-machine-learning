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
#include <sol-vector.h>
#include "stdio.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sml_matrix {
    uint16_t elem_size;
    struct sol_vector data;
};

typedef bool (*sml_cmp_cb) (void *a, void *b);
typedef void (*sml_matrix_convert_cb) (char *buf, uint16_t buf_len, void *val);

void sml_matrix_init(struct sml_matrix *m, uint16_t elem_size);
void sml_matrix_clear(struct sml_matrix *m);
void *sml_matrix_insert(struct sml_matrix *m, uint16_t i, uint16_t j);
void *sml_matrix_get(struct sml_matrix *m, uint16_t i, uint16_t j);
bool sml_matrix_equals(struct sml_matrix *m1, struct sml_matrix *m2, struct sol_vector *changed, sml_cmp_cb cmp_cb);
uint16_t sml_matrix_lines(struct sml_matrix *m);
uint16_t sml_matrix_cols(struct sml_matrix *m, uint16_t i);
void sml_matrix_remove_line(struct sml_matrix *m, uint16_t line_num);
void sml_matrix_remove_col(struct sml_matrix *m, uint16_t line_num, uint16_t col_num);
void sml_matrix_debug(struct sml_matrix *m, sml_matrix_convert_cb convert);

#define sml_matrix_cast_get(m, i, j, tmp, type)                                \
    ( tmp = sml_matrix_get(m, i, j),                                           \
    tmp ? *((type *)tmp) : 0 )

#define SML_MATRIX_FOREACH_IDX(matrix, itr_i, itr_j, i, j)                     \
    SOL_VECTOR_FOREACH_IDX (&matrix->data, itr_i, i)                            \
        SOL_VECTOR_FOREACH_IDX ((struct sol_vector *)itr_i, itr_j, j)

#define MATRIX_CONVERT(type, format) \
    static inline void sml_matrix_ ## type ## _convert(char *buf,                  \
    uint16_t buf_len, void *val)   \
    {                                                                          \
        type *tmp = (type *)val;                                              \
        snprintf(buf, buf_len, format, tmp ? *tmp : 0);                        \
    }                                                                          \
    static inline void sml_matrix_debug_ ## type(struct sml_matrix *m)                  \
    {                                                                          \
        sml_matrix_debug(m, sml_matrix_ ## type ## _convert);                      \
    }

MATRIX_CONVERT(float, "%f")
MATRIX_CONVERT(uint16_t, "%d")
MATRIX_CONVERT(uint8_t, "%d")

#undef MATRIX_CONVERT
#ifdef __cplusplus
}
#endif
