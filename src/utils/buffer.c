#include <stdlib.h>
#include <string.h>
#include "../../include/buffer.h"

void buffer_init(struct ring_buffer* buffer) {
    buffer->head = 0;
    buffer->tail = 0;
    buffer->count = 0;
    buffer->shutdown = false;
    pthread_mutex_init(&buffer->mutex, NULL);
    pthread_cond_init(&buffer->not_full, NULL);
    pthread_cond_init(&buffer->not_empty, NULL);
}

void buffer_destroy(struct ring_buffer* buffer) {
    // 清理所有剩余的条目
    while (buffer->count > 0) {
        free(buffer->entries[buffer->tail].stack);
        buffer->tail = (buffer->tail + 1) % BUFFER_SIZE;
        buffer->count--;
    }
    pthread_mutex_destroy(&buffer->mutex);
    pthread_cond_destroy(&buffer->not_full);
    pthread_cond_destroy(&buffer->not_empty);
}

void buffer_push(struct ring_buffer* buffer, struct buffer_entry* entry) {
    pthread_mutex_lock(&buffer->mutex);

    // 等待缓冲区有空间
    while (buffer->count == BUFFER_SIZE && !buffer->shutdown) {
        pthread_cond_wait(&buffer->not_full, &buffer->mutex);
    }

    if (buffer->shutdown) {
        pthread_mutex_unlock(&buffer->mutex);
        return;
    }

    // 复制数据到缓冲区
    buffer->entries[buffer->head].timestamp = entry->timestamp; // 保留生产者生成的高精度时间戳
    buffer->entries[buffer->head].pid = entry->pid;
    buffer->entries[buffer->head].tid = entry->tid;
    buffer->entries[buffer->head].stack = strdup(entry->stack); // 必须复制，因为原始stack可能被释放

    buffer->head = (buffer->head + 1) % BUFFER_SIZE;
    buffer->count++;

    // 通知消费者数据已准备好
    pthread_cond_signal(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->mutex);
}

void buffer_pop(struct ring_buffer* buffer, struct buffer_entry* entry) {
    pthread_mutex_lock(&buffer->mutex);

    // 等待缓冲区有数据
    while (buffer->count == 0 && !buffer->shutdown) {
        pthread_cond_wait(&buffer->not_empty, &buffer->mutex);
    }

    if (buffer->shutdown && buffer->count == 0) {
        pthread_mutex_unlock(&buffer->mutex);
        entry->timestamp = 0;
        entry->pid = -1;
        entry->tid = -1;
        entry->stack = NULL;
        return;
    }

    // 从缓冲区取出数据
    *entry = buffer->entries[buffer->tail];
    buffer->tail = (buffer->tail + 1) % BUFFER_SIZE;
    buffer->count--;

    // 通知生产者有空间了
    pthread_cond_signal(&buffer->not_full);
    pthread_mutex_unlock(&buffer->mutex);
}

int buffer_pop_batch(struct ring_buffer* buffer, struct buffer_entry* entries, int max_entries) {
    pthread_mutex_lock(&buffer->mutex);
    while (buffer->count == 0 && !buffer->shutdown) {
        pthread_cond_wait(&buffer->not_empty, &buffer->mutex);
    }

    if (buffer->shutdown && buffer->count == 0) {
        pthread_mutex_unlock(&buffer->mutex);
        return 0;
    }

    int i = 0;
    while (i < max_entries && buffer->count > 0) {
        entries[i] = buffer->entries[buffer->tail];
        buffer->tail = (buffer->tail + 1) % BUFFER_SIZE;
        buffer->count--;
        i++;
    }

    pthread_cond_signal(&buffer->not_full);
    pthread_mutex_unlock(&buffer->mutex);

    return i;
}

void buffer_shutdown(struct ring_buffer* buffer) {
    pthread_mutex_lock(&buffer->mutex);
    buffer->shutdown = true;
    // 唤醒所有等待的线程，让他们退出
    pthread_cond_broadcast(&buffer->not_empty);
    pthread_cond_broadcast(&buffer->not_full);
    pthread_mutex_unlock(&buffer->mutex);
}

bool buffer_is_empty(struct ring_buffer* buffer) {
    pthread_mutex_lock(&buffer->mutex);
    bool is_empty = (buffer->count == 0);
    pthread_mutex_unlock(&buffer->mutex);
    return is_empty;
}