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

#pragma once

#include "sol-flow.h"
#include "sol-flow-packet.h"
#include "sol-types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct packet_type_sml_data_packet_data {
    uint16_t inputs_len, outputs_len, output_ids_len, input_ids_len;
    struct sol_drange *inputs;
    struct sol_drange *outputs;
    bool *output_ids, *input_ids;
};

struct packet_type_sml_output_data_packet_data {
    uint16_t outputs_len;
    struct sol_drange *outputs;
};

extern const struct sol_flow_packet_type *PACKET_TYPE_SML_DATA;
extern const struct sol_flow_packet_type *PACKET_TYPE_SML_OUTPUT_DATA;

struct sol_flow_packet *sml_data_new_packet(struct packet_type_sml_data_packet_data *sml_data);
int sml_data_send_packet(struct sol_flow_node *src, uint16_t src_port, struct packet_type_sml_data_packet_data *sml_data);
int sml_data_get_packet(const struct sol_flow_packet *packet, struct packet_type_sml_data_packet_data *sml_data);

struct sol_flow_packet *sml_output_data_new_packet(struct packet_type_sml_output_data_packet_data *sml_output_data);
int sml_output_data_get_packet(const struct sol_flow_packet *packet, struct packet_type_sml_output_data_packet_data *sml_output_data);
int sml_output_data_send_packet(struct sol_flow_node *src, uint16_t src_port, struct packet_type_sml_output_data_packet_data *sml_output_data);

#ifdef __cplusplus
}
#endif
