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

#include "core/threadpool.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

task_t create_task(function_t function, void * args, void * state) {
    task_t task;
    memset(&task, 0, sizeof(task_t));
    task.function = function;
    task.args = args;
    task.state = state;
    task.start_time_ms = get_now_ms();
    return task;
}

task_node_t * create_repeatable_task(function_t function, void * args, void * state, uint64_t interval_ms) {
    task_t task = create_task(function, args, state);
    task.interval_ms = interval_ms;
    task_node_t * task_node = (task_node_t *)malloc(sizeof(task_node_t));
    memset(task_node, 0, sizeof(task_node_t));
    task_node->task = task;
    return task_node;
}

int is_task_repeatable(task_t * task) {
    return task->interval_ms > 0;
}

uint64_t get_task_repeat_interval(task_t * task) {
    return task->interval_ms;
}

uint64_t get_time_millis(struct timespec ts) {
    ts.tv_sec += ts.tv_nsec / 1000000000L;
    ts.tv_nsec = ts.tv_nsec % 1000000000L;

    uint64_t ms = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000L);
    ts.tv_nsec = ts.tv_nsec % 1000000L;

    ms += lround((double)((double)ts.tv_nsec / 1000000L));
    return ms;
}

uint64_t get_now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return get_time_millis(ts);
}

struct timespec get_timespec_millis_from_now(uint64_t millis) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_nsec += millis * 1000000;
    ts.tv_sec += ts.tv_nsec / 1000000000L;
    ts.tv_nsec = ts.tv_nsec % 1000000000L;
    return ts;
}

int condition_timed_wait(pthread_cond_t * cond, pthread_mutex_t * mutex, uint64_t millis) {
#if defined(__APPLE__)
    struct timespec ts;
    ts.tv_sec = millis / 1000L;
    ts.tv_nsec = (millis % 1000L) * 1000000L;
    return pthread_cond_timedwait_relative_np(cond, mutex, &ts);
#else
    struct timespec millis_from_now = get_timespec_millis_from_now(millis);
    return pthread_cond_timedwait(cond, mutex, &millis_from_now);
#endif
}

void threadpool_add(threadpool_t * pool, task_node_t * task) {
    pthread_mutex_lock(&pool->task_queue_mutex);
    if (pool->shuttingdown) {
        pthread_mutex_unlock(&pool->task_queue_mutex);
        return;
    }
    LL_APPEND(pool->task_queue, task);
    pool->num_tasks++;
    pthread_cond_signal(&pool->task_queue_cond);
    pthread_mutex_unlock(&pool->task_queue_mutex);
}

uint64_t get_num_tasks(threadpool_t * pool) {
    uint64_t ret = 0;
    pthread_mutex_lock(&pool->task_queue_mutex);
    ret = pool->num_tasks;
    pthread_mutex_unlock(&pool->task_queue_mutex);
    return ret;
}

threadpool_t * threadpool_create(uint64_t num_threads) {
    if (num_threads == 0) {
        return NULL;
    }
    threadpool_t * pool = (threadpool_t *)malloc(sizeof(threadpool_t));
    memset(pool, 0, sizeof(threadpool_t));
    pool->num_threads = num_threads;
    pthread_mutex_init(&pool->task_queue_mutex, NULL);
    pthread_cond_init(&pool->task_queue_cond, NULL);
    pool->shuttingdown = 0;
    pool->num_tasks = 0;
    return pool;
}

task_node_t * get_task(task_node_t ** queue) {
    if (!queue || !*queue) {
        return NULL;
    }

    task_node_t * task = *queue;
    *queue = task->next;
    task->next = NULL;
    return task;
}

int is_timer_expired(task_t * task) {
    if (task->interval_ms == 0) {
        return 1;
    }

    uint64_t now = get_now_ms();
    if ((now - task->start_time_ms) >= task->interval_ms) {
        return 1;
    }
    return 0;
}

void * threadpool_thread_function(void * pool) {
    if (!pool) {
        return NULL;
    }

    threadpool_t * thpool = (threadpool_t *)pool;
    for (;;) {
        pthread_mutex_lock(&thpool->task_queue_mutex);

        //while there are no tasks in the queue and the pool
        //is not shutting down wait on the condition variable
        while (thpool->shuttingdown == 0 && thpool->num_tasks == 0) {
            pthread_cond_wait(&thpool->task_queue_cond, &thpool->task_queue_mutex);
        }

        if (thpool->shuttingdown) {
            break;
        }

        task_node_t * task = get_task(&thpool->task_queue);

        if (!task) {
            pthread_mutex_unlock(&thpool->task_queue_mutex);
            usleep(5000);
            continue;
        }

        if (thpool->num_tasks > 0)
            thpool->num_tasks--;
        pthread_mutex_unlock(&thpool->task_queue_mutex);

        if (is_task_repeatable(&task->task)) {
            task_state_t ret = RUN_AGAIN;
            if (is_timer_expired(&task->task)) {
                ret = (*task->task.function)(task->task.args, task->task.state);
                task->task.start_time_ms = get_now_ms();
            }
            if (ret == RUN_AGAIN) {
                threadpool_add(pool, task);
            } else {
                free(task);
            }
        } else {
            (*(task->task.function))(task->task.args, task->task.state);
            free(task->task.args);
            free(task->task.state);
            free(task);
        }
        usleep(5000);
    }

    pthread_mutex_unlock(&thpool->task_queue_mutex);
    return NULL;
}

void threadpool_start(threadpool_t * pool) {
    if (!pool) {
        return;
    }

    if (!pool->started) {
        pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * pool->num_threads);
        int i;
        for (i = 0; i < pool->num_threads; ++i) {
            pthread_create(&pool->threads[i], NULL, &threadpool_thread_function, (void *)pool);
        }
        pool->started = 1;
    }
}

void threadpool_shutdown(threadpool_t * pool) {
    if (!pool || !pool->started) {
        return;
    }
    pthread_mutex_lock(&(pool->task_queue_mutex));
    if (pool->shuttingdown) {
        pthread_mutex_unlock(&(pool->task_queue_mutex));
        return;
    }
    pool->shuttingdown = 1;
    pthread_cond_broadcast(&(pool->task_queue_cond));
    pthread_mutex_unlock(&(pool->task_queue_mutex));

    int i;
    for (i = 0; i < pool->num_threads; ++i) {
        pthread_join(pool->threads[i], NULL);
    }

    pthread_mutex_lock(&pool->task_queue_mutex);
    pthread_cond_destroy(&pool->task_queue_cond);
    pthread_mutex_destroy(&pool->task_queue_mutex);
    free(pool->threads);

    task_node_t * head = pool->task_queue;
    while (head) {
        task_node_t * tmp = head;
        head = head->next;
        free(tmp);
    }
}
