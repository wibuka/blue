#include "semantic.h"

#define FNCDEF_EMPTY ((FncDef){NULL, false, false})

#define DUPLICATE_NAME_ERROR(source, name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "redeclaration of '%s'", name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define CONFLICTING_TYPES_ERROR(source, name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "conflicting types for '%s'", name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define UNDECLARED_NAME_ERROR(source, name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "use of undeclared identifier '%s'", name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define UNDECLARED_TYPE_ERROR(source, name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "unknown type name '%s'", name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define CIRCULAR_TYPE_INFERENCE_ERROR(source) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "circular type inference"); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define GLOBAL_FUNCTION_CALL_ERROR(source, name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "function call '%s' is not allowed at global scope", name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define NON_GLOBAL_STATEMENT_ERROR(source, keyword) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "%s is only allowed at global scope", keyword); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define TYPE_MISMATCH_ERROR(source, expected, found) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "type mismatch: expected '%s', found '%s'", expected, found); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define RETURN_TYPE_MISMATCH_ERROR(source, expected, found) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "return type mismatch: expected '%s', found '%s'", expected, found); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define FUNCTION_ARGUMENT_COUNT_ERROR(source, name, expected, found) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "wrong number of arguments to function call '%s': expected %zu, found %zu", \
                name, (size_t)(expected), (size_t)(found)); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define STRUCT_EXPRESSION_VALUE_COUNT_ERROR(source, name, too_few, expected, found) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "%s values in struct literal of type '%s': expected %zu, found %zu", \
                too_few ? "too few" : "too many", name, \
                (size_t)(expected), (size_t)(found)); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define STRUCT_NOT_CALLABLE_ERROR(source, name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "struct '%s' is not callable; use '%s{...}' to construct a value", \
                name, name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define OPERATOR_TYPE_ERROR(source, op, left_type, right_type) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "operator '%s' cannot be applied to '%s' and '%s'", op, left_type, right_type); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define NEGATIVE_VALUE_TO_UNSIGNED_ERROR(source, type_name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "cannot assign negative value to unsigned type '%s'", type_name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define FLOAT_TO_INT_ERROR(source, type_name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "cannot assign float value to integer type '%s'", type_name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define INT_TO_BOOL_ERROR(source) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "cannot assign integer value to type 'bool'"); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define STRING_TO_NON_POINTER_ERROR(source, type_name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "cannot assign string literal to non-pointer type '%s'", type_name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define DEREFERENCE_NON_POINTER_ERROR(source, type_name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "cannot dereference non-pointer type '%s'", type_name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define DEREFERENCE_VOID_POINTER_ERROR(source) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "cannot dereference '*u0' (void pointer)"); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define INVALID_CAST_TARGET_ERROR(source, type_name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "cannot cast to type '%s'", type_name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define POINTER_TO_INT_ERROR(source, type_name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "cannot cast pointer directly to '%s'; use 'usize'", type_name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define UNKNOWN_FIELD_ERROR(source, field, type_name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "no field '%s' in struct '%s'", field, type_name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define MEMBER_OF_NON_STRUCT_ERROR(source, type_name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "cannot access member of non-struct type '%s'", type_name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define ENUM_NO_MEMBER_ERROR(source, enum_name, member) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "enum '%s' has no member '%s'", enum_name, member); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define INVALID_STRUCT_ERROR(source) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "invalid struct definition"); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define STRUCT_FIELDS_UNDEFINED_ERROR(source, type_name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "struct '%s' fields not yet defined (forward reference)", type_name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define INVALID_MEMBER_NAME_ERROR(source) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "invalid member name"); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define UNSIZED_TYPE_ERROR(source, subject, type_name, struct_name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "cannot declare %s of incomplete type '%s': struct '%s' has no fields, so the type has no size", \
                subject, type_name, struct_name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define BREAK_OUTSIDE_LOOP_ERROR(source, keyword) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "'%s' outside of loop", keyword); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define LITERAL_OUT_OF_RANGE_ERROR(source, type_name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "integer literal is out of range for type '%s'", type_name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define LOOP_VARIABLE_TYPE_ERROR(source, type_name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "loop variable type '%s' must be an integer type", type_name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define MISSING_RETURN_ERROR(source, keyword) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "function '%s' is missing a return statement", keyword); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define GLOBAL_STATEMENT_ERROR(source, keyword) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "'%s' is not allowed at global scope", keyword); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define RANGE_TYPE_MISMATCH_ERROR(source, type_a, type_b) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "range boundaries have incompatible types '%s' and '%s'", \
                type_a, type_b); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define RANGE_NON_INTEGER_ERROR(source) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "range value must be a integer type"); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define EXPRESSION_NOT_ASSIGNABLE_ERROR(source) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "expression is not assignable"); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define ASSIGN_NON_VARIABLE_ERROR(source, name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "cannot assign to '%s' because it is not a variable", \
                name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define ASSIGN_TO_CONST_ERROR(source, name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "cannot assign to constant '%s'", \
                name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define NON_CONSTANT_LET_INIT_ERROR(source, name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "cannot initialize constant '%s' from '%s': " \
                "'let' requires a value known at compile time, " \
                "use 'using %s %s' to alias a symbol instead", \
                name, name, name, name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define NON_INDEXABLE_TYPE_ERROR(source, type_name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "cannot index expression of type '%s'", type_name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define NON_INTEGER_INDEX_ERROR(source, type_name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "array/slice index must be an integer type, found '%s'", type_name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define FUNCTION_USED_AS_VALUE_ERROR(source, name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "function '%s' cannot be used as a value", name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define INVALID_HEXADECIMAL_LITERAL_ERROR(source, value) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "invalid hexadecimal literal '%s'", value); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define INVALID_BINARY_LITERAL_ERROR(source, value) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "invalid binary literal '%s'", value); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define ENUM_VALUE_OVERFLOW_ERROR(source, name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "enum value of '%s' is out of range for type 'i32'", name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define INVALID_OVERLOAD_FUNCTION_ERROR(source, name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "'%s' is not a function", name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define EMPTY_OVERLOAD_SET_ERROR(source) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "function overload set cannot be empty"); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define DUPLICATE_OVERLOAD_ERROR(source, name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "duplicate function overload '%s': conflicting parameter types", name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

#define NO_MATCHING_FUNCTION_OVERLOAD_ERROR(source, name) \
    do { \
        semantic_errors = true; \
        snprintf(log_description, sizeof(log_description), \
                "no matching function overload for '%s'", name); \
        log_source_error(source, ERROR_UNKNOWN, log_description); \
    } while (0)

static bool semantic_errors = false;
static Scope* global_scope = NULL;

static bool eval_constant_integer(Ast* expr, long* out_val);
static int check_literal_fits_target(SourceSite source, Ast* expr, TypeData* target_type);

static const char* undeclared_type_name(const Ast* type_node) {
    if (!type_node) return "unknown";

    switch (type_node->type) {
        case AST_IDENTIFIER:
            return type_node->identifier.name;
        case AST_STAR_EXPRESSION:
            return undeclared_type_name(type_node->star_expression.object);
        case AST_ARRAY_TYPE:
            return undeclared_type_name(type_node->array_type.type);
        case AST_SLICE_TYPE:
            return undeclared_type_name(type_node->slice_type.type);
        case AST_SELECTOR_EXPRESSION:
            return undeclared_type_name(type_node->selector_expression.object);
        default:
            return "unknown";
    }
}

const char* type_to_string(Ast* type);
const char* identifier_name(Ast* stmt);
const char* get_statement_type_name(Ast* stmt);

static Symbol* Symbol_create(const char* name, TypeData* type, Ast* declaration);
static Scope* scope_create(Scope* parent);

int has_typeData(const char* name);
int has_type(const char* name, Scope* scope);

void semantic_expression(Ast* node, Scope* scope, TypeData* expected_type);
void semantic_statement(Ast* node, Scope* scope);
void semantic_selector_expression(Ast* node, Scope* scope);
static void semantic_enum_declaration(Ast* node, Scope* scope);
static char* selector_qualified_name(Ast* node);
static TypeData* get_type(Ast* type_node, Scope* scope);

static char* literal_to_string(Ast* node, const char* buffer) {
    if (!node || !is_literal(node)) return NULL;
    
    switch(node->type) {
        case AST_LITERAL_FLOAT:
            snprintf((char*)buffer, 256, "%g", node->literal_float.value);
            return (char*)buffer;
            
        case AST_LITERAL_INTEGER:
            snprintf((char*)buffer, 256, "%ld", node->literal_integer.value);
            return (char*)buffer;
            
        case AST_LITERAL_BOOLEAN:
            snprintf((char*)buffer, 256, "%s", node->literal_boolean.value ? "true" : "false");
            return (char*)buffer;
            
        case AST_LITERAL_NIL:
            snprintf((char*)buffer, 256, "%s", TYPE_NIL_STRING);
            return (char*)buffer;
            
        case AST_LITERAL_STRING:
            if (node->literal_string.value) {
                strncpy((char*)buffer, node->literal_string.value, 255);
                ((char*)buffer)[255] = '\0';
                return (char*)buffer;
            }
            return NULL;
            
        case AST_UNARY_EXPRESSION: {
            long val;
            if (eval_constant_integer(node, &val)) {
                snprintf((char*)buffer, 256, "%ld", val);
                return (char*)buffer;
            }
            return NULL;
        }
            
        default:
            return NULL;
    }
}

static char* make_qualified_name(const char* prefix, const char* name) {
    if (!prefix || !name) return NULL;
    size_t len = strlen(prefix) + strlen(name) + 2;
    char* qual = c_alloc(len);
    if (qual) snprintf(qual, len, "%s.%s", prefix, name);
    return qual;
}

static bool eval_constant_integer(Ast* expr, long* out_val) {
    if (!expr) return false;

    switch (expr->type) {
        case AST_LITERAL_INTEGER:
            *out_val = expr->literal_integer.value;
            return true;

        case AST_UNARY_EXPRESSION: {
            long arg_val;
            if (!eval_constant_integer(expr->unary_expression.argument, &arg_val)) {
                return false;
            }

            const char* op = expr->unary_expression.operator;
            if (op[1] != '\0') return false; 

            switch (op[0]) {
                case '-': *out_val = -arg_val; return true;
                case '+': *out_val =  arg_val; return true;
                case '~': *out_val = ~arg_val; return true;
                default:  return false;
            }
        }

        case AST_BINARY_EXPRESSION: {
            long left_val, right_val;
            if (!eval_constant_integer(expr->binary_expression.left, &left_val) ||
                !eval_constant_integer(expr->binary_expression.right, &right_val)) {
                return false;
            }

            const char* op = expr->binary_expression.operator;
            if (op[1] != '\0') return false; 

            switch (op[0]) {
                case '+': *out_val = left_val + right_val; return true;
                case '-': *out_val = left_val - right_val; return true;
                case '*': *out_val = left_val * right_val; return true;
                case '/':
                    if (right_val == 0) return false;
                    *out_val = left_val / right_val;
                    return true;
                case '%':
                    if (right_val == 0) return false;
                    *out_val = left_val % right_val;
                    return true;
                default: return false;
            }
        }

        default:
            return false;
    }
}

static int check_value_fits_target(long val, TypeBase target_type) {
    switch (target_type) {
        case I8:    return val >= -128 && val <= 127;
        case I16:   return val >= -32768 && val <= 32767;
        case I32:   return val >= -2147483648L && val <= 2147483647L;
        case I64:   return 1;
        case I128:  return 1;
        case U8:    return val >= 0 && val <= 255;
        case U16:   return val >= 0 && val <= 65535;
        case U32:   return val >= 0 && val <= 4294967295L;
        case U64:   return val >= 0;
        case U128:  return val >= 0;
        case USIZE: return val >= 0;
        default:    return 1;
    }
}

static int check_literal_fits_target(SourceSite source, Ast* expr, TypeData* target_type) {
    long val = 0;
    if (eval_constant_integer(expr, &val)) {
        int fits = check_value_fits_target(val, target_type->base);
        if (!fits) {
            LITERAL_OUT_OF_RANGE_ERROR(source, TypeData_to_string(target_type));
            return 0;
        }
    }
    return 1;
}

static Scope* scope_create(Scope* parent) {
    Scope* scope        = c_alloc(sizeof(Scope));
    scope->nodes        = vector_create();
    scope->parent       = parent;
    return scope;
}

static void scope_add(Scope* scope, Ast* node) {
    vector_add(&scope->nodes, node);
}

static Symbol* Symbol_create(const char* name, TypeData* type, Ast* declaration) {
    Symbol* symbol      = c_alloc(sizeof(Symbol));
    symbol->name        = name;
    symbol->type        = type;
    symbol->declaration = declaration;
    symbol->llvm_value  = NULL;
    symbol->llvm_type   = NULL;
    return symbol;
}

static int scope_is_global(const Scope* scope) {
    if (!scope)                 return 1;
    if (scope->parent == NULL)  return 1;
    return 0;
}

static Ast* resolve_struct_type_decl(Ast* decl) {
    if (!decl) return NULL;

    if (decl->type == AST_STRUCT_DECLARATION) {
        return decl;
    }

    if (decl->type == AST_USING_DECLARATION &&
        decl->symbol && decl->symbol->type) {
        TypeData* td = decl->symbol->type;
        if (td->base == TYPEDEF && td->struct_def) {
            Ast* owner = td->struct_def->owner;
            if (owner && owner->type == AST_STRUCT_DECLARATION) {
                return owner;
            }
        }
    }

    return NULL;
}

static Ast* find_declaration(const char* name, Scope* scope, int outscope, const SourceSite* ref_site, Ast* exclude) {
    if (!name || !scope) return NULL;
    Ast* fallback = NULL;
    while (scope != NULL) {
        for (int i = 0; i < vector_size(scope->nodes); i++) {
            Ast* stmt = scope->nodes[i];
            if (stmt == exclude) continue;
            if (strcmp(name, identifier_name(stmt)) != 0) continue;
            if (ref_site) {
                int is_private = 0;
                if (stmt->attributes) {
                    for (size_t k = 0; k < vector_size(stmt->attributes); k++) {
                        if (stmt->attributes[k] && strcmp(stmt->attributes[k], "@private") == 0) { is_private = 1; break; }
                    }
                }
                int is_same_file = (stmt->source.path && ref_site->path && strcmp(stmt->source.path, ref_site->path) == 0);
                if (is_private && !is_same_file) continue;
                if (is_same_file) return stmt;
                if (!fallback) fallback = stmt;
            } else {
                return stmt;
            }
        }
        if (!outscope) break;
        scope = scope->parent;
    }
    return fallback;
}


static Ast* resolve_function_alias(Ast* decl) {
    int hops = 0;
    while (decl && decl->type == AST_FUNCTION_ALIAS &&
           decl->symbol && decl->symbol->declaration &&
           hops++ < 64) {
        decl = decl->symbol->declaration;
    }
    return decl;
}

typedef struct {
    Ast* declaration;
    bool has_return_void;
    bool has_goto_statement;
    Ast** goto_statements;
    Ast** label_statements;
} FunctionRef;

FunctionRef current_function = {0};
static inline FunctionRef FncRef_create(Ast* node, bool returns_void) {
    return (FunctionRef){node, returns_void};
}
static int loop_depth = 0;

static void coerce_literal_to_match(Ast** node, TypeData** td, Ast* other_node, TypeData* other_td) {
    if ((*node)->type != AST_LITERAL_INTEGER && (*node)->type != AST_LITERAL_FLOAT)
        return;

    if (other_node->type != AST_LITERAL_INTEGER &&
        other_node->type != AST_LITERAL_FLOAT &&
        is_numeric_type(other_td) &&
        other_td->pointer_depth == 0) {
        *td = other_td;
        if ((*node)->symbol) (*node)->symbol->type = other_td;
    }
}

static void coerce_literals_together(Ast* left, Ast* right, TypeData** left_td, TypeData** right_td, TypeData* expected_type) {
    bool left_literal = left->type == AST_LITERAL_INTEGER || left->type == AST_LITERAL_FLOAT;
    bool right_literal = right->type == AST_LITERAL_INTEGER || right->type == AST_LITERAL_FLOAT;

    if (!left_literal || !right_literal)
        return;

    TypeData* coerced_td;

    if (expected_type && expected_type->base != 0 && expected_type->pointer_depth == 0 && is_numeric_type(expected_type)) {
        coerced_td = expected_type;
    } else {
        coerced_td = (left->type == AST_LITERAL_FLOAT || right->type == AST_LITERAL_FLOAT)
            ? (TypeData*)(&TYPE_F64_DATA)
            : (TypeData*)(&TYPE_I32_DATA);
    }

    *left_td = *right_td = coerced_td;
    if (left->symbol)  left->symbol->type  = coerced_td;
    if (right->symbol) right->symbol->type = coerced_td;
}

static int validate_arithmetic(TypeData* left_td, TypeData* right_td, const char* op, TypeData** result_td) {
    *result_td = left_td;
    return is_numeric_type(left_td) &&
           is_numeric_type(right_td) &&
           left_td->pointer_depth == 0 &&
           right_td->pointer_depth == 0 &&
           TypeData_match(left_td, right_td);
}

static int validate_comparison(TypeData* left_td, TypeData* right_td, const char* op, TypeData** result_td) {
    *result_td = (TypeData*)(&TYPE_BOOL_DATA);
    return (is_numeric_type(left_td) || left_td->pointer_depth > 0) &&
           TypeData_match(left_td, right_td);
}

static int validate_equality(TypeData* left_td, TypeData* right_td, const char* op, TypeData** result_td) {
    *result_td = (TypeData*)(&TYPE_BOOL_DATA);
    return TypeData_match(left_td, right_td);
}

static int validate_logical(TypeData* left_td, TypeData* right_td, const char* op, TypeData** result_td) {
    *result_td = (TypeData*)(&TYPE_BOOL_DATA);
    return (is_boolean_type(left_td)  || is_numeric_type(left_td)) &&
           (is_boolean_type(right_td) || is_numeric_type(right_td)) &&
           left_td->pointer_depth  == 0 &&
           right_td->pointer_depth == 0;
}

static int validate_bitwise(TypeData* left_td, TypeData* right_td, const char* op, TypeData** result_td) {
    *result_td = left_td;
    return is_integer_type(left_td) &&
           left_td->pointer_depth == 0 &&
           TypeData_match(left_td, right_td);
}

static int get_operator_result_type(TypeData* left_td, TypeData* right_td, const char* op, TypeData** result_td) {

    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 || strcmp(op, "+=") == 0 || strcmp(op, "-=") == 0) {
        if (is_pointer_type(left_td) && is_integer_type(right_td)) {
            *result_td = left_td;
            return 1;
        }

        if (strcmp(op, "+") == 0 &&
            is_integer_type(left_td) && is_pointer_type(right_td)) {
            *result_td = right_td;
            return 1;
        }
    }

    if (strcmp(op, "+")   == 0 || strcmp(op, "-")  == 0 || strcmp(op, "*") == 0 ||
        strcmp(op, "/")   == 0 || strcmp(op, "%")  == 0 ||
        strcmp(op, "+=")  == 0 || strcmp(op, "-=") == 0 || strcmp(op, "*=") == 0 ||
        strcmp(op, "/=")  == 0 || strcmp(op, "%=") == 0)
        return validate_arithmetic(left_td, right_td, op, result_td);

    if (strcmp(op, "&")   == 0 || strcmp(op, "|")  == 0 || strcmp(op, "^") == 0 ||
        strcmp(op, "<<")  == 0 || strcmp(op, ">>") == 0)
        return validate_bitwise(left_td, right_td, op, result_td);

    if (strcmp(op, "<")   == 0 || strcmp(op, ">")  == 0 ||
        strcmp(op, "<=")  == 0 || strcmp(op, ">=") == 0)
        return validate_comparison(left_td, right_td, op, result_td);

    if (strcmp(op, "==")  == 0 || strcmp(op, "!=") == 0)
        return validate_equality(left_td, right_td, op, result_td);

    if (strcmp(op, "and") == 0 || strcmp(op, "or") == 0)
        return validate_logical(left_td, right_td, op, result_td);

    return 0;
}

static TypeData* get_type(Ast* type_node, Scope* scope) {
    if (!type_node) return TypeData_typeinv();

    if (type_node->type == AST_ARRAY_TYPE) {
        TypeData* elem = get_type(type_node->array_type.type, scope);
        if (elem->base == TYPEINV) return elem;
        return TypeData_array(elem, type_node->array_type.length);
    }

    if (type_node->type == AST_SLICE_TYPE) {
        TypeData* elem = get_type(type_node->slice_type.type, scope);
        if (elem->base == TYPEINV) return elem;
        return TypeData_slice(elem);
    }

    if (type_node->type == AST_STAR_EXPRESSION) {
        TypeData* inner = get_type(type_node->star_expression.object, scope);
        if (inner->base == TYPEINV) return inner;
        return TypeData_pointer(inner);
    }
    if (type_node->type == AST_FUNCTION_TYPE) {
        Ast** fn_params = type_node->function_type.params;
        size_t n = fn_params ? vector_size(fn_params) : 0;
        TypeData** ptds = NULL;
        if (n > 0) {
            ptds = c_alloc(n * sizeof(TypeData*));
            for (size_t i = 0; i < n; i++) {
                ptds[i] = get_type(fn_params[i], scope);
            }
        }
        TypeData* fn_ret = type_node->function_type.return_type
            ? get_type(type_node->function_type.return_type, scope)
            : TypeData_typeinv();
        return TypeData_function(ptds, n, fn_ret);
    }

    if (type_node->type == AST_IDENTIFIER) {
        Ast* decl = find_declaration(type_node->identifier.name, scope, true, &type_node->source, NULL);
        if (decl && ((decl->type == AST_STRUCT_DECLARATION || decl->type == AST_USING_DECLARATION) &&
                     decl->symbol && decl->symbol->type)) {
            return decl->symbol->type;
        }
    }

    TypeData* td = TypeData_of(type_node);
    if (td->base != TYPEINV) return td;
    return td;
}

static const char* find_unsized_struct(TypeData* td, int depth) {
    if (!td || depth > 32) return NULL;
    if (td->pointer_depth > 0) return NULL;

    switch (td->base) {
        case ARRAY: return find_unsized_struct(td->array.element_type, depth + 1);
        case SLICE:  return find_unsized_struct(td->slice.element_type, depth + 1);
        case TYPEDEF: break;
        default:      return NULL;
    }

    StructDef* def = td->struct_def;
    if (!def) return NULL;

    Ast* owner = def->owner;
    if (!owner || owner->type != AST_STRUCT_DECLARATION) return NULL;

    Ast** fields = owner->struct_declaration.fields;
    size_t field_count = fields ? vector_size(fields) : 0;
    if (field_count == 0) return def->name ? def->name : "<anonymous>";

    Scope* scope = owner->scope;
    for (size_t i = 0; i < field_count; i++) {
        Ast* field_type = fields[i] ? fields[i]->field_declaration.type : NULL;
        if (!field_type) continue;
        const char* name = find_unsized_struct(get_type(field_type, scope), depth + 1);
        if (name) return name;
    }
    return NULL;
}

static bool has_attribute(const Ast* node, const char* attr_name) {
    if (!node || !node->attributes) return false;
    size_t count = vector_size(node->attributes);
    for (size_t i = 0; i < count; i++) {
        if (node->attributes[i] && strcmp(node->attributes[i], attr_name) == 0) {
            return true;
        }
    }
    return false;
}

static bool is_same_file(SourceSite s1, SourceSite s2) {
    if (s1.path == s2.path) return true;
    if (str_match(s1.path, s2.path)) return true;
    return false;
}

// A @private declaration only binds its name inside its own file, so two
// declarations may share a name when either of them is @private and they
// do not live in the same file.
static bool is_file_local_duplicate(const Ast* existing, const Ast* node) {
    if (!has_attribute(existing, "@private") && !has_attribute(node, "@private")) return false;
    return !is_same_file(existing->source, node->source);
}

// Resolves the type of a binary or hexadecimal integer literal,      |
// using the expected integer type when available, otherwise          |
// inferring i32, i64, or i128 from the literal's bit width.          |
// -------------------------------------------------------------------|
// source        : Location of the literal in the source code.        |
// spelling      : Literal's original source text.                    |
// bit_length    : Number of bits needed to represent the literal.    |
// is_binary     : Selects binary vs. hexadecimal diagnostics.        |
// expected_type : Expected integer type used for contextual typing.  |
static TypeData* resolve_bit_literal_type(SourceSite source, const char* spelling, int bit_length, int is_binary, TypeData* expected_type) {
    if (bit_length <= 0 || bit_length > 128) {
        if (is_binary) INVALID_BINARY_LITERAL_ERROR(source, spelling);
        else           INVALID_HEXADECIMAL_LITERAL_ERROR(source, spelling);
        return (TypeData*)(&TYPE_I32_DATA);
    }
    if (is_integer_type(expected_type)) {
        int width = TypeBase_bit_width(expected_type->base);
        if (width > 0 && bit_length > width) {
            LITERAL_OUT_OF_RANGE_ERROR(source, TypeData_to_string(expected_type));
        }
        return expected_type;
    }
    if (bit_length <= 32) return (TypeData*)(&TYPE_I32_DATA);
    if (bit_length <= 64) return (TypeData*)(&TYPE_I64_DATA);
    return (TypeData*)(&TYPE_I128_DATA);
}


static bool is_compile_time_constant_decl(Ast* decl) {
    if (!decl) return false;

    switch (decl->type) {
        case AST_VARIABLE_DECLARATION:
            return decl->variable_declaration.is_constant;
        case AST_ENUMERATOR:
            return true;
        case AST_SELECTOR_EXPRESSION:
            return is_compile_time_constant_decl(decl->selector_expression.object);
        default:
            return false;
    }
}

static void check_let_initializer_is_constant(Ast* node, Scope* scope) {
    if (!node) return;

    switch (node->type) {
        case AST_LITERAL_INTEGER:
        case AST_LITERAL_FLOAT:
        case AST_LITERAL_HEXADECIMAL:
        case AST_LITERAL_BINARY:
        case AST_LITERAL_BOOLEAN:
        case AST_LITERAL_CHAR:
        case AST_LITERAL_STRING:
        case AST_LITERAL_NIL:
        case AST_SELECTOR_EXPRESSION:
            return;

        case AST_IDENTIFIER: {
            const char* name = identifier_name(node);
            if (name && is_primitive_base(find_TypeBase(name))) return;

            Ast* decl = find_declaration(name, scope, true, &node->source, NULL);
            if (!is_compile_time_constant_decl(decl)) {
                NON_CONSTANT_LET_INIT_ERROR(node->source, name);
            }
            return;
        }

        case AST_UNARY_EXPRESSION:
            check_let_initializer_is_constant(node->unary_expression.argument, scope);
            return;

        case AST_BINARY_EXPRESSION:
            check_let_initializer_is_constant(node->binary_expression.left, scope);
            check_let_initializer_is_constant(node->binary_expression.right, scope);
            return;

        case AST_CALL_EXPRESSION:
            NON_CONSTANT_LET_INIT_ERROR(node->source, identifier_name(node));
            return;

        case AST_LITERAL_COMPOUND: {
            Ast** args = node->literal_compound.arguments;
            size_t count = vector_size(args);
            for (size_t i = 0; i < count; i++) {
                check_let_initializer_is_constant(args[i], scope);
            }
            return;
        }

        default:
            return;
    }
}

void semantic_variable_declaration(Ast* node, Scope* scope) {
    Ast* variable_name = node->variable_declaration.name;
    Ast* variable_type = node->variable_declaration.type;
    Ast* variable_init = node->variable_declaration.initializer;

    const char* var_name_str = identifier_name(variable_name);
    
    Ast* existing_var = find_declaration(var_name_str, scope, true, NULL, NULL);
    bool allow_dup = false;

    if (existing_var) {
        bool is_priv_existing = has_attribute(existing_var, "@private");
        bool is_priv_node = has_attribute(node, "@private");
        
        if ((is_priv_existing || is_priv_node) && 
            existing_var->source.path && node->source.path && 
            strcmp(existing_var->source.path, node->source.path) != 0) {
            allow_dup = true;
        }
    }

    if (!allow_dup && find_declaration(var_name_str, scope, false, NULL, NULL) != NULL) {
        DUPLICATE_NAME_ERROR(node->source, var_name_str);
    }

    scope_add(scope, node);
    node->scope = scope;

    if (variable_type) {
        semantic_statement(variable_type, scope);
    }
    
    TypeData* var_td = variable_type ? get_type(variable_type, scope) : NULL;
    if (var_td && var_td->base == TYPEINV) {
        const char* type_name = undeclared_type_name(variable_type);
        UNDECLARED_TYPE_ERROR(variable_type->source, type_name);
    }

    if (variable_init) {
        TypeData* expected = var_td ? var_td : TypeData_typeinv();
        semantic_expression(variable_init, scope, expected);
    }

    if (variable_init && variable_type) {
        TypeData* td_init = TypeData_of(variable_init);
        bool ok = TypeData_match(var_td, td_init);

        if (!ok && td_init->base == ARRAY) {
            if (is_pointer_type(var_td)) {
                TypeData* elem = td_init->array.element_type;
                TypeData* pointee = TypeData_retype(var_td, var_td->pointer_depth - 1);
                ok = TypeData_match(pointee, elem) || (pointee->base == elem->base && pointee->pointer_depth == 0);
            } else if (var_td->base == SLICE) {
                ok = TypeData_match(td_init->array.element_type, var_td->slice.element_type);
            }
        }

        if (!ok) {
            TYPE_MISMATCH_ERROR(node->source, TypeData_to_string(var_td), TypeData_to_string(td_init));
        }

        check_literal_fits_target(node->source, variable_init, var_td);
    }

    TypeData* symbol_type = variable_type ? var_td : (variable_init ? TypeData_of(variable_init) : NULL);

    if (symbol_type) {
        const char* unsized = find_unsized_struct(symbol_type, 0);
        if (unsized) {
            UNSIZED_TYPE_ERROR(node->source, var_name_str, TypeData_to_string(symbol_type), unsized);
        }
    }

    if (symbol_type) {
        const char* sym_name = var_name_str;
        node->symbol = Symbol_create(sym_name, symbol_type, node);
    } else {
        node->symbol = NULL;
    }
}

// Structural equality for type ASTs. `identifier_name()` returns NULL for
// composite types (function/array/slice/pointer), so signature comparisons
// must not strcmp its result. NULL denotes an omitted (default `u0`) type.
static int ast_type_equal(const Ast* a, const Ast* b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->type != b->type) return 0;

    switch (a->type) {
        case AST_IDENTIFIER: {
            const char* an = a->identifier.name;
            const char* bn = ((const Ast*)b)->identifier.name;
            if (!an || !bn) return an == bn;
            return strcmp(an, bn) == 0;
        }
        case AST_STAR_EXPRESSION:
            return ast_type_equal(a->star_expression.object,
                                  ((const Ast*)b)->star_expression.object);
        case AST_FUNCTION_TYPE: {
            Ast** ap = a->function_type.params;
            Ast** bp = ((const Ast*)b)->function_type.params;
            size_t an = ap ? vector_size(ap) : 0;
            size_t bn = bp ? vector_size(bp) : 0;
            if (an != bn) return 0;
            for (size_t i = 0; i < an; i++) {
                if (!ast_type_equal(ap[i], bp[i])) return 0;
            }
            return ast_type_equal(a->function_type.return_type,
                                  ((const Ast*)b)->function_type.return_type);
        }
        case AST_ARRAY_TYPE:
            if (a->array_type.length != ((const Ast*)b)->array_type.length) return 0;
            return ast_type_equal(a->array_type.type, ((const Ast*)b)->array_type.type);
        case AST_SLICE_TYPE:
            return ast_type_equal(a->slice_type.type, ((const Ast*)b)->slice_type.type);
        default:
            return 0;
    }
}

static int param_lists_type_equal(Ast** a_params, Ast** b_params) {
    size_t an = a_params ? vector_size(a_params) : 0;
    size_t bn = b_params ? vector_size(b_params) : 0;
    if (an != bn) return 0;
    for (size_t i = 0; i < an; i++) {
        const Ast* a_type = a_params[i] ? a_params[i]->parameter_declaration.type : NULL;
        const Ast* b_type = b_params[i] ? b_params[i]->parameter_declaration.type : NULL;
        if (!a_type || !b_type) return 0;
        if (!ast_type_equal(a_type, b_type)) return 0;
    }
    return 1;
}

int check_signature_match(Ast* decl_node, Ast* def_node) {
    return ast_type_equal(decl_node->function_declaration.type,
                          def_node->function_declaration.type) &&
           param_lists_type_equal(decl_node->function_declaration.params,
                                  def_node->function_declaration.params);
}

void semantic_function_declaration(Ast* node, Scope* scope) {
    const char* function_name = identifier_name(node->function_declaration.name);
    Ast* function_type = node->function_declaration.type;

    if (!scope_is_global(scope)) {
        NON_GLOBAL_STATEMENT_ERROR(node->source, function_name);
        return;
    }

    if (node->function_declaration.params) {
        Ast** params = node->function_declaration.params;
        Scope* param_scope = scope_create(scope);

        for (int i = 0; i < vector_size(params); i++) {
            semantic_statement(params[i], param_scope);
        }
    }
 
    Ast* existing = find_declaration(function_name, scope, true, NULL, NULL);

    if (existing && (has_attribute(existing, "@private") || has_attribute(node, "@private"))) {
        if (!is_same_file(node->source, existing->source)) {
            existing = NULL;
        }
    }
    if (existing) {
        if (existing->type == AST_FUNCTION_DECLARATION || node->type == AST_FUNCTION_DECLARATION) {
            if (!check_signature_match(existing, node)) {
                CONFLICTING_TYPES_ERROR(node->source, function_name);
                return;
            }
        }
        if (existing != node) {
            DUPLICATE_NAME_ERROR(node->source, function_name);
            return;
        }
    } else {
        scope_add(scope, node);
    }
    node->scope = scope;

    TypeData* return_type;
    if (function_type) {
        return_type = get_type(function_type, scope);
        if (return_type->base == TYPEINV) {
            const char* type_name = undeclared_type_name(function_type);
            UNDECLARED_TYPE_ERROR(function_type->source, type_name);
        } else {
            const char* unsized = find_unsized_struct(return_type, 0);
            if (unsized) {
                char subject[160];
                snprintf(subject, sizeof(subject), "return type of '%s'",
                         function_name ? function_name : "<anonymous>");
                UNSIZED_TYPE_ERROR(function_type->source, subject,
                                   TypeData_to_string(return_type), unsized);
            }
        }
    } else {
        return_type = (TypeData*)(&TYPE_U0_DATA);
    }


    if (!node->symbol) {
        const char* sym_name = function_name;
        node->symbol = Symbol_create(sym_name, return_type, node);
    }
}

static void semantic_function_alias(Ast* node, Scope* scope) {
    const char* alias_name = identifier_name(node->function_alias.name);

    if (!scope_is_global(scope)) {
        NON_GLOBAL_STATEMENT_ERROR(node->source, alias_name);
        return;
    }

    Ast* existing = find_declaration(alias_name, scope, true, NULL, NULL);

    if (existing && existing != node && !is_file_local_duplicate(existing, node)) {
        DUPLICATE_NAME_ERROR(node->source, alias_name);
        return;
    }

    scope_add(scope, node);
    node->scope = scope;

    const char* sym_name = alias_name;
    node->symbol = Symbol_create(sym_name, NULL, NULL);
}

static void semantic_function_alias_resolve(Ast* node, Scope* scope) {
    Ast* target_node = node->function_alias.target;
    if (!target_node || !node->symbol || node->symbol->declaration) return;

    Ast* target = NULL;

    if (target_node->type == AST_SELECTOR_EXPRESSION) {
        if (!target_node->symbol) {
            semantic_selector_expression(target_node, scope);
        }
        if (target_node->symbol && target_node->symbol->declaration) {
            target = target_node->symbol->declaration;
        }
    } else {
        const char* target_str = identifier_name(target_node);
        if (!target_str) return;
        target = find_declaration(target_str, scope, true, &node->source, NULL);
    }

    if (!target) return;

    int hops = 0;
    while (target->type == AST_FUNCTION_ALIAS && target->symbol && target->symbol->declaration) {
        target = target->symbol->declaration;
        if (target == node) break;
        if (++hops > 64) break;
    }

    if (target == node || target->type == AST_FUNCTION_ALIAS) return;

    bool target_is_function = (target->type == AST_FUNCTION_DECLARATION ||
                               target->type == AST_FUNCTION_DEFINITION);
    if (!target_is_function) {
        TypeData* target_type = TypeData_of(target);
        if (!target_type || !is_function_type(target_type)) {
            snprintf(log_description, sizeof(log_description),
                     "cannot alias '%s' because it is not a function", identifier_name(target));
            log_source_error(node->source, ERROR_UNKNOWN, log_description);
            return;
        }
    }

    node->symbol->declaration = target;
    node->symbol->type = TypeData_of(target);
}

static void semantic_function_alias_report_unresolved(Ast* node) {
    const char* target_str = node->function_alias.target ? identifier_name(node->function_alias.target) : NULL;
    if (!target_str || !node->symbol || node->symbol->declaration) return;
    UNDECLARED_NAME_ERROR(node->source, target_str);
}

static void semantic_parameter_declaration(Ast* node, Scope* scope) {
    TypeData* type = get_type(node->parameter_declaration.type, scope);
    if (type->base == TYPEINV) {
        const char* type_name = undeclared_type_name(node->parameter_declaration.type);
        UNDECLARED_TYPE_ERROR(node->parameter_declaration.type->source, type_name);
        type = TypeData_of(node);
    }
    const char* unsized = find_unsized_struct(type, 0);
    if (unsized) {
        const char* pname = identifier_name(node->parameter_declaration.name);
        char subject[128];
        snprintf(subject, sizeof(subject), "parameter '%s'", pname ? pname : "<anonymous>");
        SourceSite site = node->source.path ? node->source
                                           : node->parameter_declaration.type->source;
        UNSIZED_TYPE_ERROR(site, subject, TypeData_to_string(type), unsized);
    }
    node->symbol  = Symbol_create(identifier_name(node->parameter_declaration.name), type, node);
    scope_add(scope, node);
}

static void semantic_field_declaration(Ast* node, Scope* scope) {
    TypeData* type = get_type(node->field_declaration.type, scope);
    if (type->base == TYPEINV) {
        const char* type_name = undeclared_type_name(node->field_declaration.type);
        UNDECLARED_TYPE_ERROR(node->field_declaration.type->source, type_name);
        type = TypeData_of(node);
    }
    const char* unsized = find_unsized_struct(type, 0);
    if (unsized) {
        const char* fname = identifier_name(node);
        char subject[128];
        snprintf(subject, sizeof(subject), "field '%s'", fname ? fname : "<anonymous>");
        UNSIZED_TYPE_ERROR(node->source, subject, TypeData_to_string(type), unsized);
    }
    node->symbol  = Symbol_create(identifier_name(node), type, node);
    scope_add(scope, node);
}

static TypeData* get_type_of_decl(Ast* decl, Scope* scope) {
    Ast** params  = decl->function_declaration.params;
    size_t n      = params ? vector_size(params) : 0;
    TypeData** ptds = NULL;
    if (n > 0) {
        ptds = c_alloc(n * sizeof(TypeData*));
        for (size_t i = 0; i < n; i++) {
            Ast* pt = params[i]->parameter_declaration.type;
            ptds[i] = pt ? get_type(pt, scope) : TypeData_typeinv();
        }
    }
    TypeData* ret = decl->function_declaration.type
        ? get_type(decl->function_declaration.type, scope)
        : (TypeData*)(&TYPE_U0_DATA);
    return TypeData_function(ptds, n, ret);
}

void semantic_identifier(Ast* node, Scope* scope, TypeData* expected_type) {
    size_t row = node->source.row;
    size_t col = node->source.col;

    const char* name = identifier_name(node);
    if (is_primitive_base(find_TypeBase(name))) return;

    Ast* decl = find_declaration(name, scope, 1, &node->source, NULL);

    if (!decl) {
        UNDECLARED_NAME_ERROR(node->source, name);
        return;
    }
    if (decl->type == AST_FUNCTION_ALIAS &&
        (!decl->symbol || !decl->symbol->declaration)) {
        semantic_function_alias_resolve(decl, decl->scope);
    }
    decl = resolve_function_alias(decl);

    if (decl->type == AST_FUNCTION_DECLARATION || decl->type == AST_FUNCTION_DEFINITION) {
        if (is_function_type(expected_type)) {
            TypeData* fn_td = get_type_of_decl(decl, scope);
            Symbol* sym = Symbol_create(name, fn_td, decl);
            sym->declaration = decl;
            node->symbol = sym;
            decl->function_declaration.is_called = true;
            return;
        }
        FUNCTION_USED_AS_VALUE_ERROR(node->source, name);
    }
    else if (decl->symbol) {
        if (decl->type == AST_VARIABLE_DECLARATION) {
            decl->variable_declaration.is_called = true;
        }
        node->symbol = decl->symbol;
    }
}

void semantic_function_body(Ast* node, Scope* scope) {
    assert(node->type == AST_FUNCTION_DEFINITION);
    size_t row = node->source.row;
    size_t col = node->source.col;

    const char* function_name = identifier_name(node->function_declaration.name);
    Ast* function_type = node->function_declaration.type;
    TypeData* td_fnc = function_type ? TypeData_of(node) : (TypeData*)(&TYPE_U0_DATA);

    current_function = FncRef_create(node, is_void_type(td_fnc));
    current_function.label_statements = vector_create();
    current_function.goto_statements = vector_create();

    Scope* function_scope = scope_create(scope);

    if (node->function_declaration.params) {
        Ast** params = node->function_declaration.params;
        int params_size = vector_size(params);

        for (int i = 0; i < params_size; i++) {
            Ast* param = params[i];
            scope_add(function_scope, param);
        }
    }

    if (node->function_declaration.body) {
        Ast** stmts = node->function_declaration.body->block.statements;

        bool has_return = false;
        int stmts_size = vector_size(stmts);

        for (int i = 0; i < stmts_size; i++) {
            Ast* stmt = stmts[i];
            if (stmt->type == AST_RETURN_STATEMENT) {
                has_return = true;
            }
            if (stmt->type == AST_FUNCTION_DECLARATION || stmt->type == AST_FUNCTION_DEFINITION) {
                scope_add(function_scope, stmt);
                TypeData* td_fnc = TypeData_of(stmt);
                if (!stmt->symbol) {
                    const char* base = identifier_name(stmt);
                    const char* mangled = base;
                    if (stmt->type == AST_FUNCTION_DEFINITION) {
                        char* qual = make_qualified_name(function_name, base);
                        if (qual) mangled = qual;
                    }
                    stmt->symbol = Symbol_create(mangled, td_fnc, stmt);
                }
                if (stmt->type == AST_FUNCTION_DEFINITION) {
                    FunctionRef outer = current_function;
                    semantic_function_body(stmt, scope);
                    current_function = outer;
                }
                continue;
            }
            semantic_statement(stmt, function_scope);
        }

        if (!is_void_type(td_fnc) && !has_return) {
            MISSING_RETURN_ERROR(node->source, function_name);
        }

        if (current_function.has_goto_statement) {
            for (int i = 0; i < vector_size(current_function.goto_statements); i++) {
                Ast* goto_stmt = current_function.goto_statements[i];
                const char* label_name = identifier_name(goto_stmt->goto_statement.label);
                Ast* label_stmt = find_declaration(label_name, function_scope, false, NULL, NULL);
                if (label_stmt && label_stmt->type == AST_LABEL_STATEMENT) {
                    goto_stmt->goto_statement.label->symbol = Symbol_create(
                        label_name,
                        NULL,
                        label_stmt->label_statement.name
                    );
                }
                else {
                    UNDECLARED_NAME_ERROR(goto_stmt->source, identifier_name(goto_stmt));
                }
            }
        }
    }
}

void semantic_if_statement(Ast* node, Scope* scope) {
    size_t row = node->source.row;
    size_t col = node->source.col;
    if (scope_is_global(scope)) GLOBAL_STATEMENT_ERROR(node->source, "if");

    Ast* if_node = node;

    if (if_node->if_statement.test) {
        semantic_expression(if_node->if_statement.test, scope, (TypeData*)(&TYPE_BOOL_DATA));
        TypeData* test_td = TypeData_of(if_node->if_statement.test);
        if (!is_boolean_type(test_td) && !is_numeric_type(test_td)) {
            TYPE_MISMATCH_ERROR(node->source, "bool", TypeData_to_string(test_td));
        }
    }

    if (if_node->if_statement.consequent) {
        Scope* if_scope = scope_create(scope);
        Ast** stmts = node->if_statement.consequent->block.statements;

        int stmts_size = vector_size(stmts);

        for (int i = 0; i < stmts_size; i++) {
            Ast* stmt = stmts[i];
            semantic_statement(stmt, if_scope);
        }
    }

    if (if_node->if_statement.alternate) {
        Ast* alt = if_node->if_statement.alternate;
        if (alt->type == AST_BLOCK_STATEMENT) {
            Scope* else_scope = scope_create(scope);
            Ast** stmts = alt->block.statements;

            int stmts_size = vector_size(stmts);

            for (int i = 0; i < stmts_size; i++) {
                Ast* stmt = stmts[i];
                semantic_statement(stmt, else_scope);
            }
        }
        else {
            semantic_statement(alt, scope);
        }
    }
}

void semantic_assignment_expression(Ast* node, Scope* scope) {
    const size_t row = node->source.row;
    const size_t col = node->source.col;

    Ast* left  = node->assignment_expression.left;
    Ast* right = node->assignment_expression.right;
    const char* op = node->assignment_expression.operator
        ? node->assignment_expression.operator : "=";

    semantic_expression(left, scope, TypeData_typeinv());
    semantic_expression(right, scope, TypeData_of(left));

    assert(left->symbol != NULL);

    Ast* decl = left->symbol->declaration;

    if (decl && decl->type == AST_VARIABLE_DECLARATION && decl->variable_declaration.is_constant) {
        ASSIGN_TO_CONST_ERROR(node->source, identifier_name(left));
        return;
    }

    TypeData* lhs_type = TypeData_of(left);
    TypeData* rhs_type = TypeData_of(right);

    if (strcmp(op, "=") != 0) {
        TypeData* result_td = NULL;
        if (!get_operator_result_type(lhs_type, rhs_type, op, &result_td)) {
            OPERATOR_TYPE_ERROR(node->source, op,
                TypeData_to_string(lhs_type), TypeData_to_string(rhs_type));
            return;
        }
    }
}

void semantic_compound_literal(Ast* node, Scope* scope) {
    Ast* type_expr = node->literal_compound.type;
    Ast* decl = NULL;
    const char* type_name = NULL;

    if (type_expr->type == AST_SELECTOR_EXPRESSION) {
        if (!type_expr->symbol) {
            semantic_expression(type_expr, scope, TypeData_typeinv());
        }
        if (type_expr->symbol && type_expr->symbol->declaration) {
            decl = type_expr->symbol->declaration;
            type_name = identifier_name(type_expr->selector_expression.member);
        }
    }

    if (!decl) {
        type_name = identifier_name(type_expr);
        decl = find_declaration(type_name, scope, 1, &node->source, NULL);
    }

    if (decl && decl->type == AST_FUNCTION_ALIAS &&
        (!decl->symbol || !decl->symbol->declaration)) {
        semantic_function_alias_resolve(decl, decl->scope);
    }
    decl = resolve_function_alias(decl);

    decl = resolve_struct_type_decl(decl);

    if (!decl) {
        UNDECLARED_NAME_ERROR(node->source, type_name ? type_name : "unknown");
        return;
    }

    Ast** fields = decl->struct_declaration.fields;
    size_t field_count = vector_size(fields);

    Ast** args = node->literal_compound.arguments;
    size_t arg_count = vector_size(args);

    if (arg_count != field_count) {
        STRUCT_EXPRESSION_VALUE_COUNT_ERROR(node->source, type_name,
            arg_count < field_count, field_count, arg_count);
        return;
    }

    for (size_t i = 0; i < arg_count; i++) {
        Ast* arg = args[i];
        TypeData* expected_type = TypeData_of(fields[i]);
        if (is_polymorphic(expected_type)) {
            semantic_expression(arg, scope, TypeData_typeinv());
            continue;
        }
        semantic_expression(arg, scope, expected_type);

        TypeData* arg_type = TypeData_of(arg);
        if (!TypeData_match(arg_type, expected_type)) {
            TYPE_MISMATCH_ERROR(arg->source, TypeData_to_string(expected_type), TypeData_to_string(arg_type));
        } else {
            check_literal_fits_target(arg->source, arg, expected_type);
        }
    }

    assert(decl->symbol);
    const char* sym_name = (type_expr->type == AST_SELECTOR_EXPRESSION) ? type_name : identifier_name(decl);
    Symbol* sym = Symbol_create(sym_name ? sym_name : type_name, decl->symbol->type, decl);
    sym->declaration = decl;
    node->symbol = sym;
}

static bool function_parameters_match(Ast* a, Ast* b) {
    Ast** params_a = a->function_declaration.params;
    Ast** params_b = b->function_declaration.params;

    int count_a = vector_size(params_a);
    int count_b = vector_size(params_b);

    if (count_a != count_b) {
        return false;
    }

    for (int i = 0; i < count_a; i++) {
        TypeData* type_a = TypeData_of(params_a[i]);
        TypeData* type_b = TypeData_of(params_b[i]);

        if (!TypeData_match(type_a, type_b)) {
            return false;
        }
    }
    return true;
}

void semantic_function_overload_declaration(Ast* node, Scope* scope) {
    if (node->symbol && node->symbol->declaration) return;

    const char* overload_name = identifier_name(node->function_overload_declaration.name);
    if (!overload_name) return;

    Ast* existing = find_declaration(overload_name, scope, true, NULL, NULL);
    if (existing && existing != node && !is_file_local_duplicate(existing, node)) {
        DUPLICATE_NAME_ERROR(node->source, overload_name);
        return;
    }
    if (!existing) {
        scope_add(scope, node);
        node->scope = scope;
    }
    node->symbol = Symbol_create(overload_name, NULL, NULL);

    Ast** stmts = node->function_overload_declaration.body->block.statements;
    int count = vector_size(stmts);

    Ast** functions = vector_create();

    for (int i = 0; i < count; i++) {
        Ast* stmt = stmts[i];
        const char* name = identifier_name(stmt);

        if (!name) {
            INVALID_MEMBER_NAME_ERROR(stmt->source);
            continue;
        }

        Ast* decl = find_declaration(name, scope, true, &stmt->source, NULL);

        if (decl == NULL) {
            UNDECLARED_NAME_ERROR(stmt->source, name);
            continue;
        }

        if (decl->type != AST_FUNCTION_DECLARATION &&
            decl->type != AST_FUNCTION_DEFINITION) {
            INVALID_OVERLOAD_FUNCTION_ERROR(stmt->source, name);
            continue;
        }
        
        stmt->symbol = Symbol_create(name, TypeData_of(decl), decl);
        vector_add(&functions, decl);
    }

    if (vector_size(functions) == 0) {
        EMPTY_OVERLOAD_SET_ERROR(node->source);
        node->symbol->declaration = node;
        return;
    }

    for (int i = 0; i < vector_size(functions); i++) {
        for (int n = i + 1; n < vector_size(functions); n++) {
            if (function_parameters_match(functions[i], functions[n])) {
                DUPLICATE_OVERLOAD_ERROR(functions[n]->source, identifier_name(functions[n]));
            }
        }
    }

    node->symbol->declaration = functions[0];
}

void semantic_call_expression(Ast* node, Scope* scope) {
    Ast* callee = node->call_expression.callee;
    bool is_selector = (callee->type == AST_SELECTOR_EXPRESSION);

    Ast* decl = NULL;
    const char* callee_name = NULL;
    TypeData* indirect_td = NULL;

    if (is_selector) {
        if (!callee->symbol) {
            semantic_expression(callee, scope, TypeData_typeinv());
        }

        if (callee->symbol && callee->symbol->declaration) {
            decl = callee->symbol->declaration;
            callee_name = identifier_name(callee->selector_expression.member);

            if (has_attribute(decl, "@private") &&
                decl->source.path && node->source.path &&
                strcmp(decl->source.path, node->source.path) != 0) {
                UNDECLARED_NAME_ERROR(node->source, callee_name);
                return;
            }
        }

        if (decl && !is_function_node(decl) &&
            is_function_type(callee->symbol->type)) {
            indirect_td = callee->symbol->type;
        }
    }

    if (!decl) {
        callee_name = identifier_name(callee);
        decl = find_declaration(callee_name, scope, true, &node->source, NULL);
        if (decl && !is_function_node(decl) &&
            decl->symbol && is_function_type(decl->symbol->type)) {
            indirect_td = decl->symbol->type;
        }
    }

    if (decl && decl->type == AST_FUNCTION_ALIAS &&
        (!decl->symbol || !decl->symbol->declaration)) {
        semantic_function_alias_resolve(decl, decl->scope);
    }
    decl = resolve_function_alias(decl);

    if (resolve_struct_type_decl(decl)) {
        STRUCT_NOT_CALLABLE_ERROR(node->source, callee_name ? callee_name : "unknown");
        return;
    }

    bool is_indirect = (indirect_td != NULL);

    if (!decl || decl->type == AST_FUNCTION_ALIAS ||
        (!is_function_node(decl) && !is_indirect)) {
        UNDECLARED_NAME_ERROR(node->source, callee_name ? callee_name : "unknown");
        return;
    }

    bool is_function = !is_indirect && is_function_node(decl) &&
                       decl->type != AST_FUNCTION_OVERLOAD_DECLARATION;
    bool is_overload = decl->type == AST_FUNCTION_OVERLOAD_DECLARATION;

    if (scope_is_global(scope)) {
        GLOBAL_FUNCTION_CALL_ERROR(node->source, identifier_name(node));
        return;
    }

    Ast** expected_args;
    size_t expected_count;
    bool has_ellipsis = false;
    TypeData* call_result_td;

    if (is_overload) {
        Ast** call_args = node->call_expression.arguments;
        size_t call_args_count = vector_size(call_args);

        /* Analyze the arguments first so their types are known for matching. */
        for (size_t i = 0; i < call_args_count; i++) {
            semantic_expression(call_args[i], scope, TypeData_typeinv());
        }

        if (decl->symbol == NULL || decl->symbol->declaration == NULL) {
            semantic_function_overload_declaration(decl, global_scope);
        }

        Ast** functions       = decl->function_overload_declaration.body->block.statements;
        int   functions_count = vector_size(functions);

        decl = NULL;
        for (int i = 0; i < functions_count; i++) {
            Ast* candidate = (functions[i] && functions[i]->symbol)
                ? functions[i]->symbol->declaration : NULL;

            if (!candidate ||
                (candidate->type != AST_FUNCTION_DECLARATION &&
                 candidate->type != AST_FUNCTION_DEFINITION)) {
                continue;
            }

            Ast** params = candidate->function_declaration.params;
            size_t param_count = params ? vector_size(params) : 0;
            if (param_count != call_args_count) continue;

            bool is_match = true;
            for (size_t k = 0; k < param_count; k++) {
                TypeData* param_td = TypeData_of(params[k]);
                Ast* arg = call_args[k];

                /* An exact type match always wins. Otherwise an untyped
                   literal adapts to the parameter's type so `f(1)` can
                   select `f(x: u8)`. */
                if (!TypeData_match(TypeData_of(arg), param_td)) {
                    bool literal_adapts = false;

                    if (param_td->pointer_depth == 0 && is_numeric_type(param_td)) {
                        if (arg->type == AST_LITERAL_INTEGER && is_integer_type(param_td)) {
                            long value;
                            literal_adapts = eval_constant_integer(arg, &value) &&
                                             check_value_fits_target(value, param_td->base);
                        } else if (arg->type == AST_LITERAL_FLOAT && is_float_type(param_td)) {
                            literal_adapts = true;
                        }
                    }
                    if (!literal_adapts && arg->type == AST_LITERAL_NIL &&
                        (is_pointer_type(param_td) || is_function_type(param_td))) {
                        literal_adapts = true;
                    }

                    if (!literal_adapts) {
                        is_match = false;
                        break;
                    }
                }
            }
            if (is_match) decl = candidate;
        }

        if (decl == NULL) {
            NO_MATCHING_FUNCTION_OVERLOAD_ERROR(node->source, callee_name ? callee_name : "unknown");
            return;
        }

        is_function    = true;
        expected_args  = decl->function_declaration.params;
        expected_count = expected_args ? vector_size(expected_args) : 0;
        has_ellipsis   = false;
        call_result_td = decl->symbol->type;
    }

    else if (is_indirect) {
        expected_args  = NULL;
        expected_count = indirect_td->function.param_count;
        call_result_td = indirect_td->function.return_type;
    } else {
        expected_args  = decl->function_declaration.params;
        expected_count = vector_size(expected_args);
        has_ellipsis   = decl->function_declaration.has_ellipsis;
        call_result_td = decl->symbol->type;
    }

    Ast** call_args = node->call_expression.arguments;
    size_t call_args_count = vector_size(call_args);

    if (call_args_count < expected_count ||
        (!has_ellipsis && call_args_count > expected_count)) {
        FUNCTION_ARGUMENT_COUNT_ERROR(node->source, callee_name, expected_count, call_args_count);
        return;
    }

    for (size_t i = 0; i < call_args_count; i++) {
        Ast* arg = call_args[i];

        if (i >= expected_count) {
            semantic_expression(arg, scope, TypeData_typeinv());
            continue;
        }

        TypeData* expected_type = is_indirect
            ? indirect_td->function.param_types[i]
            : TypeData_of(expected_args[i]);
        if (is_polymorphic(expected_type)) {
            semantic_expression(arg, scope, TypeData_typeinv());
            continue;
        }

        semantic_expression(arg, scope, expected_type);
        TypeData* arg_type = TypeData_of(arg);

        if (!TypeData_match(arg_type, expected_type)) {
            TYPE_MISMATCH_ERROR(arg->source, TypeData_to_string(expected_type), TypeData_to_string(arg_type));
        } else {
            check_literal_fits_target(arg->source, arg, expected_type);
        }
    }

    assert(decl->symbol);
    
    if (is_function) {
        decl->function_declaration.is_called = true;
    }

    const char* sym_name = is_selector ? callee_name : identifier_name(decl);
    Symbol* sym = Symbol_create(sym_name ? sym_name : callee_name, call_result_td, decl);
    sym->declaration = decl;

    node->symbol = sym;
    if (is_indirect) {
        if (!is_selector && callee->symbol == NULL) {
            semantic_expression(callee, scope, indirect_td);
        }
    } else {
        callee->symbol = sym;
    }

}

static int private_struct_counter = 0;

static void semantic_struct_declaration(Ast* node, Scope* scope) {
    const char* struct_name = identifier_name(node->struct_declaration.name);

    bool is_private = has_attribute(node, "@private");

    if (!scope_is_global(scope) && !is_private) {
        if (!current_function.declaration) {
            NON_GLOBAL_STATEMENT_ERROR(node->source, struct_name);
        }
    }

    Ast* existing_struct = find_declaration(struct_name, scope, true, NULL, NULL);
    if (existing_struct) {
        bool is_priv_existing = has_attribute(existing_struct, "@private");
        bool is_priv_node = is_private;
        bool same_file = existing_struct->source.path && node->source.path && 
                         strcmp(existing_struct->source.path, node->source.path) == 0;

        bool is_file_local_dup = (is_priv_existing || is_priv_node) && !same_file;

        if (!is_file_local_dup) {
            if (find_declaration(struct_name, scope, false, NULL, NULL) != NULL ||
                (is_priv_existing != is_priv_node && same_file)) {
                DUPLICATE_NAME_ERROR(node->source, struct_name);
                return;
            }
        }
    } else if (find_declaration(struct_name, scope, false, NULL, NULL) != NULL) {
        DUPLICATE_NAME_ERROR(node->source, struct_name);
        return;
    }

    scope_add(scope, node);
    node->scope = scope;
    node->struct_declaration.is_defined = false;

    char* qual_name = NULL;

    if (is_private) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s.%d", struct_name, private_struct_counter++);
        qual_name = strdup(buf);
    } else if (!scope_is_global(scope)) {
        const char* parent = current_function.declaration ? identifier_name(current_function.declaration) : "local";
        qual_name = make_qualified_name(parent, struct_name);
    }

    const char* sym_name = qual_name ? qual_name : struct_name;
    StructDef* struct_def = qual_name ? register_struct_skeleton_qualified(node, qual_name)
                                      : register_struct_skeleton(node);

    node->symbol = Symbol_create(sym_name, TypeData_intern(TYPEDEF, 0, sym_name, struct_def), node);
}

static void semantic_struct_body(Ast* node, Scope* scope) {
    if (node->symbol && node->symbol->name) {
        Ast* decl = node;
        if (decl && decl->type == AST_STRUCT_DECLARATION) {
            decl->struct_declaration.is_defined = true;
        }

        StructDef* struct_def = NULL;
        if (node->symbol->type && node->symbol->type->base == TYPEDEF) {
            struct_def = node->symbol->type->struct_def;
        }
        if (!struct_def) {
            struct_def = register_struct_skeleton_qualified(node, node->symbol->name);
        }
        Ast** fields    = node->struct_declaration.fields;
        Scope* struct_scope = scope_create(scope);
        int field_size = vector_size(fields);
        for (int i = 0; i < field_size; i++) {
            Ast* field = fields[i];
            semantic_statement(field, struct_scope);
        }
        fill_struct_fields(struct_def, node);
        return;
    }
    const char* struct_name = identifier_name(node->struct_declaration.name);
    const char* lookup_name = struct_name;
    char* qual = NULL;
    
    if (!scope_is_global(scope)) {
        const char* parent = current_function.declaration ? identifier_name(current_function.declaration) : "local";
        qual = make_qualified_name(parent, struct_name);
        if (qual) lookup_name = qual;
    }
    
    Ast* decl = find_declaration(struct_name, scope, false, NULL, NULL);
    if (!decl) decl = find_declaration(struct_name, scope, true, NULL, NULL);
    if (decl && decl->type == AST_STRUCT_DECLARATION) {
        decl->struct_declaration.is_defined = true;
    }

    Ast* owner = (decl && decl->type == AST_STRUCT_DECLARATION) ? decl : node;
    StructDef* struct_def = NULL;
    if (owner->symbol && owner->symbol->type && owner->symbol->type->base == TYPEDEF) {
        struct_def = owner->symbol->type->struct_def;
    }
    if (!struct_def) {
        struct_def = register_struct_skeleton_qualified(owner, lookup_name);
    }
    if (!node->symbol) {
        node->symbol = Symbol_create(lookup_name,
            TypeData_intern(TYPEDEF, 0, lookup_name, struct_def), node);
    }

    Ast** fields    = node->struct_declaration.fields;
    Scope* struct_scope = scope_create(scope);

    int field_size = vector_size(fields);
    for (int i = 0; i < field_size; i++) {
        Ast* field = fields[i];
        semantic_statement(field, struct_scope);
    }

    fill_struct_fields(struct_def, node);
}

void semantic_return_statement(Ast* node, Scope* scope) {
    size_t row = node->source.row;
    size_t col = node->source.col;
    
    if (scope_is_global(scope)) {
        GLOBAL_STATEMENT_ERROR(node->source, "return");
    }

    Ast* fnc_dec = current_function.declaration;
    if (fnc_dec) {
        TypeData* dec_type = TypeData_of(current_function.declaration);
        Ast* ret_val = node->return_statement.value;

        if (!ret_val) {
            if (!is_void_type(dec_type)) {
                RETURN_TYPE_MISMATCH_ERROR(node->source, TypeData_to_string(dec_type), TYPE_U0_STRING);
            }
            return;
        }

        if (is_integer_literal(ret_val) && (is_integer_type(dec_type) || is_float_type(dec_type))) {
            ret_val->symbol = Symbol_create(NULL, dec_type, ret_val);
        }
        else if (is_float_literal(ret_val) && is_float_type(dec_type)) {
            ret_val->symbol = Symbol_create(NULL, dec_type, ret_val);
        }
        else{
            semantic_expression(node->return_statement.value, scope, dec_type);
        }

        TypeData* ret_type = TypeData_of(node->return_statement.value);
        if (!TypeData_match(ret_type, dec_type)) {
            char expected[256];
            char found[256];
            TYPE_MISMATCH_ERROR(node->source, TypeData_to_string(dec_type), TypeData_to_string(ret_type));
        }
    }
}

void semantic_loop_statement(Ast* node, Scope* scope) {
    size_t row = node->source.row;
    size_t col = node->source.col;

    if (scope_is_global(scope)) {
        GLOBAL_STATEMENT_ERROR(node->source, "for");
    }

    Ast* iterator = node->loop_statement.iterator;
    Ast* sequence = node->loop_statement.sequence;
    Ast* body     = node->loop_statement.body;
    Scope* loop_scope = scope_create(scope);

    if (sequence) {
        semantic_expression(sequence, scope, (TypeData*)(&TYPE_I32_DATA));
    }

    if (iterator) {
        if (iterator->field_declaration.type == NULL) {
            iterator->symbol = Symbol_create(identifier_name(iterator->field_declaration.name), TypeData_of(sequence), iterator);
        }
        scope_add(loop_scope, iterator);
    }

    if (body) {
        Ast** stmts = body->block.statements;
        int stmts_size = vector_size(stmts);
        for (int i = 0; i < stmts_size; i++) {
            Ast* stmt = stmts[i];
            semantic_statement(stmt, loop_scope);
        }
    }
}

void semantic_range_expression(Ast* node, Scope* scope, TypeData* expected_type) {
    size_t row = node->source.row;
    size_t col = node->source.col;

    Ast* start = node->range_expression.start;
    Ast* end   = node->range_expression.end;

    semantic_expression(start, scope, TypeData_typeinv());
    semantic_expression(end,   scope, TypeData_typeinv());

    TypeData* ts = TypeData_of(start);
    TypeData* te = TypeData_of(end);

    if (is_integer_literal(start) && is_integer_literal(end)) {
        node->symbol = Symbol_create(NULL, (TypeData*)(&TYPE_I32_DATA), node);
    } else {
        coerce_literal_to_match(&start, &ts, end, te);
        coerce_literal_to_match(&end, &te, start, ts);

        if (!is_integer_type(ts) || !is_integer_type(te)) {
            RANGE_NON_INTEGER_ERROR(node->source);
            node->symbol = Symbol_create(NULL, TypeData_typeinv(), node);
            return;
        }

        if (!TypeData_match(ts, te)) {
            RANGE_TYPE_MISMATCH_ERROR(node->source, TypeData_to_string(ts), TypeData_to_string(te));
            node->symbol = Symbol_create(NULL, TypeData_typeinv(), node);
            return;
        }

        node->symbol = Symbol_create(NULL, ts, node);
    }

    if (expected_type && expected_type->base == ARRAY &&
        node->symbol->type->base != TYPEINV) {
        long start_v, end_v;
        if (!eval_constant_integer(start, &start_v) ||
            !eval_constant_integer(end, &end_v) ||
            (size_t)(end_v - start_v) == expected_type->array.length) {
            node->symbol->type = expected_type;
        }
    }
}

void semantic_selector_expression(Ast* node, Scope* scope) {
    Ast* object = node->selector_expression.object;
    Ast* member = node->selector_expression.member;

    semantic_expression(object, scope, TypeData_typeinv());

    const char* member_name = identifier_name(member);

    if (object->type == AST_IDENTIFIER) {
        const char* obj_name = identifier_name(object);
        Ast* obj_decl = find_declaration(obj_name, scope, true, NULL, NULL);

        if (obj_decl && obj_decl->type == AST_ENUM_DECLARATION) {
            Ast** members = obj_decl->enum_declaration.members;

            for (size_t i = 0; i < vector_size(members); i++) {
                Ast* mem = members[i];

                if (!mem || !mem->symbol) {
                    continue;
                }

                if (strcmp(identifier_name(mem->enumerator.name), member_name) == 0) {
                    node->symbol = Symbol_create(NULL, mem->symbol->type, mem);
                    node->symbol->declaration = mem;
                    return;
                }
            }

            ENUM_NO_MEMBER_ERROR(
                node->source,
                identifier_name(obj_decl->enum_declaration.name),
                member_name
            );
            node->symbol = Symbol_create(NULL, TypeData_typeinv(), node);
            return;
        }
    }

    TypeData* object_td =
        (object->type == AST_SELECTOR_EXPRESSION && object->symbol)
            ? object->symbol->type
            : TypeData_of(object);

    if (object_td->base != TYPEDEF) {
        MEMBER_OF_NON_STRUCT_ERROR(
            node->source,
            TypeData_to_string(object_td)
        );
        node->symbol = Symbol_create(NULL, TypeData_typeinv(), node);
        return;
    }

    StructDef* struct_def = object_td->struct_def;

    if (!struct_def) {
        INVALID_STRUCT_ERROR(node->source);
        node->symbol = Symbol_create(NULL, TypeData_typeinv(), node);
        return;
    }

    if (!struct_def->fields) {
        STRUCT_FIELDS_UNDEFINED_ERROR(
            node->source,
            struct_def->name
        );
        node->symbol = Symbol_create(NULL, TypeData_typeinv(), node);
        return;
    }

    if (!member_name) {
        INVALID_MEMBER_NAME_ERROR(node->source);
        node->symbol = Symbol_create(NULL, TypeData_typeinv(), node);
        return;
    }

    const StructField* field = NULL;

    for (size_t i = 0; i < struct_def->field_count; i++) {
        if (struct_def->fields[i].name &&
            strcmp(member_name, struct_def->fields[i].name) == 0) {
            field = &struct_def->fields[i];
            break;
        }
    }

    if (!field) {
        UNKNOWN_FIELD_ERROR(
            node->source,
            member_name,
            struct_def->name
        );
        node->symbol = Symbol_create(NULL, TypeData_typeinv(), node);
        return;
    }

    node->selector_expression.field_index = field->index;
    node->symbol = Symbol_create(NULL, field->type, node);
}

void semantic_cast_expression(Ast* node, Scope* scope) {
    Ast* type = node->cast_expression.type;
    Ast* arg  = node->cast_expression.argument;

    if (type->type == AST_IDENTIFIER) {
        semantic_expression(type, scope, TypeData_typeinv());
    }
    TypeData* type_td_check = get_type(type, scope);
    if (type_td_check->base == TYPEINV) {
        const char* type_name = TypeData_to_string(type_td_check);
        UNDECLARED_TYPE_ERROR(type->source, type_name);
    }

    if (!type->symbol) type->symbol = Symbol_create(NULL, type_td_check, type);

    TypeData* arg_expected = (arg && arg->type == AST_LITERAL_NIL)
        ? type_td_check
        : TypeData_typeinv();
    semantic_expression(arg, scope, arg_expected);

    TypeData* arg_td  = TypeData_of(arg);
    TypeData* type_td = TypeData_of(type);

    if (arg && arg->type == AST_LITERAL_NIL &&
        type_td->base != TYPEINV &&
        !(is_pointer_type(type_td) || is_function_type(type_td))) {
        INVALID_CAST_TARGET_ERROR(node->source, TypeData_to_string(type_td));
        node->symbol = Symbol_create(NULL, type_td, node);
        return;
    }

    if (!is_pointer_type(type_td) && !is_function_type(type_td)) {
        const char* unsized = find_unsized_struct(type_td, 0);
        if (unsized) {
            UNSIZED_TYPE_ERROR(node->source, "cast target",
                               TypeData_to_string(type_td), unsized);
        }
    }

    if (is_pointer_type(arg_td) && is_integer_type(type_td) && type_td->base != USIZE) {
        POINTER_TO_INT_ERROR(node->source,TypeData_to_string(type_td));
    }

    node->symbol = Symbol_create(NULL, TypeData_of(type), node);
}

void semantic_unary_expression(Ast* node, Scope* scope, TypeData* expected_type) {
    Ast* arg = node->unary_expression.argument;
    if (!arg) return;

    TypeData* arg_expected = (is_numeric_type(expected_type) && expected_type->pointer_depth == 0)
        ? expected_type : TypeData_typeinv();
    semantic_expression(arg, scope, arg_expected);

    const char* op = node->unary_expression.operator;
    TypeData* arg_td = TypeData_of(arg);
    TypeData* result_td = arg_td;

    if (strcmp(op, "!") == 0) {
        result_td = (TypeData*)(&TYPE_BOOL_DATA);
    } else if (strcmp(op, "-") == 0) {
        result_td = arg_td;
        if (is_integer_literal(arg) && is_numeric_type(expected_type) && expected_type->pointer_depth == 0) {
            result_td = expected_type;
            if (arg->symbol) arg->symbol->type = expected_type;
        }
    } else if (strcmp(op, "&") == 0) {
        result_td = TypeData_retype(arg_td, arg_td->pointer_depth + 1);
    }

    node->symbol = Symbol_create(NULL, result_td, node);
}

static void semantic_dereference_expression(Ast* node, Scope* scope) {
    size_t row = node->source.row, col = node->source.col;
    Ast* inner = node->star_expression.object;

    semantic_expression(inner, scope, TypeData_typeinv());

    TypeData* inner_td = TypeData_of(inner);
    TypeData* result_td = inner_td;

    if (inner_td->pointer_depth == 0) {
        DEREFERENCE_NON_POINTER_ERROR(node->source, TypeData_to_string(inner_td));
        result_td = TypeData_typeinv();
    }
    else if (inner_td->pointer_depth == 1 && inner_td->base == U0) {
        DEREFERENCE_VOID_POINTER_ERROR(node->source);
        result_td = TypeData_typeinv();
    }
    else {
        result_td = TypeData_retype(inner_td, inner_td->pointer_depth - 1);
    }

    node->symbol = Symbol_create(NULL, result_td, node);
}


static void semantic_postfix_expression(Ast* node, Scope* scope) {
    size_t row = node->source.row, col = node->source.col;

    Ast* arg = node->postfix_expression.argument;
    semantic_expression(arg, scope, TypeData_typeinv());

    TypeData* arg_td = TypeData_of(arg);

    switch (arg->type) {
        case AST_IDENTIFIER:
        case AST_SELECTOR_EXPRESSION:
        case AST_INDEX_EXPRESSION:
        case AST_STAR_EXPRESSION:
            break;
        default:
            EXPRESSION_NOT_ASSIGNABLE_ERROR(node->source);
            node->symbol = Symbol_create(NULL, TypeData_typeinv(), node);
            return;
    }

    if (!is_numeric_type(arg_td)) {
        OPERATOR_TYPE_ERROR(node->source, node->postfix_expression.operator,
            TypeData_to_string(arg_td), TypeData_to_string(arg_td));
        node->symbol = Symbol_create(NULL, TypeData_typeinv(), node);
        return;
    }

    if (arg->symbol && arg->symbol->declaration &&
        arg->symbol->declaration->type == AST_VARIABLE_DECLARATION &&
        arg->symbol->declaration->variable_declaration.is_constant) {
        ASSIGN_TO_CONST_ERROR(node->source, identifier_name(arg));
        node->symbol = Symbol_create(NULL, TypeData_typeinv(), node);
        return;
    }

    node->symbol = Symbol_create(NULL, arg_td, node);
}

void semantic_binary_expression(Ast* node, Scope* scope, TypeData* expected_type) {
    size_t row = node->source.row, col = node->source.col;
    Ast* left_node  = node->binary_expression.left;
    Ast* right_node = node->binary_expression.right;
    const char* op  = node->binary_expression.operator;

    semantic_expression(left_node, scope, expected_type);
    semantic_expression(right_node, scope, expected_type);

    TypeData* left_td = TypeData_of(left_node);
    TypeData* right_td = TypeData_of(right_node);

    coerce_literal_to_match(&left_node, &left_td, right_node, right_td);
    coerce_literal_to_match(&right_node, &right_td, left_node, left_td);
    coerce_literals_together(left_node, right_node, &left_td, &right_td, expected_type);

    if (left_node->type == AST_LITERAL_NIL && right_td->pointer_depth > 0) {
        left_td = right_td;
        if (left_node->symbol) left_node->symbol->type = right_td;
    }
    if (right_node->type == AST_LITERAL_NIL && left_td->pointer_depth > 0) {
        right_td = left_td;
        if (right_node->symbol) right_node->symbol->type = left_td;
    }

    char lstr[256], rstr[256];
    TypeData* result_td;

    if ((left_td->base == 0 && right_td->base == 0) || !get_operator_result_type(left_td, right_td, op, &result_td)) {
        OPERATOR_TYPE_ERROR(node->source, op, TypeData_to_string(left_td), TypeData_to_string(right_td));
        return;
    }

    node->symbol = Symbol_create(NULL, result_td, node);
}


static void semantic_index_expression(Ast* node, Scope* scope) {
    size_t row = node->source.row;
    size_t col = node->source.col;

    Ast* object_node = node->index_expression.object;
    Ast* index_node  = node->index_expression.index;

    assert(object_node != NULL);
    assert(index_node  != NULL);

    semantic_expression(object_node, scope, TypeData_typeinv());
    semantic_expression(index_node, scope, TypeData_typeinv());

    TypeData* object_td = TypeData_of(object_node);
    TypeData* index_td  = TypeData_of(index_node);
    TypeData* result_td = NULL;

    
    if (index_node->type == AST_RANGE_EXPRESSION) {
        
        TypeData* elem_td = NULL;
        if (object_td->base == ARRAY) {
            elem_td = object_td->array.element_type;
        } else if (object_td->base == SLICE) {
            elem_td = object_td->slice.element_type;
        } else if (is_pointer_type(object_td) && object_td->pointer_depth >= 1) {
            elem_td = TypeData_retype(object_td, object_td->pointer_depth - 1);
        } else {
            NON_INDEXABLE_TYPE_ERROR(node->source, TypeData_to_string(object_td));
            node->symbol = Symbol_create(NULL, TypeData_typeinv(), node);
            return;
        }
        
        TypeData* ts = TypeData_of(index_node->range_expression.start);
        TypeData* te = TypeData_of(index_node->range_expression.end);
        if (!is_integer_type(ts) || !is_integer_type(te)) {
            NON_INTEGER_INDEX_ERROR(node->source, TypeData_to_string(index_td));
        }
        result_td = TypeData_slice(elem_td);
        node->symbol = Symbol_create(NULL, result_td, node);
        return;
    }

    if (!is_integer_type(index_td)) {
        NON_INTEGER_INDEX_ERROR(node->source, TypeData_to_string(index_td));
    }

    if (object_td->base == ARRAY) {
        result_td = object_td->array.element_type;
    }
    else if (object_td->base == SLICE) {
        result_td = object_td->slice.element_type;
    }
    else if (is_pointer_type(object_td) && object_td->pointer_depth >= 1) {
        result_td = TypeData_retype(object_td, object_td->pointer_depth - 1);
    }
    else {
        NON_INDEXABLE_TYPE_ERROR(node->source, TypeData_to_string(object_td));
        node->symbol = Symbol_create(NULL, TypeData_typeinv(), node);
        return;
    }

    node->symbol = Symbol_create(NULL, result_td, node);
}

static void semantic_polymorphic(Ast* node ,Scope* scope) {
    TypeData* type = TypeData_of(node);
    node->symbol  = Symbol_create(node->polymorphic.name, type, node);
    scope_add(scope, node);
}

static void semantic_using_declaration(Ast* node, Scope* scope) {
    const char* alias_name = identifier_name(node->using_declaration.name);
    if (!alias_name) return;

    if (is_primitive_base(find_TypeBase(alias_name))) {
        snprintf(log_description, sizeof(log_description),
                "alias name '%s' conflicts with builtin type", alias_name);
        log_source_error(node->source, ERROR_UNKNOWN, log_description);
        return;
    }

    Ast* existing = find_declaration(alias_name, scope, false, NULL, NULL);
    if (existing && !is_file_local_duplicate(existing, node)) {
        DUPLICATE_NAME_ERROR(node->source, alias_name);
        return;
    }

    scope_add(scope, node);
    node->scope = scope;

    TypeData* alias_td = node->using_declaration.target
        ? get_type(node->using_declaration.target, scope)
        : TypeData_typeinv();
    if (alias_td->base == TYPEINV) {
        const char* type_name = identifier_name(node->using_declaration.target);
        if (!type_name) type_name = TypeData_to_string(alias_td);
        UNDECLARED_TYPE_ERROR(node->using_declaration.target ? node->using_declaration.target->source : node->source, type_name);
        node->symbol = Symbol_create(alias_name, TypeData_typeinv(), node);
        return;
    }

    const char* sym_name = alias_name;
    node->symbol = Symbol_create(sym_name, alias_td, node);
}

static int enum_value_fits_type(long value, TypeData* type) {
    return check_value_fits_target(value, type->base);
}

static void semantic_enum_declaration(Ast* node, Scope* scope) {
    if (node->symbol) return;

    const char* enum_name = identifier_name(node->enum_declaration.name);
    if (!enum_name) return;

    if (find_declaration(enum_name, scope, false, NULL, NULL)) {
        DUPLICATE_NAME_ERROR(node->source, enum_name);
        return;
    }

    scope_add(scope, node);
    node->scope = scope;

    TypeData* enum_type = NULL;
    if (node->enum_declaration.type) {
        enum_type = get_type(node->enum_declaration.type, scope);
        if (enum_type->base == TYPEINV) {
            const char* type_name = undeclared_type_name(node->enum_declaration.type);
            UNDECLARED_TYPE_ERROR(node->enum_declaration.type->source, type_name);
            enum_type = (TypeData*)(&TYPE_I32_DATA);
        } else if (!is_integer_type(enum_type)) {
            TYPE_MISMATCH_ERROR(node->enum_declaration.type->source, "integer type", TypeData_to_string(enum_type));
            enum_type = (TypeData*)(&TYPE_I32_DATA);
        }
    } else {
        enum_type = (TypeData*)(&TYPE_I32_DATA);
    }

    Ast** members  = node->enum_declaration.members;
    size_t count   = members ? vector_size(members) : 0;
    long   current = 0;

    for (size_t i = 0; i < count; i++) {
        Ast* mem = members[i];
        if (!mem) continue;

        const char* mem_name = identifier_name(mem->enumerator.name);
        if (!mem_name) continue;

        long value = current;
        if (mem->enumerator.value) {
            long explicit_value = 0;
            if (eval_constant_integer(mem->enumerator.value, &explicit_value)) {
                value = explicit_value;
            }
        }
        mem->enumerator.discriminant = (uint64_t)value;

        if (find_declaration(mem_name, scope, false, NULL, NULL)) {
            DUPLICATE_NAME_ERROR(mem->source, mem_name);
        } else {
            scope_add(scope, mem);
        }

        if (!enum_value_fits_type(value, enum_type)) {
            ENUM_VALUE_OVERFLOW_ERROR(mem->source, mem_name);
        }

        mem->symbol = Symbol_create(mem_name, enum_type, mem);
        current     = value + 1;
    }

    node->symbol = Symbol_create(enum_name, enum_type, node);
}

void semantic_literal(Ast* node, Scope* scope, TypeData* expected_type) {
    TypeData* type = NULL;

    switch (node->type) {        
        case AST_LITERAL_BOOLEAN:
            type = (TypeData*)(&TYPE_BOOL_DATA);
            break;

        case AST_LITERAL_STRING:
            type = TypeData_intern(U8, 1, TYPE_U8_STRING, NULL);
            break;

        case AST_LITERAL_FLOAT:
            type = is_numeric_type(expected_type)
                ? expected_type
                : (TypeData*)(&TYPE_F32_DATA);
            break;

        case AST_LITERAL_INTEGER:
            type = is_numeric_type(expected_type)
                ? expected_type
                : (TypeData*)(&TYPE_I32_DATA);
            break;

        case AST_LITERAL_CHAR:
            type = is_numeric_type(expected_type)
                ? expected_type
                : (TypeData*)(&TYPE_U8_DATA);
            break;

        case AST_LITERAL_NIL:
            type = (expected_type &&
                    (expected_type->pointer_depth > 0 || is_function_type(expected_type)))
                ? expected_type
                : TypeData_intern(NIL, 0, TYPE_NIL_STRING, NULL);
            break;

        case AST_LITERAL_HEXADECIMAL:
            type = resolve_bit_literal_type(node->source, node->literal_hexadecimal.value,
                                            hex_bit_length(node->literal_hexadecimal.value),
                                            0, expected_type);
            break;

        case AST_LITERAL_BINARY:
            type = resolve_bit_literal_type(node->source, node->literal_binary.value,
                                            binary_bit_length(node->literal_binary.value),
                                            1, expected_type);
            break;

        case AST_LITERAL_ARRAY: {
                int size = vector_size(node->literal_array.arguments);
                if (size <= 0) {
                    type = (TypeData*)(&TYPE_U0_DATA);
                }
                Ast* prev = NULL;
                for (int i = 0; i < size; i++) {
                    Ast* arg = node->literal_array.arguments[i];
                    semantic_expression(arg, scope, expected_type);
                    if (i > 0) {
                        if (!TypeData_match(TypeData_of(prev), TypeData_of(arg))) {
                            TYPE_MISMATCH_ERROR(arg->source, TypeData_to_string(TypeData_of(prev)), TypeData_to_string(TypeData_of(arg)));
                        }
                    }
                    prev = arg;
                }
                if (size > 0 && prev) {
                    type = TypeData_array(TypeData_of(prev), size);
                }
            }
            break;

        default:
            assert(0);
    }

    node->symbol = Symbol_create(NULL, type, node);
}

void semantic_expression(Ast* node, Scope* scope, TypeData* expected_type) {
    if (!node)
        return;

    switch (node->type) {
        case AST_IDENTIFIER:
            semantic_identifier(node, scope, expected_type);
            break;

        case AST_LITERAL_ARRAY:
        case AST_LITERAL_INTEGER:
        case AST_LITERAL_FLOAT:
        case AST_LITERAL_STRING:
        case AST_LITERAL_BOOLEAN:
        case AST_LITERAL_CHAR:
        case AST_LITERAL_BINARY:
        case AST_LITERAL_HEXADECIMAL:
        case AST_LITERAL_NIL:
            semantic_literal(node, scope, expected_type);
            break;

        case AST_LITERAL_COMPOUND:
            semantic_compound_literal(node, scope);
            break;

        case AST_BINARY_EXPRESSION:
            semantic_binary_expression(node, scope, expected_type);
            break;

        case AST_RANGE_EXPRESSION:
            semantic_range_expression(node, scope, expected_type);
            break;

        case AST_UNARY_EXPRESSION:
            semantic_unary_expression(node, scope, expected_type);
            break;

        case AST_POSTFIX_EXPRESSION:
            semantic_postfix_expression(node, scope);
            break;

        case AST_STAR_EXPRESSION:
            semantic_dereference_expression(node, scope);
            break;

        case AST_CAST_EXPRESSION:
            semantic_cast_expression(node, scope);
            break;

        case AST_CALL_EXPRESSION:
            semantic_call_expression(node, scope);
            break;

        case AST_SELECTOR_EXPRESSION:
            semantic_selector_expression(node, scope);
            break;

        case AST_ASSIGNMENT_EXPRESSION:
            semantic_assignment_expression(node, scope);
            break;

        case AST_INDEX_EXPRESSION:
            semantic_index_expression(node, scope);
            break;


        default:
            printf("%s\n",identifier_name(node));
            assert(0);
    }
}


static bool using_target_is_type(Ast* target, Scope* scope) {
    if (!target) return false;
    if (target->type == AST_SELECTOR_EXPRESSION) return false;
    if (target->type != AST_IDENTIFIER) return true;

    const char* target_name = identifier_name(target);
    if (!target_name) return false;
    if (is_primitive_base(find_TypeBase(target_name))) return true;

    Ast* decl = find_declaration(target_name, scope, true, NULL, NULL);
    return decl && (decl->type == AST_STRUCT_DECLARATION ||
                    decl->type == AST_ENUM_DECLARATION  ||
                    decl->type == AST_USING_DECLARATION);
}


static void retype_using_as_function_alias(Ast* node) {
    node->type = AST_FUNCTION_ALIAS;
}

static void resolve_using_declaration_kind(Ast* node, Scope* scope) {
    if (!node || node->type != AST_USING_DECLARATION) return;
    if (!node->using_declaration.ambiguous) return;
    if (using_target_is_type(node->using_declaration.target, scope)) return;

    retype_using_as_function_alias(node);
}

void semantic_statement(Ast* node, Scope* scope) {
    if (!node)  return;
    if (!scope) return;

    switch (node->type) {
        case AST_BLOCK_STATEMENT: {
            Ast** stmts = node->block.statements;
            for (int i = 0; i < vector_size(stmts); i++) {
                semantic_statement(stmts[i], scope);
            }
            break;
        }
        case AST_VARIABLE_DECLARATION:
            semantic_variable_declaration(node, scope);        
            break;

        case AST_POSTFIX_EXPRESSION:
            semantic_postfix_expression(node, scope);
            break;

        case AST_ASSIGNMENT_EXPRESSION:
            semantic_assignment_expression(node, scope);
            break;

        case AST_IF_STATEMENT:
            semantic_if_statement(node, scope);
            break;

        case AST_CALL_EXPRESSION:
            semantic_call_expression(node, scope);
            break;

        case AST_LITERAL_COMPOUND:
            semantic_compound_literal(node, scope);
            break;

        case AST_PARAMETER_DECLARATION:
            semantic_parameter_declaration(node, scope);
            break;

        case AST_FIELD_DECLARATION:
            semantic_field_declaration(node, scope);
            break;

        case AST_STRUCT_DECLARATION:
            semantic_struct_declaration(node, scope);
            break;

        case AST_ENUM_DECLARATION:
            semantic_enum_declaration(node, scope);
            break;

        case AST_LOOP_STATEMENT:
            semantic_loop_statement(node, scope);
            break;

        case AST_RETURN_STATEMENT:
            semantic_return_statement(node, scope);
            break;

        case AST_FUNCTION_DECLARATION:
            semantic_function_declaration(node, scope);
            break;
        
        case AST_FUNCTION_DEFINITION:
            semantic_function_declaration(node, scope);
            break;

        case AST_FUNCTION_OVERLOAD_DECLARATION:
            if (scope_is_global(scope)) {
                semantic_function_overload_declaration(node, scope);
            } else {
                NON_GLOBAL_STATEMENT_ERROR(node->source, identifier_name(node));
            }
            break;

        case AST_FUNCTION_ALIAS:
            if (!scope_is_global(scope)) {
                NON_GLOBAL_STATEMENT_ERROR(node->source, identifier_name(node));
            }
            break;

        case AST_SELECTOR_EXPRESSION:
            semantic_selector_expression(node, scope);
            break;

        case AST_POLYMORPHIC:
            semantic_polymorphic(node, scope);
            break;

        case AST_GOTO_STATEMENT:
            current_function.has_goto_statement = true;
            vector_add(&current_function.goto_statements, node);
            break;

        case AST_LABEL_STATEMENT: {
            const char* label_name = identifier_name(node->label_statement.name);
            Ast* existing = find_declaration(label_name, scope, true, NULL, node);
            if (existing) {
                DUPLICATE_NAME_ERROR(node->source, label_name);
            }
            scope_add(scope, node);
            vector_add(&current_function.label_statements, node);
            break;
        }

        case AST_USING_DECLARATION:
            resolve_using_declaration_kind(node, scope);
            if (node->type == AST_FUNCTION_ALIAS) {
                if (!scope_is_global(scope)) {
                    NON_GLOBAL_STATEMENT_ERROR(node->source, identifier_name(node));
                }
            } else {
                semantic_using_declaration(node, scope);
            }
            break;

        default:
            break;
    }
}

static void declare_enums_recursive(Ast* root, Scope* scope) {
    if (!root || !root->block.statements) return;
    for (size_t i = 0; i < vector_size(root->block.statements); i++) {
        Ast* stmt = root->block.statements[i];
        if (!stmt) continue;
        if (stmt->type == AST_ENUM_DECLARATION) {
            semantic_enum_declaration(stmt, scope);
        }
    }
}

static void declare_structs_recursive(Ast* root, Scope* scope) {
    if (!root || !root->block.statements) return;
    for (size_t i = 0; i < vector_size(root->block.statements); i++) {
        Ast* stmt = root->block.statements[i];
        if (!stmt) continue;
        if (stmt->type == AST_STRUCT_DECLARATION) {
            semantic_struct_declaration(stmt, scope);
        }
    }
}

static void declare_usings_recursive(Ast* root, Scope* scope) {
    if (!root || !root->block.statements) return;
    size_t count = vector_size(root->block.statements);

    for (size_t i = 0; i < count; i++) {
        Ast* stmt = root->block.statements[i];
        if (!stmt || stmt->type != AST_USING_DECLARATION) continue;
        if (stmt->using_declaration.ambiguous) continue;
        semantic_using_declaration(stmt, scope);
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < count; i++) {
            Ast* stmt = root->block.statements[i];
            if (!stmt || stmt->type != AST_USING_DECLARATION) continue;
            if (!stmt->using_declaration.ambiguous) continue;
            if (!using_target_is_type(stmt->using_declaration.target, scope)) continue;

            stmt->using_declaration.ambiguous = 0;
            semantic_using_declaration(stmt, scope);
            changed = true;
        }
    }

    for (size_t i = 0; i < count; i++) {
        Ast* stmt = root->block.statements[i];
        if (!stmt || stmt->type != AST_USING_DECLARATION) continue;
        if (!stmt->using_declaration.ambiguous) continue;
        retype_using_as_function_alias(stmt);
    }

}

static void declare_vars_funcs_recursive(Ast* root, Scope* scope) {
    if (!root || !root->block.statements) return;
    for (size_t i = 0; i < vector_size(root->block.statements); i++) {
        Ast* stmt = root->block.statements[i];
        if (!stmt) continue;
        if (stmt->type == AST_VARIABLE_DECLARATION) {
            semantic_variable_declaration(stmt, scope);
        } else if (stmt->type == AST_FUNCTION_DECLARATION || stmt->type == AST_FUNCTION_DEFINITION) {
            semantic_function_declaration(stmt, scope);
        } else if (stmt->type == AST_FUNCTION_ALIAS) {
            semantic_function_alias(stmt, scope);
        }
    }

}

static void declare_overloads_recursive(Ast* root, Scope* scope) {
    if (!root || !root->block.statements) return;
    for (size_t i = 0; i < vector_size(root->block.statements); i++) {
        Ast* stmt = root->block.statements[i];
        if (!stmt || stmt->type != AST_FUNCTION_OVERLOAD_DECLARATION) continue;
        semantic_function_overload_declaration(stmt, scope);
    }
}

static void resolve_aliases_recursive(Ast* root, Scope* scope, int* changed) {
    if (!root || !root->block.statements) return;
    for (size_t i = 0; i < vector_size(root->block.statements); i++) {
        Ast* stmt = root->block.statements[i];
        if (!stmt) continue;
        if (stmt->type == AST_FUNCTION_ALIAS) {
            bool before = stmt->symbol && stmt->symbol->declaration;
            semantic_function_alias_resolve(stmt, scope);
            bool after = stmt->symbol && stmt->symbol->declaration;
            if (after && !before) *changed = 1;
        }
    }

}

static void report_unresolved_aliases_recursive(Ast* root, Scope* scope) {
    if (!root || !root->block.statements) return;
    for (size_t i = 0; i < vector_size(root->block.statements); i++) {
        Ast* stmt = root->block.statements[i];
        if (!stmt) continue;
        if (stmt->type == AST_FUNCTION_ALIAS) {
            semantic_function_alias_report_unresolved(stmt);
        }
    }
}

static void resolve_aliases_fixpoint(Ast* root, Scope* scope) {
    for (int round = 0; round < 64; round++) {
        int changed = 0;
        resolve_aliases_recursive(root, scope, &changed);
        if (!changed) break;
    }
    report_unresolved_aliases_recursive(root, scope);
}

static void define_bodies_recursive(Ast* root, Scope* scope) {
    if (!root || !root->block.statements) return;

    for (size_t i = 0; i < vector_size(root->block.statements); i++) {
        Ast* stmt = root->block.statements[i];
        if (!stmt || stmt->type != AST_STRUCT_DECLARATION) continue;
        semantic_struct_body(stmt, scope);
    }
    for (size_t i = 0; i < vector_size(root->block.statements); i++) {
        Ast* stmt = root->block.statements[i];
        if (!stmt) continue;
        if (stmt->type == AST_FUNCTION_DEFINITION) {
            semantic_function_body(stmt, scope);
        } 
        else if (scope_is_global(scope)) {
            switch (stmt->type) {
                case AST_IDENTIFIER:
                    GLOBAL_STATEMENT_ERROR(stmt->source, identifier_name(stmt));
                    break;

                case AST_LOOP_STATEMENT:
                    GLOBAL_STATEMENT_ERROR(stmt->source, "loop");
                    break;

                case AST_CALL_EXPRESSION:
                    GLOBAL_STATEMENT_ERROR(stmt->source, "function call");
                    break;

                case AST_ASSIGNMENT_EXPRESSION:
                    GLOBAL_STATEMENT_ERROR(stmt->source, "assignment");
                    break;

                case AST_GOTO_STATEMENT:
                    GLOBAL_STATEMENT_ERROR(stmt->source, "goto");
                    break;

                default:
                    break;
            }
        }       
    }
}

void semantic(Ast *root, Ast** builtin) {

    global_scope = scope_create(NULL);
    for (size_t i = 0; i < vector_size(builtin); i++) {
        semantic_statement(builtin[i], global_scope);
    }

    declare_enums_recursive(root, global_scope);
    declare_structs_recursive(root, global_scope);
    declare_usings_recursive(root, global_scope);
    declare_vars_funcs_recursive(root, global_scope);
    declare_overloads_recursive(root, global_scope);
    resolve_aliases_fixpoint(root, global_scope);
    define_bodies_recursive(root, global_scope);

    if (semantic_errors) exit(EXIT_FAILURE);
}
