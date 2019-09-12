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
#ifndef EXTENSIONS_COAP_NANOFI_DTLS_CONFIG_H_
#define EXTENSIONS_COAP_NANOFI_DTLS_CONFIG_H_

#include <stdint.h>

typedef struct {
    char * public_cert; // path to the public certificate pem file
    char * private_key; // path to the private key pem file
    char * root_cas; // path to the root ca file or root ca directory
    char * ca_cert; // path to the issuing CA cert file
} dtls_config;

void free_dtls_config(dtls_config dc);

#endif /* EXTENSIONS_COAP_NANOFI_DTLS_CONFIG_H_ */
