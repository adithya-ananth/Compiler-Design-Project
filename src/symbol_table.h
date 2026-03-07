#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H


#define TABLE_SIZE 200 // size of the scope table

typedef enum {
    TYPE_INT,
    TYPE_CHAR,
    TYPE_VOID
} DataType;

typedef enum {
    SYM_VARIABLE,
    SYM_FUNCTION,
    SYM_PARAMETER,
    SYM_CONSTANT,
    SYM_KEYWORD
} SymbolKind;

typedef struct Symbol {
    char *name;
    DataType type;
    SymbolKind kind;

    int line_number;
    int scope_level;

    /* Array information (for variables/parameters) */
    /* is_array: 1 if this symbol represents an array object */
    int is_array;
    /* array_size:
     *   > 0  => fixed-size array with that many elements
     *   0    => not an array
     *   -1   => array parameter declared with [], size not specified
     */
    int array_size;

    // New: pointer and multi-dim array info
    int pointer_level;
    int array_dim_count;
    int *array_sizes; // for fixed sizes
    struct ASTNode **array_dim_exprs; // for VLAs

    // For functions
    int param_count;
    DataType *param_types;
    /* For functions: per-parameter array flag
     *   param_is_array[i] == 1 if the i-th parameter is an array
     *   (e.g., declared as T a[]).
     */
    int *param_is_array;

    struct Symbol *next;   // hash chaining
} Symbol;

typedef struct Scope {
    Symbol *table[TABLE_SIZE];
    int level;
    struct Scope *parent;
    struct Scope *next_scope;
} Scope;

extern Scope *current_scope;

unsigned int hash(char *key);
void enter_scope();
void init_symbol_table();
void exit_scope();
void free_symbol_list(Symbol *sym);
Symbol *create_symbol(char *name, DataType type, SymbolKind kind, int line);
int insert_symbol(Symbol *sym);
//to lookup the current scope only
Symbol *lookup_current(char *name);
//lookup all the parent scopes
Symbol *lookup(char *name);

//functions to help print the symbol table
const char* data_type_to_string(DataType type);
const char* symbol_kind_to_string(SymbolKind kind);
void print_scope(Scope *scope);
void print_symbol_table();

#endif  // SYMBOL_TABLE_H
