#ifndef BUFFER_H
#define BUFFER_H

#include <pthread.h>
#include <stdbool.h>

#define BUFFER_SIZE 8192 // 缓冲区可以存储8192个样本

// 用于在缓冲区中存储的数据项
struct buffer_entry {
    long long timestamp; // 高精度时间戳
    int pid;
    int tid;
    char* stack; // 需要动态分配和释放
};

// 环形缓冲区结构
struct ring_buffer {
    struct buffer_entry entries[BUFFER_SIZE];
    size_t head; // 写入位置
    size_t tail; // 读取位置
    size_t count; // 当前元素数量

    pthread_mutex_t mutex;
    pthread_cond_t not_full;  // 当缓冲区不满时发出信号
    pthread_cond_t not_empty; // 当缓冲区不空时发出信号
    bool shutdown; // 用于通知消费者线程退出的标志
};

// 函数声明
void buffer_init(struct ring_buffer* buffer);
void buffer_destroy(struct ring_buffer* buffer);
void buffer_push(struct ring_buffer* buffer, struct buffer_entry* entry);
void buffer_pop(struct ring_buffer* buffer, struct buffer_entry* entry);
int buffer_pop_batch(struct ring_buffer* buffer, struct buffer_entry* entries, int max_entries);
void buffer_shutdown(struct ring_buffer* buffer);
bool buffer_is_empty(struct ring_buffer* buffer);

#endif // BUFFER_H