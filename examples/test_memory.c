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

#include <sml_ann.h>
#include <sml_fuzzy.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>

#define STRLEN 20
#define FUZZY_ENGINE 0
#define ANN_ENGINE 1

static void
_set_list_values(struct sml_object *sml, struct sml_variables_list *list)
{
    unsigned int i, len;
    struct sml_variable *sml_variable;

    len = sml_variables_list_get_length(sml, list);
    for (i = 0; i < len; i++) {
        sml_variable = sml_variables_list_index(sml, list, i);
        sml_variable_set_value(sml, sml_variable, rand());
    }
}

static bool
_read_state_cb(struct sml_object *sml, void *data)
{
    int *executions = data;

    if (*executions == 0)
        return false;
    (*executions)--;

    struct sml_variables_list *input_list, *output_list;

    input_list = sml_get_input_list(sml);
    output_list = sml_get_output_list(sml);

    _set_list_values(sml, input_list);
    _set_list_values(sml, output_list);

    return true;
}

static void
_add_terms(struct sml_object *sml, struct sml_variable *var, char *strbuf,
    int id, unsigned int num_terms)
{
    unsigned int i, step;

    step = RAND_MAX / num_terms;
    for (i = 0; i < num_terms; i++) {
        snprintf(strbuf, STRLEN, "term%d.%d", id, i);
        sml_fuzzy_variable_add_term_triangle(sml, var, strbuf, step * i,
            step * (i + 0.5), step * (i + 1));
    }
}

static struct sml_object *
_sml_new(int id)
{
    switch (id) {
    case FUZZY_ENGINE:
        return sml_fuzzy_new();
    case ANN_ENGINE:
        return sml_ann_new();
    }
    return NULL;
}

int
main(int argc, char *argv[])
{
    unsigned int inputs, outputs, executions, num_terms;
    char strbuf[STRLEN];
    struct sml_variable *var;
    struct sml_object *sml;

    if (argc < 5) {
        fprintf(stderr, "Usage: %s <engine type (0 fuzzy, 1 ann)> <inputs> "
            "<outputs> <executions> <num_terms> <seed>\n", argv[0]);
        fprintf(stderr, "Fuzzy Test: %s 0 10 2 100 10\n", argv[0]);
        fprintf(stderr, "ANN Test: %s 1 10 2 100 10\n", argv[0]);
        return 1;
    }

    inputs = atoi(argv[2]);
    outputs = atoi(argv[3]);
    executions = atoi(argv[4]);
    num_terms = atoi(argv[5]);

    if (argc > 6)
        srand((unsigned int)atol(argv[6]));
    else
        srand(time(NULL));

    if (num_terms < 1) {
        fprintf(stderr, "num_terms must be a positive value\n");
        return 1;
    }

    sml = _sml_new(atoi(argv[1]));
    if (!sml) {
        fprintf(stderr, "Failed to create sml\n");
        return 1;
    }


    sml_set_read_state_callback(sml, _read_state_cb, &executions);
    sml_set_stabilization_hits(sml, 0);

    while (inputs) {
        snprintf(strbuf, STRLEN, "input%d", inputs);
        var = sml_new_input(sml, strbuf);
        sml_variable_set_range(sml, var, 0, RAND_MAX);
        _add_terms(sml, var, strbuf, inputs, num_terms);

        inputs--;
    }

    while (outputs) {
        snprintf(strbuf, STRLEN, "output%d", outputs);
        var = sml_new_output(sml, strbuf);
        sml_variable_set_range(sml, var, 0, RAND_MAX);
        _add_terms(sml, var, strbuf, outputs, num_terms);
        outputs--;
    }

    while (sml_process(sml) == 0) ;
    sml_free(sml);

    return 0;
}
