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

#include <sol-flower-power.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <machine_learning_sml_data.h>

#include "sml_garden_gen.h"

#define ENGINE_DURATION_MIN_VAL (0)
#define ENGINE_DURATION_MAX_VAL (60)

struct sml_garden_data {
    time_t btn_pressed_timestamp;
    int last_engine_on_duration;
    bool engine_is_on;
    bool has_pending_data;

    struct sol_drange water;
    struct sol_irange timeblock;
};

static int
send_packet(struct sol_flow_node *node, struct sml_garden_data *sdata)
{
    struct packet_type_sml_data_packet_data sml_data;
    struct sol_drange inputs[2], output;

    sml_data.inputs_len = 2;
    sml_data.outputs_len = 1;

    sml_data.outputs = &output;
    sml_data.inputs = inputs;

    output.val = sdata->last_engine_on_duration;
    output.min = ENGINE_DURATION_MIN_VAL;
    output.max = ENGINE_DURATION_MAX_VAL;
    output.step = 1;

    inputs[0] = sdata->water;

    inputs[1].val = sdata->timeblock.val;
    inputs[1].min = sdata->timeblock.min;
    inputs[1].max = sdata->timeblock.max;
    inputs[1].step = sdata->timeblock.step;

    sdata->last_engine_on_duration = 0;
    SOL_DBG("Sending packet to SML");
    return sml_data_send_packet(node,
        SOL_FLOW_NODE_TYPE_SML_GARDEN_MESSAGE_CONSTRUCTOR__OUT__OUT,
        &sml_data);
}

static int
flower_power_packet_process(struct sol_flow_node *node, void *data,
    uint16_t port, uint16_t conn_id,
    const struct sol_flow_packet *packet)
{
    int r;
    const char *timestamp, *id;
    struct sml_garden_data *sdata = data;

    r = sol_flower_power_get_packet_components(packet, &id,
        &timestamp, NULL, NULL, NULL, &sdata->water);
    SOL_INT_CHECK(r, < 0, r);
    SOL_DBG("Received packet - id: %s - timestamp: %s - water:%g", id,
        timestamp, sdata->water.val);

    if (isnan(sdata->water.val)) {
        SOL_DBG("Current water value is NAN, ignoring it.");
        return 0;
    }

    /* Engine still running, wait until we can send the data. */
    if (sdata->engine_is_on) {
        sdata->has_pending_data = true;
        return 0;
    }

    return send_packet(node, sdata);
}

static int
engine_state_process(struct sol_flow_node *node, void *data,
    uint16_t port, uint16_t conn_id, const struct sol_flow_packet *packet)
{
    int r;
    bool engine_is_on;
    struct sml_garden_data *sdata = data;
    time_t now = time(NULL);

    r = sol_flow_packet_get_boolean(packet, &engine_is_on);
    SOL_INT_CHECK(r, < 0, r);
    if (engine_is_on)
        sdata->btn_pressed_timestamp = now;
    else if (sdata->engine_is_on) {
        sdata->last_engine_on_duration = now - sdata->btn_pressed_timestamp;
        SOL_DBG("Pressed for:%d seconds", sdata->last_engine_on_duration);
    }
    sdata->engine_is_on = engine_is_on;

    if (!sdata->has_pending_data || sdata->last_engine_on_duration < 1)
        return 0;

    /* Send */
    sdata->has_pending_data = false;
    return send_packet(node, sdata);
}

static int
timeblock_process(struct sol_flow_node *node, void *data,
    uint16_t port, uint16_t conn_id, const struct sol_flow_packet *packet)
{
    struct sml_garden_data *sdata = data;
    int r;

    r = sol_flow_packet_get_irange(packet, &sdata->timeblock);
    SOL_INT_CHECK(r, < 0, r);
    SOL_DBG("Timeblock changed. Now:%d", sdata->timeblock.val);
    return 0;
}

#include "sml_garden_gen.c"
