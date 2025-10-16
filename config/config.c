#define _XOPEN_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include "../include/config.h"

void print_usage(const char* program_name) {
    printf("Usage: %s <command> [options]\n\n", program_name);
    printf("Commands:\n");
    printf("  collect          Collect performance data for live flamegraph generation.\n");
    printf("  query            Query historical data to generate a flamegraph.\n");
    printf("  help             Show this help message.\n\n");

    printf("Options for 'collect' command:\n");
    printf("  --frequency=<hz>   Sampling frequency (default: %d Hz).\n", DEFAULT_SAMPLING_FREQUENCY);
    printf("  --duration=<secs>  Duration of collection in seconds (default: manual stop with Ctrl+C).\n");
    printf("  --lbr              Enable Last Branch Record (LBR) for precise call stacks.\n");
    printf("  --stack-depth=<n>  Set max stack backtrace depth (default: %d).\n", DEFAULT_MAX_STACK_DEPTH);
    printf("  --filter=<mode>    Collect scope: all|user|kernel (default: all).\n");
    printf("  --output=<path>    Path to save the collected data (e.g., /path/to/perf.db).\n");
    printf("  --flamegraph       Output in folded stack format for live flamegraphs.\n\n");

    printf("Options for 'query' command:\n");
    printf("  --input=<path>     Path to the database to query.\n");
    printf("  --from=<ts>        Start time for query (Unix timestamp).\n");
    printf("  --to=<ts>          End time for query (Unix timestamp).\n");
    printf("  --flamegraph=<path> Path to save the generated flamegraph SVG file.\n\n");
}

static void set_default_config(struct profiling_config* config) {
    config->op_mode = MODE_HELP;
    config->target_mode = TARGET_MODE_SYSTEM;
    config->target_pid = -1;
    config->target_exec = NULL;
    config->target_pids = NULL;
    config->num_target_pids = 0;
    config->target_execs = NULL;
    config->num_target_execs = 0;
    config->sampling_frequency = DEFAULT_SAMPLING_FREQUENCY;
    config->filter_mode = FILTER_ALL;
    config->cleanup_interval = DEFAULT_CLEANUP_INTERVAL;
    config->max_stack_depth = DEFAULT_MAX_STACK_DEPTH;
    config->use_lbr = false;
    config->collection_output_path = NULL;
    config->query_input_path = NULL;
    config->query_start_time = 0;
    config->query_end_time = 0;
    config->flamegraph_output_path = NULL;
    config->verbose = false;
    config->collection_duration = 0; // 默认0表示手动停止
    config->flamegraph_mode = false;
}

int parse_command_line(int argc, char* argv[], struct profiling_config* config) {
    set_default_config(config);

    if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "help") == 0) {
        config->op_mode = MODE_HELP;
        return 0;
    }

    if (strcmp(argv[1], "collect") == 0) {
        config->op_mode = MODE_COLLECT;
        optind = 2;
    } else if (strcmp(argv[1], "query") == 0) {
        config->op_mode = MODE_QUERY;
        optind = 2;
    } else {
        config->op_mode = MODE_HELP;
        optind = 1;
    }

    static struct option long_options[] = {
        {"frequency",    required_argument, 0, 'f'},
        {"duration",     required_argument, 0, 'd'},
        {"lbr",          no_argument,       0, 'l'},
        {"stack-depth",  required_argument, 0, 's'},
        {"filter",       required_argument, 0, 'm'},
        {"verbose",      no_argument,       0, 'v'},
        {"output",       required_argument, 0, 'o'},
        {"input",        required_argument, 0, 'i'},
        {"from",         required_argument, 0, 'a'},
        {"to",           required_argument, 0, 'b'},
        {"flamegraph",   required_argument, 0, 'g'},
        {"help",         no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "f:d:ls:m:vo:i:a:b:g:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'f': config->sampling_frequency = atoi(optarg); break;
            case 'd': config->collection_duration = atoi(optarg); break;
            case 'l': config->use_lbr = true; break;
            case 's': config->max_stack_depth = atoi(optarg); break;
            case 'm': {
                if (strcmp(optarg, "all") == 0) {
                    config->filter_mode = FILTER_ALL;
                } else if (strcmp(optarg, "user") == 0) {
                    config->filter_mode = FILTER_USER;
                } else if (strcmp(optarg, "kernel") == 0) {
                    config->filter_mode = FILTER_KERNEL;
                } else {
                    fprintf(stderr, "Error: invalid --filter value '%s' (expected all|user|kernel)\n", optarg);
                    return -1;
                }
                break;
            }
            case 'v': config->verbose = true; break;
            case 'o': config->collection_output_path = optarg; break;
            case 'i': config->query_input_path = optarg; break;
            case 'a': config->query_start_time = atoll(optarg); break;
            case 'b': config->query_end_time = atoll(optarg); break;
            case 'g': config->flamegraph_output_path = optarg; break;
            case 'h': config->op_mode = MODE_HELP; return 0;
            case '?':
                fprintf(stderr, "Error: Unknown or invalid option.\n");
                return -1;
        }
    }

    return 0;
}

int validate_config(struct profiling_config* config) {
    if (!config) return -1;

    if (config->op_mode == MODE_COLLECT) {
        // 如果未指定输出路径，则默认进入火焰图模式
        if (!config->collection_output_path) {
            config->flamegraph_mode = true;
        }
    } else if (config->op_mode == MODE_QUERY) {
        if (!config->query_input_path || !config->flamegraph_output_path || config->query_start_time <= 0 || config->query_end_time <= 0) {
            fprintf(stderr, "Error: 'query' mode requires --input, --from, --to, and --flamegraph=<path>.\n");
            return -1;
        }
        if (config->query_start_time >= config->query_end_time) {
            fprintf(stderr, "Error: --from time must be earlier than --to time.\n");
            return -1;
        }
    }
    
    if (config->op_mode == MODE_COLLECT) {
        if (config->sampling_frequency <= 0) {
            fprintf(stderr, "Error: Invalid sampling frequency: %d\n", config->sampling_frequency);
            return -1;
        }
    }

    return 0;
}
