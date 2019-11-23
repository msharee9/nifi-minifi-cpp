#include <nanofi/coap_server.h>
//#include "coap/coappeer.h"
#include "coap/c2protocol.h"
#include "coap/c2structs.h"
#include <coap/c2agent.h>
#include <c2_api/c2api.h>
#include "api/ecu.h"
#include "utlist.h"

typedef struct c2_server_responses {
    char uuid[37]; // key
    c2_server_response_t * response;
    UT_hash_handle hh;
} c2_server_responses_t;

int little_endian = 0;

c2_server_responses_t * responses = NULL;

void handle_c2_post_request(coap_context_t * ctx, struct coap_resource_t * resource, coap_session_t * session, coap_pdu_t * pdu,
        coap_binary_t * token, coap_string_t * query, coap_pdu_t * response) {
    printf("handling c2 client request\n");
    //This is a handle for accumulating c2responses from a c2 client.
    //The accumulated responses are then sent out as part of heartbeat response from c2 agents
    //This simulates sending c2 operations to c2 specific agents
    uint8_t * data;
    size_t len;
    coap_get_data(pdu, &len, &data);
    coap_message msg;
    msg.data = data;
    msg.length = len;

    c2_server_response_t * c2_serv_resp = NULL;
    if (len > 0) {
        c2_serv_resp = decode_c2_server_response(&msg, little_endian);
    }
    if (c2_serv_resp) {
        const char * uuid = c2_serv_resp->ident;
        c2_server_responses_t * el = NULL;
        HASH_FIND_STR(responses, uuid, el);
        if (el)  {
            LL_APPEND(el->response, c2_serv_resp);
        }
        else {
            el = (struct c2_server_responses *) malloc(sizeof(struct c2_server_responses));
            memset(el, 0, sizeof(struct c2_server_responses));
            strcpy(el->uuid, uuid);
            LL_APPEND(el->response, c2_serv_resp);
            HASH_ADD_STR(responses, uuid, el);
        }
    }
}

void handle_acknowledge_post_request(coap_context_t * ctx, struct coap_resource_t * resource, coap_session_t * session, coap_pdu_t * pdu,
                                   coap_binary_t * token, coap_string_t * query, coap_pdu_t * response) {
    printf("acknowledge received from client\n");
}

void handle_heartbeat_post_request(coap_context_t * ctx, struct coap_resource_t * resource, coap_session_t * session, coap_pdu_t * pdu,
                               coap_binary_t * token, coap_string_t * query, coap_pdu_t * response) {
    uint8_t * data;
    size_t len;
    coap_get_data(pdu, &len, &data);
    c2heartbeat_t heartbeat;
    memset(&heartbeat, 0, sizeof(c2heartbeat_t));
    size_t bytes_decoded = 0;
    if (decode_heartbeat(data, len, &heartbeat, &bytes_decoded) < 0) {
        printf("decode heartbeat failed\n");
        return;
    }

    const char * uuid = heartbeat.agent_info.ident;

    printf("heartbeat received from client %s\n", uuid);

    c2_server_responses_t * el = NULL;
    HASH_FIND_STR(responses, uuid, el);
    if (!el)  {
        free_c2heartbeat(&heartbeat);
        return;
    }

    char * payload;
    size_t length;

    encode_c2_server_response(el->response, &payload, &length);

    HASH_DEL(responses, el);
    free_c2_server_responses(el->response);
    free(el);

    response->code = COAP_RESPONSE_CODE(204);
    coap_add_data_blocked_response(resource, session, pdu, response, token,
                                   COAP_MEDIATYPE_APPLICATION_CBOR, 0x2ffff,
                                   length,
                                   (const uint8_t *)payload);
    free(payload);
    free_c2heartbeat(&heartbeat);
}
static int stop = 0;

void start_client(void * endpoint) {
    while (!stop) {
        if (coap_run_once(((CoapEndpoint*)endpoint)->server->ctx, 100) < 0) {
            return;
        }
    }
}

void start_coap_server(CoapEndpoint * endpoint) {
    while (!stop) {
        if (coap_run_once(((CoapEndpoint*)endpoint)->server->ctx, 100) < 0) {
            return;
        }
    }
}

void stop_coap_server() {

}
int main(int argc, char ** argv) {
    //initialize_coap();
    little_endian = is_little_endian();
    CoapServerContext * coapctx = create_server("192.168.1.158", "5683");
    CoapEndpoint * endpoint = create_endpoint(coapctx, "heartbeat", COAP_REQUEST_POST, handle_heartbeat_post_request);
    create_endpoint(coapctx, "acknowledge", COAP_REQUEST_POST, handle_acknowledge_post_request);
    create_endpoint(coapctx, "c2operation", COAP_REQUEST_POST, handle_c2_post_request);
    if (!endpoint) {
        return 1;
    }
    start_coap_server(endpoint);
    stop_coap_server();
    free(endpoint);
}
