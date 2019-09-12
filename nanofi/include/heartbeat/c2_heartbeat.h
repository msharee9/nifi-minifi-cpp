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

#ifndef NANOFI_INCLUDE_PROCESSORS_C2_HEARTBEAT_H_
#define NANOFI_INCLUDE_PROCESSORS_C2_HEARTBEAT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <c2_api/c2api.h>

task_state_t c2_heartbeat_sender(void * args, void * state);

#ifdef __cplusplus
}
#endif

#endif /* NANOFI_INCLUDE_PROCESSORS_C2_HEARTBEAT_H_ */
