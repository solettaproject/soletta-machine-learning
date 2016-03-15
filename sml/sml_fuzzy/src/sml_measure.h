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
#include "sml_matrix.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sml_measure {
    struct sml_matrix inputs;
    struct sml_matrix outputs;
};

struct sml_measure *sml_measure_new(uint16_t input_elem_size, uint16_t output_elem_size);
void sml_measure_free(struct sml_measure *measure);
void sml_measure_remove_input_variable(struct sml_measure *measure, uint16_t pos);
void sml_measure_remove_output_variable(struct sml_measure *measure, uint16_t pos);
void sml_measure_debug(struct sml_measure *measure, sml_matrix_convert_cb convert);
void sml_measure_init(struct sml_measure *measure, uint16_t input_elem_size, uint16_t output_elem_size);
void sml_measure_clear(struct sml_measure *measure);

#ifdef __cplusplus
}
#endif
