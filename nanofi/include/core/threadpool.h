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

#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <time.h>
#include <stdint.h>
#include "utlist.h"

typedef enum task_state {
    RUN_AGAIN,
    DONOT_RUN_AGAIN
} task_state_t;

typedef task_state_t (*function_t)(void * args, void * state);

typedef struct task {
    function_t function;
    void * args;
    void * state;
    int64_t start_time_ms;
    uint64_t interval_ms;
} task_t;

typedef struct task_node {
    task_t task;
    struct task_node * next;
} task_node_t;

typedef struct threadpool {
    task_node_t * task_queue;
    pthread_mutex_t task_queue_mutex;
    pthread_cond_t task_queue_cond;
    int num_threads;
    pthread_t * threads;
    int shuttingdown;
    int num_tasks;
    int started;
} threadpool_t;

task_node_t * create_repeatable_task(function_t function, void * args, void* state, uint64_t interval_seconds);
task_node_t * create_oneshot_task(function_t function, void * args, void * state);
int is_task_repeatable(task_t * task);
uint64_t get_task_repeat_interval(task_t * task);
uint64_t get_num_tasks(threadpool_t * pool);

uint64_t get_time_millis(struct timespec ts);
uint64_t get_now_ms();

/* uses CLOCK_MONOTONIC */
struct timespec get_timespec_millis_from_now(uint64_t millis);

int condition_timed_wait(pthread_cond_t * cond, pthread_mutex_t * mutex, uint64_t millis);

void threadpool_add(threadpool_t * pool, task_node_t * task);
void threadpool_start(threadpool_t * pool);
void threadpool_shutdown(threadpool_t * pool);
threadpool_t * threadpool_create(uint64_t num_threads);
void * threadpool_thread_function(void * pool);

#ifdef __cplusplus
}
#endif


#endif /* THREADPOOL_H_ */
