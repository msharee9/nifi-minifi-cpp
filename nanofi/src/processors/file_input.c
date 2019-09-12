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

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <processors/file_input.h>

void initialize_file_input(file_input_context_t * ctx) {
    pthread_mutex_init(&ctx->stop_mutex, NULL);
    pthread_cond_init(&ctx->stop_cond, NULL);
}

void start_file_input(file_input_context_t * ctx) {
    pthread_mutex_lock(&ctx->stop_mutex);
    ctx->stop = 0;
    pthread_mutex_unlock(&ctx->stop_mutex);
}

void stop_file_input(file_input_context_t * ctx) {
    pthread_mutex_lock(&ctx->stop_mutex);
    ctx->stop = 1;
    pthread_cond_broadcast(&ctx->stop_cond);
    pthread_mutex_unlock(&ctx->stop_mutex);
}

int validate_file_path(const char * file_path) {
    if (!file_path) {
        return -1;
    }
    struct stat stats;
    int ret = stat(file_path, &stats);

    if (ret == -1) {
        printf("Error occurred while getting file status {file: %s, error: %s}\n", file_path, strerror(errno));
        return -1;
    }

    if (S_ISDIR(stats.st_mode)){
        printf("Error: %s is a directory!\n", file_path);
        return -1;
    }
    return 0;
}

int validate_file_delimiter(const char * delimiter_str, char * delim) {
    if (!delimiter_str || strlen(delimiter_str) == 0) {
        return -1;
    }

    char delimiter[3];
    strncpy(delimiter, delimiter_str, 2);
    delimiter[2] = '\0';
    *delim = delimiter[0];

    if (*delim == '\\' && strlen(delimiter) > 1) {
        switch (delimiter[1]) {
        case 'r':
            *delim = '\r';
            break;
        case 'n':
            *delim = '\n';
            break;
        case 't':
            *delim = '\t';
            break;
        case '\\':
            *delim = '\\';
            break;
        }
    }
    return 0;
}

int str_to_uint(const char * input_str, uint64_t * out) {
    if (!input_str) {
        return -1;
    }
    errno = 0;
    *out = (uint64_t)(strtoul(input_str, NULL, 10));
    if (errno != 0) {
        return -1;
    }
    return 0;
}

int validate_file_properties(struct file_input_context * context) {
    if (!context || !context->input_properties) {
        return -1;
    }

    properties_t * props = context->input_properties;
    properties_t * el = NULL;
    HASH_FIND_STR(props, "file_path", el);
    if (!el) {
        return -1;
    }
    char * file_path = el->value;
    if (!file_path) {
        return -1;
    }

    properties_t * cs = NULL;
    properties_t * dl = NULL;
    HASH_FIND_STR(props, "chunk_size", cs);
    HASH_FIND_STR(props, "delimiter", dl);
    if (dl && cs) {
        return -1;
    }

    if (!dl && !cs) {
        return -1;
    }

    char * chunk_size_str = NULL;
    char * delimiter = NULL;

    if (cs) {
        chunk_size_str = cs->value;
    }
    if (dl) {
        delimiter = dl->value;
    }

    el = NULL;
    HASH_FIND_STR(props, "tail_frequency", el);
    if (!el) {
        return -1;
    }

    char * tail_frequency_str = el->value;

    uint64_t chunk_size_uint = 0;
    uint64_t tail_frequency_uint = 0;
    char delim = '\0';

    if ((validate_file_path(file_path) < 0)
        || (dl && validate_file_delimiter(delimiter, &delim) < 0)
        || (cs && str_to_uint(chunk_size_str, &chunk_size_uint) < 0)
        || (str_to_uint(tail_frequency_str, &tail_frequency_uint) < 0)
        || (dl && delim == '\0')) {
        return -1;
    }

    //populate file input context with parameters
    size_t file_path_len = strlen(file_path);
    char * fp = context->file_path;
    if (fp) free(fp);
    context->file_path = (char *)malloc(file_path_len + 1);
    strcpy(context->file_path, file_path);

    context->tail_frequency = tail_frequency_uint;

    if (cs)
     context->chunk_size = chunk_size_uint;
    if (dl)
        context->delimiter = delim;
    return 0;
}

int set_file_input_property(file_input_context_t * ctx, const char * name, const char * value) {
    return add_property(&ctx->input_properties, name, value);
}

message_t * create_message(char * data, size_t len, properties_t * props) {
    attribute_set as = prepare_attributes(props);
    return prepare_message(data, len, as);
}

void enqueue_chunk(file_input_context_t * ctx, data_buff_t chunk) {
    if (chunk.len > 0 && chunk.data) {
        size_t fp_len = strlen(ctx->file_path);
        chunk.file_path = (char *)malloc(fp_len + 1);
        strcpy(chunk.file_path, ctx->file_path);

        int length = snprintf(NULL, 0, "%llu", ctx->current_offset);
        char * offset_str = (char *)malloc(length + 1);
        snprintf(offset_str, length + 1, "%llu", ctx->current_offset);

        properties_t * props = NULL;
        add_property(&props, "current offset", offset_str);
        add_property(&props, "file path", chunk.file_path);

        message_t * msg = create_message(chunk.data, chunk.len, props);
        free(chunk.data);
        free(offset_str);
        free(chunk.file_path);
        free_properties(props);
        size_t bytes_enqueued = enqueue_message(ctx->msg_queue, msg);
        if (bytes_enqueued < chunk.len) {
            //we were not able to enqueue all of chunk.len bytes
            //therefore, update the current offset
            ctx->current_offset -= (chunk.len - bytes_enqueued);
        }
    }
}

void read_file_chunk(file_input_context_t * ctx) {
    errno = 0;
    FILE * fp = fopen(ctx->file_path, "rb");
    if (!fp) {
        printf("Error opening file %s, error: %s\n", ctx->file_path, strerror(errno));
        return;
    }
    size_t bytes_read = 0;
    char * data = (char *)malloc((ctx->chunk_size + 1) * sizeof(char));
    fseek(fp, ctx->current_offset, SEEK_SET);
    while ((bytes_read = fread(data, 1, ctx->chunk_size, fp)) > 0) {
        if (bytes_read < ctx->chunk_size) {
            break;
        }
        data[ctx->chunk_size] = '\0';
        data_buff_t buff;
        buff.data = (char *)malloc(strlen(data) + 1);
        memcpy(buff.data, data, strlen(data) + 1);
        buff.len = strlen(data);
        ctx->current_offset = ftell(fp);
        size_t old_offset = ctx->current_offset;
        enqueue_chunk(ctx, buff);
        if (old_offset > ctx->current_offset) {
            //we were not able to ship all of the chunk read from
            //the file. let's stop reading and come back later
            break;
        }
        fseek(fp, ctx->current_offset, SEEK_SET);
    }
    free(data);
    fclose(fp);
}


void read_file_delim(file_input_context_t * ctx) {
    FILE * fp = fopen(ctx->file_path, "rb");
    errno = 0;
    if (!fp) {
        printf("Unable to open file. {file: %s, reason: %s}\n", ctx->file_path, strerror(errno));
        return;
    }

    char data[4096 + 1];
    memset(data, 0, 4097);
    fseek(fp, ctx->current_offset, SEEK_SET);
    size_t bytes_read = 0;
    while ((bytes_read = fread(data, 1, 4096, fp)) > 0) {
        data[bytes_read] = '\0';
        const char * begin = data;
        const char * end = NULL;

        while ((end = strchr(begin, ctx->delimiter))) {
            uint64_t len = end - begin;
            if (len > 0) {
                data_buff_t buff;
                buff.data = (char *)malloc(len + 1);
                memcpy(buff.data, begin, len);
                buff.data[len] = '\0';
                buff.len = len;
                ctx->current_offset += (len + 1);
                size_t old_offset = ctx->current_offset;
                enqueue_chunk(ctx, buff);
                if (old_offset > ctx->current_offset) {
                    fclose(fp);
                    return;
                }
            }
            else {
                ctx->current_offset += 1;
            }
            begin = (end + 1);
        }
        fseek(fp, ctx->current_offset, SEEK_SET);
    }
    fclose(fp);
}

file_input_context_t * create_file_input_context() {
    file_input_context_t * ctx = (file_input_context_t *)malloc(sizeof(file_input_context_t));
    memset(ctx, 0, sizeof(file_input_context_t));
    initialize_file_input(ctx);
    return ctx;
}

task_state_t file_reader_processor(void * args, void * state) {
    file_input_context_t * ctx = (file_input_context_t *)args;
    pthread_mutex_lock(&ctx->msg_queue->queue_lock);
    if (ctx->msg_queue->stop) {
        stop_file_input(ctx);
        pthread_mutex_unlock(&ctx->msg_queue->queue_lock);
        return DONOT_RUN_AGAIN;
    }
    pthread_mutex_unlock(&ctx->msg_queue->queue_lock);

    if (ctx->chunk_size > 0) {
        read_file_chunk(ctx);
    } else {
        read_file_delim(ctx);
    }
    return RUN_AGAIN;
}

void free_file_input_properties(file_input_context_t * ctx) {
    properties_t * ip_props = ctx->input_properties;
    ctx->input_properties = NULL;
    free_properties(ip_props);
}

void free_file_input_context(file_input_context_t * ctx) {
    free_properties(ctx->input_properties);
    free(ctx->file_path);
    pthread_mutex_destroy(&ctx->stop_mutex);
    pthread_cond_destroy(&ctx->stop_cond);
    free(ctx);
}

void wait_file_input_stop(file_input_context_t * ctx) {
    pthread_mutex_lock(&ctx->stop_mutex);
    while (!ctx->stop) {
        pthread_cond_wait(&ctx->stop_cond, &ctx->stop_mutex);
    }
    pthread_mutex_unlock(&ctx->stop_mutex);
}

void set_file_params(file_input_context_t * f_ctx,
        const char * file_path,
        uint64_t tail_freq,
        uint64_t current_offset) {
    size_t fp_len = strlen(file_path);
    f_ctx->file_path = (char *)malloc(fp_len + 1);
    strcpy(f_ctx->file_path, file_path);
    f_ctx->tail_frequency = tail_freq;
    f_ctx->current_offset = current_offset;
}

void set_file_chunk_params(file_input_context_t * f_ctx,
        const char * file_path,
        uint64_t tail_freq,
        uint64_t current_offset,
        uint64_t chunk_size) {
    set_file_params(f_ctx, file_path, tail_freq, current_offset);
    f_ctx->chunk_size = chunk_size;
}

void set_file_delim_params(file_input_context_t * f_ctx,
        const char * file_path,
        uint64_t tail_freq,
        uint64_t current_offset,
        char delim) {
    set_file_params(f_ctx, file_path, tail_freq, current_offset);
    f_ctx->delimiter = delim;
}

data_buff_t read_file_chunk_data(FILE * fp, file_input_context_t * ctx) {
    data_buff_t chunk;
    memset(&chunk, 0, sizeof(data_buff_t));
    if (!ctx || ctx->chunk_size == 0 || !fp) {
        return chunk;
    }
    size_t bytes_read = 0;
    chunk.data = (char *)malloc(ctx->chunk_size + 1);
    fseek(fp, ctx->current_offset, SEEK_SET);
    bytes_read = fread(chunk.data, 1, ctx->chunk_size, fp);
    chunk.data[bytes_read] = '\0';
    chunk.len = bytes_read;
    ctx->current_offset += bytes_read;
    return chunk;
}

void update_msg_attributes(message_t * msg, size_t bytes_enqueued) {
    attribute_set as = msg->as;
    size_t msg_len = msg->len;
    const char * key = "current offset";
    int i;
    for (i = 0; i < as.size; ++i) {
        if (strcmp(as.attributes[i].key, key) == 0) {
            char * value = (char *)(as.attributes[i].value);
            errno = 0;
            uint64_t curr_offset = (uint64_t)strtoul(value, NULL, 10);
            curr_offset -= (msg_len - bytes_enqueued);
            int length = snprintf(NULL, 0, "%llu", curr_offset);
            char * offset_str = (char *)malloc(length + 1);
            snprintf(offset_str, length + 1, "%llu", curr_offset);
            free(value);
            as.attributes[i].value = (void *)offset_str;
            as.attributes[i].value_size = strlen(offset_str);
            return;
        }
    }
}

