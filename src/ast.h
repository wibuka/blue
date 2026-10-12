#pragma once

typedef enum {
    BIND_IMMUTABLE,
    BIND_MUTABLE,
} BindingType;

typedef enum {
    AST_ROOT,

    AST_NAMESPACE,

    AST_FUNCTION_DECLARATION,
    AST_FUNCTION_DEFINITION,
    AST_FUNCTION_ALIAS,
    AST_ENUM_DECLARATION,
    AST_STRUCT_DECLARATION,
    AST_FIELD_DECLARATION,
    AST_PARAMETER_DECLARATION,
    AST_VARIABLE_DECLARATION,

    AST_IMPORT_STATEMENT,
    AST_USING_DECLARATION,
    AST_BLOCK_STATEMENT,
    AST_IF_STATEMENT,
    AST_LOOP_STATEMENT,
    AST_RETURN_STATEMENT,
    AST_CONTINUE_STATEMENT,
    AST_BREAK_STATEMENT,

    AST_ASSIGNMENT_EXPRESSION,
    AST_INDEX_EXPRESSION,
    AST_BINARY_EXPRESSION,
    AST_UNARY_EXPRESSION,
    AST_POSTFIX_EXPRESSION,
    AST_CAST_EXPRESSION,
    AST_CALL_EXPRESSION,
    AST_SELECTOR_EXPRESSION,
    AST_STAR_EXPRESSION,
    AST_RANGE_EXPRESSION,

    AST_LITERAL_COMPOUND,
    AST_LITERAL_INTEGER,
    AST_LITERAL_FLOAT,
    AST_LITERAL_HEXADECIMAL,
    AST_LITERAL_BINARY,
    AST_LITERAL_BOOLEAN,
    AST_LITERAL_CHAR,
    AST_LITERAL_STRING,
    AST_LITERAL_ARRAY,
    AST_LITERAL_NIL,

    AST_ENUMERATOR,
    AST_SLICE_TYPE,
    AST_ARRAY_TYPE,
    AST_POLYMORPHIC,
    AST_FUNCTION_TYPE,

    AST_IDENTIFIER,
} AstType;

typedef struct LLVMOpaqueValue *LLVMValueRef;
typedef struct LLVMOpaqueType *LLVMTypeRef;

typedef struct Symbol {
    const char*  name;
    TypeData*    type;
    Ast*         declaration;
    LLVMValueRef llvm_value;
    LLVMTypeRef  llvm_type;
} Symbol;

extern Ast** builtin_declarations;
void Ast_build();

typedef struct Ast {
    SourceSite source;
    AstType type;

    union {

        struct {
            Ast* name;
        } namespace;

        struct {
            Ast* name;
            Ast* target;
        } function_alias;

        struct {
            Ast* name;
            Ast* type;
            Ast* initializer;
            int  is_constant;
            bool is_called;
        } variable_declaration;

        struct {
            Ast* name;
            Ast* type;
        } parameter_declaration;
        
        struct {
            Ast*  name;
            Ast*  type;
            Ast** members; 
        } enum_declaration;

        struct {
            Ast*  name;
            Ast** fields;
            bool  is_defined;
        } struct_declaration;

        struct {
            Ast* name;
            Ast* type;
            int  index;
        } field_declaration;

        struct {
            Ast*  name;
            Ast*  type;
            Ast*  body;
            Ast** params;
            bool  has_ellipsis;
            bool  is_called;
            bool  is_defined;
        } function_declaration;

        struct {
            Ast** statements;
        } block;

        struct {
            Ast* iterator;
            Ast* sequence;
            Ast* body;
        } loop_statement;

        struct {
            Ast* test;
            Ast* consequent;
            Ast* alternate;
        } if_statement;

        struct {
            Ast* value;
        } return_statement;

        struct {
            int placeholder;
        } continue_statement;

        struct {
            int placeholder;
        } break_statement;

        struct {
            Ast* name;
            Ast* path;
        } import_statement;

        struct {
            Ast* name;
            Ast* target;
            int  ambiguous;
        } using_declaration;

        struct {
            char* operator;
            Ast*  argument;
        } unary_expression;

        struct {
            char* operator;
            Ast*  argument;
        } postfix_expression;

        struct {
            Ast*  left;
            char* operator;
            Ast*  right;
        } binary_expression;

        struct {
            Ast*  callee;
            Ast** arguments;
        } call_expression;

        struct {
            Ast*  left;
            char* operator;
            Ast*  right;
        } assignment_expression;

        struct {
            Ast* type;
            Ast* argument;
        } cast_expression;

        struct {
            Ast*   object;
            Ast*   member;
            size_t field_index;
        } selector_expression;

        struct {
            Ast* object;
            Ast* index;
        } index_expression;

        struct {
            Ast* object;
        } star_expression;

        struct {
            Ast* start;
            Ast* end;
        } range_expression;

        struct {
            Ast*  type;
            Ast** arguments;
        } literal_compound;

        struct {
            char* value;
        } literal_hexadecimal;

        struct {
            char* value;
        } literal_binary;

        struct {
            long value;
        } literal_integer;

        struct {
            double value;
        } literal_float;

        struct {
            Ast** arguments;
        } literal_array;

        struct {
            char*  value;
            size_t length;
        } literal_string;

        struct {
            int value;
        } literal_boolean;

        struct {
            char value;
        } literal_char;

        struct {
            Ast* type;
            size_t length;
        } array_type;

        struct {
            Ast* type;
        } slice_type;

        struct {
            Ast*  name;
            Ast*  value;
            uint64_t discriminant;
        } enumerator;

        struct {
            Ast** params;
            Ast*  return_type;
        } function_type;

        struct {
            const char* name;
        } polymorphic;

        struct {
            const char* name;
        } identifier;
    };

    Symbol* symbol;
    Scope* scope;
    char** attributes;
} Ast;

Ast* Ast_create(AstType type);
int ast_merge(Ast* target, Ast* source);
const char* identifier_name(Ast* node);

static inline Ast* Ast_create_identifier(const char* name) {
    Ast* res = Ast_create(AST_IDENTIFIER);
    res->identifier.name = name;
    return res;
}

static inline Ast* Ast_create_polymorphic(const char* name) {
    Ast* res = Ast_create(AST_POLYMORPHIC);
    res->polymorphic.name = name;
    return res;
}

static inline int is_global_node(Ast* node) {
    if (!node || !node->scope) return 1;
    if (!node->scope->parent)  return 1;
    return 1;
}

static inline int is_float_literal(Ast* expr) {
    if (!expr) return 0;
    if (expr->type == AST_LITERAL_FLOAT) return 1;
    if (expr->type == AST_UNARY_EXPRESSION && strcmp(expr->unary_expression.operator, "-") == 0)
        return is_float_literal(expr->unary_expression.argument);
    return 0;
}

static inline int is_integer_literal(Ast* expr) {
    if (!expr) return 0;
    if (expr->type == AST_LITERAL_INTEGER) return 1;
    if (expr->type == AST_UNARY_EXPRESSION && strcmp(expr->unary_expression.operator, "-") == 0)
        return is_integer_literal(expr->unary_expression.argument);
    return 0;
}

static inline int is_string_literal(Ast* expr) {
    if (!expr) return 0;
    if (expr->type == AST_LITERAL_STRING) return 1;
    return 0;
}

static inline int is_literal(Ast* expr) {
    if (!expr) return 0;
    switch (expr->type) {
        case AST_LITERAL_INTEGER:
        case AST_LITERAL_FLOAT:
        case AST_LITERAL_BOOLEAN:
        case AST_LITERAL_STRING:
        case AST_LITERAL_NIL:
            return 1;
        case AST_UNARY_EXPRESSION:
            return is_literal(expr->unary_expression.argument);
        case AST_BINARY_EXPRESSION:
            return is_literal(expr->binary_expression.left) && is_literal(expr->binary_expression.right);
        default:
            return 0;
    }
}

static inline int is_nil_literal(Ast* expr) {
    if (!expr) return 0;
    return expr->type == AST_LITERAL_NIL;
}

static inline int is_function_node(Ast* node) {
    return node != NULL && (node->type == AST_FUNCTION_DEFINITION || node->type == AST_FUNCTION_DECLARATION);
}

