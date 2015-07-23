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

/*
 * Example sensor file:
 * Run a SML simulation based on sensors and output status data saved in a file
 * Conf file is a standard fll file with inputs and outputs description.
 * Data file is a regular text file with data read from sensors and outputs.
 * Each line contains data from one SML iteration, separated by whitespaces.
 * Inputs data should be written in order, followed by outputs data and then,
 * optionally, by expected outputs data. If expected output data is provided,
 * this example will print staticts results about the SML execution
 */
#include <sml_ann.h>
#include <sml_fuzzy.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <glib.h>

#define THRESHOLD (0.1)
#define LINE_LEN (1024)
#define INITIAL_REQUIRED_OBS (10)
#define FUZZY_ENGINE 0
#define ANN_ENGINE 1

typedef struct {
    /*
     * For this test output_state_changed_cb calls that have incorrect values are
     * considered false_positives and calls with correct values are considered
     * true positives. When we are suppose to call output_state_changed_cb, but it isn't
     * called, it is a false_negative. True negatives are when we are not
     * suppose to call output_state_changed_cb and it is not called.
     * Note that if we are suppose to call output_state_changed_cb and it is called with
     * an incorrect value, it is considered a false positive
     * A value is considered correct if the difference between the expected
     * value and the real value is below a threshold.
     */
    unsigned int false_positives;
    unsigned int false_negatives;
    unsigned int true_negatives;
    unsigned int true_positives;
    float mse; //Mean Square Error
} Stat;

typedef struct {
    FILE *f;
    unsigned int read_count;
    unsigned int output_state_changed_count;
    float threshold;
    float *outputs;
    float *output_state_changed_outputs;
    float *expected_outputs;
    Stat *stats;
} Context;

static void
_initialize_context(Context *ctx, struct sml_object *sml)
{
    struct sml_variables_list *output_list = sml_get_output_list(sml);
    uint16_t len = sml_variables_list_get_length(sml, output_list);

    memset(ctx, 0, sizeof(Context));
    ctx->outputs = malloc(sizeof(float) * len);
    ctx->expected_outputs = malloc(sizeof(float) * len);
    ctx->output_state_changed_outputs = malloc(sizeof(float) * len);
    ctx->stats = calloc(1, sizeof(Stat) * len);
}

static void
_clear_context(Context *ctx)
{
    fclose(ctx->f);
    free(ctx->outputs);
    free(ctx->expected_outputs);
    free(ctx->output_state_changed_outputs);
    free(ctx->stats);
}

static bool
_read_line(FILE *f, char *line, size_t len)
{
    char *ret;

    if (feof(f))
        return false;

    while ((ret = fgets(line, len, f)))
        if (line[0] == '#' || line[0] == '\n')
            continue;
        else
            break;

    return ret != NULL;
}

/*
 * Read sensors values stored in data file
 */
static bool
_read_state_cb(struct sml_object *sml, void *data)
{
    uint16_t i, ind, input_len, output_len, tokens_len;
    float value;
    char line[LINE_LEN];
    char **tokens;
    Context *ctx = data;
    struct sml_variables_list *input_list, *output_list;
    struct sml_variable *sml_variable;

    input_list = sml_get_input_list(sml);
    output_list = sml_get_output_list(sml);
    input_len = sml_variables_list_get_length(sml, output_list);
    output_len = sml_variables_list_get_length(sml, output_list);

    if (!_read_line(ctx->f, line, LINE_LEN))
        return false;

    tokens = g_strsplit(line, " ", 0);
    if (!tokens)
        return false;

    tokens_len = g_strv_length(tokens);
    if (tokens_len < input_len + output_len) {
        fprintf(stderr, "Line in file has not enought data\n");
        g_strfreev(tokens);
        return false;
    }

    printf("Reading sensors: Inputs {");
    for (i = 0; i < input_len; i++) {
        sml_variable = sml_variables_list_index(sml, input_list, i);
        value = atof(tokens[i]);
        sml_variable_set_value(sml, sml_variable, value);
        if (i != 0)
            printf(", ");
        printf("%s: %f", sml_variable_get_name(sml, sml_variable), value);
    }
    printf("}, Outputs {");

    for (i = 0; i < output_len; i++) {
        sml_variable = sml_variables_list_index(sml, output_list, i);
        value = atof(tokens[input_len + i]);
        ctx->outputs[i] = value;
        sml_variable_set_value(sml, sml_variable, value);
        if (i != 0)
            printf(", ");
        printf("%s: %f", sml_variable_get_name(sml, sml_variable), value);
    }
    printf("}, Expected {");

    for (i = 0; i < output_len; i++) {
        ind = input_len + output_len + i;
        sml_variable = sml_variables_list_index(sml, output_list, i);
        if (i != 0)
            printf(", ");
        printf("%s: ", sml_variable_get_name(sml, sml_variable));
        if (ind < tokens_len && tokens[ind][0] != 0 && tokens[ind][0] != '?') {
            ctx->expected_outputs[i] = atof(tokens[ind]);
            printf("%f", ctx->expected_outputs[i]);
        } else {
            printf("?");
            ctx->expected_outputs[i] = NAN;
        }
    }
    printf("}\n");
    g_strfreev(tokens);

    for (i = 0; i < output_len; i++)
        ctx->output_state_changed_outputs[i] = NAN;

    ctx->read_count++;
    return true;
}

static void
_output_state_changed_cb(struct sml_object *sml,
    struct sml_variables_list *changed, void *data)
{
    uint16_t i, j, len, changed_len, found = 0;
    struct sml_variable *var, *changed_var;
    struct sml_variables_list *list;
    Context *ctx = data;

    printf("SML Change State {");
    list = sml_get_output_list(sml);
    changed_len = sml_variables_list_get_length(sml, changed);
    len = sml_variables_list_get_length(sml, list);
    for (i = 0; i < len; i++) {
        var = sml_variables_list_index(sml, list, i);
        for (j = 0; j < changed_len; j++) {
            changed_var = sml_variables_list_index(sml, list, j);
            if (var == changed_var) {
                ctx->output_state_changed_outputs[i] = sml_variable_get_value(
                    sml, var);
                if (found++ > 0)
                    printf(", ");
                printf("%s: %f", sml_variable_get_name(sml, var),
                    ctx->output_state_changed_outputs[i]);
            }
        }
    }
    printf("}\n");
}

static bool
compare(float new, float old, float threshold)
{
    if (isnan(new))
        return true;
    return fabs(new - old) < threshold;
}

static void
_process_state(struct sml_object *sml, Context *ctx)
{
    unsigned int i, len;

    len = sml_variables_list_get_length(sml, sml_get_output_list(sml));
    for (i = 0; i < len; i++) {
        float new_output = ctx->output_state_changed_outputs[i];
        if (!compare(new_output, ctx->expected_outputs[i], ctx->threshold)) {
            if (compare(new_output, ctx->outputs[i], ctx->threshold))
                ctx->stats[i].false_negatives++;
            else
                ctx->stats[i].false_positives++;
        } else {
            if (compare(new_output, ctx->outputs[i], ctx->threshold))
                ctx->stats[i].true_negatives++;
            else
                ctx->stats[i].true_positives++;
        }
        float error = ctx->expected_outputs[i] - new_output;
        if (!isnan(error))
            ctx->stats[i].mse += error * error;
    }
}

static void
_print_results(Context *ctx, struct sml_object *sml)
{
    unsigned int i, len;

    printf("Total Tests: %d\n", ctx->read_count);
    len = sml_variables_list_get_length(sml, sml_get_output_list(sml));
    for (i = 0; i < len; i++) {
        printf("Output %d:\n", i);
        Stat *stat = &ctx->stats[i];
        unsigned int total = stat->false_positives + stat->false_negatives +
            stat->true_positives + stat->true_negatives;
        unsigned int threshold_error = stat->false_positives +
            stat->false_negatives;

        float precision = stat->true_positives /
            (float)(stat->true_positives + stat->false_positives);
        float recall = stat->true_positives /
            (float)(stat->false_negatives + stat->true_positives);
        printf("\tErrors: %d (%1.2f%%)\n", threshold_error,
            (threshold_error / (float)total) * 100);
        printf("\tAccuracy: %f\n",
            (stat->true_positives + stat->true_negatives) / (float)total);
        printf("\tPrecision: %f\n", precision);
        printf("\tF-Score: %f\n", 2 * (precision * recall) /
            (precision + recall));
        printf("\tMean Square Root: %f\n", stat->mse / total);
    }
    printf("\n");
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
    struct sml_object *sml;
    Context ctx;

    if (argc < 4) {
        fprintf(stderr, "Correct usage %s <engine type (0 fuzzy, 1 ann)> "
            "description.fll data.dat <threshold>\n", argv[0]);
        fprintf(stderr, "Test Fuzzy: %s 0 examples/data/example_in_out.fll "
            "examples/data/example_in_out.txt\n", argv[0]);
        fprintf(stderr, "Test ANN: %s 1 examples/data/example_in_out.fll "
            "examples/data/example_in_out.txt\n", argv[0]);
        return 1;
    }

    sml = _sml_new(atoi(argv[1]));
    if (!sml) {
        fprintf(stderr, "Failed to create sml\n");
        return 1;
    }

    if (!sml_load_fll_file(sml, argv[2])) {
        fprintf(stderr, "Failed to open %s\n", argv[2]);
        return 2;
    }

    _initialize_context(&ctx, sml);
    printf("file %s\n", argv[3]);
    ctx.f = fopen(argv[3], "r");
    if (!ctx.f) {
        fprintf(stderr, "Failed to open %s\n", argv[3]);
        return 4;
    }

    if (argc > 4) {
        ctx.threshold = atof(argv[4]);
        if (ctx.threshold <= 0) {
            fprintf(stderr, "threshold must be positive\n");
            return 5;
        }
    } else
        ctx.threshold = THRESHOLD;

    sml_set_read_state_callback(sml, _read_state_cb, &ctx);
    sml_set_output_state_changed_callback(sml, _output_state_changed_cb, &ctx);
    sml_ann_set_initial_required_observations(sml, INITIAL_REQUIRED_OBS);
    while (!sml_process(sml))
        _process_state(sml, &ctx);

    _print_results(&ctx, sml);
    _clear_context(&ctx);

    sml_print_debug(sml, false);
    sml_free(sml);

    return 0;
}
