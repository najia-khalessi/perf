#include "../include/hash.h"
#include "../include/header.h"
#include <string.h>

// 简易可迭代哈希表实现：分离链接 + 字符串键
// 该实现满足当前Trie遍历需求，后续可替换为更高效版本。

// 哈希函数 - 用于PID
unsigned int hash_pid(int pid) {
    return pid % HASHTABLE_SIZE;
}

// 哈希函数 - 用于字符串
unsigned int hash_str(const char *str) {
    unsigned int hash = 0;
    while (*str) {
        hash = (hash << 5) - hash + *str++;
    }
    return hash % HASHTABLE_SIZE;
}

struct hash_node {
    char* key;
    void* value;
    struct hash_node* next;
};

struct hash_table {
    size_t bucket_count;
    struct hash_node** buckets;
};

static unsigned int bucket_index(struct hash_table* t, const char* key) {
    (void)t; // bucket_count与HASHTABLE_SIZE保持一致，使用hash_str散列
    return hash_str(key);
}

struct hash_table* hash_table_create(size_t size) {
    struct hash_table* t = (struct hash_table*)calloc(1, sizeof(struct hash_table));
    if (!t) return NULL;
    t->bucket_count = HASHTABLE_SIZE; // 固定桶数，与全局常量一致
    t->buckets = (struct hash_node**)calloc(t->bucket_count, sizeof(struct hash_node*));
    if (!t->buckets) {
        free(t);
        return NULL;
    }
    return t;
}

void hash_table_destroy(struct hash_table* table) {
    if (!table) return;
    for (size_t i = 0; i < table->bucket_count; ++i) {
        struct hash_node* node = table->buckets[i];
        while (node) {
            struct hash_node* next = node->next;
            free(node->key);
            // 注意：不释放value，由调用方管理生命周期
            free(node);
            node = next;
        }
    }
    free(table->buckets);
    free(table);
}

int hash_table_insert(struct hash_table* table, const char* key, void* value) {
    if (!table || !key) return -1;
    unsigned int idx = bucket_index(table, key);
    struct hash_node* node = table->buckets[idx];
    while (node) {
        if (strcmp(node->key, key) == 0) {
            node->value = value;
            return 0;
        }
        node = node->next;
    }
    struct hash_node* new_node = (struct hash_node*)calloc(1, sizeof(struct hash_node));
    if (!new_node) return -1;
    new_node->key = strdup(key);
    if (!new_node->key) { free(new_node); return -1; }
    new_node->value = value;
    new_node->next = table->buckets[idx];
    table->buckets[idx] = new_node;
    return 0;
}

void* hash_table_lookup(struct hash_table* table, const char* key) {
    if (!table || !key) return NULL;
    unsigned int idx = bucket_index(table, key);
    struct hash_node* node = table->buckets[idx];
    while (node) {
        if (strcmp(node->key, key) == 0) {
            return node->value;
        }
        node = node->next;
    }
    return NULL;
}

int hash_table_remove(struct hash_table* table, const char* key) {
    if (!table || !key) return -1;
    unsigned int idx = bucket_index(table, key);
    struct hash_node* node = table->buckets[idx];
    struct hash_node* prev = NULL;
    while (node) {
        if (strcmp(node->key, key) == 0) {
            if (prev) prev->next = node->next; else table->buckets[idx] = node->next;
            free(node->key);
            free(node);
            return 0;
        }
        prev = node;
        node = node->next;
    }
    return -1;
}

// 迭代器实现
void hash_iter_init(struct hash_table* table, struct hash_iter* iter) {
    iter->table = table;
    iter->index = 0;
    iter->_node = NULL;
}

bool hash_iter_next(struct hash_iter* iter, const char** key, void** value) {
    if (!iter || !iter->table) return false;
    struct hash_node* node = (struct hash_node*)iter->_node;
    if (node) {
        // 继续当前桶
        *key = node->key;
        *value = node->value;
        iter->_node = node->next;
        return true;
    }
    // 前进到下一个非空桶
    while (iter->index < iter->table->bucket_count) {
        struct hash_node* head = iter->table->buckets[iter->index++];
        if (head) {
            *key = head->key;
            *value = head->value;
            iter->_node = head->next;
            return true;
        }
    }
    return false;
}