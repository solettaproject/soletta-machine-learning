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
