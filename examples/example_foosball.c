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
 * Foosball game prediction example
 *
 * This example loads from a file data from foosball matches. It feeds SML with
 * games results and use it to predict future winners.
 *
 * Team1 is using red uniforms and team2 is using yellow uniforms
 */
#include <sml_ann.h>
#include <sml_fuzzy.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <glib.h>

#define LINE_SIZE (256)
#define DISCRETE_THRESHOLD (0.45)
#define REQUIRED_OBS 5
#define THRESHOLD (0.1)
#define FUZZY_ENGINE 0
#define ANN_ENGINE 1
#define MIN_PLAYERS 4
#define WINNER_NONE 0
#define WINNER1 1
#define WINNER2 2

typedef struct {
    FILE *f;
    struct sml_object *sml;
    int sml_engine;
    uint16_t num_players;
    int reads, predictions, rights;
    uint16_t val_offense1, val_defense1, val_offense2, val_defense2;
    int val_score1, val_score2, val_winner;
    char line[LINE_SIZE];
    bool first_train;

    struct sml_variable *offense1, *defense1;
    struct sml_variable *offense2, *defense2;

    struct sml_variable *winner;
    char **players;
} Context;

static bool
_read_next_line(char *line, int len, FILE *f)
{
    while (fgets(line, len, f)) {
        if (line[0] == '\n' || line[0] == '\0' || line[0] == '#')
            continue;
        return true;
    }

    return false;
}

static bool
_find_player_by_name(Context *ctx, const char *name, uint16_t *number)
{
    uint16_t i;

    for (i = 0; i < ctx->num_players; i++)
        if (!strcmp(ctx->players[i], name)) {
            *number = i;
            return true;
        }

    fprintf(stderr, "Player %s not found\n", name);
    return false;
}

static bool
_read_data(Context *ctx)
{
    int ret;
    char p1[LINE_SIZE], p2[LINE_SIZE], p3[LINE_SIZE], p4[LINE_SIZE];

    if (!_read_next_line(ctx->line, LINE_SIZE, ctx->f))
        return false;

    ret = sscanf(ctx->line, "%s %s %s %s %d %d", p1, p2, p3, p4,
        &ctx->val_score1, &ctx->val_score2);
    if (ret < 6)
        return false;

    if (!_find_player_by_name(ctx, p1, &ctx->val_offense1))
        return false;

    if (!_find_player_by_name(ctx, p2, &ctx->val_defense1))
        return false;

    if (!_find_player_by_name(ctx, p3, &ctx->val_offense2))
        return false;

    if (!_find_player_by_name(ctx, p4, &ctx->val_defense2))
        return false;

    if (ctx->val_score1 > ctx->val_score2)
        ctx->val_winner = WINNER1;
    else if (ctx->val_score2 > ctx->val_score1)
        ctx->val_winner = WINNER2;
    else
        ctx->val_winner = WINNER_NONE;

    return true;
}

static bool
_read_state_cb(struct sml_object *sml, void *data)
{
    Context *ctx = data;

    if (ctx->first_train) {
        sml_variable_set_value(sml, ctx->offense1, ctx->val_offense1);
        sml_variable_set_value(sml, ctx->defense1, ctx->val_defense1);
        sml_variable_set_value(sml, ctx->offense2, ctx->val_offense2);
        sml_variable_set_value(sml, ctx->defense2, ctx->val_defense2);
        sml_variable_set_value(ctx->sml, ctx->winner, ctx->val_winner);
    } else {
        sml_variable_set_value(sml, ctx->offense1, ctx->val_offense2);
        sml_variable_set_value(sml, ctx->defense1, ctx->val_defense2);
        sml_variable_set_value(sml, ctx->offense2, ctx->val_offense1);
        sml_variable_set_value(sml, ctx->defense2, ctx->val_defense1);
        if (ctx->val_winner == WINNER_NONE)
            sml_variable_set_value(ctx->sml, ctx->winner, WINNER_NONE);
        else if (ctx->val_winner == WINNER1)
            sml_variable_set_value(ctx->sml, ctx->winner, WINNER2);
        else
            sml_variable_set_value(ctx->sml, ctx->winner, WINNER1);
    }


    return true;
}

static bool
_do_prediction(Context *ctx)
{
    char *result = "";
    float prediction;

    sml_variable_set_value(ctx->sml, ctx->offense1, ctx->val_offense1);
    sml_variable_set_value(ctx->sml, ctx->defense1, ctx->val_defense1);
    sml_variable_set_value(ctx->sml, ctx->offense2, ctx->val_offense2);
    sml_variable_set_value(ctx->sml, ctx->defense2, ctx->val_defense2);
    sml_variable_set_value(ctx->sml, ctx->winner, NAN);

    sml_predict(ctx->sml);
    ctx->reads++;
    prediction = roundf(sml_variable_get_value(ctx->sml, ctx->winner));

    if (!isnan(prediction)) {
        ctx->predictions++;
        if (ctx->val_winner == ((int)prediction)) {
            ctx->rights++;
            result = "right";
        } else
            result = "wrong";
    }

    printf("Game %d team 1 (%d, %d) x team2 (%d, %d): predicted winner: %.0f "
        "real winner: %d %s\n", ctx->reads,
        ctx->val_offense1, ctx->val_defense1, ctx->val_offense2,
        ctx->val_defense2, prediction, ctx->val_winner, result);


    return true;
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

static struct sml_variable *
_create_input(Context *ctx, const char *name)
{
    char term_name[LINE_SIZE];
    struct sml_variable *v;
    uint16_t i;

    v = sml_new_input(ctx->sml, name);
    sml_variable_set_range(ctx->sml, v, 0, ctx->num_players - 1);

    if (ctx->sml_engine == FUZZY_ENGINE) {
        snprintf(term_name, LINE_SIZE, "%s_%s", name, ctx->players[0]);
        sml_fuzzy_variable_add_term_ramp(ctx->sml, v, term_name,
            0 + DISCRETE_THRESHOLD, 0, 1);
        for (i = 1; i < ctx->num_players - 1; i++) {
            snprintf(term_name, LINE_SIZE, "%s_%s", name, ctx->players[i]);
            sml_fuzzy_variable_add_term_triangle(ctx->sml, v, term_name,
                i - DISCRETE_THRESHOLD, i,
                i + DISCRETE_THRESHOLD, 1);
        }
        snprintf(term_name, LINE_SIZE, "%s_%s", name, ctx->players[i]);
        sml_fuzzy_variable_add_term_ramp(ctx->sml, v, term_name,
            ctx->num_players - 1 - DISCRETE_THRESHOLD,
            ctx->num_players - 1, 1);
    }

    return v;
}

static bool
_initialize_sml(Context *ctx)
{
    sml_set_stabilization_hits(ctx->sml, 0);
    sml_set_read_state_callback(ctx->sml, _read_state_cb, ctx);
    if (ctx->sml_engine == ANN_ENGINE)
        sml_ann_set_initial_required_observations(ctx->sml, REQUIRED_OBS);

    ctx->offense1 = _create_input(ctx, "red_striker");
    ctx->defense1 = _create_input(ctx, "red_goalkeeper");
    ctx->offense2 = _create_input(ctx, "yellow_striker");
    ctx->defense2 = _create_input(ctx, "yellow_goalkeeper");

    //number of the winner team
    ctx->winner = sml_new_output(ctx->sml, "winner");
    sml_variable_set_range(ctx->sml, ctx->winner, 0, 2);
    if (ctx->sml_engine == FUZZY_ENGINE) {
        sml_fuzzy_variable_add_term_ramp(ctx->sml, ctx->winner, "none",
            0 + DISCRETE_THRESHOLD, 0, 1);
        sml_fuzzy_variable_add_term_triangle(ctx->sml, ctx->winner,
            "red_winner", WINNER1 - DISCRETE_THRESHOLD, WINNER1,
            WINNER1 + DISCRETE_THRESHOLD, 1);
        sml_fuzzy_variable_add_term_ramp(ctx->sml, ctx->winner, "yellow_winner",
            WINNER2 - DISCRETE_THRESHOLD, WINNER2,
            1);
    }

    return true;
}

static void
_free_players(char **players, uint16_t num_players)
{
    uint16_t i;

    for (i = 0; i < num_players; i++)
        free(players[i]);
    free(players);
}

static bool
_read_config(Context *ctx, const char *filename)
{
    uint16_t i;
    FILE *f;
    size_t line_len;

    f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Failed to open the config file: %s\n", filename);
        return false;
    }

    if (!_read_next_line(ctx->line, LINE_SIZE, f))
        goto config_error;

    ctx->num_players = atoi(ctx->line);
    if (ctx->num_players < MIN_PLAYERS) {
        fprintf(stderr, "%d is not enough players.\n", ctx->num_players);
        goto config_error;
    }

    printf("%d players:\n", ctx->num_players);
    ctx->players = malloc(sizeof(char *) * ctx->num_players);
    for (i = 0; i < ctx->num_players; i++) {
        if (!_read_next_line(ctx->line, LINE_SIZE, f)) {
            _free_players(ctx->players, i);
            goto config_error;
        }

        line_len = strlen(ctx->line);
        ctx->line[line_len - 1] = 0;
        ctx->players[i] = malloc(strlen(ctx->line) + 100);
        strncpy(ctx->players[i], ctx->line, line_len);
        printf("\t%s\n", ctx->players[i]);
    }
    printf("\n");

    fclose(f);
    return true;

config_error:
    fclose(f);
    return false;
}

int
main(int argc, char *argv[])
{
    int error = 0;
    Context ctx;

    if (argc < 4) {
        fprintf(stderr, "Correct usage %s <config_file> <data_file> <engine>\n",
            argv[0]);
        return -1;
    }

    if (!_read_config(&ctx, argv[1]))
        return -1;

    ctx.f = fopen(argv[2], "r");
    if (!ctx.f) {
        fprintf(stderr, "Failed to open %s\n", argv[1]);
        return -1;
    }

    ctx.sml_engine = atoi(argv[3]);
    ctx.sml = _sml_new(ctx.sml_engine);
    ctx.reads = ctx.rights = ctx.predictions = 0;
    if (!ctx.sml || !_initialize_sml(&ctx)) {
        fprintf(stderr, "Failed to initialize sml\n");
        error = -1;
        goto end;
    }

    while (_read_data(&ctx)) {
        _do_prediction(&ctx);
        ctx.first_train = true;
        if ((error = sml_process(ctx.sml))) {
            fprintf(stderr, "sml_process error number %d\n", error);
            break;
        }

        ctx.first_train = false;
        if ((error = sml_process(ctx.sml))) {
            fprintf(stderr, "sml_process error number %d\n", error);
            break;
        }
    }

    sml_print_debug(ctx.sml, false);
    printf("Right guesses: %d of %d (%d games) \n", ctx.rights,
        ctx.predictions, ctx.reads);
    printf("Right predictions percentage:%f%%\n",
        ctx.rights * 100.0 / ctx.reads);
    printf("Total right predictions percentage:%f%%\n",
        ctx.rights * 100.0 / ctx.predictions);
    sml_free(ctx.sml);
    _free_players(ctx.players, ctx.num_players);

end:
    fclose(ctx.f);
    return error;
}
