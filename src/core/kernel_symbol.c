#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "../../include/kernel_symbol.h"
#include "../../include/rbtree.h"

// 定义 kallsyms 文件路径
#define KALLSYMS_PATH "/proc/kallsyms"

static bool insert_symbol(struct rb_root *root, struct kernel_symbol *new_symbol) {
    struct rb_node **link = &root->rb_node;
    struct rb_node *parent = NULL;

    while (*link) {
        parent = *link;
        struct kernel_symbol *entry = rb_entry(parent, struct kernel_symbol, node);
        if (new_symbol->address < entry->address) {
            link = &(*link)->rb_left;
        } else if (new_symbol->address > entry->address) {
            link = &(*link)->rb_right;
        } else {
            return false;
        }
    }

    rb_link_node(&new_symbol->node, parent, link);
    rb_insert_color(&new_symbol->node, root);
    return true;
}

struct rb_root *load_kernel_symbols(void) {
    FILE *file = fopen(KALLSYMS_PATH, "r");
    if (!file) {
        perror("错误：无法打开 /proc/kallsyms");
        return NULL;
    }

    struct rb_root *root = malloc(sizeof(struct rb_root));
    if (!root) {
        fclose(file);
        return NULL;
    }
    *root = RB_ROOT;

    char *line = NULL;
    size_t len = 0;

    while (getline(&line, &len, file) != -1) {
        unsigned long address;
        char type;
        char name[256];

        if (sscanf(line, "%lx %c %255s", &address, &type, name) == 3) {
            if (type == 't' || type == 'T') {
                struct kernel_symbol *sym = malloc(sizeof(struct kernel_symbol));
                if (!sym) continue;

                sym->name = strdup(name);
                if (!sym->name) {
                    free(sym);
                    continue;
                }
                sym->address = address;
                sym->type = type;

                if (!insert_symbol(root, sym)) {
                    free(sym->name);
                    free(sym);
                }
            }
        }
    }

    free(line);
    fclose(file);
    return root;
}

const char *find_kernel_symbol(struct rb_root *root, uint64_t addr) {
    if (!root || !root->rb_node) {
        return "unknown_kernel_function";
    }

    struct rb_node *node = root->rb_node;
    struct kernel_symbol *found_symbol = NULL;

    while (node) {
        struct kernel_symbol *current_symbol = rb_entry(node, struct kernel_symbol, node);
        if (addr >= current_symbol->address) {
            found_symbol = current_symbol;
            node = node->rb_right;
        } else {
            node = node->rb_left;
        }
    }

    if (found_symbol) {
        return found_symbol->name;
    }

    return "unknown_kernel_function";
}

void free_kernel_symbols(struct rb_root *root) {
    if (!root) return;

    struct rb_node *node = rb_first(root);
    while (node) {
        struct kernel_symbol *sym = rb_entry(node, struct kernel_symbol, node);
        rb_erase(node, root);
        free(sym->name);
        free(sym);
        node = rb_first(root);
    }
    free(root);
}