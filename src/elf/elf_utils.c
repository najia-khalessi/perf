#include "../../include/elf_utils.h"

// This is the unified, top-level function for address symbolization.
int find_symbol_for_address(struct system_context* sys, int pid, uint64_t addr, struct symbol_info* result) {
    // 1. Find or create the process info
    if (!sys || !sys->process_table) return -1;
    struct process_info* proc = find_new_process(sys->process_table, pid);
    if (!proc) {
        return -1;
    }

    // 2. Find the VMA for the address
    struct virtual_memory_area* vma = find_vma_from_process(proc, addr);
    if (!vma || !vma->mapping_name || vma->mapping_name[0] == '[') {
        return -1; // Not found or anonymous mapping
    }

    // 3. Calculate the relative address
    uint64_t relative_addr = get_relative_address(addr, vma);

    // 4. Find or load the ELF file (pid-aware)
    struct elf_file* elf = find_or_create_elf(sys, pid, vma->mapping_name);
    if (!elf) {
        return -1;
    }

    // 5. Find the symbol name in the ELF file
    const char* name = find_symbol_name_from_elf(elf, relative_addr);
    if (name) {
        result->symbol_name = strdup(name);
        result->file_path = strdup(elf->file_path); // Populate the file_path
        // Note: The caller is responsible for freeing symbol_name and file_path
        return 0; // Success
    }

    return -1; // Not found
}
