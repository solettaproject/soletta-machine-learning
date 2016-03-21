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

/* Very basic sample to introduce people to SML.
 * It simulates a scenario with a presence sensor and a light bulb
 * controlled by the user, that eventually forgets to turn lights off.
 */

/* WARNING: when this sample breaks, it means "Soletta Machine Learning:
 * Light Sensor Tutorial" wiki page must to be updated.
 * All patches applied here should be applied there as well.
 *
 * https://github.com/solettaproject/soletta/wiki/Soletta-Machine-Learning%3A-Light-Sensor-Tutorial
 */

#include <math.h>
#include <sml.h>
#include <sml_fuzzy.h>
#include <stdio.h>


static int switch_state = 0;

static bool
read_state_cb(struct sml_object *sml, void *data)
{
    struct sml_variables_list *inputs, *outputs;
    struct sml_variable *sensor, *light;

    static int sensor_state = 0;
    static int off_count = 0;
    static unsigned int count = 0;

    /* user is present after 10 reads */
    if (count == 10) {
        printf("User got in the room.\n");
        sensor_state = 1;
        switch_state = 1;
        count++;
    }
    /* and stay there for 5 more reads */
    else if (count == 15) {
        printf("User left the room.\n");
        off_count++;
        sensor_state = 0;
        /* most of times she remembers to swith off lights
         * when she leaves */
        if (off_count % 4 == 0) {
            printf("Oops! User forgot to turn lights off.\n");
        } else {
            switch_state = 0;
        }
        count = 0;
    }
    /* ... forever on this cycle */
    else {
        count++;
    }

    inputs = sml_get_input_list(sml);
    sensor = sml_variables_list_index(sml, inputs, 0);
    sml_variable_set_value(sml, sensor, sensor_state);

    outputs = sml_get_output_list(sml);
    light = sml_variables_list_index(sml, outputs, 0);
    sml_variable_set_value(sml, light, switch_state);

    return true;
}

static void
output_state_changed_cb(struct sml_object *sml,
    struct sml_variables_list *changed, void *data)
{
    struct sml_variable *light;
    double prediction;

    light = sml_variables_list_index(sml, changed, 0);
    prediction = sml_variable_get_value(sml, light);

    /* When SML can't predict a value, it'll be set to NaN */
    if (isnan(prediction)) {
        printf("Sorry, can't predict light state.\n");
        return;
    }

    /* prediction is equal to current state */
    if ((prediction > 0.5) == switch_state) {
        return;
    }

    if (prediction > 0.5) {
        printf("Light should be turned ON.\n");
    } else {
        printf("Light should be turned OFF.\n");
    }
}

int
main(int argc, char *argv[])
{
    struct sml_object *sml;
    struct sml_variable *sensor, *light;

    sml = sml_fuzzy_new();

    sensor = sml_new_input(sml, "PresenceSensor");
    sml_variable_set_range(sml, sensor, 0, 1);
    sml_fuzzy_variable_set_default_term_width(sml, sensor, 0.5);

    light = sml_new_output(sml, "Light");
    sml_variable_set_range(sml, light, 0, 1);
    sml_fuzzy_variable_set_default_term_width(sml, light, 0.5);

    sml_set_read_state_callback(sml, read_state_cb, NULL);
    sml_set_output_state_changed_callback(sml, output_state_changed_cb, NULL);

    for (int i = 0; i < 150; i++) {
        if (sml_process(sml) < 0) {
            printf("Failed to process\n");
        }
    }

    sml_free(sml);

    return 0;
}
