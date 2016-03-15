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
#include <glib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <sml_main_loop.h>
#include <sml_log.h>
#include <stdio.h>

#define VAR_TEMPERATURE (0)
#define VAR_PRESENCE (1)
#define VAR_POWER (0)
#define ERROR_START (20)
#define ERROR_FREQUENCY (10)
#define READ_TIMEOUT (10)
#define STABILIZATION_HITS (0)
#define INITIAL_REQUIRED_OBS (10)
#define FUZZY_ENGINE 0
#define ANN_ENGINE 1

/*
 * Air Conditioner controller simulator code.
 * Controller that simulates an air conditioner operated by a regular user.
 * In some cases user forgets to set the air conditioner power and SML is expect
 * to set it for her.
 */

typedef enum {
    COLD = 0,
    WARM,
    HOT
} Temperature;

typedef struct {
    int presence;
    int power;
    Temperature temp;
    int reads;
} Air_Conditioner_Controller;

static void
_initialize_acc(Air_Conditioner_Controller *acc)
{
    srand(time(NULL));
    memset(acc, 0, sizeof(Air_Conditioner_Controller));
}

static float
_generate_temperature(Temperature t)
{
    float min, max;

    if (t == COLD) {
        min = 0.0;
        max = 15.0;
    } else if (t == WARM) {
        min = 15.0;
        max = 23.0;
    } else {
        min = 23.0;
        max = 50.0;
    }

    return ((float)rand() / (float)RAND_MAX) * (max - min) + min;
}

static float
_calculate_power(Air_Conditioner_Controller *acc)
{
    switch (acc->temp) {
    case HOT:
        return 3;
    case WARM:
        return 2;
    default:
        return 1;
    }
}

static bool
_read_sensor_values(Air_Conditioner_Controller *acc, float *temp,
    float *presence, float *power)
{
    bool error;
    char *temps[3] = { "COLD", "WARM", "HOT" };
    char *presences[2] = { "ABSENT", "PRESENT" };
    char *temperature_str, *user_str, *status_str;
    float t;

    acc->temp = (acc->temp + 1) % 3;
    t = _generate_temperature(acc->temp);
    acc->presence = rand() % 2;

    if (acc->reads > ERROR_START) {
        error =  (rand() % ERROR_FREQUENCY) == 0;
    } else
        error = false;
    acc->reads++;

    if (acc->presence) {
        if (error)
            status_str = "User didn't change the air conditioner power "
                " - current at ";
        else {
            status_str = "User changed the air conditioner power to ";
            acc->power = _calculate_power(acc);
        }
    } else {
        if (error)
            status_str = "User left the room and let the air conditioner power"
                " at ";
        else {
            status_str = "User changed the air conditioner power to ";
            acc->power = 0; //Turn off if user is absent
        }
    }

    temperature_str = temps[acc->temp];
    user_str = presences[acc->presence];
    printf("Temperature is %2.0f (%s) and User is %s. [%s %d]\n", t,
        temperature_str, user_str, status_str, acc->power);

    *temp = t;
    *presence = acc->presence;
    *power = acc->power;

    fflush(stdout);
    return true;
}

static void
_update_air_conditioner_power(Air_Conditioner_Controller *acc, float new_value)
{
    int new_power;

    if (isnan(new_value))
        return;

    new_power = roundf(new_value);

    if (new_power != acc->power) {
        printf("SML fixed air conditioner power to %d\n", new_power);
        acc->power = new_power;
    }
}

/*
 * Air conditioner system controlled by SML code
 */

static void
_output_state_changed_cb(struct sml_object *sml,
    struct sml_variables_list *changed, void *data)
{
    struct sml_variable *sml_var = sml_variables_list_index(sml, changed, 0);
    float new_value = sml_variable_get_value(sml, sml_var);

    _update_air_conditioner_power(data, new_value);
}

static bool
_read_state_cb(struct sml_object *sml, void *data)
{
    struct sml_variable *temperature_var, *presence_var, *power_var;
    struct sml_variables_list *input_list, *output_list;
    Air_Conditioner_Controller *acc = data;
    float temp, presence, power;

    input_list = sml_get_input_list(sml);
    output_list = sml_get_output_list(sml);

    temperature_var = sml_variables_list_index(sml, input_list,
        VAR_TEMPERATURE);
    presence_var = sml_variables_list_index(sml, input_list, VAR_PRESENCE);
    power_var = sml_variables_list_index(sml, output_list, VAR_POWER);

    if (!_read_sensor_values(acc, &temp, &presence, &power))
        return false;

    sml_variable_set_value(sml, temperature_var, temp);
    sml_variable_set_value(sml, presence_var, presence);
    sml_variable_set_value(sml, power_var, power);

    sml_print_debug(sml, false);
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

int
main(int argc, char **argv)
{
    Air_Conditioner_Controller acc;
    struct sml_variable *temp, *presence, *power;
    struct sml_object *sml;

    _initialize_acc(&acc);

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <engine_type (0 fuzzy, 1 ann)>\n", argv[0]);
        fprintf(stderr, "Fuzzy Test: %s 0\n", argv[0]);
        fprintf(stderr, "Ann Test: %s 1\n", argv[0]);
        return -1;
    }

    sml = _sml_new(atoi(argv[1]));
    if (!sml) {
        sml_critical("Failed to create sml");
        return -1;
    }

    sml_main_loop_init();

    //Set stabilization hits and initial required obs to a low value because
    //this is a simulation and we don't want to wait for a long time before
    //getting interesting results
    sml_set_stabilization_hits(sml, STABILIZATION_HITS);
    sml_ann_set_initial_required_observations(sml, INITIAL_REQUIRED_OBS);

    //Create temperature input
    temp = sml_new_input(sml, "Temperature");
    sml_variable_set_range(sml, temp, 0, 48);
    sml_fuzzy_variable_set_default_term_width(sml, temp, 16);

    //Create presence input
    presence = sml_new_input(sml, "Presence");
    sml_variable_set_range(sml, presence, 0, 1);
    sml_fuzzy_variable_set_default_term_width(sml, presence, 0.5);

    //Create power output
    power = sml_new_output(sml, "Power");
    sml_variable_set_range(sml, power, 0, 3);
    sml_fuzzy_variable_set_default_term_width(sml, power, 1);
    sml_fuzzy_variable_set_is_id(sml, power, true);

    //set SML callbacks
    sml_set_read_state_callback(sml, _read_state_cb, &acc);
    sml_main_loop_schedule_sml_process(sml, READ_TIMEOUT);
    sml_set_output_state_changed_callback(sml, _output_state_changed_cb, &acc);

    sml_main_loop_run();

    sml_free(sml);
    sml_main_loop_shutdown();
    return 0;
}
