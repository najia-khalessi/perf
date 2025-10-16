#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "../../include/header.h"
#include "../../include/config.h"
#include "../../include/perf.h"
#include "../../include/database.h"
#include "../../include/buffer.h"
#include "../../include/perf.h"

#define BATCH_SIZE 1000 // 每次批量插入1000条

extern volatile sig_atomic_t stop;
static sqlite3* db;
static struct ring_buffer data_buffer;
static pthread_t db_writer_thread_id;

static void* db_writer_thread(void* arg) {
    struct buffer_entry entries[BATCH_SIZE];
    while (true) {
        int count = buffer_pop_batch(&data_buffer, entries, BATCH_SIZE);
        if (count > 0) {
            if (db_insert_batch(db, entries, count) != SQLITE_OK) {
                fprintf(stderr, "Error: Failed to write batch to database. Stopping collection.\n");
                stop = 1; // 设置全局停止标志
            }
            for (int i = 0; i < count; i++) {
                free(entries[i].stack);
            }
        }
        if (stop && buffer_is_empty(&data_buffer)) {
            break;
        }
    }
    return NULL;
}

int start_collection(struct profiling_config* config) {
    if (config->flamegraph_mode) {
        struct system_context sys;
        if (initialize_system(&sys)) {
            fprintf(stderr, "Error: Failed to initialize system context\n");
            return 1;
        }

        struct perf_event_manager* manager = perf_event_init_with_config(config);
        if (!manager) {
            fprintf(stderr, "Error: Failed to initialize perf events.\n");
            cleanup_system(&sys);
            return 1;
        }

        printf("Collection started. Press Ctrl+C to stop...\n");

        time_t start_time = time(NULL);

        while (!stop) {
            if (config->collection_duration > 0) {
                if (time(NULL) - start_time >= config->collection_duration) {
                    printf("\nCollection duration reached. Stopping...\n");
                    stop = 1;
                    continue;
                }
            }
            main_loop(&sys, manager, &stop, NULL, config);
        }

        printf("\nStopping collection...\n");
        cleanup_system(&sys);
        perf_event_cleanup_manager(manager);
        printf("Collection finished.\n");
        return 0;
    }

    buffer_init(&data_buffer);

    // 直接使用 start.sh 脚本中生成的文件名，不再附加时间戳
    const char* db_path = config->collection_output_path;
    printf("Database will be saved to: %s\n", db_path);

    db = db_open(db_path);
    if (!db) {
        return 1;
    }

    if (pthread_create(&db_writer_thread_id, NULL, db_writer_thread, NULL) != 0) {
        fprintf(stderr, "Error: Failed to create database writer thread\n");
        db_close(db);
        return 1;
    }

    struct system_context sys;
    if (initialize_system(&sys)) {
        fprintf(stderr, "Error: Failed to initialize system context\n");
        stop = 1;
        pthread_join(db_writer_thread_id, NULL);
        db_close(db);
        return 1;
    }

    struct perf_event_manager* manager = perf_event_init_with_config(config);
    if (!manager) {
        fprintf(stderr, "Error: Failed to initialize perf events.\n");
        stop = 1; // 触发写入线程退出
        pthread_join(db_writer_thread_id, NULL);
        db_close(db);
        cleanup_system(&sys);
        return 1;
    }

    printf("Collection started. Press Ctrl+C to stop...\n");

    time_t start_time = time(NULL);

    while (!stop) {
        if (config->collection_duration > 0) {
            if (time(NULL) - start_time >= config->collection_duration) {
                printf("\nCollection duration reached. Stopping...\n");
                stop = 1;
                continue;
            }
        }
        main_loop(&sys, manager, &stop, &data_buffer, config); // Pass config
    }

    printf("\nStopping collection and saving remaining data...\n");

    buffer_shutdown(&data_buffer);
    pthread_join(db_writer_thread_id, NULL);

    cleanup_system(&sys);
    perf_event_cleanup_manager(manager);
    db_close(db);

    printf("Collection finished.\n");
    return 0;
}
