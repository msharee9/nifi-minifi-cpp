#ifdef __cplusplus
extern "C" {
#endif
#include <coap/c2structs.h>
#include <coap/c2protocol.h>
#include <coap/c2agent.h>
#include <c2_api/c2api.h>
#include <api/ecu.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "utlist.h"
#include "uthash.h"

typedef struct config_params {
    char * key; // name of the param
    char * value; // value of the param
    UT_hash_handle hh;
} config_params;

char ** parse_line(char * line, size_t len) {
    char * tok = strtok(line, " =");
    char ** tokens = (char **)malloc(sizeof(char *) * 2);
    int i = 0;
    while (tok) {
        if (i > 2) break;
        size_t s = strlen(tok);
        char * token = (char *)malloc(sizeof(char) * (s + 1));
        memset(token, 0, (s + 1));
        strcpy(token, tok);
        tokens[i++] = token;
        tok = strtok(NULL, " =\n");
    }
    return tokens;
}

struct config_params * read_configuration_file(const char * file_path) {
    if (!file_path) {
        printf("no file path provided\n");
        return NULL;
    }

    struct config_params * params = NULL;
    FILE * fp = fopen(file_path, "r");
    char * line = NULL;
    size_t size = 0;
    ssize_t read;
    if (!fp) {
        printf("Cannot open file %s\n", file_path);
        return NULL;
    }
#ifndef WIN32
    while ((read = getline(&line, &size, fp)) > 0) {
#else
    size = 1024;
    line = (char *)malloc(1024);
    while (fgets(line, size, fp) != NULL) {
#endif
        char ** tokens = parse_line(line, size);
        struct config_params * el = (struct config_params *)malloc(sizeof(struct config_params));
        size_t key_len = strlen(tokens[0]);
        size_t value_len = strlen(tokens[1]);
        el->key = (char *)malloc(key_len + 1);
        memset(el->key, 0, key_len + 1);
        strcpy(el->key, tokens[0]);

        el->value = (char *)malloc(value_len + 1);
        memset(el->value, 0, value_len + 1);
        strcpy(el->value, tokens[1]);

        free(tokens[0]);
        free(tokens[1]);
        char ** tmp = tokens;
        free(tmp);

        HASH_ADD_STR(params, key, el);
    }
    free(line);
    return params;
}

c2operation c2_operation_from_string(const char * operation) {
    if (strcmp(operation, "START") == 0) {
        return START;
    }

    if (strcmp(operation, "STOP") == 0) {
        return STOP;
    }

    if (strcmp(operation, "RESTART") == 0) {
        return RESTART;
    }

    if (strcmp(operation, "UPDATE") == 0) {
        return UPDATE;
    }

    return -1;
}

/*
 * ./c2_client <host> <port> <c2operation enum> <c2agent uuid>
 */
int main(int argc, char ** argv) {

    if (argc < 7) {
        printf("usage: ./c2_client <config_file> <c2operation enum> <c2agent uuid> <ecu uuid> <input_params_file> <output_params_file>\n");
        return 1;
    }

    const char * operation = argv[2];
    const char * uuid = argv[3];

    c2operation op = c2_operation_from_string(operation);

    if (op < 0) {
        printf("Operation %s not recognized\n", operation);
        return 1;
    }

    const char * config_file = argv[1];

    char * input_file = NULL;
    char * output_file = NULL;
    if (op == START || op == UPDATE || op == RESTART) {
        input_file = argv[5];
        if (argc > 5) {
            output_file = argv[6];
        }
    }

    struct config_params * config = read_configuration_file(config_file);

    struct config_params * input_params = NULL;
    if (input_file) {
        input_params = read_configuration_file(input_file);
    }

    struct config_params * output_params = NULL;
    if (output_file) {
        output_params = read_configuration_file(output_file);
    }

    struct config_params * host = NULL;
    HASH_FIND_STR(config, "host", host);
    if (!host) {
        return 1;
    }
    char * host_name = host->value;
    printf("host = %s\n", host_name);

    struct config_params * port = NULL;
    HASH_FIND_STR(config, "port", port);
    if (!port) {
        return 1;
    }
    char * port_str = port->value;
    printf("port = %s\n", port_str);
    size_t port_num = (uint64_t)strtoul(port_str, NULL, 10);

    c2context_t * c2 = create_c2_agent(host_name, port_str);
    c2->is_little_endian = is_little_endian();
    initialize_coap(c2);

    c2_server_response_t * resp_list = NULL;
    c2_server_response_t * resp = (c2_server_response_t *)malloc(sizeof(c2_server_response_t));
    memset(resp, 0, sizeof(c2_server_response_t));
    resp->ident = (char *)malloc(strlen(uuid) + 1);
    strcpy(resp->ident, uuid);

    char * operand_str = argv[4];

    resp->operand = (char *)malloc(strlen(operand_str) + 1);
    strcpy(resp->operand, operand_str);

    resp->operation = op;

    //Read from input_params .. Just iterate and fill in the key,value in c2response
    if (input_params) {
        struct config_params * el, *tmp = NULL;
        HASH_ITER(hh, input_params, el, tmp) {
            properties_t * op_args = (properties_t *)malloc(sizeof(properties_t));
            op_args->key = (char *)malloc(strlen(el->key) + 1);
            memset(op_args->key, 0, strlen(el->key) + 1);
            strcpy(op_args->key, el->key);
            op_args->value = (char *)malloc(strlen(el->value) + 1);
            memset(op_args->value, 0, strlen(el->value) + 1);
            strcpy(op_args->value, el->value);
            HASH_ADD_KEYPTR(hh, resp->args, op_args->key, strlen(op_args->key), op_args);
        }
    }

    if (output_params) {
        struct config_params * el, *tmp = NULL;
        HASH_ITER(hh, output_params, el, tmp) {
            properties_t * op_args = (properties_t *)malloc(sizeof(properties_t));
            op_args->key = (char *)malloc(strlen(el->key) + 1);
            memset(op_args->key, 0, strlen(el->key) + 1);
            strcpy(op_args->key, el->key);
            op_args->value = (char *)malloc(strlen(el->value) + 1);
            memset(op_args->value, 0, strlen(el->value) + 1);
            strcpy(op_args->value, el->value);
            HASH_ADD_KEYPTR(hh, resp->args, op_args->key, strlen(op_args->key), op_args);
        }
    }

    LL_APPEND(resp_list, resp);
    char * payload;
    size_t length;
    encode_c2_server_response(resp_list, &payload, &length);
#ifdef WIN32
    WSADATA wsadata;
    int err = WSAStartup(MAKEWORD(2, 2), &wsadata);
    if (err != 0) {
        return NULL;
    }
#endif
    CoapMessage coap_msg;
    coap_msg.data_ = (uint8_t*)payload;
    coap_msg.size_ = length;
    printf("sending payload\n");
    send_payload(c2, "c2operation", &coap_msg);
    printf("payload sent\n");
#ifdef WIN32
    WSACleanup();
#endif
}

#ifdef __cplusplus
}
#endif

