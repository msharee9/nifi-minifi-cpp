/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NIFI_MINIFI_CPP_C2PROTOCOL_H
#define NIFI_MINIFI_CPP_C2PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "c2structs.h"
#include "coap/coapprotocol.h"

size_t encode_heartbeat(const struct c2heartbeat * heartbeat, char ** buff, size_t * length);
int decode_heartbeat(const unsigned char * payload, size_t length, struct c2heartbeat * heartbeat, size_t * bytes_decoded);

struct encoded_data encode_deviceinfo(const struct deviceinfo * dev_info);
struct encoded_data encode_agentinfo(const struct agentinfo * agent_info);

int decode_deviceinfo(const unsigned char * payload, size_t length, struct deviceinfo * dev_info, size_t * decoded_bytes);
int decode_agentinfo(const unsigned char * payload, size_t length, struct agentinfo  * agent_info, size_t * decoded_bytes);
int decode_agentmanifest(const unsigned char * payload, size_t length, struct agent_manifest * ag_manifest, size_t * bytes_decoded);

void free_c2heartbeat(c2heartbeat_t * c2_heartbeat);

void encode_c2_response(const c2_response_t * response, char ** buff, size_t * length);

void encode_c2_server_response(c2_server_response_t * response, char ** buff, size_t * length);
c2_server_response_t * decode_c2_server_response(const struct coap_message * msg, int is_little_endian);
void print_heartbeat(c2heartbeat_t * heartbeat);
void reallocate_buffer(unsigned char ** buffer, size_t sub_length, size_t copy_bytes, size_t * total_len);

#ifdef __cplusplus
}
#endif
#endif //NIFI_MINIFI_CPP_C2PROTOCOL_H
