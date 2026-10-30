#include "ast_explorer.h"

const char *ast_type_name(AstType type) {
    switch (type) {
        case AST_ROOT:                          return "ROOT";
        case AST_LITERAL_INTEGER:               return "LITERAL_INTEGER";
        case AST_LITERAL_FLOAT:                 return "LITERAL_FLOAT";
        case AST_LITERAL_STRING:                return "LITERAL_STRING";
        case AST_LITERAL_CHAR:                  return "LITERAL_CHAR";
        case AST_LITERAL_BOOLEAN:               return "LITERAL_BOOLEAN";
        case AST_LITERAL_NIL:                   return "LITERAL_NIL";
        case AST_LITERAL_ARRAY:                 return "LITERAL_ARRAY";
        case AST_LITERAL_COMPOUND:              return "LITERAL_COMPOUND";
        case AST_LITERAL_HEXADECIMAL:           return "LITERAL_HEXADECIMAL";
        case AST_LITERAL_BINARY:                return "LITERAL_BINARY";
        case AST_VARIABLE_DECLARATION:          return "VARIABLE_DECLARATION";
        case AST_FUNCTION_OVERLOAD_DECLARATION: return "FUNCTION_OVERLOADING";
        case AST_FUNCTION_DECLARATION:          return "FUNCTION_DECLARATION";
        case AST_FUNCTION_DEFINITION:           return "FUNCTION_DEFINITION";
        case AST_FUNCTION_ALIAS:                return "FUNCTION_ALIAS";
        case AST_PARAMETER_DECLARATION:         return "PARAMETER_DECLARATION";
        case AST_FIELD_DECLARATION:             return "FIELD_DECLARATION";
        case AST_STRUCT_DECLARATION:            return "STRUCT_DECLARATION";
        case AST_ASSIGNMENT_EXPRESSION:         return "ASSIGNMENT_EXPRESSION";
        case AST_INDEX_EXPRESSION:              return "INDEX_EXPRESSION";
        case AST_BINARY_EXPRESSION:             return "BINARY_EXPRESSION";
        case AST_UNARY_EXPRESSION:              return "UNARY_EXPRESSION";
        case AST_POSTFIX_EXPRESSION:            return "POSTFIX_EXPRESSION";
        case AST_SELECTOR_EXPRESSION:           return "SELECTOR_EXPRESSION";
        case AST_CALL_EXPRESSION:               return "CALL_EXPRESSION";
        case AST_CAST_EXPRESSION:               return "CAST_EXPRESSION";
        case AST_RANGE_EXPRESSION:              return "RANGE_EXPRESSION";
        case AST_IMPORT_STATEMENT:              return "IMPORT";
        case AST_USING_DECLARATION:             return "USING";
        case AST_IF_STATEMENT:                  return "IF";
        case AST_LOOP_STATEMENT:                return "LOOP";
        case AST_BLOCK_STATEMENT:               return "BLOCK";
        case AST_RETURN_STATEMENT:              return "RETURN";
        case AST_CONTINUE_STATEMENT:            return "CONTINUE";
        case AST_BREAK_STATEMENT:               return "BREAK";
        case AST_GOTO_STATEMENT:                return "GOTO";
        case AST_LABEL_STATEMENT:               return "LABEL";
        case AST_STAR_EXPRESSION:               return "STAR";
        case AST_ARRAY_TYPE:                    return "ARRAY_TYPE";
        case AST_SLICE_TYPE:                    return "SLICE_TYPE";
        case AST_FUNCTION_TYPE:                 return "FUNCTION_TYPE";
        case AST_POLYMORPHIC:                   return "POLYMORPHIC";
        case AST_IDENTIFIER:                    return "IDENTIFIER";
        default:                                return "UNKNOWN";
    }
}

static cJSON *ast_to_json(Ast *node);

static cJSON *ast_array_to_json(Ast **arr) {
    cJSON *jarr = cJSON_CreateArray();
    if (!arr) return jarr;
    size_t n = vector_size(arr);
    for (size_t i = 0; i < n; i++) {
        Ast *elem = ((Ast**)arr)[i];
        cJSON *je = ast_to_json(elem);
        cJSON_AddItemToArray(jarr, je);
    }
    return jarr;
}

static void add_source(cJSON *obj, Ast *node) {
    if (!obj || !node) return;
    cJSON *src = cJSON_CreateObject();
    cJSON_AddStringToObject(src, "path", node->source.path ? node->source.path : "");
    cJSON_AddNumberToObject(src, "row", (double)node->source.row);
    cJSON_AddNumberToObject(src, "col", (double)node->source.col);
    cJSON_AddItemToObject(obj, "source", src);
}

static void add_attributes(cJSON *obj, Ast *node) {
    if (!obj || !node || !node->attributes) return;
    size_t n = vector_size(node->attributes);
    if (n == 0) return;
    cJSON *arr = cJSON_CreateArray();
    for (size_t i = 0; i < n; i++) {
        const char *a = node->attributes[i];
        if (a) cJSON_AddItemToArray(arr, cJSON_CreateString(a));
    }
    cJSON_AddItemToObject(obj, "attributes", arr);
}

static cJSON *ast_to_json(Ast *node) {
    if (node == NULL) return cJSON_CreateNull();

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "type", ast_type_name(node->type));
    add_source(obj, node);
    add_attributes(obj, node);

    switch (node->type) {
        case AST_IDENTIFIER:
            cJSON_AddStringToObject(obj, "name", node->identifier.name ? node->identifier.name : "");
            break;
        case AST_POLYMORPHIC:
            cJSON_AddStringToObject(obj, "name", node->polymorphic.name ? node->polymorphic.name : "");
            break;
        case AST_LITERAL_INTEGER:
        case AST_LITERAL_HEXADECIMAL:
        case AST_LITERAL_BINARY:
            cJSON_AddNumberToObject(obj, "value", (double)node->literal_integer.value);
            break;
        case AST_LITERAL_FLOAT:
            cJSON_AddNumberToObject(obj, "value", node->literal_float.value);
            break;
        case AST_LITERAL_STRING:
            cJSON_AddStringToObject(obj, "value", node->literal_string.value ? node->literal_string.value : "");
            cJSON_AddNumberToObject(obj, "length", (double)node->literal_string.length);
            break;
        case AST_LITERAL_CHAR: {
            cJSON_AddNumberToObject(obj, "value", (double)(unsigned char)node->literal_char.value);
            char buf[2] = { node->literal_char.value, '\0' };
            cJSON_AddStringToObject(obj, "char", buf);
            break;
        }
        case AST_LITERAL_BOOLEAN:
            cJSON_AddBoolToObject(obj, "value", node->literal_boolean.value);
            break;
        case AST_LITERAL_NIL:
            break;
        case AST_LITERAL_ARRAY: {
            cJSON_AddItemToObject(obj, "elements", ast_array_to_json(node->literal_array.arguments));
            break;
        }
        case AST_LITERAL_COMPOUND: {
            if (node->literal_compound.type)
                cJSON_AddItemToObject(obj, "type", ast_to_json(node->literal_compound.type));
            else
                cJSON_AddNullToObject(obj, "type");
            cJSON_AddItemToObject(obj, "elements", ast_array_to_json(node->literal_compound.arguments));
            break;
        }
        case AST_ROOT:
        case AST_BLOCK_STATEMENT: {
            cJSON_AddItemToObject(obj, "statements", ast_array_to_json(node->block.statements));
            break;
        }
        case AST_BINARY_EXPRESSION: {
            cJSON_AddStringToObject(obj, "operator", node->binary_expression.operator ? node->binary_expression.operator : "");
            cJSON_AddItemToObject(obj, "left", ast_to_json(node->binary_expression.left));
            cJSON_AddItemToObject(obj, "right", ast_to_json(node->binary_expression.right));
            break;
        }
        case AST_ASSIGNMENT_EXPRESSION: {
            cJSON_AddStringToObject(obj, "operator", node->assignment_expression.operator ? node->assignment_expression.operator : "=");
            cJSON_AddItemToObject(obj, "left", ast_to_json(node->assignment_expression.left));
            cJSON_AddItemToObject(obj, "right", ast_to_json(node->assignment_expression.right));
            break;
        }
        case AST_INDEX_EXPRESSION: {
            cJSON_AddItemToObject(obj, "object", ast_to_json(node->index_expression.object));
            cJSON_AddItemToObject(obj, "index", ast_to_json(node->index_expression.index));
            break;
        }
        case AST_ARRAY_TYPE: {
            cJSON_AddItemToObject(obj, "length", ast_to_json(node->array_type.length));
            cJSON_AddItemToObject(obj, "elementType", ast_to_json(node->array_type.type));
            break;
        }
        case AST_SLICE_TYPE: {
            cJSON_AddItemToObject(obj, "elementType", ast_to_json(node->slice_type.type));
            break;
        }
        case AST_FUNCTION_TYPE: {
            cJSON_AddItemToObject(obj, "params", ast_array_to_json(node->function_type.params));
            if (node->function_type.return_type)
                cJSON_AddItemToObject(obj, "returnType", ast_to_json(node->function_type.return_type));
            else
                cJSON_AddNullToObject(obj, "returnType");
            break;
        }
        case AST_CALL_EXPRESSION: {
            cJSON_AddItemToObject(obj, "callee", ast_to_json(node->call_expression.callee));
            cJSON_AddItemToObject(obj, "arguments", ast_array_to_json(node->call_expression.arguments));
            break;
        }
        case AST_VARIABLE_DECLARATION: {
            cJSON_AddItemToObject(obj, "name", ast_to_json(node->variable_declaration.name));
            if (node->variable_declaration.type)
                cJSON_AddItemToObject(obj, "varType", ast_to_json(node->variable_declaration.type));
            else
                cJSON_AddNullToObject(obj, "varType");
            if (node->variable_declaration.initializer)
                cJSON_AddItemToObject(obj, "initializer", ast_to_json(node->variable_declaration.initializer));
            else
                cJSON_AddNullToObject(obj, "initializer");
            cJSON_AddBoolToObject(obj, "isConstant", node->variable_declaration.is_constant);
            break;
        }
        case AST_UNARY_EXPRESSION: {
            cJSON_AddStringToObject(obj, "operator", node->unary_expression.operator ? node->unary_expression.operator : "");
            cJSON_AddItemToObject(obj, "argument", ast_to_json(node->unary_expression.argument));
            break;
        }
        case AST_POSTFIX_EXPRESSION: {
            cJSON_AddStringToObject(obj, "operator", node->postfix_expression.operator ? node->postfix_expression.operator : "");
            cJSON_AddItemToObject(obj, "argument", ast_to_json(node->postfix_expression.argument));
            break;
        }
        case AST_CAST_EXPRESSION: {
            cJSON_AddItemToObject(obj, "targetType", ast_to_json(node->cast_expression.type));
            cJSON_AddItemToObject(obj, "argument", ast_to_json(node->cast_expression.argument));
            break;
        }
        case AST_IF_STATEMENT: {
            cJSON_AddItemToObject(obj, "test", ast_to_json(node->if_statement.test));
            cJSON_AddItemToObject(obj, "consequent", ast_to_json(node->if_statement.consequent));
            if (node->if_statement.alternate)
                cJSON_AddItemToObject(obj, "alternate", ast_to_json(node->if_statement.alternate));
            else
                cJSON_AddNullToObject(obj, "alternate");
            break;
        }
        case AST_PARAMETER_DECLARATION: {
            cJSON_AddItemToObject(obj, "name", ast_to_json(node->parameter_declaration.name));
            cJSON_AddItemToObject(obj, "paramType", ast_to_json(node->parameter_declaration.type));
            break;
        }
        case AST_FUNCTION_OVERLOAD_DECLARATION:
            cJSON_AddItemToObject(obj, "name", ast_to_json(node->function_overload_declaration.name));
            cJSON_AddItemToObject(obj, "body", ast_to_json(node->function_overload_declaration.body));
            break;
        case AST_FUNCTION_DECLARATION:
        case AST_FUNCTION_DEFINITION: {
            cJSON_AddItemToObject(obj, "name", ast_to_json(node->function_declaration.name));
            cJSON_AddItemToObject(obj, "params", ast_array_to_json(node->function_declaration.params));
            if (node->function_declaration.type)
                cJSON_AddItemToObject(obj, "returnType", ast_to_json(node->function_declaration.type));
            else
                cJSON_AddNullToObject(obj, "returnType");
            if (node->function_declaration.body)
                cJSON_AddItemToObject(obj, "body", ast_to_json(node->function_declaration.body));
            else
                cJSON_AddNullToObject(obj, "body");
            cJSON_AddBoolToObject(obj, "hasEllipsis", node->function_declaration.has_ellipsis);
            cJSON_AddBoolToObject(obj, "isDefined", node->function_declaration.is_defined);
            cJSON_AddBoolToObject(obj, "isCalled", node->function_declaration.is_called);
            break;
        }
        case AST_FUNCTION_ALIAS: {
            cJSON_AddItemToObject(obj, "name", ast_to_json(node->function_alias.name));
            cJSON_AddItemToObject(obj, "target", ast_to_json(node->function_alias.target));
            break;
        }
        case AST_RETURN_STATEMENT: {
            if (node->return_statement.value)
                cJSON_AddItemToObject(obj, "value", ast_to_json(node->return_statement.value));
            else
                cJSON_AddNullToObject(obj, "value");
            break;
        }
        case AST_CONTINUE_STATEMENT:
        case AST_BREAK_STATEMENT:
            break;
        case AST_GOTO_STATEMENT: {
            cJSON_AddItemToObject(obj, "label", ast_to_json(node->goto_statement.label));
            break;
        }
        case AST_LABEL_STATEMENT: {
            cJSON_AddItemToObject(obj, "name", ast_to_json(node->label_statement.name));
            break;
        }
        case AST_LOOP_STATEMENT: {
            if (node->loop_statement.iterator)
                cJSON_AddItemToObject(obj, "iterator", ast_to_json(node->loop_statement.iterator));
            else
                cJSON_AddNullToObject(obj, "iterator");
            if (node->loop_statement.sequence)
                cJSON_AddItemToObject(obj, "sequence", ast_to_json(node->loop_statement.sequence));
            else
                cJSON_AddNullToObject(obj, "sequence");
            cJSON_AddItemToObject(obj, "body", ast_to_json(node->loop_statement.body));
            break;
        }
        case AST_STAR_EXPRESSION: {
            cJSON_AddItemToObject(obj, "object", ast_to_json(node->star_expression.object));
            break;
        }
        case AST_SELECTOR_EXPRESSION: {
            cJSON_AddItemToObject(obj, "object", ast_to_json(node->selector_expression.object));
            cJSON_AddItemToObject(obj, "member", ast_to_json(node->selector_expression.member));
            cJSON_AddNumberToObject(obj, "fieldIndex", (double)node->selector_expression.field_index);
            break;
        }
        case AST_RANGE_EXPRESSION: {
            cJSON_AddItemToObject(obj, "start", ast_to_json(node->range_expression.start));
            cJSON_AddItemToObject(obj, "end", ast_to_json(node->range_expression.end));
            break;
        }
        case AST_STRUCT_DECLARATION: {
            cJSON_AddItemToObject(obj, "name", ast_to_json(node->struct_declaration.name));
            cJSON_AddItemToObject(obj, "fields", ast_array_to_json(node->struct_declaration.fields));
            cJSON_AddBoolToObject(obj, "isDefined", node->struct_declaration.is_defined);
            break;
        }
        case AST_IMPORT_STATEMENT: {
            if (node->import_statement.name)
                cJSON_AddItemToObject(obj, "name", ast_to_json(node->import_statement.name));
            else
                cJSON_AddNullToObject(obj, "name");
            cJSON_AddItemToObject(obj, "path", ast_to_json(node->import_statement.path));
            break;
        }
        case AST_USING_DECLARATION: {
            if (node->using_declaration.name)
                cJSON_AddItemToObject(obj, "name", ast_to_json(node->using_declaration.name));
            else
                cJSON_AddNullToObject(obj, "name");
            cJSON_AddItemToObject(obj, "aliasType", ast_to_json(node->using_declaration.target));
            break;
        }
        case AST_FIELD_DECLARATION: {
            cJSON_AddItemToObject(obj, "name", ast_to_json(node->field_declaration.name));
            cJSON_AddItemToObject(obj, "fieldType", ast_to_json(node->field_declaration.type));
            cJSON_AddNumberToObject(obj, "index", (double)node->field_declaration.index);
            break;
        }
        default:
            break;
    }
    return obj;
}

void ast_explorer_to_stream(Ast *root, FILE *out) {
    if (root == NULL || out == NULL) return;
    cJSON *json = ast_to_json(root);
    char *printed = cJSON_Print(json);
    if (printed) {
        fputs(printed, out);
        fputc('\n', out);
    }
    cJSON_Delete(json);
}

void ast_explorer(Ast *root) {
    ast_explorer_to_stream(root, stdout);
}

int ast_dump(Ast *root, CLI *cli) {
    if (cli->output) {
        FILE *f = fopen(cli->output, "w");
        if (!f) {
            fprintf(stderr, "Error: Could not open output file '%s'\n", cli->output);
            return 1;
        }
        ast_explorer_to_stream(root, f);
        fclose(f);
    } else {
        ast_explorer_to_stream(root, stdout);
    }
    return 0;
}
