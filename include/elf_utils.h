#ifndef ELF_UTILS_H
#define ELF_UTILS_H

#include "header.h"

// This header aggregates all function prototypes needed for symbolization
// to be shared between live mode (handler.c) and query mode (query.c).

// from process.c (align with actual implementations)
struct process_info* find_process(struct process_hash_table* table, int pid);
struct process_info* find_new_process(struct process_hash_table* process_table, int pid);
struct virtual_memory_area* find_vma_from_process(struct process_info* proc, uint64_t addr);

// from vma.c
uint64_t get_relative_address(uint64_t real_addr, struct virtual_memory_area* vma);

// from elf.c
struct elf_file* find_or_create_elf(struct system_context* sys, int pid, const char* file_path);

// from symbol_table.c
const char* find_symbol_name_from_elf(struct elf_file* elf, uint64_t relative_addr);

// A top-level symbolization function
int find_symbol_for_address(struct system_context* sys, int pid, uint64_t addr, struct symbol_info* result);

// Kernel address check
static inline bool is_kernel_addr(uint64_t addr) {
    return addr >= 0xffffffff80000000;
}

#endif // ELF_UTILS_H
