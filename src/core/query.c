#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#include "../../include/header.h"
#include "../../include/config.h"
#include "../../include/hash.h" // For symbol aggregation
#include "../../include/elf_utils.h"

// Helper to symbolize a single IP for the query mode
static void symbolize_ip_for_query(struct system_context* sys, int pid, uint64_t ip, char* buffer, size_t buffer_size) {
    if (is_kernel_addr(ip)) {
        const char* symbol = find_kernel_symbol(sys->kernel_symbols, ip);
        snprintf(buffer, buffer_size, "%s", symbol ? symbol : "[unknown_kernel]");
    } else {
        struct symbol_info result;
        if (find_symbol_for_address(sys, pid, ip, &result) == 0) {
            snprintf(buffer, buffer_size, "%s in %s", result.symbol_name, result.file_path);
        } else {
            snprintf(buffer, buffer_size, "0x%llx in [unknown_module]", (unsigned long long)ip);
        }
    }
}

// 临时的前缀树(Trie)节点定义，用于聚合调用栈
struct trie_node {
    char* symbol;
    long long count;
    struct hash_table* children;
    struct trie_node* parent;
};

static struct trie_node* create_trie_node(const char* symbol, struct trie_node* parent) {
    struct trie_node* node = calloc(1, sizeof(struct trie_node));
    node->symbol = symbol ? strdup(symbol) : NULL;
    node->count = 0;
    node->children = hash_table_create(16); // Start with a small hash table for children
    node->parent = parent;
    return node;
}

static void free_trie(struct trie_node* node) {
    if (!node) return;
    if (node->symbol) free(node->symbol);
    // 递归释放所有子节点（hash迭代器支持）
    if (node->children) {
        struct hash_iter iter;
        hash_iter_init(node->children, &iter);
        const char* key;
        void* value;
        while (hash_iter_next(&iter, &key, &value)) {
            struct trie_node* child = (struct trie_node*)value;
            free_trie(child);
        }
        hash_table_destroy(node->children);
    }
    free(node);
}

static void print_folded_stack(FILE* f, struct trie_node* node) {
    if (node->count > 0) {
        char buffer[16384] = {0}; // Increased buffer size to prevent overflow
        char* current = buffer + sizeof(buffer) - 1;
        *current = '\0';

        struct trie_node* temp = node;
        while (temp && temp->parent) { // Stop at root's children
            size_t len = strlen(temp->symbol);
            current -= len;
            memcpy(current, temp->symbol, len);
            if (temp->parent->parent) { // Add semicolon if not the top-level function
                current--;
                *current = ';';
            }
            temp = temp->parent;
        }
        fprintf(f, "%s %lld\n", current, node->count);
    }

    // 遍历子节点并递归打印
    if (node->children) {
        struct hash_iter iter;
        hash_iter_init(node->children, &iter);
        const char* key;
        void* value;
        while (hash_iter_next(&iter, &key, &value)) {
            struct trie_node* child = (struct trie_node*)value;
            print_folded_stack(f, child);
        }
    }
}


int start_query(struct profiling_config* config) {
    sqlite3* db;
    if (sqlite3_open_v2(config->query_input_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        fprintf(stderr, "Error: Can't open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    // 注意：raw_samples.timestamp 以纳秒记录，而命令行 --from/--to 解析为秒。
    // 为了正确查询区间，需要将秒转换为纳秒。
    char sql[1024];
    long long start_ns = config->query_start_time * 1000000000LL;
    long long end_ns = config->query_end_time * 1000000000LL;
    snprintf(sql, sizeof(sql), 
             "SELECT pid, callstack FROM raw_samples WHERE timestamp BETWEEN %lld AND %lld;",
             start_ns, end_ns);

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "Error: Failed to prepare SQL statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    printf("Processing data between %lld and %lld...\n", config->query_start_time, config->query_end_time);

    // 初始化符号化所需系统上下文
    struct system_context sys;
    if (initialize_system(&sys)) {
        fprintf(stderr, "Error: Failed to initialize system context for query.\n");
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return 1;
    }

    struct trie_node* root = create_trie_node(NULL, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int pid = sqlite3_column_int(stmt, 0);
        const char* raw_callstack = (const char*)sqlite3_column_text(stmt, 1);
        if (!raw_callstack) continue;

        struct trie_node* current_node = root;

        char* stack_copy = strdup(raw_callstack);
        char* token = strtok(stack_copy, ";");
        
        while (token != NULL) {
            uint64_t ip = strtoull(token, NULL, 16);
            
            char symbol_buffer[1024];
            symbolize_ip_for_query(&sys, pid, ip, symbol_buffer, sizeof(symbol_buffer));

            // 过滤掉不需要显示的符号
            if (strstr(symbol_buffer, "__SCT__tp_func_tls_device_tx_resync_send") != NULL) {
                token = strtok(NULL, ";");
                continue;
            }

            // Find or insert into trie
            struct trie_node* next_node = hash_table_lookup(current_node->children, symbol_buffer);
            if (!next_node) {
                next_node = create_trie_node(symbol_buffer, current_node);
                hash_table_insert(current_node->children, symbol_buffer, next_node);
            }
            current_node = next_node;

            token = strtok(NULL, ";");
        }
        current_node->count++; // Increment count at the end of the stack
        free(stack_copy);
    }

    printf("Data processed. Generating flamegraph...\n");

    char command[2048];
    snprintf(command, sizeof(command), "third_party/flamegraph.pl > %s", config->flamegraph_output_path);
    
    FILE* pipe = popen(command, "w");
    if (!pipe) {
        fprintf(stderr, "Error: Failed to run flamegraph.pl script.\n");
        cleanup_system(&sys);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return 1;
    }

    // 遍历Trie并输出folded stacks
    print_folded_stack(pipe, root);

    pclose(pipe);
    free_trie(root);
    cleanup_system(&sys);
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    printf("Flamegraph generated at: %s\n", config->flamegraph_output_path);

    return 0;
}
