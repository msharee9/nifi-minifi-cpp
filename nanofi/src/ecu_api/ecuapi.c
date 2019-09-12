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

#include <ecu_api/ecuapi.h>
#include <processors/file_input.h>
#include <processors/site2site_output.h>
#include <core/threadpool.h>
#include <uuid/uuid.h>

void initialize_ecu(const char * name, ecu_context_t * ecu, io_type_t ip, void * ip_ctx, io_type_t op, void * op_ctx) {
    if (!ecu) return;

    if (name && strlen(name) > 0) {
        size_t len = strlen(name);
        ecu->name = (char *)malloc(len + 1);
        strcpy(ecu->name, name);
    }

    UUID_FIELD uuid;
    uuid_generate(uuid);
    uuid_unparse_lower(uuid, ecu->uuid);

    pthread_mutex_init(&ecu->ctx_lock, NULL);
    ecu->input = ip;
    ecu->output = op;
    if (!ip_ctx) {
        switch (ecu->input) {
        case TAILFILE: {
            file_input_context_t * f_ctx = create_file_input_context();
            ecu->input_processor_ctx = (void *)f_ctx;
            break;
        }
        case KAFKA:
        case MQTTIO:
        case SITE2SITE:
            break;
        case MANUAL: {
            manual_input_context_t * m_ctx = create_manual_input_context();
            ecu->input_processor_ctx = (void *)m_ctx;
            break;
        }
        default:
            break;
        }
    } else {
        ecu->input_processor_ctx = ip_ctx;
    }

    if (!op_ctx) {
        switch (ecu->output) {
        case SITE2SITE: {
            site2site_output_context_t * s2s_ctx = create_s2s_output_context();
            ecu->output_processor_ctx = (void *)s2s_ctx;
            break;
        }
        case TAILFILE:
        case KAFKA:
        case MQTTIO:
            break;
        default:
            break;
        }
    } else {
        ecu->output_processor_ctx = op_ctx;
    }
}

ecu_context_t * allocate_ecu() {
    ecu_context_t * ecu_ctx = (ecu_context_t *)malloc(sizeof(struct ecu_context));
    memset(ecu_ctx, 0, sizeof(struct ecu_context));
    return ecu_ctx;
}

ecu_context_t * create_ecu(const char * name, io_type_t input, io_type_t output) {
    ecu_context_t * ecu_ctx = allocate_ecu();
    initialize_ecu(name, ecu_ctx, input, NULL, output, NULL);
    return ecu_ctx;
}

ecu_context_t * create_ecu_iocontext(io_type_t ip, void * ip_ctx, io_type_t op, void * op_ctx) {
    ecu_context_t * ecu = allocate_ecu();
    initialize_ecu(NULL, ecu, ip, ip_ctx, op, op_ctx);
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

    switch (ecu->input) {
    case TAILFILE: {
        if (!ip_ctx) {
            ip_ctx = create_file_input_context();
            ecu->input_processor_ctx = ip_ctx;
        }
        file_input_context_t * ctx = (file_input_context_t *)ip_ctx;
        return set_file_input_property(ctx, name, value);
    }
    break;
    case SITE2SITE:
    case KAFKA:
    case MQTTIO:
        return -1;
    default:
        return -1;
    }
}

int set_ecu_output_property(ecu_context_t * ecu, const char * name, const char * value) {
    if (!ecu || !name || !value) {
        return -1;
    }

    void * op_ctx = ecu->output_processor_ctx;

    switch (ecu->output) {
    case SITE2SITE: {
        if (!op_ctx) {
            op_ctx = create_s2s_output_context();
            ecu->output_processor_ctx = op_ctx;
        }
        site2site_output_context_t * ctx = (site2site_output_context_t *)op_ctx;
        return set_s2s_output_property(ctx, name, value);
    }
    break;
    case TAILFILE:
    case KAFKA:
    case MQTTIO:
        return -1;
    default:
        return -1;
    }
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

    switch (ctx->input) {
    case TAILFILE: {
        file_input_context_t * f_ctx = (file_input_context_t *)(ctx->input_processor_ctx);
        return f_ctx->input_properties;
    }
    case SITE2SITE:
    case KAFKA:
    case MQTTIO:
        return NULL;
    default:
        return NULL;
    }
}

properties_t * get_output_properties(ecu_context_t * ctx) {
    if (!ctx) {
        return NULL;
    }

    switch (ctx->output) {
    case SITE2SITE: {
        site2site_output_context_t * s2s_ctx = (site2site_output_context_t *)ctx->output_processor_ctx;
        return s2s_ctx->output_properties;
    }
    case TAILFILE:
    case KAFKA:
    case MQTTIO:
        return NULL;
    default:
        return NULL;
    }
}

int set_output_property(io_type_t type, void * op_ctx, const char * name, const char * value) {
    if (!name || !value || !op_ctx) {
        return -1;
    }

    switch (type) {
    case SITE2SITE: {
        site2site_output_context_t * ctx = (site2site_output_context_t *)op_ctx;
        return set_s2s_output_property(ctx, name, value);
    }
    break;
    case TAILFILE:
    case KAFKA:
    case MQTTIO:
        return -1;
    default:
        return -1;
    }
}

int set_input_property(io_type_t type, void * ip_ctx, const char * name, const char * value) {
    if (!name || !value || !ip_ctx) {
        return -1;
    }

    switch (type) {
    case TAILFILE: {
        file_input_context_t * ctx = (file_input_context_t *)ip_ctx;
        return set_file_input_property(ctx, name, value);
    }
    break;
    case SITE2SITE:
    case KAFKA:
    case MQTTIO:
        return -1;
    default:
        return -1;
    }
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

    switch (ctx->input) {
    case TAILFILE:
        return validate_file_properties((file_input_context_t *)(ctx->input_processor_ctx));
    case SITE2SITE:
    case KAFKA:
    case MQTTIO:
        return -1;
    default:
        return -1;
    }
}

int validate_output(struct ecu_context * ctx) {
    if (!ctx) {
        return -1;
    }

    switch (ctx->output) {
    case SITE2SITE:
        return validate_s2s_properties((site2site_output_context_t *)(ctx->output_processor_ctx));
    case TAILFILE:
    case KAFKA:
    case MQTTIO:
        return -1;
    default:
        return -1;
    }
}

void * create_input(io_type_t type) {
    switch (type) {
    case TAILFILE: {
        return (void *)create_file_input_context();
    }
    case SITE2SITE:
    case KAFKA:
    case MQTTIO:
        return NULL;
    default:
        return NULL;
    }
}

void * create_output(io_type_t type) {
    switch (type) {
    case SITE2SITE: {
        return (void *)create_s2s_output_context();
    }
    case TAILFILE:
    case KAFKA:
    case MQTTIO:
        return NULL;
    default:
        return NULL;
    }
}

int start_ecu_context(ecu_context_t * ecu_ctx) {
    pthread_mutex_lock(&ecu_ctx->ctx_lock);
    if (ecu_ctx->started) {
        pthread_mutex_unlock(&ecu_ctx->ctx_lock);
        return 0;
    }

    if (validate_input(ecu_ctx) < 0) {
        pthread_mutex_unlock(&ecu_ctx->ctx_lock);
        return -1;
    }

    if (validate_output(ecu_ctx) < 0) {
        pthread_mutex_unlock(&ecu_ctx->ctx_lock);
        return -1;
    }

    if (!ecu_ctx->msg_queue) {
        ecu_ctx->msg_queue = create_msg_queue(1024);
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
        set_attribute_update_cb(file_ctx->msg_queue, &update_msg_attributes);
        start_file_input(file_ctx);
        task_node_t * task = create_repeatable_task(&file_reader_processor, (void *)file_ctx, NULL, file_ctx->tail_frequency * 500);
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
        task_node_t * task = create_repeatable_task(&site2site_writer_processor, (void *)s2s_ctx, NULL, 2000);
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

    threadpool_start(ecu_ctx->thread_pool);
    ecu_ctx->started = 1;
    pthread_mutex_unlock(&ecu_ctx->ctx_lock);
    return 0;
}

void destroy_msg_queue(message_queue_t ** queue) {
    message_queue_t * mq = *queue;
    if (mq) {
        pthread_mutex_destroy(&mq->queue_lock);
        pthread_cond_destroy(&mq->write_notify);
        free(mq);
        *queue = NULL;
    }
}

void wait_input_stop(ecu_context_t * ctx) {
    if (!ctx) return;
    switch (ctx->input) {
    case TAILFILE: {
        file_input_context_t * f_ctx = (file_input_context_t *)(ctx->input_processor_ctx);
        wait_file_input_stop(f_ctx);
    }
    break;
    case SITE2SITE:
    case KAFKA:
    case MQTTIO:
    default:
        break;
    }
}

void wait_output_stop(ecu_context_t * ctx) {
    if (!ctx) return;
    switch (ctx->output) {
    case SITE2SITE: {
        site2site_output_context_t * s2s_ctx = (site2site_output_context_t *)(ctx->output_processor_ctx);
        wait_s2s_output_stop(s2s_ctx);
    }
    break;
    case TAILFILE:
    case KAFKA:
    case MQTTIO:
    default:
        break;
    }
}

int stop_ecu_context(ecu_context_t * ctx) {
    pthread_mutex_lock(&ctx->ctx_lock);
    if (!ctx->started) {
        pthread_mutex_unlock(&ctx->ctx_lock);
        return 0;
    }
    stop_message_queue(ctx->msg_queue);
    wait_input_stop(ctx);
    wait_output_stop(ctx);
    ctx->started = 0;
    pthread_mutex_unlock(&ctx->ctx_lock);
    return 0;
}

void free_ecu_configuration(ecu_context_t * ctx) {
    free_properties(ctx->ecu_configuration);
}

void free_ecu_context(ecu_context_t * ctx) {
    switch (ctx->input) {
    case TAILFILE: {
        file_input_context_t * f_ctx = (file_input_context_t *)(ctx->input_processor_ctx);
        ctx->input_processor_ctx = NULL;
        free_file_input_context(f_ctx);
        break;
    }
    case KAFKA:
    case SITE2SITE:
    case MQTTIO:
        break;
    case MANUAL: {
        manual_input_context_t * m_ctx = (manual_input_context_t *)(ctx->input_processor_ctx);
        ctx->input_processor_ctx = NULL;
        free_manual_input_context(m_ctx);
        break;
    }
    }

    switch (ctx->output) {
    case SITE2SITE: {
        site2site_output_context_t * s2s_ctx = (site2site_output_context_t *)(ctx->output_processor_ctx);
        ctx->output_processor_ctx = NULL;
        free_s2s_output_context(s2s_ctx);
        break;
    }
    case KAFKA:
    case TAILFILE:
    case MQTTIO:
        break;
    }
}

void clear_ecu_input(ecu_context_t * ecu_ctx) {
    switch (ecu_ctx->input) {
    case TAILFILE: {
        file_input_context_t * f_ctx = (file_input_context_t *)(ecu_ctx->input_processor_ctx);
        free_file_input_properties(f_ctx);
        break;
    }
    case KAFKA:
    case SITE2SITE:
    case MQTTIO:
        break;
    }
}

void clear_ecu_output(ecu_context_t * ecu_ctx) {
    switch (ecu_ctx->output) {
    case SITE2SITE: {
        site2site_output_context_t * s2s_ctx = (site2site_output_context_t *)(ecu_ctx->output_processor_ctx);
        free_s2s_output_properties(s2s_ctx);
        break;
    }
    case KAFKA:
    case TAILFILE:
    case MQTTIO:
        break;
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
    pthread_mutex_lock(&ctx->ctx_lock);
    pthread_mutex_destroy(&ctx->ctx_lock);
    free(ctx);
}

int on_start(ecu_context_t * ecu_ctx, io_type_t input, io_type_t output, properties_t * input_props, properties_t * output_props) {
    if (!ecu_ctx || !input_props || !output_props) {
        return -1;
    }

    pthread_mutex_lock(&ecu_ctx->ctx_lock);
    if (ecu_ctx->started) {
        pthread_mutex_unlock(&ecu_ctx->ctx_lock);
        return 0;
    }

    ecu_ctx->input = input;
    if (set_input_properties(ecu_ctx, input_props) < 0) {
        pthread_mutex_unlock(&ecu_ctx->ctx_lock);
        return -1;
    }

    ecu_ctx->output = output;
    if (set_output_properties(ecu_ctx, output_props) < 0) {
        pthread_mutex_unlock(&ecu_ctx->ctx_lock);
        return -1;
    }

    pthread_mutex_unlock(&ecu_ctx->ctx_lock);
    return start_ecu_context(ecu_ctx);
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
    message_t * msg = prepare_message(payload, len, prepare_attributes(attrs));
    add_message(man_ctx, msg);
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
    switch (ecu->input) {
    case TAILFILE: {
        file_input_context_t * f_ctx = (file_input_context_t *)(ecu->input_processor_ctx);
        return clone_properties(f_ctx->input_properties);
    }
    default:
        return NULL;
    }
}

properties_t * get_output_args(ecu_context_t * ecu) {
    switch (ecu->output) {
    case SITE2SITE: {
    	site2site_output_context_t * s2s_ctx = (site2site_output_context_t *)(ecu->output_processor_ctx);
        return clone_properties(s2s_ctx->output_properties);
    }
    default:
        return NULL;
    }
}

void get_io_name(int type, char ** io_name) {
    if (type == TAILFILE) {
        const char * file = "FILE";
        char * name = (char *)malloc(strlen(file) + 1);
        memset(name, 0, strlen(file) + 1);
        strcpy(name, file);
        *io_name = name;
    } else if (type == SITE2SITE) {
        const char * s2s = "SITETOSITE";
        char * name = (char *)malloc(strlen(s2s) + 1);
        memset(name, 0, strlen(s2s) + 1);
        strcpy(name, s2s);
        *io_name = name;
    } else if (type == KAFKA) {
        const char * kfk = "KAFKA";
        char * name = (char *)malloc(strlen(kfk) + 1);
        memset(name, 0, strlen(kfk) + 1);
        strcpy(name, kfk);
        *io_name = name;
    } else if (type == MQTTIO) {
        const char * mqtt = "MQTT";
        char * name = (char *)malloc(strlen(MQTT) + 1);
        memset(name, 0, strlen(MQTT) + 1);
        strcpy(name, mqtt);
        *io_name = name;
    } else {
        *io_name = NULL;
    }
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
    const char * ip4 = "tail_frequency";
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
}
