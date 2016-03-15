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
#include <string.h>
#include <stdbool.h>
#include <glib.h>
#include <libgen.h>
#include <fl/Headers.h>
#include <math.h>

#define FUZZY_ENGINE 0
#define ANN_ENGINE 1

typedef struct {
    fl::Engine *engine;
    struct sml_object *sml;
    int repeat;
    double increment;
    double error;
    double noise;
    bool use_incorrect_output;
    int false_positive;
    int positive_count;
} Context;

double _double_rand(double min, double max) {
    return min + (((double) rand() / RAND_MAX) *
           (max - min));
}

static void
_engine_initialize(Context *ctx)
{
    std::vector<fl::InputVariable*> inputs = ctx->engine->inputVariables();
    for (std::vector<fl::InputVariable*>::const_iterator it =
         inputs.begin();
         it != inputs.end(); ++it) {
        fl::InputVariable *input = *it;
        input->setInputValue(input->getMinimum());
    }
    ctx->engine->process();
}

static fl::Variable *
_search_variable(Context *ctx, const char *id)
{
    std::vector<fl::InputVariable*> inputs = ctx->engine->inputVariables();
    for (std::vector<fl::InputVariable*>::const_iterator it =
         inputs.begin();
         it != inputs.end(); ++it) {
        fl::InputVariable *input = *it;
        if (!strcmp(input->getName().c_str(), id)) {
            return input;
        }
    }

    std::vector<fl::OutputVariable*> outputs = ctx->engine->outputVariables();
    for (std::vector<fl::OutputVariable*>::const_iterator it =
         outputs.begin();
         it != outputs.end(); ++it) {
        fl::OutputVariable *output = *it;
        if (!strcmp(output->getName().c_str(), id)) {
            return output;
        }
    }
    return NULL;
}

static fl::scalar
_value_from_variable(Context *ctx, fl::Variable *v)
{
    fl::InputVariable *in = dynamic_cast<fl::InputVariable*>(v);
    if (in != NULL)
        return in->getInputValue();

    if (ctx->use_incorrect_output) {
        fl::OutputVariable *out = dynamic_cast<fl::OutputVariable*>(v);
        if (out != 0) {
            if ((out->getMaximum() - out->getOutputValue()) <
                (out->getOutputValue() - out->getMinimum()))
                return out->getMinimum();
            return out->getMaximum();
        }
    }

    fl::OutputVariable *out = dynamic_cast<fl::OutputVariable*>(v);
    if (out != 0)
        return out->getOutputValue();

    return 0;
}

void
set_value(Context *ctx, struct sml_variable *sml_variable)
{
    float value;
    fl::Variable *v;
    char var_name[SML_VARIABLE_NAME_MAX_LEN + 1];

    if (sml_variable_get_name(ctx->sml, sml_variable, var_name,
            sizeof(var_name))) {
        printf("Failed to get name for variable %p", sml_variable);
        return;
    }

    v = _search_variable(ctx, var_name);

    if (!v) {
        sml_variable_set_value(ctx->sml, sml_variable, NAN);
        return;
    }

    if (ctx->error > 0 && (rand() % ((int) (1 / ctx->error)) == 0))
        value = _double_rand(v->getMinimum(), v->getMaximum());
    else {
        value = _value_from_variable(ctx, v);
        if (ctx->noise > 0) {
            value += _double_rand(-ctx->noise, ctx->noise);
        }
        if (value < v->getMinimum())
            value = v->getMinimum();
        else if (value > v->getMaximum())
            value = v->getMaximum();
    }
    sml_variable_set_value(ctx->sml, sml_variable, value);
    printf("%s> %f\n", var_name, value);
    for (int i = 0; i < v->numberOfTerms(); i++) {
        fl::Term *t = v->getTerm(i);
        printf(" %s> %f\n", t->getName().c_str(), t->membership(value));
    }
}

void
set_list_values(Context *ctx, struct sml_variables_list *list)
{
    unsigned int i, len;
    len = sml_variables_list_get_length(ctx->sml, list);
    for (i = 0; i < len; i++) {
        struct sml_variable *sml_variable = sml_variables_list_index(ctx->sml,
            list, i);
        set_value(ctx, sml_variable);
    }
}

bool
read_state_cb(struct sml_object *sml, void *data)
{
    Context *ctx = (Context *)data;
    struct sml_variables_list *input_list, *output_list;

    if (!ctx->repeat)
        return false;

    input_list = sml_get_input_list(sml);
    output_list = sml_get_output_list(sml);

    printf("\nread_state_cb:\n");
    set_list_values(ctx, input_list);
    set_list_values(ctx, output_list);

    //calculate next values
    std::vector<fl::InputVariable*> inputs = ctx->engine->inputVariables();
    int pos = inputs.size() - 1;
    while (pos >= 0) {
        fl::InputVariable *input = inputs[pos];
        fl::scalar new_value = input->getInputValue() + ctx->increment;
        if (new_value > input->getMaximum()) {
            input->setInputValue(input->getMinimum());
            pos--;
        } else {
            input->setInputValue(new_value);
            break;
        }
    }

    //The execution has ended
    if (pos < 0) {
        _engine_initialize(ctx);
        ctx->repeat--;
    }
    ctx->engine->process();

    return true;
}

static void
_output_state_changed_cb_test_false_positive (struct sml_object *sml, struct sml_variables_list *changed, void *data)
{
    Context *ctx = (Context *) data;
    char var_name[SML_VARIABLE_NAME_MAX_LEN + 1];
    unsigned int i, len;

    ctx->false_positive++;

    printf("False Posive called:\n");
    len = sml_variables_list_get_length(sml, changed);
    for (i = 0; i < len; i++) {
        struct sml_variable *sml_variable = sml_variables_list_index(sml,
                                                              changed, i);
        if (!sml_variable_get_name(sml, sml_variable, var_name,
                    sizeof(var_name)))
            printf("%s> %f\n", var_name,
                sml_variable_get_value(sml, sml_variable));
    }
}

static void
_output_state_changed_cb_false_negative (struct sml_object *sml, struct sml_variables_list *changed, void *data)
{
    Context *ctx = (Context *) data;
    char var_name[SML_VARIABLE_NAME_MAX_LEN + 1];
    unsigned int i, len;

    ctx->positive_count++;

    printf("Change State Called:\n");
    len = sml_variables_list_get_length(sml, changed);
    for (i = 0; i < len; i++) {
        struct sml_variable *sml_variable = sml_variables_list_index(sml,
                                                              changed, i);
        if (!sml_variable_get_name(sml, sml_variable, var_name,
                    sizeof(var_name)))
            printf("%s> %f\n", var_name,
                sml_variable_get_value(sml, sml_variable));
    }
}

static unsigned int
_number_of_states (Context *ctx)
{
    unsigned int i, len, n = 1;

    std::vector<fl::InputVariable*> inputs = ctx->engine->inputVariables();
    len = inputs.size();
    for (i=0; i < len; i++) {
        fl::InputVariable *input = inputs[i];
        n *= (unsigned int) ((input->getMaximum() - input->getMinimum()) /
                             ctx->increment) + 1;
    }

    return n;
}

static struct sml_object *
_sml_new(int id)
{
    switch(id) {
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
    Context ctx;
    memset(&ctx, 0, sizeof(Context));

    if (argc < 4) {
        fprintf(stderr, "Correct usage %s engine type (0 fuzzy, 1 ann)"
                " in.fll out.fll <increment> <repeat> "
                "<random_noise_range> <random_error_freq> <seed>\n",
                argv[0]);
        fprintf(stderr, "Test1: %s engine type (0 fuzzy, 1 ann)"
                " examples/data/fuzzy_test_1.fll "
                "examples/data/fuzzy_test_1.fll 1\n",
                argv[0]);
        fprintf(stderr, "Test2: %s engine type (0 fuzzy, 1 ann)"
                " examples/data/fuzzy_test_2.fll "
                "examples/data/fuzzy_test_2.fll 0.1\n",
                argv[0]);
        return 1;
    }

    if (argc > 4) {
        ctx.increment = atof(argv[4]);
        if (ctx.increment <= 0) {
            fprintf(stderr, "Increment must be positive\n");
            return 2;
        }
    } else
        ctx.increment = 0.1;

    if (argc > 5) {
        ctx.repeat = atof(argv[5]);
        if (ctx.repeat <= 0) {
            fprintf(stderr, "Repeat must be positive\n");
            return 2;
        }
    } else
        ctx.repeat = 1;

    if (argc > 6) {
        ctx.noise = atof(argv[6]);
        if (ctx.noise < 0) {
            fprintf(stderr, "Noise range must be positive\n");
            return 2;
        }
    }

    if (argc > 7) {
        ctx.error = atof(argv[7]);
        if (ctx.error < 0 || ctx.error > 1) {
            fprintf(stderr, "Error frequency must be between 0.0 and 1.0\n");
            return 2;
        }
    }

    ctx.sml = _sml_new(atoi(argv[1]));
    if (!ctx.sml) {
        fprintf(stderr, "Failed to create sml");
        return 1;
    }

    fl::FllImporter importer;
    ctx.engine = importer.fromFile(argv[2]);
    _engine_initialize(&ctx);

    if (!sml_load_fll_file(ctx.sml, argv[3])) {
        fprintf(stderr, "Failed to open %s\n", argv[3]);
        return 3;
    }

    unsigned int seed;
    if (argc > 8) {
        seed = (unsigned int) atol(argv[8]);
    } else {
        seed = time(NULL);
    }
    srand(seed);

    printf("Starting test with seed = %d\n", seed);

    sml_set_read_state_callback(ctx.sml, read_state_cb, &ctx);
    sml_set_stabilization_hits(ctx.sml, 0);

    printf("Learning...\n");
    while (sml_process(ctx.sml) == 0);

    printf("Testing false positives...\n");

    sml_set_learn_disabled(ctx.sml, true);
    sml_set_output_state_changed_callback(ctx.sml, _output_state_changed_cb_test_false_positive,
                                       &ctx);
    ctx.repeat = 1;
    ctx.error = 0;
    ctx.noise = 0;

    _engine_initialize(&ctx);

    while (sml_process(ctx.sml) == 0);

    printf("Testing false negatives...\n");
    sml_set_output_state_changed_callback(ctx.sml, _output_state_changed_cb_false_negative,
                                  &ctx);
    ctx.repeat = 1;
    ctx.use_incorrect_output = true;
    _engine_initialize(&ctx);

    while (sml_process(ctx.sml) == 0);

    sml_print_debug(ctx.sml, false);

    unsigned int number_of_states = _number_of_states(&ctx);
    printf("==========================================================\n");
    printf("Tests performed in the same dataset used to learn.\n"
            "False positives and false negatives should be 0.\n\n");
    printf("False positives (change state called when not needed): %d of %d\n",
           ctx.false_positive, number_of_states);
    printf("False negative (change state not called when needed): %d of %d\n",
           number_of_states - ctx.positive_count, number_of_states);
    printf("==========================================================\n");

    sml_free(ctx.sml);
    delete ctx.engine;

    return 0;
}
