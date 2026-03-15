#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol_table.h"
#include "ast.h"
#include "semantic.h"

static Symbol *current_function = NULL;
int semantic_errors = 0;
static int break_context_depth = 0;
static int loop_context_depth = 0;

static int current_local_offset = 0;
static int current_param_offset = 16; /* standard positive offset for arg passing */

int get_type_size(DataType t, int pointer_level, Symbol *struct_def);


static Symbol *current_class = NULL;

static Symbol *find_struct_member(Symbol *struct_sym, const char *name) {
    if (!struct_sym || struct_sym->kind != SYM_STRUCT) return NULL;
    Symbol *m = struct_sym->members;
    while (m) {
        if ((m->unmangled_name && strcmp(m->unmangled_name, name) == 0) || strcmp(m->name, name) == 0)
            return m;
        m = m->next_member;
    }
    /* Also check virtual methods */
    m = struct_sym->virtual_methods;
    while (m) {
        if ((m->unmangled_name && strcmp(m->unmangled_name, name) == 0) || strcmp(m->name, name) == 0)
            return m;
        m = m->next_member;
    }
    return NULL;
}

static Symbol *find_virtual_method(Symbol *struct_sym, const char *name) {
    Symbol *m = struct_sym->virtual_methods;
    while (m) {
        if ((m->unmangled_name && strcmp(m->unmangled_name, name) == 0) || strcmp(m->name, name) == 0)
            return m;
        m = m->next_member;
    }
    return NULL;
}

static void replace_virtual_method(Symbol *struct_sym, Symbol *new_method) {
    Symbol **m = &struct_sym->virtual_methods;
    while (*m) {
        if (((*m)->unmangled_name && new_method->unmangled_name && strcmp((*m)->unmangled_name, new_method->unmangled_name) == 0) ||
            strcmp((*m)->name, new_method->name) == 0) {
            new_method->next_member = (*m)->next_member;
            *m = new_method;
            return;
        }
        m = &(*m)->next_member;
    }
}

/** get_symbol_size is now redundant, removed. **/

void semantic_error(int line, const char *msg) {
    printf("Semantic Error (line %d): %s\n", line, msg);
    semantic_errors++;
}

const char* type_to_string(DataType t) {
    switch (t) {
        case TYPE_INT: return "int";
        case TYPE_CHAR: return "char";
        case TYPE_VOID: return "void";
        case TYPE_STRUCT: return "struct";
        default: return "unknown";
    }
}

char* get_mangled_name(const char *prefix, const char *name, ASTNode *params) {
    char buf[512];
    if (prefix)
        snprintf(buf, sizeof(buf), "%s_%s", prefix, name);
    else
        snprintf(buf, sizeof(buf), "%s", name);
    
    ASTNode *p = params;
    while (p) {
        if (p->str_val && strcmp(p->str_val, "this") == 0) {
            p = p->next;
            continue;
        }
        strcat(buf, "_");
        strcat(buf, type_to_string(p->left->data_type));
        p = p->next;
    }
    return strdup(buf);
}

const char* type_to_string(DataType t) {
    switch (t) {
        case TYPE_INT: return "int";
        case TYPE_CHAR: return "char";
        case TYPE_VOID: return "void";
        case TYPE_STRUCT: return "struct";
        default: return "unknown";
    }
}

char* get_mangled_name(const char *prefix, const char *name, ASTNode *params) {
    char buf[512];
    if (prefix)
        snprintf(buf, sizeof(buf), "%s_%s", prefix, name);
    else
        snprintf(buf, sizeof(buf), "%s", name);
    
    ASTNode *p = params;
    while (p) {
        if (p->str_val && strcmp(p->str_val, "this") == 0) {
            p = p->next;
            continue;
        }
        strcat(buf, "_");
        strcat(buf, type_to_string(p->left->data_type));
        p = p->next;
    }
    return strdup(buf);
}

const char* type_to_string(DataType t);
char* get_mangled_name(const char *prefix, const char *name, ASTNode *params);

const char* type_to_string(DataType t) {
    switch (t) {
        case TYPE_INT: return "int";
        case TYPE_CHAR: return "char";
        case TYPE_VOID: return "void";
        case TYPE_STRUCT: return "struct";
        default: return "unknown";
    }
}

char* get_mangled_name(const char *prefix, const char *name, ASTNode *params) {
    char buf[512];
    if (prefix)
        snprintf(buf, sizeof(buf), "%s_%s", prefix, name);
    else
        snprintf(buf, sizeof(buf), "%s", name);
    
    ASTNode *p = params;
    while (p) {
        if (p->str_val && strcmp(p->str_val, "this") == 0) {
            p = p->next;
            continue;
        }
        strcat(buf, "_");
        strcat(buf, type_to_string(p->left->data_type));
        p = p->next;
    }
    return strdup(buf);
}

void analyze_struct_def(ASTNode *node) {
    if (!node || node->type != NODE_STRUCT_DEF) return;

    if (!node->str_val) {
        semantic_error(node->line_number, "Unnamed struct definitions are not supported");
        return;
    }

    Symbol *existing = lookup(node->str_val);
    if (existing) {
        semantic_error(node->line_number, "Struct redeclared");
        return;
    }

    Symbol *sym = create_symbol(node->str_val, TYPE_STRUCT, SYM_STRUCT, node->line_number);
    if (!insert_symbol(sym)) {
        semantic_error(node->line_number, "Failed to insert struct symbol");
        return;
    }

    sym->is_class = node->is_class;
    current_class = sym;
    int current_access = node->is_class ? 1 : 0; // 1 = private, 0 = public
    int offset = 0;
    int has_base_vtable = 0;

    if (node->base_class_name) {
        Symbol *base = lookup(node->base_class_name);
        if (!base || base->kind != SYM_STRUCT) {
            semantic_error(node->line_number, "Unknown base class");
            return;
        }
        sym->base_class = base;

        Symbol *b_mem = base->members;
        while (b_mem) {
            Symbol *m = create_symbol(b_mem->name, b_mem->type, b_mem->kind, b_mem->line_number);
            if (b_mem->unmangled_name) m->unmangled_name = strdup(b_mem->unmangled_name);
            m->pointer_level = b_mem->pointer_level;
            m->struct_def = b_mem->struct_def;
            m->is_array = b_mem->is_array;
            m->array_size = b_mem->array_size;
            m->access_modifier = b_mem->access_modifier;
            m->struct_offset = b_mem->struct_offset;
            m->next_member = sym->members;
            sym->members = m;
            b_mem = b_mem->next_member;
        }

        Symbol *b_v = base->virtual_methods;
        while (b_v) {
            Symbol *v = create_symbol(b_v->name, b_v->type, b_v->kind, b_v->line_number);
            if (b_v->unmangled_name) v->unmangled_name = strdup(b_v->unmangled_name);
            v->is_virtual = 1;
            v->vtable_index = b_v->vtable_index;
            v->next_member = sym->virtual_methods;
            sym->virtual_methods = v;
            b_v = b_v->next_member;
        }

        offset = base->struct_size;
        if (base->virtual_methods) has_base_vtable = 1;
    }

    for (ASTNode *member = node->body; member; member = member->next) {
        if (!member) continue;

        if (member->type == NODE_ACCESS_SPEC) {
            current_access = member->access_modifier;
            continue;
        }

        if (member->type == NODE_FUNC_DEF) {
            char mangled_name[256];
            if (member->is_constructor) {
                snprintf(mangled_name, sizeof(mangled_name), "%s__ctor", sym->name);
            } else if (member->is_destructor) {
                snprintf(mangled_name, sizeof(mangled_name), "%s__dtor", sym->name);
            } else {
                char *new_mangled = get_mangled_name(sym->name, member->str_val, member->params);
                strncpy(mangled_name, new_mangled, sizeof(mangled_name));
                free(new_mangled);
            }
            char *orig_name = member->str_val;
            member->str_val = strdup(mangled_name);

            /* Prepend implicit 'this' parameter if not present */
            int has_this = 0;
            for (ASTNode *p = member->params; p; p = p->next) {
                if (p->str_val && strcmp(p->str_val, "this") == 0) {
                    has_this = 1;
                    break;
                }
            }
            if (!has_this) {
                ASTNode *this_type = create_node(NODE_TYPE);
                this_type->data_type = TYPE_STRUCT;
                this_type->str_val = strdup(sym->name);
                this_type->pointer_level = 1;
                
                ASTNode *this_param = create_node(NODE_PARAM);
                this_param->str_val = strdup("this");
                this_param->left = this_type;
                this_param->line_number = member->line_number;
                
                this_param->next = member->params;
                member->params = this_param;
            }

            analyze_function(member);
            Symbol *func = lookup(member->str_val);
            member->str_val = orig_name; // Restore AST

            if (func) {
                func->unmangled_name = strdup(orig_name);
                func->access_modifier = current_access;
                Symbol *existing_v = find_virtual_method(sym, orig_name);
                if (member->is_virtual || existing_v) {
                    func->is_virtual = 1;
                    if (existing_v) {
                        func->vtable_index = existing_v->vtable_index;
                        replace_virtual_method(sym, func);
                    } else {
                        func->next_member = sym->virtual_methods;
                        sym->virtual_methods = func;
                    }
                }
            }
            free(orig_name);
            continue;
        }

        Symbol *m = create_symbol(member->str_val, member->left->data_type, SYM_VARIABLE, member->line_number);
        m->access_modifier = current_access;
        m->pointer_level = member->pointer_level;
        m->array_dim_count = member->array_dim_count;
        if (member->array_dim_count > 0) {
            m->array_sizes = malloc(sizeof(int) * member->array_dim_count);
            for (int i = 0; i < member->array_dim_count; i++) {
                if (member->array_dim_exprs && member->array_dim_exprs[i] && member->array_dim_exprs[i]->type == NODE_CONST_INT) {
                    m->array_sizes[i] = member->array_dim_exprs[i]->int_val;
                } else {
                    m->array_sizes[i] = -1;
                }
            }
        }

        if (member->left->data_type == TYPE_STRUCT && member->left->str_val) {
            Symbol *sub = lookup(member->left->str_val);
            if (!sub || sub->kind != SYM_STRUCT) {
                semantic_error(member->line_number, "Unknown struct type for member");
            } else {
                m->struct_def = sub;
            }
        }

        int size = get_type_size(m->type, m->pointer_level, m->struct_def);
        if (m->array_dim_count > 0) {
            int total = size;
            for (int i = 0; i < m->array_dim_count; i++) {
                if (m->array_sizes[i] <= 0) { total = 0; break; }
                total *= m->array_sizes[i];
            }
            size = total;
        }

        m->struct_offset = offset;
        offset += size;
        m->next_member = sym->members;
        sym->members = m;
    }

    sym->struct_size = offset;

    if (sym->virtual_methods && !has_base_vtable) {
        int ptr_size = 4;
        Symbol *m = sym->members;
        while (m) {
            m->struct_offset += ptr_size;
            m = m->next_member;
        }
        sym->struct_size += ptr_size;

        int idx = 0;
        Symbol *v = sym->virtual_methods;
        while (v) {
            v->vtable_index = idx++;
            v = v->next_member;
        }
        sym->vtable_size = idx;
    } else if (sym->virtual_methods && has_base_vtable) {
        int idx = 0;
        Symbol *b_v = sym->base_class->virtual_methods;
        while (b_v) { idx++; b_v = b_v->next_member; }
        
        Symbol *v = sym->virtual_methods;
        while (v) {
            if (v->vtable_index == -1) {
                v->vtable_index = idx++;
            }
            v = v->next_member;
        }
        sym->vtable_size = idx;
    }
    
    current_class = NULL;
}

void analyze_member_access(ASTNode *node);

void analyze_function(ASTNode *node) {

    // Create function symbol (global scope)
    Symbol *func = create_symbol(
        node->str_val,
        node->left->data_type,   // return type
        SYM_FUNCTION,
        node->line_number
    );

    if (!insert_symbol(func)) {
        semantic_error(node->line_number, "Function redeclared");
        return;
    }

    current_function = func;

    // -------------------------------------------------
    // First pass: count parameters
    // -------------------------------------------------
    int count = 0;
    ASTNode *param = node->params;

    while (param) {
        count++;
        param = param->next;
    }

    func->param_count = count;

    if (count > 0) {
        func->param_types = malloc(sizeof(DataType) * count);
        func->param_is_array = malloc(sizeof(int) * count);
        for (int k = 0; k < count; ++k) {
            func->param_is_array[k] = 0;
        }
    } else {
        func->param_types = NULL;
        func->param_is_array = NULL;
    }

    // -------------------------------------------------
    // Second pass: store parameter types
    // -------------------------------------------------
    param = node->params;
    int i = 0;

    while (param) {
        func->param_types[i] = param->left->data_type;
        /* NODE_PARAM.int_val == 1 means array parameter (T a[]) */
        func->param_is_array[i] = (param->int_val != 0);
        param = param->next;
        i++;
    }

    // -------------------------------------------------
    // Enter function scope
    // -------------------------------------------------
    enter_scope();

    // -------------------------------------------------
    // Insert parameters into function scope
    // -------------------------------------------------
    param = node->params;
    i = 0;

    current_local_offset = 0;
    current_param_offset = 16;

    while (param) {
        Symbol *sym = create_symbol(
            param->str_val,
            param->left->data_type,
            SYM_PARAMETER,
            param->line_number
        );

        if (param->int_val != 0) {
            /* Array parameter: behaves like pointer to first element */
            sym->is_array = 1;
            sym->array_size = -1; /* unknown size (decayed parameter) */
        }

        sym->frame_offset = current_param_offset;
        current_param_offset += 4; /* Assign 4 bytes per parameter (assuming 32-bit pointers/ints) */

        if (param->left->data_type == TYPE_STRUCT && param->left->str_val) {
            sym->struct_def = lookup(param->left->str_val);
        }

        if (!insert_symbol(sym))
            semantic_error(param->line_number, "Parameter redeclared");

        param = param->next;
        i++;
    }

    // -------------------------------------------------
    // Analyze function body
    // -------------------------------------------------
    int body_returns = analyze_node(node->body);

    current_function->local_vars_size = current_local_offset;

     if (current_function->type != TYPE_VOID && !body_returns) {
        semantic_error(node->line_number,
                       "Non-void function must return a value");
    }

    exit_scope();

    current_function = NULL;
}


void analyze_declaration(ASTNode *node) {
printf("Declaring %s at scope level %d\n",
       node->str_val,
       current_scope->level);
    Symbol *sym = create_symbol(
        node->str_val,
        node->left->data_type,
        SYM_VARIABLE,
        node->line_number
    );

    /* Handle array declarations:
     * NODE_VAR_DECL => scalar or scalar with initializer
     * NODE_ARRAY_DECL => fixed-size array (int_val > 0)
     */
    if (node->type == NODE_ARRAY_DECL) {
        if (node->int_val <= 0) {
            semantic_error(node->line_number,
                           "Array size must be positive");
        } else if (node->left->data_type == TYPE_VOID) {
            semantic_error(node->line_number,
                           "Array element type cannot be void");
        }
        sym->is_array = 1;
        sym->array_size = node->int_val;
    } else if (node->type == NODE_VAR_DECL) {
        sym->pointer_level = node->pointer_level;
        sym->array_dim_count = node->array_dim_count;
        if (node->array_dim_count > 0) {
            sym->is_array = 1;
            sym->array_sizes = malloc(sizeof(int) * node->array_dim_count);
            sym->array_dim_exprs = malloc(sizeof(ASTNode*) * node->array_dim_count);
            for (int i = 0; i < node->array_dim_count; i++) {
                ASTNode *expr = node->array_dim_exprs[i];
                if (expr && expr->type == NODE_CONST_INT) {
                    sym->array_sizes[i] = expr->int_val;
                } else {
                    sym->array_sizes[i] = -1; // VLA
                    sym->array_dim_exprs[i] = expr;
                }
            }
        }
    }

    /* If this variable is of struct type, link it to its definition */
    if (node->left->data_type == TYPE_STRUCT && node->left->str_val) {
        Symbol *struct_sym = lookup(node->left->str_val);
        if (!struct_sym || struct_sym->kind != SYM_STRUCT) {
            semantic_error(node->line_number,
                           "Unknown struct type");
        } else {
            sym->struct_def = struct_sym;
        }
    }

    /* Compute offset and size */
    int size = get_type_size(node->left->data_type, sym->pointer_level, sym->struct_def);
    if (sym->is_array && sym->array_size > 0) {
        size = size * sym->array_size;
    } else if (sym->is_array && sym->array_dim_count > 0) {
        int total_elements = 1;
        for (int i=0; i < sym->array_dim_count; i++) {
            if (sym->array_sizes[i] > 0) total_elements *= sym->array_sizes[i];
        }
        size = size * total_elements;
    } else if (sym->pointer_level > 0 || sym->is_array) {
        size = 4; // pointer size
    }

    size = (size + 3) & ~3; // Align to 4 bytes boundary
    current_local_offset += size;
    sym->frame_offset = -current_local_offset;

    if (!insert_symbol(sym))
        semantic_error(node->line_number, "Variable redeclared");

    // If initializer exists
    if (node->right) {
        analyze_node(node->right);

        if (node->left->data_type != node->right->data_type)
            semantic_error(node->line_number,
                           "Type mismatch in initialization");
    }
}

int analyze_block(ASTNode *node) {
    enter_scope();

    ASTNode *stmt = node->left;
    int returns = 0;

    while (stmt) {
        returns = analyze_node(stmt);
        if (returns)
            break;   // everything after unreachable
        stmt = stmt->next;
    }

    exit_scope();

    return returns;
}

void analyze_variable(ASTNode *node) {

    Symbol *sym = lookup(node->str_val);
    
    if (!sym && current_class) {
        Symbol *m = find_struct_member(current_class, node->str_val);
        if (m) {
            /* Transform into member access: this.member */
            char *member_name = node->str_val; // strdup? node already owns it
            node->type = NODE_MEMBER_ACCESS;
            node->left = create_var_node("this");
            node->left->line_number = node->line_number;
            node->str_val = member_name;
            node->int_val = 0; // dot access
            analyze_member_access(node);
            return;
        }
    }

    if (!sym) {
        semantic_error(node->line_number,
                       "Undeclared variable");
        node->data_type = TYPE_INT;
        return;
    }

    node->data_type = sym->type;
}

void analyze_member_access(ASTNode *node) {
    /* node->left is base expression; node->str_val is member name */
    analyze_node(node->left);

    if (!node->left) {
        semantic_error(node->line_number, "Invalid member access");
        node->data_type = TYPE_INT;
        return;
    }

    /* Only support base being a variable for now */
    if (node->left->type != NODE_VAR) {
        semantic_error(node->line_number,
                       "Unsupported member access base expression");
        node->data_type = TYPE_INT;
        return;
    }

    Symbol *base = lookup(node->left->str_val);
    if (!base) {
        semantic_error(node->line_number,
                       "Undeclared variable in member access");
        node->data_type = TYPE_INT;
        return;
    }

    Symbol *struct_def = NULL;
    if (node->int_val == 0) {
        /* dot access */
        if (base->type != TYPE_STRUCT) {
            semantic_error(node->line_number,
                           "Member access on non-struct type");
            node->data_type = TYPE_INT;
            return;
        }
        struct_def = base->struct_def;
    } else {
        /* arrow access */
        if (base->type != TYPE_STRUCT || base->pointer_level == 0) {
            semantic_error(node->line_number,
                           "Arrow access on non-struct pointer");
            node->data_type = TYPE_INT;
            return;
        }
        struct_def = base->struct_def;
    }

    if (!struct_def) {
        semantic_error(node->line_number,
                       "Unknown struct type in member access");
        node->data_type = TYPE_INT;
        return;
    }

    node->struct_def = struct_def;

    Symbol *member = find_struct_member(struct_def, node->str_val);
    if (!member) {
        semantic_error(node->line_number,
                       "Unknown struct member");
        node->data_type = TYPE_INT;
        return;
    }
    node->member_sym = member;

    if (member->access_modifier == 1) { // Private
        // In CD2, methods are processed while current_class is set. 
        if (!current_class || current_class != struct_def) {
            semantic_error(node->line_number, "Private member access violation");
        }
    }

    node->data_type = member->type;
    node->pointer_level = member->pointer_level;
    node->member_offset = member->struct_offset;
}

void analyze_assignment(ASTNode *node) {

    analyze_node(node->left);
    analyze_node(node->right);

    if (node->left->data_type == TYPE_VOID ||
        node->right->data_type == TYPE_VOID)
        return;  // don't cascade

    /* LHS must be an lvalue: VAR, INDEX, MEMBER_ACCESS, or deref (*) */
    if (node->left->type != NODE_VAR && node->left->type != NODE_INDEX &&
        node->left->type != NODE_MEMBER_ACCESS &&
        !(node->left->type == NODE_UN_OP && node->left->int_val == '*')) {
        semantic_error(node->line_number,
                       "Left-hand side of assignment must be a modifiable lvalue");
        return;
    }

    if (node->left->data_type != node->right->data_type)
        semantic_error(node->line_number,
                       "Assignment type mismatch");

    node->data_type = node->left->data_type;
}

void analyze_binary(ASTNode *node) {

    analyze_node(node->left);
    analyze_node(node->right);

    if (node->left->data_type == TYPE_VOID ||
        node->right->data_type == TYPE_VOID)
        return;  // don't cascade


    if (node->left->data_type != node->right->data_type)
        semantic_error(node->line_number,
                       "Binary operand type mismatch");

    node->data_type = node->left->data_type;
}


void analyze_unary(ASTNode *node) {
    analyze_node(node->left);
    node->data_type = node->left->data_type;
}

void analyze_index(ASTNode *node) {
    if (!node || node->type != NODE_INDEX) return;

    /* NODE_INDEX.left is the base expression, NODE_INDEX.right is the index */
    ASTNode *base = node->left;
    ASTNode *idx = node->right;

    analyze_node(base);
    analyze_node(idx);

    if (idx->data_type != TYPE_INT) {
        semantic_error(node->line_number,
                       "Array index expression must be of type int");
    }

    /* Determine the result type based on base */
    if (base->type == NODE_VAR) {
        Symbol *sym = lookup(base->str_val);
        if (!sym) {
            semantic_error(node->line_number,
                           "Undeclared variable in indexing expression");
            node->data_type = TYPE_INT;
            return;
        }
        if (sym->pointer_level > 0) {
            // Pointer indexing: result is base type with pointer_level - 1
            node->data_type = sym->type;
            // Note: we don't decrement sym->pointer_level, just for this node
        } else if (sym->is_array && sym->array_dim_count > 0) {
            // Array indexing: result is element type, with dim_count - 1
            node->data_type = sym->type;
            // If dim_count == 1, it's scalar
        } else {
            semantic_error(node->line_number,
                           "Indexing is only allowed on array variables or pointers");
            node->data_type = sym->type;
            return;
        }
    } else if (base->type == NODE_INDEX) {
        // Multi-dim: base is already an index, so result type is base's type
        node->data_type = base->data_type;
    } else {
        semantic_error(node->line_number,
                       "Invalid base expression for array indexing");
        node->data_type = TYPE_INT;
    }
}


void analyze_function_call(ASTNode *node) {

    /* Analyze the function expression */
    analyze_node(node->left);

    Symbol *sym = NULL;

    if (node->left->type == NODE_VAR) {
        /* Direct function call */
        sym = lookup(node->left->str_val);
    } else if (node->left->type == NODE_MEMBER_ACCESS) {
        /* Method call: find the method in the struct */
        if (node->left->member_sym && node->left->member_sym->kind == SYM_FUNCTION) {
            sym = node->left->member_sym;
            /* Overload resolution: generate mangled name with actual arg types */
            char buf[512] = "";
            snprintf(buf, sizeof(buf), "%s_%s", node->left->struct_def->name, node->left->str_val);
            
            ASTNode *arg = node->right;
            while (arg) {
                analyze_node(arg);
                strcat(buf, "_");
                strcat(buf, type_to_string(arg->data_type));
                arg = arg->next;
            }
            Symbol *overload = lookup(buf);
            if (overload) sym = overload;
            else {
                /* Try backward compatible name if no overloading was used */
                char fallback[256];
                snprintf(fallback, sizeof(fallback), "%s_%s", node->left->struct_def->name, node->left->str_val);
                Symbol *fb = lookup(fallback);
                if (fb) sym = fb;
            }
        }
        if (sym && (sym->is_virtual || node->left->type == NODE_MEMBER_ACCESS)) {
            if (sym->is_virtual) node->is_virtual_call = 1;
            node->call_struct = node->left->struct_def;
            /* Add the object as first argument */
            ASTNode *obj_expr = node->left->left;
            /* For pointer, pass as is; for value, take address */
            /* Assume for struct value, passing by value. */
            // Prepend obj_expr to arg list
            obj_expr->next = node->right;
            node->right = obj_expr;
        }
    } else {
        semantic_error(node->line_number, "Invalid function call expression");
        return;
    }

    if (!sym || sym->kind != SYM_FUNCTION) {
        semantic_error(node->line_number,
                       "Undeclared function");
        return;
    }

    node->func_sym = sym;

    ASTNode *arg = node->right;
    int i = 0;

    while (arg) {
        analyze_node(arg);

        if (i >= sym->param_count)
            semantic_error(node->line_number,
                           "Too many arguments");

        else {
            /* Type compatibility: base type must match */
            if (arg->data_type != sym->param_types[i]) {
                semantic_error(node->line_number,
                               "Argument type mismatch");
            }
            /* Array parameter: require array argument */
            if (sym->param_is_array &&
                sym->param_is_array[i]) {
                if (arg->type != NODE_VAR) {
                    semantic_error(node->line_number,
                                   "Array parameter requires array variable argument");
                } else {
                    Symbol *arg_sym = lookup(arg->str_val);
                    if (!arg_sym || !arg_sym->is_array) {
                        semantic_error(node->line_number,
                                       "Array parameter requires array variable argument");
                    }
                }
            }
        }

        arg = arg->next;
        i++;
    }

    if (i < sym->param_count)
        semantic_error(node->line_number,
                       "Too few arguments");

    node->data_type = sym->type;
}

int analyze_return(ASTNode *node) {

    if (!current_function){
        semantic_error(node->line_number,
                       "Return outside function");
        return 0;
   }

   if (node->left){ // return with some parameter
    analyze_node(node->left);

    if (node->left->data_type != current_function->type)
        semantic_error(node->line_number,
                       "Return type mismatch");

   } else { //for simply return with no return value
           if(current_function->type != TYPE_VOID)
                   semantic_error(node->line_number,
                                   "Return type mismatch");
   }
   return 1;
}


int analyze_if(ASTNode *node) {

    // Analyze condition
    analyze_node(node->cond);

    if (node->cond->data_type == TYPE_VOID){
         semantic_error(node->line_number,
                   "Invalid condition type");
    }
    // THEN block
    int then_returns = analyze_node(node->left);

    int else_returns = 0;

    // ELSE block
    if (node->right) {
            else_returns = analyze_node(node->right);
    }

    return (then_returns && else_returns);
}

int analyze_while(ASTNode *node) {

    analyze_node(node->cond);

    if (node->cond->data_type == TYPE_VOID)
         semantic_error(node->line_number,
                   "Invalid condition type");


    break_context_depth++;
    loop_context_depth++;
    analyze_node(node->body);
    loop_context_depth--;
    break_context_depth--;

    return 0;

}

int analyze_for(ASTNode *node) {

    if (node->init)
        analyze_node(node->init);

    if (node->cond){
        analyze_node(node->cond);
       if (node->cond->data_type == TYPE_VOID)
       {
               semantic_error(node->line_number, "Invalid condition type");
       }
    }

    if (node->incr)
        analyze_node(node->incr);

    break_context_depth++;
    loop_context_depth++;
    analyze_node(node->body);
    loop_context_depth--;
    break_context_depth--;

    return 0;
}

int analyze_switch(ASTNode *node) {

    if (node->cond) {
        analyze_node(node->cond);
        if (node->cond->data_type != TYPE_INT &&
            node->cond->data_type != TYPE_CHAR) {
            semantic_error(node->line_number,
                           "Switch expression must be of type int or char");
        }
    }

    break_context_depth++;

    int has_default = 0;

    for (ASTNode *c = node->body; c; c = c->next) {
        if (!c) continue;

        if (c->type != NODE_CASE) {
            analyze_node(c);
            continue;
        }

        if (c->left) {
            ASTNode *expr = c->left;
            analyze_node(expr);

            if (expr->type != NODE_CONST_INT &&
                expr->type != NODE_CONST_CHAR) {
                semantic_error(c->line_number,
                               "Case label must be constant int or char");
            } else {
                /* Check for duplicate case values */
                for (ASTNode *prev = node->body; prev != c; prev = prev->next) {
                    if (prev->type == NODE_CASE && prev->left &&
                        (prev->left->type == NODE_CONST_INT ||
                         prev->left->type == NODE_CONST_CHAR)) {
                        if (prev->left->int_val == expr->int_val) {
                            semantic_error(c->line_number,
                                           "Duplicate case label in switch");
                            break;
                        }
                    }
                }
            }
        } else {
            if (has_default) {
                semantic_error(c->line_number,
                               "Multiple default labels in switch");
            }
            has_default = 1;
        }

        /* Analyze statements in the case body */
        if (c->body) {
            analyze_list(c->body);
        }
    }

    break_context_depth--;

    return 0;
}


int analyze_node(ASTNode *node) {
    if (!node) return 0;

    switch(node->type) {

        case NODE_STRUCT_DEF:
            analyze_struct_def(node);
            return 0;

        case NODE_FUNC_DEF:
            analyze_function(node);
            return 0;

        case NODE_VAR_DECL:
            analyze_declaration(node);
            return 0;
        case NODE_ARRAY_DECL:
            /* array declarations share the same logic as variable declarations */
            analyze_declaration(node);
            return 0;

        case NODE_BLOCK:
            return analyze_block(node);

        case NODE_IF:
            return analyze_if(node);

        case NODE_WHILE:
            return analyze_while(node);

        case NODE_FOR:
            return analyze_for(node);

        case NODE_SWITCH:
            return analyze_switch(node);

        case NODE_RETURN:
            return analyze_return(node);
        case NODE_BREAK:
            if (break_context_depth <= 0) {
                semantic_error(node->line_number,
                               "break statement not within loop or switch");
            }
            return 0;

        case NODE_CONTINUE:
            if (loop_context_depth <= 0) {
                semantic_error(node->line_number,
                               "continue statement not within loop");
            }
            return 0;

        case NODE_ASSIGN:
            analyze_assignment(node);
            return 0;

        case NODE_BIN_OP:
            analyze_binary(node);
            return 0;

        case NODE_UN_OP:
            analyze_unary(node);
            return 0;

        case NODE_INDEX:
            analyze_index(node);
            return 0;

        case NODE_MEMBER_ACCESS:
            analyze_member_access(node);
            return 0;

        case NODE_CONST_INT:
            node->data_type = TYPE_INT;
            return 0;

        case NODE_CONST_CHAR:
            node->data_type = TYPE_CHAR;
            return 0;

        case NODE_STR_LIT:
            node->data_type = TYPE_CHAR;
            return 0;

        case NODE_VAR:
            analyze_variable(node);
            return 0;

        case NODE_FUNC_CALL:
            analyze_function_call(node);
            return 0;

        case NODE_TYPE:
            // type nodes usually need no semantic work
            // break;
        case NODE_EMPTY:
            return 0;

        default:
            return 0;
    }
}

int analyze_list(ASTNode *node) {

    int returns = 0;

    while (node) {
        returns = analyze_node(node);
        if(returns)
        { break; }
        node = node->next;
    }
    return returns;
}

int get_type_size(DataType t, int pointer_level, Symbol *struct_def) {
    if (pointer_level > 0) return 4;
    if (t == TYPE_INT) return 4;
    if (t == TYPE_CHAR) return 1;
    if (t == TYPE_VOID) return 0;
    if (t == TYPE_STRUCT) {
        if (struct_def) return struct_def->struct_size;
        return 0;
    }
    return 4; // default
}

void semantic_analyze(ASTNode *node) {
    if (!node) return;
    analyze_list(node);
}
