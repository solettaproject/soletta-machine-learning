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
