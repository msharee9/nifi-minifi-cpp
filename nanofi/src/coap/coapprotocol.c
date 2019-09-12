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

#include <string.h>

#include "coap/c2structs.h"
#include "coap/coapprotocol.h"

#include <core/synchutils.h>
#include <core/log.h>
#include <nanofi/coap_connection.h>
#include <nanofi/coap_functions.h>
#include <nanofi/dtls_config.h>

void insert_coap_message(c2context_t * c2, const struct coap_context_t * ctx, const struct coap_message * message) {
    if (!ctx || !message) {
        return;
    }
    acquire_lock(&c2->coap_msgs_lock);
    struct coap_messages * cm = NULL;
    HASH_FIND_PTR(c2->messages, &ctx, cm);
    if (cm) {
        //erase the existing message in the table
        free(cm->coap_msg.data);
        HASH_DEL(c2->messages, cm);
        struct coap_messages * tmp = cm;
        free(tmp);
    }
    cm = (struct coap_messages *)malloc(sizeof(struct coap_messages));
    cm->ctx = (void *)ctx;
    cm->coap_msg.code = message->code;
    cm->coap_msg.length = message->length;
    cm->coap_msg.data = message->data;
    HASH_ADD_PTR(c2->messages, ctx, cm);
    release_lock(&c2->coap_msgs_lock);
}

struct coap_message * get_coap_message(c2context_t * c2, const struct coap_context_t * ctx) {
    if (!ctx) {
        return NULL;
    }
    acquire_lock(&c2->coap_msgs_lock);
    struct coap_messages * cm = NULL;
    HASH_FIND_PTR(c2->messages, &ctx, cm);
    struct coap_message * msg = NULL;
    if (cm) {
        if (cm->coap_msg.length) {
            msg = (struct coap_message *) malloc(sizeof(struct coap_message));
            memset(msg, 0, sizeof(struct coap_message));
            msg->length = cm->coap_msg.length;
            msg->data = (uint8_t *) malloc(msg->length * sizeof(uint8_t));
            memcpy(msg->data, cm->coap_msg.data, msg->length);
            msg->code = cm->coap_msg.code;

            free(cm->coap_msg.data);
            HASH_DEL(c2->messages, cm);
            free(cm);
        }
    }
    release_lock(&c2->coap_msgs_lock);
    return msg;
}

void receive_error(void * receiver_context, coap_context_t * ctx, unsigned char code) {
    const char * error = coap_response_phrase(code);
    logc(err, "%s", error ? error : "Unknown");
}

void receive_message(void * receiver, struct coap_context_t * ctx, CoapMessage * const msg) {
    c2context_t * c2 = (c2context_t *)receiver;
    struct coap_message coap_msg;
    coap_msg.data = (char *)malloc(msg->size_ * sizeof(char));
    memcpy(coap_msg.data, msg->data_, msg->size_);
    coap_msg.code = msg->code_;
    coap_msg.length = msg->size_;
    free_coap_message(msg);
    insert_coap_message(c2, ctx, &coap_msg);
}

void initialize_coap(c2context_t * c2_ctx) {
    callback_pointers cbs;
    cbs.data_received = receive_message;
    cbs.received_error = receive_error;
    initialize_lock(&c2_ctx->coap_msgs_lock);
    init_coap_api((void *)c2_ctx, &cbs);
}

int validate_ssl_file_paths(dtls_config * ssl) {
    int ret = is_file_exists(ssl->public_cert)
            && is_file_exists(ssl->private_key)
            && is_file_exists(ssl->ca_cert);
    if (ssl->root_cas) {
        ret &= (is_file_exists(ssl->root_cas) || is_directory(ssl->root_cas));
    }
    return ret;
}

int validate_ssl_configuration(const properties_t * c2_props, dtls_config * dc) {
    properties_t * el;
    memset(dc, 0, sizeof(dtls_config));
    HASH_FIND_STR(c2_props, c2_ssl_certificate, el);
    if (!el) {
        logc(err, "%s", "Public certificate not configured");
        return -1;
    }
    copystr(el->value, &dc->public_cert);
    HASH_FIND_STR(c2_props, c2_ssl_ca_certificate, el);
    if (!el) {
        logc(err, "%s", "CA certificate not configured");
        return -1;
    }
    copystr(el->value, &dc->ca_cert);
    HASH_FIND_STR(c2_props, c2_ssl_private_key, el);
    if (!el) {
        logc(err, "%s", "Private key no configured");
        return -1;
    }
    copystr(el->value, &dc->private_key);
    HASH_FIND_STR(c2_props, c2_ssl_root_ca_certificate, el);
    if (el) {
        copystr(el->value, &dc->root_cas);
    }
    return validate_ssl_file_paths(dc);
}

struct coap_message * send_payload(c2context_t * c2, const char * endpoint, const CoapMessage * const message) {
    const char * host = c2->config.host;

    CoapPDU * pdu = create_secure_connection(COAP_REQUEST_POST, host, endpoint, (uint16_t)c2->config.port, message, &c2->config.ssl);
    if (!pdu) {
        logc(err, "%s", "Failed to create Coap PDU");
        return NULL;
    }

    if (send_pdu(pdu) < 0) {
        free_pdu(pdu);
        return NULL;
    }
    struct coap_message * response = get_coap_message(c2, pdu->ctx);
    free_pdu(pdu);
    return response;
}
