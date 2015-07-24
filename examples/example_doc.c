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

/* Very basic sample to introduce people to SML.
 * It simulates a scenario with a presence sensor and a light bulb
 * controlled by the user, that eventually forgets to turn lights off.
 */

/* WARNING: when this sample breaks, it means "Getting Started on SML"
 * wiki page must to be updated.
 * All patches applied here should be applied there as well.
 *
 * https://github.com/solettaproject/soletta/wiki/Getting-Started-on-SML
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
        if (!sml_process(sml)) {
            printf("Failed to process\n");
        }
    }

    sml_free(sml);

    return 0;
}
