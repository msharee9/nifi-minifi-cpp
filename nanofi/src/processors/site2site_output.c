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

#include <errno.h>
#include <stdio.h>
#include <processors/site2site_output.h>

void initialize_s2s_output(site2site_output_context_t * ctx) {
    pthread_mutex_init(&ctx->client_mutex, NULL);
    pthread_mutex_init(&ctx->stop_mutex, NULL);
    pthread_cond_init(&ctx->stop_cond, NULL);
}

void start_s2s_output(site2site_output_context_t * ctx) {
    pthread_mutex_lock(&ctx->stop_mutex);
    ctx->stop = 0;
    pthread_mutex_unlock(&ctx->stop_mutex);
}

void free_s2s_output_context(site2site_output_context_t * ctx) {
    free_properties(ctx->output_properties);
    free(ctx->host_name);
    if (ctx->client) {
        destroyClient(ctx->client);
    }
    pthread_mutex_destroy(&ctx->client_mutex);
    pthread_mutex_destroy(&ctx->stop_mutex);
    pthread_cond_destroy(&ctx->stop_cond);
    free(ctx);
}

void write_to_s2s(site2site_output_context_t * s2s_ctx, message_t * msgs) {
    message_t * head = msgs;
    while (head) {
        if (head->len) {
            char * payload = (char *)malloc(head->len + 1);
            memcpy(payload, head->buff, head->len);
            payload[head->len] = '\0';
            message_t * tmp = head;
            pthread_mutex_lock(&s2s_ctx->client_mutex);
            transmitPayload(s2s_ctx->client, payload, &head->as);
            free(payload);
            pthread_mutex_unlock(&s2s_ctx->client_mutex);
            head = head->next;
            tmp->next = NULL;
            free_message(tmp);
        }
    }
}

task_state_t site2site_writer_processor(void * args, void * state) {
    site2site_output_context_t * s2s_ctx = (site2site_output_context_t *)args;
    pthread_mutex_lock(&s2s_ctx->msg_queue->queue_lock);
    if (s2s_ctx->msg_queue->stop) {
        //drain messages
        pthread_mutex_lock(&s2s_ctx->stop_mutex);
        message_t * msg;
        while ((msg = dequeue_message_nolock(s2s_ctx->msg_queue)) != NULL) {
            write_to_s2s(s2s_ctx, msg);
        }
        s2s_ctx->stop = 1;
        pthread_cond_broadcast(&s2s_ctx->stop_cond);
        pthread_mutex_unlock(&s2s_ctx->stop_mutex);
        pthread_mutex_unlock(&s2s_ctx->msg_queue->queue_lock);
        return DONOT_RUN_AGAIN;
    }
    pthread_mutex_unlock(&s2s_ctx->msg_queue->queue_lock);

    message_t * msg = dequeue_message(s2s_ctx->msg_queue);
    if (msg) {
        write_to_s2s(s2s_ctx, msg);
    }
    return RUN_AGAIN;
}

int validate_s2s_properties(site2site_output_context_t * ctx) {
    if (!ctx) {
        return -1;
    }
    properties_t * props = ctx->output_properties;
    properties_t * tcp_el;
    HASH_FIND_STR(props, "tcp_port", tcp_el);
    if (!tcp_el) {
        return -1;
    }
    if (!tcp_el->value) {
        return -1;
    }
    uint64_t tcp_port = (uint64_t)strtoul(tcp_el->value, NULL, 10);
    if (errno != 0) {
        return -1;
    }
    ctx->tcp_port = tcp_port;

    properties_t * nifi_el = NULL;
    HASH_FIND_STR(props, "nifi_port_uuid", nifi_el);
    if (!nifi_el) {
        return -1;
    }
    if (!nifi_el->value) {
        return -1;
    }
    strcpy(ctx->port_uuid, nifi_el->value);

    properties_t * host_el = NULL;
    HASH_FIND_STR(props, "host_name", host_el);
    if (!host_el) {
        return -1;
    }

    if (!host_el->value) {
        return -1;
    }

    size_t hlen = strlen(host_el->value);
    char * host_name = (char *)malloc(hlen + 1);
    strcpy(host_name, host_el->value);
    char * hn = ctx->host_name;
    if (hn) free(hn);
    ctx->host_name = host_name;

    if (ctx->client) {
        struct CRawSiteToSiteClient * cl = ctx->client;
        if (cl) {
            destroyClient(cl);
        }
    }
    ctx->client = createClient(ctx->host_name, (uint16_t)tcp_port, ctx->port_uuid);
    return 0;
}

site2site_output_context_t * create_s2s_output_context() {
    site2site_output_context_t * ctx = (site2site_output_context_t *)malloc(sizeof(site2site_output_context_t));
    memset(ctx, 0, sizeof(site2site_output_context_t));
    initialize_s2s_output(ctx);
    return ctx;
}

int set_s2s_output_property(site2site_output_context_t * ctx, const char * name, const char * value) {
    return add_property(&ctx->output_properties, name, value);
}

void free_s2s_output_properties(site2site_output_context_t * ctx) {
    properties_t * props = ctx->output_properties;
    ctx->output_properties = NULL;
    free_properties(props);
}

void wait_s2s_output_stop(site2site_output_context_t * ctx) {
    pthread_mutex_lock(&ctx->stop_mutex);
    while (!ctx->stop) {
        pthread_cond_wait(&ctx->stop_cond, &ctx->stop_mutex);
    }
    pthread_mutex_unlock(&ctx->stop_mutex);
}
