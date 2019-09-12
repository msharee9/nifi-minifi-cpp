/*
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

#ifndef NIFI_MINIFI_CPP_ECUAPI_H
#define NIFI_MINIFI_CPP_ECUAPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "uthash.h"
#include <core/threadpool.h>
#include <core/message_queue.h>
#include <coap/c2structs.h>

typedef enum io_type {
    SITE2SITE,
    TAILFILE,
    KAFKA,
    MQTTIO,
    MANUAL
} io_type_t;

typedef struct ecu_context {
    char * name;
    char uuid[37];
    io_type_t input;
    io_type_t output;
    //configuration parameters from configuration file
    //contains, c2 host, c2 port, c2 heartbeat uri
    //c2 acknowledge uri, heartbeat interval
    properties_t * ecu_configuration;
    void * input_processor_ctx;
    void * output_processor_ctx;
    threadpool_t * thread_pool;
    message_queue_t * msg_queue;
    int started;
    pthread_mutex_t ctx_lock;
} ecu_context_t;

typedef int (*on_start_callback_t)(ecu_context_t * ecu_ctx, io_type_t input, io_type_t output, properties_t * input_props, properties_t * output_props);
typedef int (*on_stop_callback_t)(ecu_context_t * ecu_ctx);

typedef struct manual_input_context {
    message_t * message;
} manual_input_context_t;

ecu_context_t * create_ecu(const char * name, io_type_t ip, io_type_t op);
void set_input(io_type_t type);
void set_output(io_type_t type);
int set_ecu_input_property(ecu_context_t * ctx, const char * name, const char * value);
int set_ecu_output_property(ecu_context_t * ctx, const char * name, const char * value);
int start_ecu_context(ecu_context_t * ctx);
int stop_ecu_context(ecu_context_t * ctx);
void destroy_ecu(ecu_context_t * ctx);

int on_start(ecu_context_t * ecu_ctx, io_type_t input, io_type_t output, properties_t * input_props, properties_t * output_props);
int on_update(ecu_context_t * ecu_ctx, io_type_t input, io_type_t output, properties_t * input_props, properties_t * output_props);
int on_stop(ecu_context_t * ecu_ctx);
int on_clear(ecu_context_t * ecu_ctx);

void free_properties(properties_t * prop);
properties_t * get_input_properties(ecu_context_t * ctx);
properties_t * get_output_properties(ecu_context_t * ctx);
properties_t * clone_properties(properties_t * props);

void * create_input(io_type_t type);
void * create_output(io_type_t type);
ecu_context_t * create_ecu_iocontext(io_type_t ip, void * ip_ctx, io_type_t op, void * op_ctx);
int set_output_property(io_type_t type, void * op_ctx, const char * name, const char * value);
int set_input_property(io_type_t type, void * ip_ctx, const char * name, const char * value);
int add_property(struct properties ** head, const char * name, const char * value);

manual_input_context_t * create_manual_input_context();
void ingest_input_data(ecu_context_t * ctx, const char * payload, size_t len, properties_t * attrs);
void ecu_push_output(ecu_context_t * ctx);
void free_manual_input_context(manual_input_context_t * ctx);
void ingest_and_push_out(ecu_context_t * ctx, const char * payload, size_t len, properties_t * attrs);

void get_input_name(ecu_context_t * ecu, char ** input);
void get_output_name(ecu_context_t * ecu, char ** output);
io_type_t get_io_type(const char * name);
properties_t * get_input_args(ecu_context_t * ecu);
properties_t * get_output_args(ecu_context_t * ecu);

io_manifest get_io_manifest();

#ifdef __cplusplus
}
#endif
#endif //NIFI_MINIFI_CPP_ECUAPI_H
