#include "ast.h"

Ast** builtin_declarations = NULL;

Ast* Ast_create(AstType type) {
    Ast* res = (Ast*)c_alloc(sizeof(Ast));
    if (!res) return NULL;
    memset(res, 0, sizeof(*res));
    res->type = type;
    res->source.row = 0;
    res->source.col = 0;
    return res;
}

void Ast_build() {
    builtin_declarations = vector_create();
    {
        Ast* function_node = Ast_create(AST_FUNCTION_DEFINITION);
        Ast* function_name = Ast_create_identifier("sizeof");
        Ast* function_type = Ast_create_identifier(TYPE_USIZE_STRING);
        Ast* function_body = Ast_create(AST_BLOCK_STATEMENT);
        Ast** function_params = vector_create();

        vector_add(&function_params, Ast_create(AST_POLYMORPHIC));
        function_params[0]->polymorphic.name = "T";

        function_node->function_declaration.name   = function_name;
        function_node->function_declaration.type   = function_type;
        function_node->function_declaration.params = function_params;
        function_node->function_declaration.body   = function_body;
        function_node->function_declaration.is_defined = true;
        function_node->function_declaration.is_called  = false;
        function_node->function_declaration.has_ellipsis = false;

        vector_add(&builtin_declarations, function_node);
    }
}

int ast_merge(Ast* target, Ast* source) {
    Ast** stmts = source->block.statements;
    for (int i = 0; i < vector_size(stmts); i++) {
        vector_add(&target->block.statements, stmts[i]);
    }
    return 1;
}

const char* identifier_name(Ast* node) {
    if (node == NULL) return NULL;
    switch (node->type) {
        case AST_IDENTIFIER:
            return node->identifier.name;
        case AST_VARIABLE_DECLARATION:
            return identifier_name(node->variable_declaration.name);
        case AST_FUNCTION_DEFINITION:
        case AST_FUNCTION_DECLARATION:
        case AST_FUNCTION_ALIAS:
            return identifier_name(node->function_declaration.name);
        case AST_FUNCTION_OVERLOAD_DECLARATION:
            return identifier_name(node->function_overload_declaration.name);
        case AST_STRUCT_DECLARATION:
            return identifier_name(node->struct_declaration.name);
        case AST_ENUM_DECLARATION:
            return identifier_name(node->enum_declaration.name);
        case AST_ENUMERATOR:
            return identifier_name(node->enumerator.name);
        case AST_PARAMETER_DECLARATION:
            return identifier_name(node->parameter_declaration.name);
        case AST_USING_DECLARATION:
            return identifier_name(node->using_declaration.name);
        case AST_CALL_EXPRESSION:
            return identifier_name(node->call_expression.callee);
        case AST_LITERAL_COMPOUND:
            return identifier_name(node->literal_compound.type);
        case AST_UNARY_EXPRESSION:
            return identifier_name(node->unary_expression.argument);
        case AST_SELECTOR_EXPRESSION:
            return identifier_name(node->selector_expression.member);
        case AST_STAR_EXPRESSION:
            return identifier_name(node->star_expression.object);
        case AST_FIELD_DECLARATION:
            return identifier_name(node->field_declaration.name);
        case AST_LABEL_STATEMENT:
            return identifier_name(node->label_statement.name);
        case AST_GOTO_STATEMENT:
            return identifier_name(node->goto_statement.label);
        case AST_POLYMORPHIC:
            return node->identifier.name;
        case AST_RETURN_STATEMENT:
            if (node->return_statement.value) return identifier_name(node->return_statement.value);
            return "return";
        case AST_CONTINUE_STATEMENT:
            return "continue";
        case AST_BREAK_STATEMENT:
            return "break";

        default:
            return NULL;
    }
}
