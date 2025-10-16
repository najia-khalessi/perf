#ifndef HASH_H
#define HASH_H

#include <stdint.h>
#include <stdlib.h> // For size_t
#include <stdbool.h>

// Forward declaration for opaque pointer
struct hash_table;

// Hash Functions
unsigned int hash_pid(int pid);
unsigned int hash_str(const char* str);
uint64_t hash_file(const char* filename);

// Generic Hash Table API (string key -> void* value)
struct hash_table* hash_table_create(size_t size);
void hash_table_destroy(struct hash_table* table);
int hash_table_insert(struct hash_table* table, const char* key, void* value);
void* hash_table_lookup(struct hash_table* table, const char* key);
int hash_table_remove(struct hash_table* table, const char* key);

// Iterator API for traversing all entries in the table
struct hash_iter {
    struct hash_table* table;
    size_t index;
    void* _node; // internal pointer, opaque to users
};

void hash_iter_init(struct hash_table* table, struct hash_iter* iter);
bool hash_iter_next(struct hash_iter* iter, const char** key, void** value);

#endif // HASH_H
