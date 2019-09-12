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

#ifndef NIFI_MINIFI_CPP_C2STRUCTS_H
#define NIFI_MINIFI_CPP_C2STRUCTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <limits.h>
#include <stddef.h>
#include <uthash.h>
#include <pthread.h>
#include <core/cstructs.h>

typedef struct systeminfo {
    char machine_arch[256];
    uint64_t physical_mem;
    uint16_t v_cores;
} systeminfo;

typedef struct networkinfo {
    char * device_id;
    char host_name[_POSIX_HOST_NAME_MAX + 1];
    char ip_address[46];
} networkinfo;

typedef struct buildinfo {
    const char * version;
    const char * revision;
    uint64_t timestamp;
    const char * target_arch;
    const char * compiler;
    const char * compiler_flags;
} buildinfo;

typedef struct agentmanifest {
    char * ident;
    const char * agent_type;
    const char * version;
    struct buildinfo build_info;
} agentmanifest;

typedef struct agentinfo {
    char ident[37]; //uuid of the agent
    char * agent_class;
    uint64_t uptime;
} agentinfo;

typedef struct deviceinfo {
    char * ident;
    struct systeminfo system_info;
    struct networkinfo network_info;
} deviceinfo;

typedef struct {
    char ** inputs;
    size_t ip_len;
    char ** outputs;
    size_t op_len;
} io_capabilities;

typedef struct {
    char * name;
    size_t num_params;
    char ** params;
} ioparams;

typedef struct {
    size_t num_ips;
    ioparams * input_params;
    size_t num_ops;
    ioparams * output_params;
} io_manifest;

typedef struct {
    char uuid[37];
    const char * name;
    const char * input;
    properties_t * ip_args;
    const char * output;
    properties_t * op_args;
} ecuinfo;

typedef struct agent_manifest {
    char manifest_id[37];
    char agent_type[7]; //"nanofi"
    char version[6]; //"0.0.1"
    size_t num_ecus;
    ecuinfo * ecus;
    //io_capabilities io_caps;
    io_manifest io;
} agent_manifest;

typedef struct c2heartbeat {
    int is_error;
    deviceinfo device_info;
    agentinfo agent_info;
    int has_ag_manifest;
    agent_manifest ag_manifest;
} c2heartbeat_t;

typedef struct encoded_data {
    unsigned char * buff;
    size_t length;
    size_t written;
    struct encoded_data * next;
} encoded_data;

typedef enum c2operation {
    ACKNOWLEDGE,
    HEARTBEAT,
    CLEAR,
    DESCRIBE,
    RESTART,
    START,
    UPDATE,
    STOP
} c2operation;

//This is the c2 heartbeat response sent by c2 server
//to the nanofi agent in response to heartbeat
typedef struct c2_server_response {
    char * ident;
    c2operation operation;
    char * operand;
    properties_t * args;
    struct c2_server_response * next;
} c2_server_response_t;

//This is the acknowledgment sent by nanofi agent to the
//c2 server in response to c2 requested operation
typedef struct c2_response {
    char * ident;
    c2operation operation;
    struct c2_response * next;
} c2_response_t;

typedef struct c2_message_ctx {
    //responses to heartbeat from c2 server
    c2_server_response_t * c2_serv_resps;
    pthread_mutex_t serv_resp_lock;
    pthread_cond_t serv_resp_cond;
    pthread_condattr_t serv_resp_cond_attr;

    //responses to c2 server
    c2_response_t * c2_resps;
    pthread_mutex_t resp_lock;
    pthread_cond_t resp_cond;
    pthread_condattr_t resp_cond_attr;
} c2_message_ctx_t;

#ifdef __cplusplus
}
#endif

#endif //NIFI_MINIFI_CPP_C2STRUCTS_H
