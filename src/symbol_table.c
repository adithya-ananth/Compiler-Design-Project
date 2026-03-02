#include "symbol_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Scope *current_scope = NULL;

static Scope *all_scopes = NULL;

unsigned int hash(char *key) {
    unsigned int h = 0;
    while (*key)
        h = (h << 4) + *key++;
    return h % TABLE_SIZE;
}

void enter_scope() {
    Scope *new_scope = malloc(sizeof(Scope));
    memset(new_scope->table, 0, sizeof(new_scope->table));

    new_scope->parent = current_scope;
    new_scope->level = current_scope ? current_scope->level + 1 : 0;

    // add to all_scopes list
    new_scope->next_scope = all_scopes;
    all_scopes = new_scope;

    current_scope = new_scope;
}


void free_symbol_list(Symbol *sym) {
    while (sym) {
        Symbol *temp = sym;
        sym = sym->next;

        free(temp->name);
        if (temp->param_types)
            free(temp->param_types);
        free(temp);
    }
}

void exit_scope() {
    if (!current_scope) return;

    /*Scope *temp = current_scope;
    for (int i = 0; i < TABLE_SIZE; i++) {
        free_symbol_list(temp->table[i]);
    }
    */
    current_scope = current_scope->parent;

    // Optional: free all symbols
    // free(temp);
}

void init_symbol_table() {
    enter_scope();   // global scope
}


Symbol *create_symbol(char *name, DataType type,
                      SymbolKind kind, int line) {

    Symbol *sym = malloc(sizeof(Symbol));
    sym->name = strdup(name);
    sym->type = type;
    sym->kind = kind;
    sym->line_number = line;
    sym->scope_level = current_scope->level;
    sym->param_count = 0;
    sym->param_types = NULL;
    sym->next = NULL;

    return sym;
}

int insert_symbol(Symbol *sym) {
    if (!current_scope) return 0;
    unsigned int index = hash(sym->name);

    Symbol *head = current_scope->table[index];

    // Check redeclaration in same scope
    while (head) {
        if (strcmp(head->name, sym->name) == 0) {
            return 0; // already exists
        }
        head = head->next;
    }

    sym->next = current_scope->table[index];
    current_scope->table[index] = sym;

    return 1; // success
}

Symbol *lookup_current(char *name) {
    if(!current_scope) return NULL;
    unsigned int index = hash(name);
    Symbol *sym = current_scope->table[index];

    while (sym) {
        if (strcmp(sym->name, name) == 0)
            return sym;
        sym = sym->next;
    }
    return NULL;
}

Symbol *lookup(char *name) {
    Scope *scope = current_scope;

    while (scope) {
        unsigned int index = hash(name);
        Symbol *sym = scope->table[index];

        while (sym) {
            if (strcmp(sym->name, name) == 0)
                return sym;
            sym = sym->next;
        }

        scope = scope->parent;
    }

    return NULL;
}

const char* data_type_to_string(DataType type) {
    switch(type) {
        case TYPE_INT:  return "int";
        case TYPE_CHAR: return "char";
        case TYPE_VOID: return "void";
        default:        return "unknown";
    }
}

const char* symbol_kind_to_string(SymbolKind kind) {
    switch(kind) {
        case SYM_VARIABLE:  return "variable";
        case SYM_FUNCTION:  return "function";
        case SYM_PARAMETER: return "parameter";
        case SYM_CONSTANT:  return "constant";
        case SYM_KEYWORD:   return "keyword";
        default:            return "unknown";
    }
}


void print_scope(Scope *scope) {
    printf("Scope Level: %d\n", scope->level);

    for (int i = 0; i < TABLE_SIZE; i++) {
        Symbol *sym = scope->table[i];
        while (sym) {
            printf("Name: %-10s | Type: %-6s | Kind: %-9s | Line: %d | Scope: %d\n",
            sym->name,
            data_type_to_string(sym->type),
            symbol_kind_to_string(sym->kind),
            sym->line_number,
            sym->scope_level);
            sym = sym->next;
        }
    }
}

void print_symbol_table() {
    printf("\n=========== SYMBOL TABLE ===========\n");

    Scope *scope = all_scopes;

    while (scope) {
        print_scope(scope);
        printf("------------------------------------\n");
        scope = scope->next_scope;
    }

    printf("====================================\n");
}
