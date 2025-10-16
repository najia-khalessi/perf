#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libelf.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "include/header.h"
#include "include/config.h"
#include "include/perf.h"
#include "include/database.h"
#include "include/buffer.h"

// 声明外部定义的函数
int start_collection(struct profiling_config* config);
int start_query(struct profiling_config* config);

// Define the global config instance
struct profiling_config global_config;

volatile sig_atomic_t stop = 0;

void sig_handler(int sig) {
    stop = 1;
}

#define BATCH_SIZE 100

// 移除实时分析（live）模式，按文档仅支持 collect 与 query


int main(int argc, char* argv[]) {
    if (elf_version(EV_CURRENT) == EV_NONE) {
        fprintf(stderr, "Error: Failed to initialize libelf\n");
        return 1;
    }

    struct profiling_config config;
    if (parse_command_line(argc, argv, &config) != 0) {
        return 1;
    }

    if (config.op_mode == MODE_HELP) {
        print_usage(argv[0]);
        return 0;
    }

    if (validate_config(&config) != 0) {
        return 1;
    }

    signal(SIGINT, sig_handler);

    // 仅在需要perf事件的采集模式下要求root权限
    if (config.op_mode == MODE_COLLECT && getuid() != 0) {
        fprintf(stderr, "Error: 'collect' mode must be run as root.\n");
        return 1;
    }

    if (config.verbose) {
        fprintf(stderr, "[DEBUG] main: Before start_collection/start_query call.\n");
    }

    switch (config.op_mode) {
        case MODE_COLLECT:
            return start_collection(&config);
        case MODE_QUERY:
            return start_query(&config);
        default:
            fprintf(stderr, "Error: Unknown operation mode.\n");
            return 1;
    }

    return 0;
}