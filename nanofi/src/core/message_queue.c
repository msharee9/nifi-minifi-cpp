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

#include <time.h>
#include <errno.h>
#include <core/message_queue.h>

message_queue_t * create_msg_queue(uint64_t capacity_bytes) {
    ring_buffer_t * rb = (ring_buffer_t *)malloc(sizeof(ring_buffer_t));
    memset(rb, 0, sizeof(ring_buffer_t));
    rb->capacity = capacity_bytes;
    rb->data = (char *)malloc(rb->capacity);
    message_queue_t * mq = (message_queue_t *)malloc(sizeof(message_queue_t));
    memset(mq, 0, sizeof(message_queue_t));
    mq->ring_buff = rb;
    pthread_mutex_init(&mq->queue_lock, NULL);
    pthread_cond_init(&mq->write_notify, NULL);
    return mq;
}

void set_attribute_update_cb(message_queue_t * mq, attribute_set_cb_t cb) {
    mq->attr_cb = cb;
}

void free_queue(message_queue_t * mq) {
	if (!mq) return;
    pthread_mutex_lock(&mq->queue_lock);
    message_attrs_t * head = mq->attrs;
    while (head) {
        message_attrs_t * tmp = head;
        head = head->next;
        free_attributes(tmp->as);
        free(tmp);
    }
    free_ring_buffer(mq->ring_buff);
    pthread_mutex_destroy(&mq->queue_lock);
    pthread_cond_destroy(&mq->write_notify);
    free(mq);
}

attribute_set prepare_attributes(properties_t * attributes) {
    attribute_set as;
    memset(&as, 0, sizeof(attribute_set));
    if (!attributes) return as;

    as.size = HASH_COUNT(attributes);
    attribute * attrs = (attribute *)malloc(as.size * sizeof(attribute));

    properties_t *p, *tmp;
    int i = 0;
    HASH_ITER(hh, attributes, p, tmp) {
        attrs[i].key = (char *)malloc(strlen(p->key) + 1);
        strcpy(attrs[i].key, p->key);
        char * value = (char *)malloc(strlen(p->value) + 1);
        strcpy(value, p->value);
        attrs[i].value = (void *)value;
        attrs[i].value_size = strlen(value);
        i++;
    }
    as.attributes = attrs;
    return as;
}

message_t * prepare_message(char * payload, size_t len, attribute_set as) {
    if (!payload || len == 0) {
        return NULL;
    }

    message_t * msg = (message_t *)malloc(sizeof(message_t));
    memset(msg, 0, sizeof(message_t));
    msg->buff = (char *)malloc(len);
    memcpy(msg->buff, payload, len);
    msg->len = len;
    msg->as = as;
    return msg;
}

size_t enqueue_message(message_queue_t * mq, message_t * msg) {
   if (!msg || !mq) {
       return 0;
   }
   pthread_mutex_lock(&mq->queue_lock);
   char * payload = msg->buff;
   size_t length = msg->len;

   struct timespec ts;
   clock_gettime(CLOCK_REALTIME, &ts);
   ts.tv_nsec += 3 * 1000000;
   ts.tv_sec += ts.tv_nsec / 1000000000L;
   ts.tv_nsec = ts.tv_nsec % 1000000000L;

   size_t bytes_enqueued = 0;
   while (length - bytes_enqueued > 0) {
       bytes_enqueued += write_ring_buffer(mq->ring_buff, payload + bytes_enqueued, (length - bytes_enqueued));
       if (bytes_enqueued < length) {
           int ret = pthread_cond_timedwait(&mq->write_notify, &mq->queue_lock, &ts);
           if (ret == ETIMEDOUT) {
               //timed out waiting to enqueue rest of the payload
               break;
           }
       }
   }

   if (bytes_enqueued) {
       //update the message attributes before enqueuing the attribute set
       if (mq->attr_cb && (bytes_enqueued < length)) {
           mq->attr_cb(msg, bytes_enqueued);
       }
       message_attrs_t * attrs = (message_attrs_t *)malloc(sizeof(message_attrs_t));
       memset(attrs, 0, sizeof(message_attrs_t));
       attrs->as = copy_attributes(msg->as);
       attrs->length = msg->len;
       LL_APPEND(mq->attrs, attrs);
   }
   free(payload);
   free_attributes(msg->as);
   free(msg);
   pthread_mutex_unlock(&mq->queue_lock);
   return bytes_enqueued;
}

message_t * dequeue_message(message_queue_t * mq) {
    if (!mq) {
        return NULL;
    }

    pthread_mutex_lock(&mq->queue_lock);
    message_t * msg = NULL;
    if (mq->attrs) {
        message_attrs_t * head = mq->attrs;
        mq->attrs = mq->attrs->next;
        head->next = NULL;
        size_t len = head->length;
        char * payload = (char *)malloc(len);
        size_t bytes_read = read_ring_buffer(mq->ring_buff, payload, len);
        if (bytes_read) {
            msg = (message_t *)malloc(sizeof(message_t));
            memset(msg, 0, sizeof(message_t));
            msg->as = head->as;
            msg->buff = payload;
            msg->len = bytes_read;
        }
        free(head);
    }
    pthread_cond_broadcast(&mq->write_notify);
    pthread_mutex_unlock(&mq->queue_lock);
    return msg;
}

void stop_message_queue(message_queue_t * queue) {
    pthread_mutex_lock(&queue->queue_lock);
    queue->stop = 1;
    pthread_mutex_unlock(&queue->queue_lock);
}

void start_message_queue(message_queue_t * queue) {
    pthread_mutex_lock(&queue->queue_lock);
    queue->stop = 0;
    pthread_mutex_unlock(&queue->queue_lock);
}

message_t * dequeue_message_nolock(message_queue_t * mq) {
    if (!mq) {
        return NULL;
    }

    message_t * msg = NULL;
    if (mq->attrs) {
        message_attrs_t * head = mq->attrs;
        mq->attrs = mq->attrs->next;
        head->next = NULL;
        size_t len = head->length;
        char * payload = (char *)malloc(len);
        size_t bytes_read = read_ring_buffer(mq->ring_buff, payload, len);
        if (bytes_read) {
            msg = (message_t *)malloc(sizeof(message_t));
            memset(msg, 0, sizeof(message_t));
            msg->as = head->as;
            msg->buff = payload;
            msg->len = bytes_read;
        }
        free(head);
    }
    return msg;
}

void free_attributes(attribute_set as) {
    int i;
    for (i = 0; i < as.size; ++i) {
        free(as.attributes[i].key);
        free(as.attributes[i].value);
    }
    free(as.attributes);
}

void free_message(message_t * msg) {
    while (msg) {
        message_t * tmp = msg;
        msg = msg->next;
        free_attributes(tmp->as);
        free(tmp->buff);
        free(tmp);
    }
}

attribute_set copy_attributes(attribute_set as) {
	attribute_set copy;
	memset(&copy, 0, sizeof(attribute_set));
	copy.size = as.size;
	copy.attributes = (attribute *)malloc(copy.size * sizeof(attribute));
	int i;
	for (i = 0; i < copy.size; ++i) {
		size_t key_len = strlen(as.attributes[i].key);
		copy.attributes[i].key = (char *)malloc(key_len + 1);
		strcpy(copy.attributes[i].key, as.attributes[i].key);

		char * value = (char *)malloc(as.attributes[i].value_size + 1);
		strcpy(value, (char *)as.attributes[i].value);
		copy.attributes[i].value = (void *)value;
		copy.attributes[i].value_size = as.attributes[i].value_size;
	}
	return copy;
}
