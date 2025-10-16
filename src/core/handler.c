/**
 * @file handler.c
 * @brief 采样数据处理核心模块
 * 
 * 负责将perf_event产生的原始采样数据转换为可读的性能信息。
 * 实现了完整的地址到符号的转换流程：
 * 
 * 数据流：
 * sample_data → 进程查找 → VMA定位 → ELF解析 → 符号名称
 * 
 * 关键技术：
 * - 进程过滤：根据配置只处理目标进程
 * - VMA树查找：使用红黑树快速定位地址所属内存区域
 * - ELF缓存：避免重复解析同一ELF文件
 * - 符号解析：将运行时地址转换为函数名
 * 
 * 过滤策略：
 * - 系统模式：处理所有进程
 * - 目标模式：只处理指定PID的进程
 * - 多目标模式：只处理指定PID列表中的进程
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "../../include/header.h"
#include "../../include/config.h"
#include "../../include/database.h"
#include "../../include/buffer.h"
#include "../../include/elf_utils.h"

// PERF_CONTEXT_MAX 是有效IP地址的上限。
// 超过此值的地址是上下文标记。此值来自内核UAPI
#ifndef PERF_CONTEXT_MAX
#define PERF_CONTEXT_MAX ((__u64)-4095)
#endif

// 已移除live模式相关文件输出逻辑，采样仅在collect模式缓存原始调用栈

/**
 * @brief 解析来自perf_event_header的原始采样数据。
 * @param header 指向环形缓冲区中perf_event_header的指针。
 * @param result 指向要填充的callchain_result结构体的指针。
 *
 * 此函数根据配置的sample_type解释头部之后的数据。
 * 它能处理软件调用链和LBR分支栈。
 */
void parse_sample_data(struct perf_event_header *header, struct callchain_result *result, uint64_t max_ips) {
    extern struct profiling_config global_config;
    
    // 安全地初始化结构体成员，而不是使用memset，以保护由调用者设置的ips指针。
    result->pid = 0;
    result->tid = 0;
    result->ip = 0;
    result->nr = 0;

    // 设置安全边界，所有读取都不能超过这个指针
    char *end_ptr = (char *)header + header->size;
    char *ptr = (char *)header + sizeof(struct perf_event_header);

    // 1. 安全地读取 IP
    if (ptr + sizeof(uint64_t) > end_ptr) return;
    result->ip = *(uint64_t *)ptr;
    ptr += sizeof(uint64_t);

    // 2. 安全地读取 PID/TID
    if (ptr + sizeof(uint64_t) > end_ptr) return;
    result->pid = *(uint32_t *)ptr;
    result->tid = *(uint32_t *)(ptr + 4);
    ptr += sizeof(uint64_t);

    if (global_config.use_lbr) {
        // LBR 模式: 安全地解析分支栈
        if (ptr + sizeof(uint64_t) > end_ptr) return;
        uint64_t nr_from_data = *(uint64_t *)ptr;
        ptr += sizeof(uint64_t);

        // 根据剩余字节和最大深度，计算实际要复制的分支数量
        uint64_t remaining_bytes = end_ptr - ptr;
        uint64_t nr_from_size = remaining_bytes / sizeof(struct perf_branch_entry);
        uint64_t nr = (nr_from_data < nr_from_size) ? nr_from_data : nr_from_size;
        uint64_t count_to_copy = (nr < max_ips) ? nr : max_ips;

        if (global_config.verbose) {
            fprintf(stderr, "[DEBUG] LBR Mode: nr_from_data=%llu, nr_from_size=%llu, nr=%llu, max_ips=%llu, count_to_copy=%llu\n",
                    (unsigned long long)nr_from_data, (unsigned long long)nr_from_size, (unsigned long long)nr, (unsigned long long)max_ips, (unsigned long long)count_to_copy);
        }

        result->nr = 0;
        struct perf_branch_entry *branches = (struct perf_branch_entry *)ptr;
        for (uint64_t i = 0; i < count_to_copy; i++) {
            // 我们只关心分支的来源地址 'from'，它构成了调用栈
            if (branches[i].from) {
                result->ips[result->nr++] = branches[i].from;
            }
        }
        if (global_config.verbose) {
            fprintf(stderr, "[DEBUG] LBR Mode: Final result->nr=%llu\n", (unsigned long long)result->nr);
        }
    } else {
        // 软件模式: 安全地解析调用栈
        if (ptr + sizeof(uint64_t) > end_ptr) return;
        uint64_t nr_from_data = *(uint64_t *)ptr;
        ptr += sizeof(uint64_t);

        // 根据记录总大小计算真实的调用栈深度，防止读取垃圾值
        uint64_t remaining_bytes = end_ptr - ptr;
        uint64_t nr_from_size = remaining_bytes / sizeof(uint64_t);

        // 取两个nr中较小的一个，并确保不超过我们自己的缓冲区大小
        uint64_t nr = (nr_from_data < nr_from_size) ? nr_from_data : nr_from_size;
        uint64_t count_to_copy = (nr < max_ips) ? nr : max_ips;
        
        if (global_config.verbose) {
            fprintf(stderr, "[DEBUG] Software Mode: nr_from_data=%llu, nr_from_size=%llu, nr=%llu, max_ips=%llu, count_to_copy=%llu\n",
                    (unsigned long long)nr_from_data, (unsigned long long)nr_from_size, (unsigned long long)nr, (unsigned long long)max_ips, (unsigned long long)count_to_copy);
        }

        result->nr = count_to_copy;
        if (count_to_copy > 0) {
            memcpy(result->ips, ptr, count_to_copy * sizeof(uint64_t));
        }
        if (global_config.verbose) {
            fprintf(stderr, "[DEBUG] Software Mode: Final result->nr=%llu\n", (unsigned long long)result->nr);
        }
    }
}

/**
 * @brief 对完整的调用链进行符号化并打印结果。
 * @param sys 系统上下文，包含进程哈希表和ELF缓存。
 * @param callchain 解析后的调用链数据，包括PID、IP和栈。 
 *
 * 此函数遍历调用链中的每个地址，将其解析为符号（函数名），
 * 并打印符号化的栈回溯。
 */
// 辅助函数：符号化调用链并格式化为字符串
static char* symbolize_and_format_callstack(struct system_context *sys, struct callchain_result *callchain) {
    size_t buffer_size = 8192; // 初始大小
    char* result = malloc(buffer_size);
    if (!result) return NULL;

    char* current = result;
    size_t remaining = buffer_size;
    *current = '\0';

    for (uint64_t i = 0; i < callchain->nr; i++) {
        uint64_t addr = callchain->ips[i];
        if (addr >= PERF_CONTEXT_MAX) continue;

        const char* symbol_str = NULL;
        char* temp_buffer = NULL;
        struct symbol_info s_info = {0}; // 在循环的开始处声明并初始化

        if (is_kernel_addr(addr)) {
            const char* symbol = find_kernel_symbol(sys->kernel_symbols, addr);
            if (symbol) {
                size_t len = strlen(symbol) + 10;
                temp_buffer = malloc(len);
                snprintf(temp_buffer, len, "%s[内核]", symbol);
                symbol_str = temp_buffer;
            } else {
                temp_buffer = malloc(64);
                snprintf(temp_buffer, 64, "0x%llx[内核]", (unsigned long long)addr);
                symbol_str = temp_buffer;
            }
        } else {
            if (find_symbol_for_address(sys, callchain->pid, addr, &s_info) == 0) {
                symbol_str = s_info.symbol_name;
            } else {
                temp_buffer = malloc(64);
                snprintf(temp_buffer, 64, "0x%llx", (unsigned long long)addr);
                symbol_str = temp_buffer;
            }
        }

        while (true) {
            int written = snprintf(current, remaining, "%s%s", (i > 0 ? ";" : ""), symbol_str);
            if (written < 0) {
                // 编码错误
                free(result);
                if (temp_buffer) free(temp_buffer);
                if (s_info.symbol_name) free(s_info.symbol_name);
                if (s_info.file_path) free(s_info.file_path);
                return NULL;
            }

            if (written < remaining) {
                // 成功
                current += written;
                remaining -= written;
                break;
            }
            
            // 空间不足，扩容
            size_t new_size = buffer_size * 2;
            char* new_buffer = realloc(result, new_size);
            if (!new_buffer) {
                free(result);
                if (temp_buffer) free(temp_buffer);
                if (s_info.symbol_name) free(s_info.symbol_name);
                if (s_info.file_path) free(s_info.file_path);
                return NULL;
            }
            current = new_buffer + (current - result);
            result = new_buffer;
            remaining = new_size - (current - result);
            buffer_size = new_size;
        }
        
        if (temp_buffer) {
            free(temp_buffer);
        }
        

    }

    return result;
}

// 辅助函数：为火焰图格式化调用栈
static char* format_for_flamegraph(struct system_context *sys, struct callchain_result *callchain) {
    size_t buffer_size = 4096; // 初始大小
    char* result = malloc(buffer_size);
    if (!result) return NULL;

    char* current = result;
    size_t remaining = buffer_size;
    *current = '\0';

    // 1. 获取进程名
    struct process_info* pinfo = find_new_process(sys->process_table, callchain->pid);
    const char* proc_name = pinfo ? pinfo->process_name : "unknown";
    
    int written = snprintf(current, remaining, "%s", proc_name);
    if (written < 0 || written >= remaining) {
        free(result);
        return NULL;
    }
    current += written;
    remaining -= written;

    // 2. 遍历调用栈并符号化
    for (uint64_t i = 0; i < callchain->nr; i++) {
        uint64_t addr = callchain->ips[i];
        if (addr >= PERF_CONTEXT_MAX) continue;

        const char* symbol_str = NULL;
        char* temp_buffer = NULL;
        struct symbol_info s_info = {0};

        if (is_kernel_addr(addr)) {
            const char* symbol = find_kernel_symbol(sys->kernel_symbols, addr);
            if (symbol) {
                size_t len = strlen(symbol) + 3; // for ";k"
                temp_buffer = malloc(len);
                if(temp_buffer) snprintf(temp_buffer, len, ";%s", symbol);
            } else {
                temp_buffer = malloc(64);
                if(temp_buffer) snprintf(temp_buffer, 64, ";0x%llx", (unsigned long long)addr);
            }
            symbol_str = temp_buffer;
        } else {
            if (find_symbol_for_address(sys, callchain->pid, addr, &s_info) == 0) {
                size_t len = strlen(s_info.symbol_name) + 2;
                temp_buffer = malloc(len);
                if(temp_buffer) snprintf(temp_buffer, len, ";%s", s_info.symbol_name);
                symbol_str = temp_buffer;
                free(s_info.symbol_name); 
                if (s_info.file_path) free(s_info.file_path);
            } else {
                temp_buffer = malloc(64);
                if(temp_buffer) snprintf(temp_buffer, 64, ";0x%llx", (unsigned long long)addr);
                symbol_str = temp_buffer;
            }
        }
        
        if (!symbol_str) {
            symbol_str = ";unknown_symbol";
        }

        size_t symbol_len = strlen(symbol_str);

        if (remaining < symbol_len + 1) {
            size_t new_size = buffer_size * 2;
            char* new_buffer = realloc(result, new_size);
            if (!new_buffer) {
                free(result);
                if (temp_buffer) free(temp_buffer);
                return NULL;
            }
            current = new_buffer + (current - result);
            result = new_buffer;
            remaining = new_size - (current - result);
            buffer_size = new_size;
        }
        
        memcpy(current, symbol_str, symbol_len);
        current += symbol_len;
        remaining -= symbol_len;

        if (temp_buffer) {
            free(temp_buffer);
        }
    }
    *current = '\0'; // Ensure null termination

    return result;
}

void symbolize_sample(struct system_context *sys, struct callchain_result *callchain, struct ring_buffer *buffer, const struct profiling_config* config) {
    // 火焰图模式：直接打印折叠后的调用栈
    if (config->flamegraph_mode) {
        if (callchain->nr > 0) {
            char* formatted_stack = format_for_flamegraph(sys, callchain);
            if (formatted_stack) {
                printf("%s 1\n", formatted_stack);
                free(formatted_stack);
            }
        }
        return;
    }

    // collect 模式：进行符号化并记录
    if (config->op_mode == MODE_COLLECT) {
        if (callchain->nr > 0 && buffer) {
            struct buffer_entry entry;
            entry.pid = callchain->pid;
            entry.tid = callchain->tid;
            
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            entry.timestamp = (long long)ts.tv_sec * 1000000000 + ts.tv_nsec;

            entry.stack = symbolize_and_format_callstack(sys, callchain);
            if (entry.stack) {
                buffer_push(buffer, &entry);
                // buffer_push拥有所有权，不需要在这里释放
            }
        }
        return;
    }

    // query模式不在采集时进行符号化，离线分析由 src/core/query.c 完成
}