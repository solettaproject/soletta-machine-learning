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

#include "sml_measure.h"
#include "sml_log.h"
#include <stdlib.h>

struct sml_measure *
sml_measure_new(uint16_t input_elem_size, uint16_t output_elem_size)
{
    struct sml_measure *measure = malloc(sizeof(struct sml_measure));

    if (!measure)
        return NULL;
    sml_measure_init(measure, input_elem_size, output_elem_size);
    return measure;
}

void
sml_measure_free(struct sml_measure *measure)
{
    if (!measure)
        return;
    sml_measure_clear(measure);
    free(measure);
}


void
sml_measure_init(struct sml_measure *measure, uint16_t input_elem_size,
    uint16_t output_elem_size)
{
    sml_matrix_init(&measure->inputs, input_elem_size);
    sml_matrix_init(&measure->outputs, output_elem_size);
}

void
sml_measure_clear(struct sml_measure *measure)
{
    sml_matrix_clear(&measure->inputs);
    sml_matrix_clear(&measure->outputs);
}

void
sml_measure_debug(struct sml_measure *measure, sml_matrix_convert_cb convert)
{
    sml_debug("\tInputs:");
    sml_matrix_debug(&measure->inputs, convert);
    sml_debug("\tOutputs:");
    sml_matrix_debug(&measure->outputs, convert);
}

void
sml_measure_remove_input_variable(struct sml_measure *measure, uint16_t pos)
{
    sml_matrix_remove_line(&measure->inputs, pos);
}

void
sml_measure_remove_output_variable(struct sml_measure *measure, uint16_t pos)
{
    sml_matrix_remove_line(&measure->outputs, pos);
}
