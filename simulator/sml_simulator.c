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

#include <sml.h>
#include <sml_ann.h>
#include <sml_fuzzy.h>
#include <sml_naive.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

/*
 * All the inputs and outputs are handled directly with variables API.
 * It works with both backends: neural newtworks and fuzzy.
 *
 * Config is read from a file with the following description:
 *
 * TIME_BLOCKS X
 * READ_FREQ Y
 * DAYS Z
 * INPUT Input1 Min Max
 * TERM Term1 Min Max
 * TERM Term2 Min Max
 * TERM Term3 Min Max
 * INPUT Input2 Min Max
 * TERM Term1 Min Max
 * TERM Term2 Min Max
 * OUTPUT Output1 Min Max
 * TERM Term1 Min Max
 * TERM Term2 Min Max
 * ...
 *
 * Data is read from a file with the following description:
 * DAY HH:MM [Input|Output|EXP_Output] STATE
 */

#define TIME_BLOCKS (48)
#define READ_FREQ (2)
#define DAYS (14)

#define TIME_STR ("Time")
#define WEEKDAY_STR ("Weekday")

#define LINE_SIZE (256)
#define STR_PARSE_LEN 32
#define NAME_SIZE 32
#define EXP_NAME_SIZE (NAME_SIZE + 4)

#define STR_FORMAT(WIDTH) "%" #WIDTH "s"
#define STR_FMT(W) STR_FORMAT(W)

#define AUTOMATIC_TERMS (15)
#define DISABLED "[DISABLED]"
#define ENABLED "[ENABLED]"
#define BEGIN_EXPECTATIONS ("BEGIN_EXPECTATIONS")
#define END_EXPECTATIONS ("END_EXPECTATIONS")
#define FLOAT_THRESHOLD 0.01
#define DISCRETE_THRESHOLD 0.45
#define LAST_VALUE_DEVIATION (0.05)
#define DEFAULT_EXPECTATION_BLOCK_NAME "BLOCK_"

#define FUZZY_ENGINE 0
#define ANN_ENGINE 1
#define NAIVE_ENGINE 2
#define FUZZY_ENGINE_NO_SIMPLIFICATION 3

typedef struct {
    float min;
    float max;
    char name[NAME_SIZE];
} Term;

typedef struct {
    Term *term;
    float value;
    int time;
} Event;

typedef struct {
    bool enabled;
    int time;
} Status_Event;

typedef struct {
    float guess_value;
    float cur_value;
    float min;
    float max;
    struct sml_variable *sml_var;
    GList *terms;
    GList *events;
    GList *status_events;
    Event *last_event;
    int right_guesses;
    int changes_counter;
    int expectations_right_guesses;
    int expectations_counter;
    char name[NAME_SIZE];
} Variable;

typedef struct {
    Variable *output;
    GList *events;
    Event *last_event;
    char name[EXP_NAME_SIZE];
} Expectation;

typedef struct {
    int begin;
    int end;
    char name[NAME_SIZE];
    bool error;
} Expectation_Block;

typedef struct {
    struct sml_object *sml;
    struct sml_variable *weekday;
    struct sml_variable *time;
    GList *inputs;
    GList *outputs;
    GList *expectations;
    GList *expectation_blocks;
    GList *cur_expectation_block;
    GRand *rand;
    int time_blocks;
    int days;
    int reads;
    int read_counter;
    int read_freq;
    double duration;
    double max_iteration_duration;
    bool debug;
    bool enable_time_input;
    bool enable_weekday_input;
    int engine_type;
} Context;

enum {
    LINE_ERROR,
    LINE_EMPTY,
    LINE_OK
};

static const char *WEEKDAYS[] =
{
    "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
};

static void
free_element(gpointer data)
{
    free(data);
}

static void
free_variable(gpointer data)
{
    Variable *variable = data;

    g_list_free_full(variable->events, free_element);
    g_list_free_full(variable->status_events, free_element);
    g_list_free_full(variable->terms, free_element);
    free(variable);
}

static void
free_expectation(gpointer data)
{
    Expectation *expec = data;

    g_list_free_full(expec->events, free_element);
    free(expec);
}

static float
get_weekday(int reads, int read_freq)
{
    float weekday;
    int day;

    day = reads / (24 * 60 / read_freq);
    weekday = day % 7 + 0.5;

    return weekday;
}

static float
get_time(Context *ctx)
{
    int total_blocks = ctx->read_counter /
        (60 / ctx->read_freq * 24 / ctx->time_blocks);

    return total_blocks % ctx->time_blocks;
}

static Event *
get_event(GList *events, int reads)
{
    Event *last_event = NULL;
    GList *itr;

    for (itr = events; itr; itr = itr->next) {
        Event *event = itr->data;

        // event->time >= last_event guarantees that guesses events
        // will be considered even if it had an event in the same time
        if ((event->time <= reads) &&
            ((!last_event) || (event->time >= last_event->time)))
            last_event = event;
    }

    if (!last_event)
        fprintf(stderr, "Failed to find event for read %i\n", reads);

    return last_event;
}

static Status_Event *
get_status_event(GList *status_events, int reads)
{
    GList *itr;

    for (itr = status_events; itr; itr = itr->next) {
        Status_Event *status_event = itr->data;
        if (status_event->time == reads)
            return status_event;
    }

    return NULL;
}

static bool
term_is_discrete(Term *term)
{
    if (!term)
        return false;
    if (term->max - term->min < FLOAT_THRESHOLD)
        return true;
    return false;
}

static bool
is_nan_event(Event *event)
{
    return !event->term && isnan(event->value);
}

static float
event_get_value(Event *event, GRand *rand)
{
    gint32 rand_val;
    float result;
    Term *term = event->term;

    if (!term)
        return event->value;

    if (term_is_discrete(term))
        return term->min;

    rand_val = g_rand_int_range(rand, term->min, term->max * 1000);
    rand_val %= (gint32)((term->max - term->min) * 1000);
    result = (float)rand_val * 0.0001 + term->min;

    return result;
}

static void
print_term(Term *term)
{
    printf("Term: %p, Name: %s, Min: %f, Max: %f\n",
        term, term->name, term->min, term->max);
}

static void
print_event(Event *event)
{
    if (event->term)
        printf("Event: %p, Term: %s, Time: %d\n",
            event, event->term->name, event->time);
    else
        printf("Event: %p, Value: %f, Time: %d\n",
            event, event->value, event->time);
}

static void
print_status_event(Status_Event *status_event)
{
    printf("Event: %p, Enabled: %s, Time: %d\n",
        status_event, status_event->enabled ? "true" : "false",
        status_event->time);
}

static void
print_variable(Variable *var)
{
    GList *itr;

    printf("= Variable: %s =\n", var->name);

    printf("struct sml_object Variable: %p\n", var->sml_var);
    printf("Range %f - %f\n", var->min, var->max);
    printf("Current value: %f\n", var->cur_value);
    printf("Guess value: %f\n", var->guess_value);
    printf("Last event: %p\n", var->last_event);

    printf("Terms:\n");
    for (itr = var->terms; itr; itr = itr->next) {
        print_term(itr->data);
    }

    printf("Events:\n");
    for (itr = var->events; itr; itr = itr->next)
        print_event(itr->data);

    printf("Status Events:\n");
    for (itr = var->status_events; itr; itr = itr->next)
        print_status_event(itr->data);

    printf("====================\n");
}

static void
print_expectation(Expectation *expec)
{
    GList *itr;

    printf("= Expectation: %s for %p =\n", expec->name, expec->output);
    printf("Last event: %p\n", expec->last_event);
    printf("Events:\n");
    for (itr = expec->events; itr; itr = itr->next)
        print_event(itr->data);
    printf("====================\n");
}

static void
print_scenario(Context *ctx)
{
    GList *itr;

    printf("=== Scenario ===\n");
    for (itr = ctx->inputs; itr; itr = itr->next)
        print_variable(itr->data);
    for (itr = ctx->outputs; itr; itr = itr->next)
        print_variable(itr->data);
    for (itr = ctx->expectations; itr; itr = itr->next)
        print_expectation(itr->data);
    printf("====================\n");
}

static float
event_get_value_with_deviation(GRand *rand, float last_value, Term *term,
    Variable *var)
{
    float min, max;

    if (term_is_discrete(term) || isnan(last_value))
        return last_value;

    min = fabs(last_value - LAST_VALUE_DEVIATION);
    max = last_value + LAST_VALUE_DEVIATION;
    if (term) {
        if (max > term->max)
            max = term->max;
        if (min < term->min)
            min = term->min;
    } else {
        if (max > var->max)
            max = var->max;
        if (min < var->min)
            min = var->min;
    }

    float val;
    val = (float)g_rand_double_range(rand, min, max);
    return val;
}

static void
variable_set_value(struct sml_object *sml, Variable *var, int reads, bool debug,
    GRand *rand)
{
    Event *event;
    Status_Event *status_event;
    float value;

    event = get_event(var->events, reads);
    if (!event) {
        fprintf(stderr, "Failed to find event %s for reads: %d\n", var->name,
            reads);
        return;
    }
    if (!is_nan_event(event)) {
        if (event == var->last_event) {
            value = event_get_value_with_deviation(rand, var->cur_value,
                event->term, var);
        } else {
            value = event_get_value(event, rand);
            var->last_event = event;
        }
        if (!isnan(value))
            var->guess_value = var->cur_value = value;
    } else
        var->cur_value = var->guess_value;

    status_event = get_status_event(var->status_events, reads);
    if (debug) {
        printf("\tVar: %s %f", var->name, var->cur_value);
        if (event->term)
            printf(" - %s", event->term->name);
        if (status_event)
            printf(status_event->enabled ? " enabled" : " disabled");
        printf("\n");
    }

    sml_variable_set_value(sml, var->sml_var, var->cur_value);
    if (status_event)
        sml_variable_set_enabled(sml, var->sml_var, status_event->enabled);
}

static bool
read_state_cb(struct sml_object *sml, void *data)
{
    float time, weekday;
    Context *ctx = data;
    GList *itr;

    time = get_time(ctx);
    weekday = get_weekday(ctx->read_counter, ctx->read_freq);

    if (ctx->time)
        sml_variable_set_value(sml, ctx->time, time);
    if (ctx->weekday)
        sml_variable_set_value(sml, ctx->weekday, weekday);

    if (ctx->debug)
        printf("%i::READ(%i%%) - Weekday:%s, TB: %d\n", ctx->read_counter,
            ctx->read_counter  * 100 / ctx->reads, WEEKDAYS[(int)weekday],
            (int)time);

    for (itr = ctx->inputs; itr; itr = itr->next) {
        Variable *var = itr->data;
        variable_set_value(sml, var, ctx->read_counter, ctx->debug, ctx->rand);
    }

    for (itr = ctx->outputs; itr; itr = itr->next) {
        Variable *var = itr->data;
        variable_set_value(sml, var, ctx->read_counter, ctx->debug, ctx->rand);
    }

    ctx->read_counter++;

    return true;
}

static Event *
new_event(int time, Term *term, float value)
{
    Event *event = calloc(1, sizeof(Event));

    event->time = time;
    event->term = term;
    event->value = value;
    return event;
}

static Expectation *
get_expectation(GList *expectations, Variable *output)
{
    Expectation *expec;
    GList *itr;

    for (itr = expectations; itr; itr = itr->next) {
        expec = itr->data;
        if (expec->output == output)
            return expec;
    }

    fprintf(stderr, "Failed to find expectation %s\n", output->name);
    return NULL;
}

static GList *
get_terms(GList *terms, float value)
{
    GList *itr, *result = NULL;
    Term *term;

    for (itr = terms; itr; itr = itr->next) {
        term = itr->data;
        if (term_is_discrete(term)) {
            if (((int)round(value)) == ((int)round(term->min)))
                result = g_list_append(result, term);
        } else {
            if ((value >= term->min) && (value <= term->max))
                result = g_list_append(result, term);
        }
    }

    return result;
}

static void
_debug_guess(int read_counter, const char *name, float guess_value,
    GList *terms, Event *event)
{
    GList *itr;

    printf("%i::GUESS %s %f - ", read_counter, name, guess_value);

    if (!terms)
        printf("(null)");
    else if (!terms->next)
        printf("%s", ((Term *)terms->data)->name);
    else {
        printf("(");
        for (itr = terms; itr; itr = itr->next) {
            printf("%s", ((Term *)itr->data)->name);
            if (itr->next)
                printf(", ");
        }
        printf(")");
    }
    printf(", Expected: ");

    if (!event)
        printf("(null). ");
    else if (event->term)
        printf("%s. ", event->term->name);
    else
        printf("%f. ", event->value);
}

static bool
event_contains_guess(Event *event, GList *terms)
{
    GList *itr;
    Term *term;

    if (event->term) {
        for (itr = terms; itr; itr = itr->next)
            if (event->term == itr->data)
                return true;
        return false;
    }

    for (itr = terms; itr; itr = itr->next) {
        term = itr->data;
        if ((event->value >= term->min) && (event->value <= term->max))
            return true;
    }
    return false;
}

static void
output_state_changed_cb(struct sml_object *sml,
    struct sml_variables_list *changed, void *data)
{
    GList *itr;
    Context *ctx = data;
    float val;


    if (ctx->debug)
        printf("%i::output_state_changed_cb called.\n", ctx->read_counter - 1);

    for (itr = ctx->outputs; itr; itr = itr->next) {
        Variable *var = itr->data;
        val = sml_variable_get_value(sml, var->sml_var);
        if (!isnan(val))
            var->guess_value = val;
    }
}

static int
set_time(Context *ctx, int day, int hour, int min)
{
    int total_min = (day * 24 + hour) * 60 + min;

    return total_min / ctx->read_freq;
}

static int
parse_value(Context *ctx, const char *line, int *time, char *sensor,
    char *state)
{
    int ret, day, hour, min;

    // empty or comment line
    if ((line[0] == '\n') || (line[0] == '#'))
        return LINE_EMPTY;

    ret = sscanf(line, "%d %d:%d " STR_FMT(STR_PARSE_LEN) " "
        STR_FMT(STR_PARSE_LEN) " \n", &day, &hour, &min, sensor, state);
    if (ret < 4)
        return LINE_ERROR;
    if (ret < 5)
        state[0] = 0; //state is optional

    *time = set_time(ctx, day, hour, min);

    if (ctx->debug)
        printf("READ: %d %02d:%02d %s %s\n", day, hour, min, sensor, state);

    return LINE_OK;
}

static Variable *
parse_sensor(Context *ctx, const char *sensor)
{
    GList *itr;

    if (!sensor) {
        fprintf(stderr, "Error: null sensor\n");
        return NULL;
    }

    for (itr = ctx->inputs; itr; itr = itr->next) {
        Variable *var = itr->data;
        if (!strcmp(var->name, sensor))
            return var;
    }

    for (itr = ctx->outputs; itr; itr = itr->next) {
        Variable *var = itr->data;
        if (!strcmp(var->name, sensor))
            return var;
    }

    return NULL;
}

static Expectation *
parse_expectation(Context *ctx, const char *sensor)
{
    GList *itr;

    if (!sensor) {
        fprintf(stderr, "Error: null sensor\n");
        return NULL;
    }

    for (itr = ctx->expectations; itr; itr = itr->next) {
        Expectation *expec = itr->data;
        if (!strcmp(expec->name, sensor))
            return expec;
    }

    fprintf(stderr, "Error: unknown expectation %s\n", sensor);
    return NULL;
}

static Term *
parse_state(Variable *var, const char *state)
{
    GList *itr;

    if (!state) {
        fprintf(stderr, "Error: null state\n");
        return NULL;
    }

    for (itr = var->terms; itr; itr = itr->next) {
        Term *term = itr->data;
        if (!strcmp(term->name, state))
            return term;
    }

    return NULL;
}

static Status_Event *
parse_status_event(int time, const char *state)
{
    bool enabled;

    if (!strcmp(state, DISABLED))
        enabled = false;
    else if (!strcmp(state, ENABLED))
        enabled = true;
    else
        return NULL;

    Status_Event *status = calloc(1, sizeof(Status_Event));
    status->enabled = enabled;
    status->time = time;
    return status;
}

static Expectation_Block *
add_expectation_block(GList **list, char *name, int begin, int end)
{
    Expectation_Block *eblock = malloc(sizeof(Expectation_Block));

    if (!eblock)
        return NULL;

    eblock->begin = begin;
    eblock->end = end;
    eblock->error = false;
    if (name && strlen(name)) {
        strncpy(eblock->name, name, NAME_SIZE);
        eblock->name[NAME_SIZE - 1] = '\0';
    } else
        snprintf(eblock->name, NAME_SIZE, "%s%d",
            DEFAULT_EXPECTATION_BLOCK_NAME, g_list_length(*list));

    *list = g_list_append(*list, eblock);
    return eblock;
}

static bool
read_values(const char *file_path, Context *ctx)
{
    FILE *f;
    int line_num;
    char line[LINE_SIZE];
    Expectation_Block *eblock = NULL;

    f = fopen(file_path, "r");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", file_path);
        return false;
    }

    if (ctx->debug)
        printf("=== Parsing ===\n");

    line_num = 0;
    // mark all events on values and expectations vectors
    while (fgets(line, LINE_SIZE, f)) {
        Variable *var;
        Event *event;
        Status_Event *status_event;
        Term *term;
        Expectation *expec = NULL;
        int time, ret;
        char sensor[STR_PARSE_LEN], state[STR_PARSE_LEN];
        float value = 0;

        ret = parse_value(ctx, line, &time, sensor, state);
        if (ret == LINE_ERROR) {
            fprintf(stderr, "Failed to read line %d: %s\n", line_num, line);
            goto error;
        }

        line_num++;

        if (ret == LINE_EMPTY)
            continue;

        if (time >= ctx->reads) {
            fprintf(stderr, "Time %d exceeds reads %d on line %d\n",
                time, ctx->reads, line_num - 1);
            goto error;
        }

        if (!strcmp(sensor, BEGIN_EXPECTATIONS)) {
            if (eblock) {
                fprintf(stderr, "BEGIN_EXPECTATIONS block on line %d is inside"
                    " another BEGIN_EXPECTATIONS block\n", line_num - 1);
                goto error;
            }
            eblock = add_expectation_block(&ctx->expectation_blocks, state,
                time, 0);
            continue;
        } else if (!strcmp(sensor, END_EXPECTATIONS)) {
            if (!eblock) {
                fprintf(stderr, "Missing BEGIN_EXPECTATIONS block before "
                    "END_EXPECTATIONS block on line %d\n", line_num - 1);
                goto error;
            }
            eblock->end = time;
            eblock = NULL;
            continue;
        }

        var = parse_sensor(ctx, sensor);
        if (!var) {
            expec = parse_expectation(ctx, sensor);
            if (expec)
                var = expec->output;
            else
                goto error;
        }

        status_event = parse_status_event(time, state);
        if (status_event) {
            if (expec) {
                fprintf(stderr, "Error: EXP_%s is not suppose to have status "
                    "changes\n", var->name);
                free(status_event);
                goto error;
            }
            var->status_events = g_list_append(var->status_events,
                status_event);
        } else {
            term = parse_state(var, state);
            if (!term)
                value = atof(state);

            event = new_event(time, term, value);

            if (expec)
                expec->events = g_list_append(expec->events, event);
            else
                var->events = g_list_append(var->events, event);
        }

    }

    ctx->cur_expectation_block = ctx->expectation_blocks;
    if (ctx->debug)
        printf("=== Parsing concluded ===\n");

    fclose(f);
    return true;

error:
    fclose(f);
    return false;
}

static void
print_results(Context *ctx)
{
    GList *itr;
    float hits;
    int count = 0;

    printf("===============================\n");
    printf("Results\n");
    printf("Sensor reads: %d\n", ctx->read_counter);
    printf("Right guesses:\n");

    for (itr = ctx->outputs; itr; itr = itr->next) {
        Variable *var = itr->data;

        if (var->expectations_counter != 0)
            hits = (var->expectations_right_guesses * 100) /
                (float)var->expectations_counter;
        else
            hits = 0.0;

        printf("\tVariable%d %s : %d of %d - %2.2f%% ( %f )\n", count++,
            var->name, var->expectations_right_guesses,
            var->expectations_counter, hits, (hits / 100));
    }
    printf("Max Iteration Duration (in ms): %.2f\n",
        ctx->max_iteration_duration * 1000);
    printf("Average Duration (in ms): %.2f\n",
        1000 * ctx->duration / (float)ctx->reads);
    printf("Expectation blocks with errors:\n");
    for (itr = ctx->expectation_blocks; itr; itr = itr->next) {
        Expectation_Block *b = itr->data;
        if (b->error)
            printf("\t%s\n", b->name);
    }

    printf("===============================\n");
}

static Variable *
add_input(Context *ctx, const char *name, float min, float max)
{
    Variable *var = calloc(1, sizeof(Variable));

    strncpy(var->name, name, NAME_SIZE);
    var->name[NAME_SIZE - 1] = '\0';
    var->sml_var = sml_new_input(ctx->sml, name);
    var->min = min;
    var->max = max;
    sml_variable_set_range(ctx->sml, var->sml_var, min, max);
    ctx->inputs = g_list_append(ctx->inputs, var);
    return var;
}

static Variable *
add_output(Context *ctx, const char *name, float min, float max)
{
    Variable *var = calloc(1, sizeof(Variable));

    strncpy(var->name, name, NAME_SIZE);
    var->name[NAME_SIZE - 1] = '\0';
    var->sml_var = sml_new_output(ctx->sml, name);
    var->min = min;
    var->max = max;
    sml_variable_set_range(ctx->sml, var->sml_var, min, max);
    ctx->outputs = g_list_append(ctx->outputs, var);
    return var;
}

static void
add_expectation(Context *ctx, Variable *output, const char *name)
{
    Expectation *expec = calloc(1, sizeof(Expectation));

    snprintf(expec->name, EXP_NAME_SIZE, "EXP_%s", name);
    expec->output = output;
    ctx->expectations = g_list_append(ctx->expectations, expec);
}

static void
add_term(Context *ctx, Variable *var, const char *name, float min, float max)
{
    Term *term = calloc(1, sizeof(Term));

    strncpy(term->name, name, NAME_SIZE);
    term->name[NAME_SIZE - 1] = '\0';
    term->min = min;
    term->max = max;
    var->terms = g_list_append(var->terms, term);

    if (ctx->engine_type != FUZZY_ENGINE &&
        ctx->engine_type != FUZZY_ENGINE_NO_SIMPLIFICATION)
        return;

    if (term_is_discrete(term)) {
        if (fabs(var->min - min) < FLOAT_THRESHOLD)
            sml_fuzzy_variable_add_term_ramp(ctx->sml, var->sml_var, name,
                min + DISCRETE_THRESHOLD, min);
        else if (fabs(var->max - min) < FLOAT_THRESHOLD)
            sml_fuzzy_variable_add_term_ramp(ctx->sml, var->sml_var, name,
                min - DISCRETE_THRESHOLD, min);
        else
            sml_fuzzy_variable_add_term_triangle(ctx->sml, var->sml_var, name,
                min - DISCRETE_THRESHOLD, min,
                min + DISCRETE_THRESHOLD);
    } else {
        if (fabs(var->min - min) < FLOAT_THRESHOLD)
            sml_fuzzy_variable_add_term_ramp(ctx->sml, var->sml_var, name, max,
                min);
        else if (fabs(var->max - max) < FLOAT_THRESHOLD)
            sml_fuzzy_variable_add_term_ramp(ctx->sml, var->sml_var, name, min,
                max);
        else
            sml_fuzzy_variable_add_term_triangle(ctx->sml, var->sml_var, name,
                min, min + (max - min) / 2, max);
    }
}

static void
variable_add_terms(Context *ctx, Variable *var)
{
    float step;
    int i;
    char name[NAME_SIZE];

    step = (var->max - var->min) / AUTOMATIC_TERMS;
    for (i = 0; i < AUTOMATIC_TERMS; i++) {
        snprintf(name, NAME_SIZE, "t%d", i);
        add_term(ctx, var, name, var->min + i * step,
            var->min + (i + 1.5) * step);
    }
}

static bool
read_config(const char *file_path, Context *ctx)
{
    Variable *last_var = NULL;
    GList *itr;
    FILE *f;
    char line[LINE_SIZE];
    char name[NAME_SIZE];
    int tmp;

    // default values
    ctx->read_freq = READ_FREQ;
    ctx->days = DAYS;
    ctx->time_blocks = TIME_BLOCKS;
    ctx->read_counter = 0;
    ctx->enable_time_input = 1;
    ctx->enable_weekday_input = 1;
    ctx->inputs = NULL;
    ctx->outputs = NULL;
    ctx->expectations = NULL;
    ctx->expectation_blocks = NULL;
    ctx->time = NULL;
    ctx->weekday = NULL;

    f = fopen(file_path, "r");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", file_path);
        return false;
    }

    while (fgets(line, LINE_SIZE, f)) {
        float min = 0, max = 0;
        int ret;

        // empty or comment line
        if ((line[0] == '\n') || (line[0] == '#'))
            continue;

        ret = sscanf(line, "TIME_BLOCKS %d\n", &ctx->time_blocks);
        if (ret > 0) {
            last_var = NULL;
            continue;
        }

        ret = sscanf(line, "DAYS %d\n", &ctx->days);
        if (ret > 0) {
            last_var = NULL;
            continue;
        }

        ret = sscanf(line, "READ_FREQ %d\n", &ctx->read_freq);
        if (ret > 0) {
            last_var = NULL;
            continue;
        }

        ret = sscanf(line, "ENABLE_TIME_INPUT %d\n",
            &tmp);
        if (ret > 0) {
            ctx->enable_time_input = !!tmp;
            last_var = NULL;
            continue;
        }

        ret = sscanf(line, "ENABLE_WEEKDAY_INPUT %d\n",
            &tmp);
        if (ret > 0) {
            ctx->enable_weekday_input = !!tmp;
            last_var = NULL;
            continue;
        }

        ret = sscanf(line, "INPUT " STR_FMT(NAME_SIZE) " %f %f\n",
            name, &min, &max);
        if (ret > 0) {
            last_var = add_input(ctx, name, min, max);
            continue;
        }

        ret = sscanf(line, "OUTPUT " STR_FMT(NAME_SIZE) " %f %f\n",
            name, &min, &max);
        if (ret > 0) {
            last_var = add_output(ctx, name, min, max);
            add_expectation(ctx, last_var, name);
            continue;
        }

        ret = sscanf(line, "TERM " STR_FMT(NAME_SIZE) " %f %f\n",
            name, &min, &max);
        if (ret > 0) {
            if (!last_var) {
                fprintf(stderr, "Failed to find var for term %s\n", line);
                goto error;
            }
            add_term(ctx, last_var, name, min, max);
            continue;
        }

        fprintf(stderr, "Unknow configuration %s\n", line);
        goto error;
    }

    ctx->reads = 24 * 60 / ctx->read_freq * ctx->days;

    for (itr = ctx->inputs; itr; itr = itr->next) {
        Variable *var = itr->data;
        if (!var->terms)
            variable_add_terms(ctx, var);
    }

    for (itr = ctx->outputs; itr; itr = itr->next) {
        Variable *var = itr->data;
        if (!var->terms)
            variable_add_terms(ctx, var);
    }

    fclose(f);
    return true;

error:
    fclose(f);
    return false;
}

static void
add_time_day(Context *ctx)
{
    int i;
    char term_name[8];

    if (ctx->enable_time_input && ctx->time_blocks) {
        ctx->time = sml_new_input(ctx->sml, TIME_STR);
        sml_variable_set_range(ctx->sml, ctx->time, 0, ctx->time_blocks);
    }

    if (ctx->enable_weekday_input) {
        /* Mon == 0; Sun == 6 */
        ctx->weekday = sml_new_input(ctx->sml, WEEKDAY_STR);
        sml_variable_set_range(ctx->sml, ctx->weekday, 0, 7);
    }

    if (ctx->engine_type != FUZZY_ENGINE &&
        ctx->engine_type != FUZZY_ENGINE_NO_SIMPLIFICATION)
        return;

    //create fuzzy terms
    if (ctx->time) {
        snprintf(term_name, sizeof(term_name), "%d",  0);
        sml_fuzzy_variable_add_term_ramp(ctx->sml, ctx->time, term_name, 1, 0);
        for (i = 1; i < ctx->time_blocks - 1; i++) {
            snprintf(term_name, sizeof(term_name), "%d",  i);
            sml_fuzzy_variable_add_term_triangle(ctx->sml, ctx->time,
                term_name, i - 1, i, i + 1);
        }
        snprintf(term_name, sizeof(term_name), "%d",  i);
        sml_fuzzy_variable_add_term_ramp(ctx->sml, ctx->time, term_name, i - 1,
            i);
    }

    if (ctx->weekday) {
        sml_fuzzy_variable_add_term_triangle(ctx->sml, ctx->weekday,
            "Monday", 0, 0.5, 1);
        sml_fuzzy_variable_add_term_triangle(ctx->sml, ctx->weekday,
            "Tuesday", 1, 1.5, 2);
        sml_fuzzy_variable_add_term_triangle(ctx->sml, ctx->weekday,
            "Wednesday", 2, 2.5, 3);
        sml_fuzzy_variable_add_term_triangle(ctx->sml, ctx->weekday,
            "Thursday", 3, 3.5, 4);
        sml_fuzzy_variable_add_term_triangle(ctx->sml, ctx->weekday,
            "Friday", 4, 4.5, 5);
        sml_fuzzy_variable_add_term_triangle(ctx->sml, ctx->weekday,
            "Saturday", 5, 5.5, 6);
        sml_fuzzy_variable_add_term_triangle(ctx->sml, ctx->weekday,
            "Sunday", 6, 6.5, 7);
    }
}

struct sml_object *
_sml_new(int id)
{
    switch (id) {
    case FUZZY_ENGINE:
    case FUZZY_ENGINE_NO_SIMPLIFICATION:
        return sml_fuzzy_new();
    case ANN_ENGINE:
        return sml_ann_new();
    case NAIVE_ENGINE:
        return sml_naive_new();
    }
    return NULL;
}

static void
_process_results(Context *ctx, double duration)
{
    GList *itr, *terms;
    int read_counter = ctx->read_counter - 1; // it was incremented on read_state_cb
    Expectation_Block *eblock;

    if (ctx->max_iteration_duration < duration)
        ctx->max_iteration_duration = duration;

    if (!ctx->cur_expectation_block)
        return;

    eblock = ctx->cur_expectation_block->data;
    if (read_counter < eblock->begin)
        return;

    for (itr = ctx->outputs; itr; itr = itr->next) {
        Variable *var = itr->data;
        Event *event;
        Expectation *expec;

        if (!sml_variable_is_enabled(ctx->sml, var->sml_var))
            continue;

        expec = get_expectation(ctx->expectations, var);
        if (!expec) {
            fprintf(stderr, "Failed to find expectation %s\n", var->name);
            _debug_guess(read_counter, var->name, var->guess_value,
                NULL, NULL);
            printf("SML is wrong\n");
            continue;
        }

        event = get_event(expec->events, read_counter);
        //if event value is indifferent (nan), skip it
        if (isnan(event->value))
            continue;

        var->changes_counter++;
        terms = get_terms(var->terms, var->guess_value);
        if (!terms) {
            fprintf(stderr, "Failed to find term for %s and value %f\n",
                var->name, var->guess_value);
            if (ctx->debug)
                _debug_guess(read_counter, var->name, var->guess_value,
                    NULL, event);
            printf("SML is wrong\n");
            continue;
        }

        if (ctx->debug)
            _debug_guess(read_counter, var->name, var->guess_value,
                terms, event);

        if (event_contains_guess(event, terms)) {
            var->right_guesses++;
            if (ctx->debug)
                printf("SML is right\n");
        } else {
            if (ctx->debug)
                printf("SML is wrong\n");
        }
        g_list_free(terms);
    }

    if (read_counter >= eblock->end) {
        ctx->cur_expectation_block = ctx->cur_expectation_block->next;

        for (itr = ctx->outputs; itr; itr = itr->next) {
            Variable *var = itr->data;
            if (var->changes_counter) {
                if (var->right_guesses == var->changes_counter)
                    var->expectations_right_guesses++;
                else
                    eblock->error = true;
                var->expectations_counter++;
            }
            var->right_guesses = var->changes_counter = 0;
        }
    }
}

int
main(int argc, char *argv[])
{
    Context ctx;
    int i, tic, toc, total_toc, total_tic;
    double duration;
    int error;

    if (argc < 6) {
        fprintf(stderr, "%s TEST.conf TEST.data SEED_VAL DEBUG_VAL "
            "ENGINE_TYPE(0 fuzzy, 1 ann, 2 naive, "
            "3 fuzzy_no_simplification) [MAX_MEMORY_FOR_OBSERVATION] "
            "[ANN_CACHE_SIZE] [ANN_PSEUDO_REHEARSAL_STRATETY (1 for true, 0 for false)]\n",
            argv[0]);
        fprintf(stderr,
            "Eg: %s simple_office.conf simple_office.forget_lights_on.data "
            "30 1 1\n",
            argv[0]);
        return 1;
    }

    ctx.rand = g_rand_new_with_seed(atoi(argv[3]));
    ctx.debug = !!atoi(argv[4]);

    ctx.engine_type = atoi(argv[5]);
    ctx.sml = _sml_new(ctx.engine_type);
    if (!ctx.sml) {
        fprintf(stderr, "Failed to create sml\n");
        return 2;
    }

    if (!read_config(argv[1], &ctx)) {
        fprintf(stderr, "Failed to read configuration: %s\n", argv[1]);
        return 3;
    }

    if (!read_values(argv[2], &ctx)) {
        fprintf(stderr, "Failed to read data: %s\n", argv[2]);
        return 4;
    }

    add_time_day(&ctx);

    sml_set_read_state_callback(ctx.sml, read_state_cb, &ctx);
    sml_set_output_state_changed_callback(ctx.sml, output_state_changed_cb,
        &ctx);
    sml_set_stabilization_hits(ctx.sml, 0);
    if (ctx.engine_type == FUZZY_ENGINE_NO_SIMPLIFICATION)
        sml_fuzzy_set_simplification_disabled(ctx.sml, true);

    if (argc >= 7) {
        int observations = atoi(argv[6]);
        if (observations < 0) {
            fprintf(stderr, "MAX_MEMORY_FOR_OBSERVATIOS (%s) must be a non "
                "negative value\n", argv[6]);
            return 5;
        }
        sml_set_max_memory_for_observations(ctx.sml, observations);
    }

    if (ctx.engine_type == ANN_ENGINE) {
        if (argc >= 8) {
            int cache_size = atoi(argv[7]);
            if (cache_size < 0 || cache_size >= UINT16_MAX) {
                fprintf(stderr, "ANN_CACHE_SIZE (%s) must be greater or equal "
                    "to 0 an less or equal to %d\n", argv[7], UINT16_MAX);
                return 6;
            }
            sml_ann_set_cache_max_size(ctx.sml, cache_size);
        }
        if (argc >= 9)
            sml_ann_use_pseudorehearsal_strategy(ctx.sml, atoi(argv[8]) != 0);
    }

    if (ctx.debug)
        print_scenario(&ctx);

    ctx.max_iteration_duration = -1;
    total_tic = clock();
    for (i = 0; i < ctx.reads; i++) {
        tic = clock();
        if ((error = sml_process(ctx.sml))) {
            fprintf(stderr, "=== Unexpected error in simulation. "
                "Error code: %d ===\n", error);
            break;
        }
        toc = clock();
        duration = ((double)toc - tic) / CLOCKS_PER_SEC;
        _process_results(&ctx, duration);
    }
    total_toc = clock();
    ctx.duration = ((double)total_toc - total_tic) / CLOCKS_PER_SEC;

    // scenario may changes thanks to new events created automatically
    if (ctx.debug) {
        print_scenario(&ctx);
        sml_print_debug(ctx.sml, true);
    }
    print_results(&ctx);

    sml_free(ctx.sml);

    g_list_free_full(ctx.inputs, free_variable);
    g_list_free_full(ctx.outputs, free_variable);
    g_list_free_full(ctx.expectations, free_expectation);
    g_list_free_full(ctx.expectation_blocks, free_element);
    g_rand_free(ctx.rand);

    return 0;
}
