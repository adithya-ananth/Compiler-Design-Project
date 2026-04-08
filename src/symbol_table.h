#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H


#define TABLE_SIZE 200 // size of the scope table

typedef enum {
    TYPE_INT,
    TYPE_CHAR,
    TYPE_VOID,
    TYPE_STRUCT
} DataType;

typedef enum {
    SYM_VARIABLE,
    SYM_FUNCTION,
    SYM_PARAMETER,
    SYM_CONSTANT,
    SYM_KEYWORD,
    SYM_STRUCT
} SymbolKind;

typedef struct Symbol {
    char *name;
    char *unmangled_name;
    DataType type;
    SymbolKind kind;

    int line_number;
    int scope_level;

    int frame_offset;    // Offset relative to FP (s0)
    int local_vars_size; // For functions: total size of local variables

    /* For struct types: points to struct definition symbol (SYM_STRUCT) */
    struct Symbol *struct_def;
    /* Size in bytes for struct types */
    int struct_size;
    /* List of members for struct definitions */
    struct Symbol *members;
    /* List of virtual methods for struct definitions */
    struct Symbol *virtual_methods;
    /* Size of vtable */
    int vtable_size;
    /* Is class (vs struct) */
    int is_class;
    /* Base class symbol for inheritance */
    struct Symbol *base_class;

    /* Access modifier (0=public, 1=private, 2=protected) */
    int access_modifier;

    /* For member symbols (fields): offset within the struct */
    int struct_offset;

    /* Array information (for variables/parameters) */
    /* is_array: 1 if this symbol represents an array object */
    int is_array;
    /* array_size:
     *   > 0  => fixed-size array with that many elements
     *   0    => not an array
     *   -1   => array parameter declared with [], size not specified
     */
    int array_size;
    int is_vla;  // 1 if this is a variable length array

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
    char **param_names;

    // For functions: virtual flag
    int is_virtual;
    // For virtual functions: index in vtable
    int vtable_index;

    // Address-taken flag for register allocation
    int is_address_taken;

    struct Symbol *next;          // hash chaining
    struct Symbol *next_member;   // linked list for struct/class members
} Symbol;

typedef struct Scope {
    Symbol *table[TABLE_SIZE];
    int level;
    struct Scope *parent;
    struct Scope *next_scope;
} Scope;

extern Scope *current_scope;
extern Scope *all_scopes;

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

Symbol** get_all_structs_with_vtables(int *count);

#endif  // SYMBOL_TABLE_H