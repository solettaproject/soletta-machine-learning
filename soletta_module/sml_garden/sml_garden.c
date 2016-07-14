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

#include <sol-flow.h>
#include <sol-flower-power.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <machine_learning_sml_data.h>

#include "sml_garden_gen.h"

#define ENGINE_DURATION_MIN_VAL (0)
#define ENGINE_DURATION_MAX_VAL (30)

struct sml_garden_data {
    time_t btn_pressed_timestamp;
    time_t last_timestamp;
    uint8_t last_engine_on_duration;
    bool has_pending_data;

    struct sol_drange cur_water, last_water;
    struct sol_irange cur_timeblock, last_timeblock;
};

static const struct sol_flow_packet_type *PACKET_TYPE_FLOWER_POWER;

static int
send_sml_garden_packet(struct sol_flow_node *node, int port,
    double last_engine_on_duration, struct sol_drange *water_val,
    struct sol_irange *timeblock)
{
    struct packet_type_sml_data_packet_data sml_data;
    struct sol_drange inputs[2], output;

    sml_data.input_ids_len = sml_data.output_ids_len = 0;
    sml_data.input_ids = sml_data.output_ids = NULL;
    sml_data.inputs_len = 2;
    sml_data.outputs_len = 1;

    sml_data.outputs = &output;
    sml_data.inputs = inputs;

    output.val = last_engine_on_duration;
    output.min = ENGINE_DURATION_MIN_VAL;
    output.max = ENGINE_DURATION_MAX_VAL;
    output.step = 1;

    inputs[0] = *water_val;

    inputs[1].val = timeblock->val;
    inputs[1].min = timeblock->min;
    inputs[1].max = timeblock->max;
    inputs[1].step = timeblock->step;

    return sml_data_send_packet(node, port, &sml_data);
}

static int
send_predict_packet(struct sol_flow_node *node, struct sml_garden_data *sdata)
{
    SOL_DBG("Sending predict packet to SML");
    return send_sml_garden_packet(node,
        SOL_FLOW_NODE_TYPE_SML_GARDEN_MESSAGE_CONSTRUCTOR__OUT__OUT_PREDICT,
        0, &sdata->last_water, &sdata->cur_timeblock);
}

static int
send_packet_if_needed(struct sol_flow_node *node, struct sml_garden_data *sdata)
{
    int r;

    //If not ready to send packet
    if (!sdata->has_pending_data || sdata->btn_pressed_timestamp != 0)
        return 0;

    sdata->has_pending_data = false;

    SOL_DBG("Sending packet to SML");
    r = send_sml_garden_packet(node,
        SOL_FLOW_NODE_TYPE_SML_GARDEN_MESSAGE_CONSTRUCTOR__OUT__OUT,
        sdata->last_engine_on_duration, &sdata->last_water,
        &sdata->last_timeblock);

    sdata->last_engine_on_duration = 0;
    return r;
}

static int
flower_power_packet_process(struct sol_flow_node *node, void *data,
    uint16_t port, uint16_t conn_id,
    const struct sol_flow_packet *packet)
{
    int r;
    struct sml_garden_data *sdata = data;
    struct sol_flower_power_data fpd;

    if (sol_flow_packet_get_type(packet) != PACKET_TYPE_FLOWER_POWER)
        return -EINVAL;

    r = sol_flow_packet_get(packet, &fpd);
    SOL_INT_CHECK(r, < 0, r);

    SOL_DBG("Received packet - id: %s - timestamp: %lld - water:%g", fpd.id,
        (long long)fpd.timestamp.tv_sec, fpd.water.val);

    if (!sdata->last_timestamp ||
        fpd.timestamp.tv_sec != sdata->last_timestamp) {
        sdata->cur_water = fpd.water;
        sdata->last_timestamp = fpd.timestamp.tv_sec;
    }

    return 0;
}

static int
engine_state_process(struct sol_flow_node *node, void *data,
    uint16_t port, uint16_t conn_id, const struct sol_flow_packet *packet)
{
    int r;
    bool engine_is_on;
    struct sml_garden_data *sdata = data;
    time_t now = time(NULL);

    r = sol_flow_packet_get_bool(packet, &engine_is_on);
    SOL_INT_CHECK(r, < 0, r);

    if (engine_is_on)
        sdata->btn_pressed_timestamp = now;
    else if (sdata->btn_pressed_timestamp) {
        now -= sdata->btn_pressed_timestamp;
        SOL_DBG("Pressed for:%ld seconds", now);
        if (sdata->last_engine_on_duration + now > ENGINE_DURATION_MAX_VAL)
            sdata->last_engine_on_duration = ENGINE_DURATION_MAX_VAL;
        else
            sdata->last_engine_on_duration += now;

        sdata->btn_pressed_timestamp = 0;
        r = send_packet_if_needed(node, sdata);
        SOL_INT_CHECK(r, < 0, r);
    }

    return 0;
}

static bool
empty_irange(struct sol_irange *val)
{
    return val->val == 0 && val->min == 0 && val->max == 0 && val->step == 0;
}

static int
timeblock_process(struct sol_flow_node *node, void *data,
    uint16_t port, uint16_t conn_id, const struct sol_flow_packet *packet)
{
    struct sml_garden_data *sdata = data;
    int r, send_error = 0;
    bool has_new_flower_power_packet, send_predict;

    has_new_flower_power_packet = !isnan(sdata->cur_water.val);

    if (sdata->last_engine_on_duration > 0 || !has_new_flower_power_packet)
        send_predict = false;
    else
        send_predict = true;

    if (!empty_irange(&sdata->cur_timeblock) &&
        (has_new_flower_power_packet || sdata->last_engine_on_duration > 0)) {
        sdata->last_timeblock = sdata->cur_timeblock;
        if (has_new_flower_power_packet) {
            sdata->last_water = sdata->cur_water;
            sdata->cur_water.val = NAN;
        }
        sdata->has_pending_data = true;
        send_error = send_packet_if_needed(node, sdata);
        if (send_error)
            SOL_DBG("Send packet to process SML failed with error=%d\n",
                send_error);
    }

    r = sol_flow_packet_get_irange(packet, &sdata->cur_timeblock);
    SOL_INT_CHECK(r, < 0, r);
    SOL_DBG("Timeblock changed. Now:%d", sdata->cur_timeblock.val);

    if (send_predict)
        return send_predict_packet(node, sdata);
    return 0;
}

static int
message_constructor_open(struct sol_flow_node *node, void *data,
    const struct sol_flow_node_options *options)
{
    int r;
    struct sml_garden_data *sdata = data;

    sdata->cur_water.val = NAN;

    r = sol_flow_get_packet_type("flower-power", PACKET_TYPE_FLOWER_POWER, &PACKET_TYPE_FLOWER_POWER);
    SOL_INT_CHECK(r, < 0, r);

    return 0;
}

#include "sml_garden_gen.c"
