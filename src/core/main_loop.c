#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
// #include <sys/time.h>

#include "../../include/header.h"
#include "../../include/config.h"
#include "../../include/perf.h"

// 上下文结构，用于传递多个参数给回调
struct callback_context {
    struct system_context *sys_info;
    struct ring_buffer *buffer;
    const struct profiling_config* config;
};

// dispatch_sample_event 是一个回调函数，用于根据事件类型分发来自ring buffer的perf事件
static void dispatch_sample_event(struct perf_event_header *header, void *context) {
    extern struct profiling_config global_config;
    if (global_config.verbose) {
        fprintf(stderr, "[DEBUG] dispatch_sample_event called. Header type: %u\n", header->type);
    }
    struct callback_context *ctx = (struct callback_context *)context;
    
    if (header->type == PERF_RECORD_SAMPLE) {
        struct callchain_result result;
        uint64_t ips_buffer[256];
        result.ips = ips_buffer;

        parse_sample_data(header, &result, 128);
        // 传递系统上下文、结果和缓冲区
        symbolize_sample(ctx->sys_info, &result, ctx->buffer, ctx->config);
    }
}

void main_loop(struct system_context* system_info, struct perf_event_manager* manager, volatile sig_atomic_t *stop, struct ring_buffer *buffer, const struct profiling_config* config) {
    time_t last_cleanup_time = time(NULL);
// gettimeofday 量化时间 方便日后debug    
// struct timeval start_time, end_time;

    // 设置回调上下文
    struct callback_context context = {
        .sys_info = system_info,
        .buffer = buffer,
        .config = config
    };

    time_t start_time = time(NULL);

    while (!*stop) {
        // 如果设置了采集时长，检查是否超时
        if (config->collection_duration > 0) {
            if (time(NULL) - start_time >= config->collection_duration) {
                *stop = 1;
                continue;
            }
        }
// gettimeofday(&start_time, NULL);

        int total_events_processed = 0;
        for (int i = 0; i < manager->num_events; i++) {
            total_events_processed += perf_event_consume_ring_buffer(&manager->events[i], dispatch_sample_event, &context);
        }

// gettimeofday(&end_time, NULL);
/*
观察cpu利用率，核心多可能出现永远空转
if (config->verbose && total_events_processed > 0) {
    long seconds = end_time.tv_sec - start_time.tv_sec;
    long micros = ((seconds * 1000000) + end_time.tv_usec) - (start_time.tv_usec);
    printf("Processed %d events in %ld microseconds\n", total_events_processed, micros);
}
*/

        if (total_events_processed == 0) {
            time_t current_time = time(NULL);
            if (current_time - last_cleanup_time >= config->cleanup_interval) {
                cleanup_dead_processes(system_info);
                last_cleanup_time = current_time;
            }
            usleep(100000);
        }
    }
}