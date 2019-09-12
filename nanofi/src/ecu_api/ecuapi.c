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

#include <core/threadpool.h>
#include <ecu_api/ecuapi.h>
#include <processors/file_input.h>
#include <processors/site2site_output.h>
#include <core/cuuid.h>
#include <core/log.h>

int validate_io_type(io_type_t ip, io_type_t op) {
    return ip >= TAILFILE && ip <= MANUAL && op >= TAILFILE && op < MQTTIO;
}

int initialize_ecu(ecu_context_t * ecu, const char * name, io_type_t ip, void * ip_ctx, io_type_t op, void * op_ctx) {
    if (!ecu) return -1;

    if (!validate_io_type(ip, op)) {
        logc(err, "Input/Output is out of range. Valid range is %d to %d", TAILFILE, MQTTIO);
        return -1;
    }

    if (name && strlen(name) > 0) {
        size_t len = strlen(name);
        ecu->name = (char *)malloc(len + 1);
        strcpy(ecu->name, name);
    }

    CIDGenerator gen;
    gen.implementation_ = CUUID_DEFAULT_IMPL;
    generate_uuid(&gen, ecu->uuid);
    ecu->uuid[36] = '\0';

    initialize_lock(&ecu->ctx_lock);
    ecu->input = ip;
    ecu->output = op;
    if (!ip_ctx && input_map[ip].create_input_context) {
        ecu->input_processor_ctx = input_map[ip].create_input_context();
    } else {
        ecu->input_processor_ctx = ip_ctx;
    }

    if (!op_ctx && output_map[op].create_output_context) {
        ecu->output_processor_ctx = output_map[op].create_output_context();
    } else {
        ecu->output_processor_ctx = op_ctx;
    }
    return 0;
}

ecu_context_t * allocate_ecu() {
    ecu_context_t * ecu_ctx = (ecu_context_t *)malloc(sizeof(struct ecu_context));
    memset(ecu_ctx, 0, sizeof(struct ecu_context));
    return ecu_ctx;
}

void free_ecu_context(ecu_context_t * ctx) {
    if (!ctx) return;
    if (input_map[ctx->input].free_input_context) {
        void * ip_ctx = ctx->input_processor_ctx;
        ctx->input_processor_ctx = NULL;
        input_map[ctx->input].free_input_context(ip_ctx);
    }

    if (output_map[ctx->output].free_output_context) {
        void * op_ctx = ctx->output_processor_ctx;
        ctx->output_processor_ctx = NULL;
        output_map[ctx->output].free_output_context(op_ctx);
    }
}

ecu_context_t * create_ecu(const char * name, io_type_t input, io_type_t output) {
    ecu_context_t * ecu_ctx = allocate_ecu();
    if (initialize_ecu(ecu_ctx, name, input, NULL, output, NULL) < 0) {
        free(ecu_ctx);
        logc(err, "%s", "ECU initialization failed");
        return NULL;
    }
    logc(info, "Created ecu {uuid: %s}", ecu_ctx->uuid);
    return ecu_ctx;
}

ecu_context_t * create_ecu_iocontext(io_type_t ip, void * ip_ctx, io_type_t op, void * op_ctx) {
    ecu_context_t * ecu = allocate_ecu();
    if (initialize_ecu(ecu, NULL, ip, ip_ctx, op, op_ctx) < 0) {
        free(ecu);
        logc(err, "%s", "ECU initialization failed");
        return NULL;
    }
    logc(info, "Created ecu {uuid: %s}", ecu->uuid);
    return ecu;
}

void free_property(properties_t * prop) {
    if (prop) {
        free(prop->key);
        free(prop->value);
    }
}

void free_properties(properties_t * prop) {
    if (prop) {
        properties_t * el, *tmp = NULL;
        HASH_ITER(hh, prop, el, tmp) {
            HASH_DEL(prop, el);
            free(el->key);
            free(el->value);
            free(el);
        }
    }
}

int add_property(struct properties ** head, const char * name, const char * value) {
    if (!head || !name || !value) {
        return -1;
    }
    properties_t * el = NULL;
    HASH_FIND_STR(*head, name, el);
    if (el) {
        HASH_DEL(*head, el);
        free_property(el);
        free(el);
    }

    properties_t * new_prop = (properties_t *) malloc(sizeof(struct properties));
    size_t name_len = strlen(name);
    size_t value_len = strlen(value);
    new_prop->key = (char *) malloc(name_len + 1);
    memset(new_prop->key, 0, name_len + 1);
    strcpy(new_prop->key, name);

    new_prop->value = (char *) malloc(value_len + 1);
    memset(new_prop->value, 0, value_len + 1);
    strcpy(new_prop->value, value);

    HASH_ADD_KEYPTR(hh, *head, new_prop->key, strlen(new_prop->key), new_prop);
    return 0;
}

int set_ecu_input_property(ecu_context_t * ecu, const char * name, const char * value) {
    if (!ecu || !name || !value) {
        return -1;
    }

    void * ip_ctx = ecu->input_processor_ctx;
    if (!ip_ctx) {
        if (!input_map[ecu->input].create_input_context) {
            return -1;
        }
        ip_ctx = input_map[ecu->input].create_input_context();
        ecu->input_processor_ctx = ip_ctx;
    }
    if (!input_map[ecu->input].set_input_property) {
        return -1;
    }
    return input_map[ecu->input].set_input_property(ip_ctx, name, value);
}

int set_ecu_output_property(ecu_context_t * ecu, const char * name, const char * value) {
    if (!ecu || !name || !value) {
        return -1;
    }

    void * op_ctx = ecu->output_processor_ctx;
    if (!op_ctx) {
        if (!output_map[ecu->output].create_output_context) {
            return -1;
        }
        op_ctx = output_map[ecu->output].create_output_context();
        ecu->output_processor_ctx = op_ctx;
    }
    if (!output_map[ecu->output].set_output_property) {
        return -1;
    }
    return output_map[ecu->output].set_output_property(op_ctx, name, value);
}

int set_input_properties(ecu_context_t * ecu_ctx, properties_t * props) {
    if (!ecu_ctx || !props) {
        return -1;
    }
    properties_t * el, *tmp = NULL;
    HASH_ITER(hh, props, el, tmp) {
        if (set_ecu_input_property(ecu_ctx, el->key, el->value) < 0) {
            return -1;
        }
    }
    return 0;
}

int set_output_properties(ecu_context_t * ecu_ctx, properties_t * props) {
    if (!ecu_ctx || !props) {
        return -1;
    }
    properties_t * el, *tmp = NULL;
    HASH_ITER(hh, props, el, tmp) {
        if (set_ecu_output_property(ecu_ctx, el->key, el->value) < 0) {
            return -1;
        }
    }
    return 0;
}

properties_t * get_input_properties(ecu_context_t * ctx) {
    if (!ctx) {
        return NULL;
    }

    if (!input_map[ctx->input].get_input_properties) {
        return NULL;
    }
    return input_map[ctx->input].get_input_properties(ctx->input_processor_ctx);
}

properties_t * get_output_properties(ecu_context_t * ctx) {
    if (!ctx) {
        return NULL;
    }

    if (!output_map[ctx->output].get_output_properties) {
        return NULL;
    }
    return output_map[ctx->output].get_output_properties(ctx->output_processor_ctx);
}

int set_output_property(io_type_t type, void * op_ctx, const char * name, const char * value) {
    if (!name || !value || !op_ctx) {
        return -1;
    }
    if (!output_map[type].set_output_property) {
        return -1;
    }
    return output_map[type].set_output_property(op_ctx, name, value);
}

int set_input_property(io_type_t type, void * ip_ctx, const char * name, const char * value) {
    if (!name || !value || !ip_ctx) {
        return -1;
    }
    if (!input_map[type].set_input_property) {
        return -1;
    }
    return input_map[type].set_input_property(ip_ctx, name, value);
}

properties_t * clone_properties(properties_t * props) {
    if (!props) {
        return NULL;
    }

    properties_t * clone = NULL;
    properties_t * el, *tmp;
    HASH_ITER(hh, props, el, tmp) {
        properties_t * entry = (properties_t *)malloc(sizeof(properties_t));
        size_t key_len = strlen(el->key);
        size_t val_len = strlen(el->value);
        entry->key = (char *)malloc(key_len + 1);
        entry->value = (char *)malloc(val_len + 1);
        strcpy(entry->key, el->key);
        strcpy(entry->value, el->value);
        HASH_ADD_KEYPTR(hh, clone, entry->key, strlen(entry->key), entry);
    }
    return clone;
}

int validate_input(struct ecu_context * ctx) {
    if (!ctx) {
        return -1;
    }

    if (!input_map[ctx->input].validate_input_properties) {
        return -1;
    }
    if (input_map[ctx->input].validate_input_properties(ctx->input_processor_ctx) < 0) {
        logc(err, "Input properties validation failed for %s", io_type_str[ctx->input]);
        return -1;
    }
    return 0;
}

int validate_output(struct ecu_context * ctx) {
    if (!ctx) {
        return -1;
    }
    if (!output_map[ctx->output].validate_output_properties) {
        return -1;
    }
    if (output_map[ctx->output].validate_output_properties(ctx->output_processor_ctx) < 0) {
        logc(err, "%s", "Output properties validation failed for %s", io_type_str[ctx->output]);
        return -1;
    }
    return 0;
}

void * create_input(io_type_t type) {
    if (!input_map[type].create_input_context) {
        return NULL;
    }
    return input_map[type].create_input_context();
}

void * create_output(io_type_t type) {
    if (!output_map[type].create_output_context) {
        return NULL;
    }
    return output_map[type].create_output_context();
}

int start_ecu_context(ecu_context_t * ecu_ctx) {
    acquire_lock(&ecu_ctx->ctx_lock);
    if (ecu_ctx->started) {
        logc(info, "ECU is already started, {uuid: %s}", ecu_ctx->uuid);
        release_lock(&ecu_ctx->ctx_lock);
        return 0;
    }

    if (validate_input(ecu_ctx) < 0) {
        logc(err, "Input validation failed for ecu, {uuid: %s}", ecu_ctx->uuid);
        release_lock(&ecu_ctx->ctx_lock);
        return -1;
    }

    if (validate_output(ecu_ctx) < 0) {
        logc(err, "Output validation failed for ecu {uuid: %s}", ecu_ctx->uuid);
        release_lock(&ecu_ctx->ctx_lock);
        return -1;
    }

    if (!ecu_ctx->msg_queue) {
        ecu_ctx->msg_queue = create_msg_queue(4096);
    }
    start_message_queue(ecu_ctx->msg_queue);

    if (!ecu_ctx->thread_pool) {
        ecu_ctx->thread_pool = threadpool_create(3);
    }

    switch (ecu_ctx->input) {
    case TAILFILE: {
        if (!ecu_ctx->input_processor_ctx) {
            ecu_ctx->input_processor_ctx = (void *)create_file_input_context();
        }
        file_input_context_t * file_ctx = (file_input_context_t *)(ecu_ctx->input_processor_ctx);
        file_ctx->msg_queue = ecu_ctx->msg_queue;
        set_attribute_update_cb(file_ctx->msg_queue, &get_updated_attributes);
        start_file_input(file_ctx);
        task_node_t * task = create_repeatable_task(&file_reader_processor, (void *)file_ctx, NULL, file_ctx->tail_frequency_ms);
        threadpool_add(ecu_ctx->thread_pool, task);
        break;
    }
    case SITE2SITE:
    case KAFKA:
    case MQTTIO:
        break;
    default:
        break;
    }

    switch (ecu_ctx->output) {
    case SITE2SITE: {
        if (!ecu_ctx->output_processor_ctx) {
            ecu_ctx->output_processor_ctx = (void *)create_s2s_output_context();
        }
        site2site_output_context_t * s2s_ctx = (site2site_output_context_t *)(ecu_ctx->output_processor_ctx);
        s2s_ctx->msg_queue = ecu_ctx->msg_queue;
        start_s2s_output(s2s_ctx);
        task_node_t * task = create_repeatable_task(&site2site_writer_processor, (void *)s2s_ctx, NULL, 100);
        threadpool_add(ecu_ctx->thread_pool, task);
        break;
    }
    case TAILFILE:
    case KAFKA:
    case MQTTIO:
        break;
    default:
        break;
    }

    if (threadpool_start(ecu_ctx->thread_pool) < 0) {
        logc(err, "Threadpool could not be started. failed to start ecu {uuid: %s}", ecu_ctx->uuid);
        release_lock(&ecu_ctx->ctx_lock);
        return -1;
    }
    ecu_ctx->started = 1;
    release_lock(&ecu_ctx->ctx_lock);
    logc(info, "ECU started, {uuid: %s}", ecu_ctx->uuid);
    return 0;
}

void destroy_msg_queue(message_queue_t ** queue) {
    message_queue_t * mq = *queue;
    if (mq) {
        destroy_lock(&mq->queue_lock);
        destroy_cv(&mq->write_notify);
        free(mq);
        *queue = NULL;
    }
}

void wait_input_stop(ecu_context_t * ctx) {
    if (!ctx) return;
    if (input_map[ctx->input].wait_input_stop) {
        input_map[ctx->input].wait_input_stop(ctx->input_processor_ctx);
    }
}

void wait_output_stop(ecu_context_t * ctx) {
    if (!ctx) return;
    if (output_map[ctx->output].wait_output_stop) {
        output_map[ctx->output].wait_output_stop(ctx->output_processor_ctx);
    }
}

int stop_ecu_context(ecu_context_t * ctx) {
    acquire_lock(&ctx->ctx_lock);
    if (!ctx->started) {
        logc(info, "Stopping an already stopped ecu, {uuid: %s}", ctx->uuid);
        release_lock(&ctx->ctx_lock);
        return 0;
    }
    stop_message_queue(ctx->msg_queue);
    wait_input_stop(ctx);
    wait_output_stop(ctx);
    ctx->started = 0;
    release_lock(&ctx->ctx_lock);
    logc(info, "ECU stopped {uuid: %s}", ctx->uuid);
    return 0;
}

void free_ecu_configuration(ecu_context_t * ctx) {
    free_properties(ctx->ecu_configuration);
}

void clear_ecu_input(ecu_context_t * ecu_ctx) {
    if (!ecu_ctx) return;
    if (input_map[ecu_ctx->input].free_input_properties) {
        input_map[ecu_ctx->input].free_input_properties(ecu_ctx->input_processor_ctx);
    }
}

void clear_ecu_output(ecu_context_t * ecu_ctx) {
    if (!ecu_ctx) return;
    if (output_map[ecu_ctx->output].free_output_properties) {
        output_map[ecu_ctx->output].free_output_properties(ecu_ctx->output_processor_ctx);
    }
}

void destroy_ecu(ecu_context_t * ctx) {
    stop_ecu_context(ctx);
    threadpool_shutdown(ctx->thread_pool);
    free_queue(ctx->msg_queue);
    free_ecu_configuration(ctx);
    free_ecu_context(ctx);
    free(ctx->name);
    free(ctx->thread_pool);
    acquire_lock(&ctx->ctx_lock);
    destroy_lock(&ctx->ctx_lock);
    free(ctx);
    logc(info, "%s", "ECU destroyed");
}

int on_start(ecu_context_t * ecu_ctx, io_type_t input, io_type_t output, properties_t * input_props, properties_t * output_props) {
    if (!ecu_ctx || !input_props || !output_props) {
        return -1;
    }

    acquire_lock(&ecu_ctx->ctx_lock);
    if (ecu_ctx->started) {
        logc(info, "ECU is already started, {uuid: %s}", ecu_ctx->uuid);
        release_lock(&ecu_ctx->ctx_lock);
        return 0;
    }

    ecu_ctx->input = input;
    if (set_input_properties(ecu_ctx, input_props) < 0) {
        free_ecu_context(ecu_ctx);
        logc(err, "Could not start ecu, setting input properties failed {uuid: %s}", ecu_ctx->uuid);
        release_lock(&ecu_ctx->ctx_lock);
        return -1;
    }

    ecu_ctx->output = output;
    if (set_output_properties(ecu_ctx, output_props) < 0) {
        free_ecu_context(ecu_ctx);
        logc(err, "Could not start ecu, setting output properties failed {uuid: %s}", ecu_ctx->uuid);
        release_lock(&ecu_ctx->ctx_lock);
        return -1;
    }

    release_lock(&ecu_ctx->ctx_lock);
    if (start_ecu_context(ecu_ctx) < 0) {
        free_ecu_context(ecu_ctx);
        return -1;
    }
    return 0;
}

int on_stop(ecu_context_t * ecu_ctx) {
    return stop_ecu_context(ecu_ctx);
}

int on_clear(ecu_context_t * ecu_ctx) {
    if (on_stop(ecu_ctx) < 0) {
        return -1;
    }

    clear_ecu_input(ecu_ctx);
    clear_ecu_output(ecu_ctx);
    return 0;
}

int on_update(ecu_context_t * ecu_ctx, io_type_t input, io_type_t output, properties_t * input_props, properties_t * output_props) {
    if (on_clear(ecu_ctx) < 0) {
        return -1;
    }

    free_ecu_context(ecu_ctx);

    if (on_start(ecu_ctx, input, output, input_props, output_props) < 0) {
        return -1;
    }
    return 0;
}

void add_message(manual_input_context_t * ctx, message_t * msg) {
    if (!ctx) return;
    LL_APPEND(ctx->message, msg);
}

manual_input_context_t * create_manual_input_context() {
    manual_input_context_t * ctx = (manual_input_context_t *)malloc(sizeof(manual_input_context_t));
    memset(ctx, 0, sizeof(manual_input_context_t));
    return ctx;
}

void ingest_input_data(ecu_context_t * ctx, const char * payload, size_t len, properties_t * attrs) {
    if (ctx->input != MANUAL) {
        return;
    }
    manual_input_context_t * man_ctx = (manual_input_context_t *)(ctx->input_processor_ctx);
    const message_t * msg = prepare_message(payload, len, prepare_attributes(attrs));
    add_message(man_ctx, (message_t *)msg);
}

void ecu_push_output(ecu_context_t * ctx) {
    message_t * msgs = NULL;
    switch (ctx->input) {
    case MANUAL: {
        manual_input_context_t * man_ctx = (manual_input_context_t *)(ctx->input_processor_ctx);
        msgs = man_ctx->message;
        man_ctx->message = NULL;
        break;
    }
    default:
        break;
    }

    if (validate_output(ctx) < 0) {
        return;
    }

    switch (ctx->output) {
    case SITE2SITE: {
        site2site_output_context_t * s2s_ctx = (site2site_output_context_t *)(ctx->output_processor_ctx);
        write_to_s2s(s2s_ctx, msgs);
        break;
    }
    case TAILFILE:
    case KAFKA:
    case MQTTIO:
    default:
        break;
    }
}

void free_manual_input_context(manual_input_context_t * ctx) {
    message_t * msgs = ctx->message;
    free_message(msgs);
    free(ctx);
}

void ingest_and_push_out(ecu_context_t * ctx, const char * payload, size_t len, properties_t * attrs) {
    ingest_input_data(ctx, payload, len, attrs);
    ecu_push_output(ctx);
}

properties_t * get_input_args(ecu_context_t * ecu) {
    if (!ecu || !input_map[ecu->input].clone_input_properties) return NULL;
    return input_map[ecu->input].clone_input_properties(ecu->input_processor_ctx);
}

properties_t * get_output_args(ecu_context_t * ecu) {
    if (!ecu || !output_map[ecu->output].clone_output_properties) return NULL;
    return output_map[ecu->output].clone_output_properties(ecu->output_processor_ctx);
}

void get_io_name(int type, char ** io_name) {
    if (type < TAILFILE || type > MANUAL) {
        *io_name = NULL;
        return;
    }
    const char * io_str = io_type_str[type];
    size_t len = strlen(io_str);
    char * name = (char *)malloc((len + 1) * sizeof(char));
    memset(name, 0, len + 1);
    strcpy(name, io_str);
    *io_name = name;
}

void get_input_name(ecu_context_t * ecu, char ** input) {
    if (!ecu) return;
    get_io_name(ecu->input, input);
}

void get_output_name(ecu_context_t * ecu, char ** output) {
    if (!ecu) return;
    get_io_name(ecu->output, output);
}

io_type_t get_io_type(const char * name) {
    if (!name) return -1;

    if (strcasecmp(name, "FILE") == 0) {
        return TAILFILE;
    }

    if (strcasecmp(name, "MQTT") == 0) {
        return MQTTIO;
    }

    if (strcasecmp(name, "KAFKA") == 0) {
        return KAFKA;
    }

    if (strcasecmp(name, "SITETOSITE") == 0) {
        return SITE2SITE;
    }
    return -1;
}

io_manifest get_io_manifest() {
    io_manifest io_mnfst;
    memset(&io_mnfst, 0, sizeof(io_mnfst));
    io_mnfst.num_ips = 2;
    io_mnfst.input_descrs = file_input_desc;

    io_mnfst.num_ops = 1;
    io_mnfst.output_descrs = sitetosite_output_desc;
    return io_mnfst;
}

/*
io_manifest get_io_manifest() {
	io_manifest io_mnfst;

    const char * file = "FILE";
    size_t fl = strlen(file);
    io_mnfst.num_ips = 1;
    io_mnfst.input_params = (ioparams *)malloc(io_mnfst.num_ips * sizeof(ioparams));
    io_mnfst.input_params[0].name = (char *)malloc(fl + 1);
    strcpy(io_mnfst.input_params[0].name, file);
    io_mnfst.input_params[0].num_params = 4;
    io_mnfst.input_params[0].params = (char **)malloc(io_mnfst.input_params[0].num_params * sizeof(char *));
    const char * ip1 = "file_path";
    const char * ip2 = "chunk_size";
    const char * ip3 = "delimiter";
    const char * ip4 = "tail_frequency_ms";
    io_mnfst.input_params[0].params[0] = (char *)malloc(strlen(ip1) + 1);
    io_mnfst.input_params[0].params[1] = (char *)malloc(strlen(ip2) + 1);
    io_mnfst.input_params[0].params[2] = (char *)malloc(strlen(ip3) + 1);
    io_mnfst.input_params[0].params[3] = (char *)malloc(strlen(ip4) + 1);
    strcpy(io_mnfst.input_params[0].params[0], ip1);
    strcpy(io_mnfst.input_params[0].params[1], ip2);
    strcpy(io_mnfst.input_params[0].params[2], ip3);
    strcpy(io_mnfst.input_params[0].params[3], ip4);

    const char * s2s = "SITETOSITE";
    size_t sl = strlen(s2s);
    io_mnfst.num_ops = 1;
    io_mnfst.output_params = (ioparams *)malloc(io_mnfst.num_ops * sizeof(ioparams));
    io_mnfst.output_params[0].name = (char *)malloc(sl + 1);
    strcpy(io_mnfst.output_params[0].name, s2s);
    io_mnfst.output_params[0].num_params = 3;
    io_mnfst.output_params[0].params = (char **)malloc(io_mnfst.output_params[0].num_params * sizeof(char *));
    const char * op1 = "host_name";
    const char * op2 = "tcp_port";
    const char * op3 = "nifi_input_port_uuid";
    io_mnfst.output_params[0].params[0] = (char *)malloc(strlen(op1) + 1);
    io_mnfst.output_params[0].params[1] = (char *)malloc(strlen(op2) + 1);
    io_mnfst.output_params[0].params[2] = (char *)malloc(strlen(op3) + 1);
    strcpy(io_mnfst.output_params[0].params[0], op1);
    strcpy(io_mnfst.output_params[0].params[1], op2);
    strcpy(io_mnfst.output_params[0].params[2], op3);

    return io_mnfst;
}*/
