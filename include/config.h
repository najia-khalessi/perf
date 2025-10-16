#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <sys/types.h>

// 过滤模式
enum filter_mode {
    FILTER_ALL,     // 显示全部
    FILTER_USER,    // 仅用户态
    FILTER_KERNEL   // 仅内核态
};

// 监控目标模式
enum profiling_target_mode {
    TARGET_MODE_SYSTEM,    // 系统级监控
    TARGET_MODE_PID,       // 单目标PID
    TARGET_MODE_EXEC,      // 单目标可执行文件
    TARGET_MODE_MULTI_PID, // 多目标PID
    TARGET_MODE_MULTI_EXEC,// 多目标可执行文件
    TARGET_MODE_NONE,      // 未指定
};

// 主操作模式（仅保留文档定义的两种模式）
enum operation_mode {
    MODE_COLLECT,   // 仅数据采集
    MODE_QUERY,     // 离线查询与分析
    MODE_HELP       // 显示帮助
};

// 性能分析配置
extern struct profiling_config {
    enum operation_mode op_mode;            // 主操作模式
    enum profiling_target_mode target_mode; // 监控目标模式
    
    // 采集模式通用配置
    pid_t target_pid;                   // 目标进程PID
    char* target_exec;                  // 目标可执行文件路径
    pid_t* target_pids;                 // 多目标PID列表
    int num_target_pids;                // 多目标PID数量
    char** target_execs;                // 多目标可执行文件列表
    int num_target_execs;               // 多目标可执行文件数量
    int sampling_frequency;             // 采样频率 (Hz)
    enum filter_mode filter_mode;       // 显示过滤模式
    int cleanup_interval;               // 死进程清理间隔 (秒)
    int max_stack_depth;                // 最大栈回溯深度
    bool use_lbr;                       // 是否启用LBR
    int collection_duration; // 采集时长（秒），0表示手动停止

    // 采集模式配置
    char* collection_output_path;       // 采集数据输出路径 (SQLite DB)

    // 查询模式配置
    char* query_input_path;             // 查询数据输入路径 (SQLite DB)
    long long query_start_time;         // 查询起始时间 (Unix timestamp)
    long long query_end_time;           // 查询结束时间 (Unix timestamp)
    char* flamegraph_output_path;       // 火焰图输出路径 (SVG)

    // 通用配置
    bool verbose;                       // 详细输出
    bool flamegraph_mode;               // 火焰图模式
} global_config;

// 函数声明
void print_usage(const char* program_name);
int parse_command_line(int argc, char* argv[], struct profiling_config* config);
int validate_config(struct profiling_config* config);

// 默认配置
#define DEFAULT_SAMPLING_FREQUENCY 30    // 30Hz
#define DEFAULT_CLEANUP_INTERVAL 30      // 30秒
#define DEFAULT_MAX_STACK_DEPTH 127      // 默认栈深度，上限接近内核允许的最大值
#define MAX_TARGETS 32                   // 最大目标进程数

#endif // CONFIG_H
