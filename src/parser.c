#include "tokenizer.h"

typedef struct {
    TokenInfo* tokens;
    TokenInfo* attributes;
    size_t current;
    size_t size;
    bool newline;
} TokenParser;

typedef enum {
    EXPR_ALLOW_STRUCT_LITERAL,
    EXPR_DISALLOW_STRUCT_LITERAL
} ExprContext;

typedef struct {
    ExprContext stack[512];
    size_t depth;
} ExprContextStack;

static ExprContextStack context_stack = { .depth = 0 };

static void push_expr_context(ExprContext ctx) {
    if (context_stack.depth >= 512) exit(1);
    context_stack.stack[context_stack.depth++] = ctx;
}

static void pop_expr_context(void) {
    if (context_stack.depth > 0) {
        context_stack.depth--;
    }
}

static ExprContext current_expr_context(void) {
    if (context_stack.depth == 0) return EXPR_ALLOW_STRUCT_LITERAL;
    return context_stack.stack[context_stack.depth - 1];
}

TokenParser parser = {0};

static bool is_reserved_builtin_name(const char* name) {
    if (name == NULL) return false;
    return strcmp(name, "sizeof") == 0 || strcmp(name, "typeid") == 0;
}

int token_end(){
    return parser.current >= parser.size;
}

void token_skip(const size_t step){
    if (parser.current + step >= parser.size){
        parser.current = parser.size - 1;
        return;
    }
    parser.current += step;
}

static TokenInfo token_seek(const size_t pos){
    if (pos >= parser.size){
        TokenInfo empty = {0};
        return empty;
    }
    return parser.tokens[pos];
}

static TokenInfo token_back(){
    if (parser.current == 0) {
        TokenInfo empty = {0};
        return empty;
    }
    return token_seek(parser.current - 1);
}

static TokenInfo token_current() {
    if (parser.current >= parser.size) {
        if (parser.size == 0) {
            TokenInfo empty = {0};
            return empty;
        }
        return parser.tokens[parser.size - 1];
    }
    assert(parser.tokens[parser.current].type != TOKEN_NEWLINE);
    return parser.tokens[parser.current];
}

static TokenInfo token_front(){
    if (parser.current == 0) {
        TokenInfo empty = {0};
        return empty;
    }
    return token_seek(parser.current - 1);
}

static TokenInfo token_next(){
    if (parser.current >= parser.size) {
        return (TokenInfo){0};
    }
    parser.newline = false;
    while (1) {
        parser.current += 1;
        if (parser.current >= parser.size) {
            return (TokenInfo){0};
        }
        if (parser.tokens[parser.current].type == TOKEN_ATTRIBUTE) {
            vector_add(&parser.attributes, parser.tokens[parser.current]);
            continue;
        }
        if (parser.tokens[parser.current].type == TOKEN_NEWLINE) {
            parser.newline = true;
            continue;
        }
        return token_current();
    }
}

static TokenInfo token_advance() {
    TokenInfo res = token_current();
    token_next();
    return res;
}

static TokenInfo token_first() {
    parser.current = 0;
    while (1) {
        if (parser.current >= parser.size) return (TokenInfo){0};
        if (parser.tokens[parser.current].type == TOKEN_NEWLINE) {
            parser.current += 1;
            continue;
        }
        if (parser.tokens[parser.current].type == TOKEN_ATTRIBUTE) {
            vector_add(&parser.attributes, parser.tokens[parser.current]);
            parser.current += 1;
            continue;
        }
        return token_current();
    }
}

static void add_attributes(Ast* node) {
    if (!node->attributes) node->attributes = vector_create();
    size_t size = vector_size(parser.attributes);
    for (int i = 0; i < size; i++) {
        vector_add(&node->attributes, parser.attributes[i].data);
    }
    vector_clear(parser.attributes);
}

Ast* parser_import_statement();
Ast* parser_goto_statement();
Ast* parser_primary_expression();
Ast* parser_expression_statement();
Ast* parser_enum_declaration();
Ast* parser_struct_declaration();
Ast* parser_variable_declaration();
Ast* parser_function_declaration();
Ast* parser_infinity_loop_statement();
Ast* parser_range_loop_statement();
Ast* parser_if_statement();
Ast* parser_block_statement();
Ast* parser_return_statement();
Ast* parse_type();
Ast* parser_selector_expression(Ast* object);
Ast* parser_postfix_expression(Ast* left);
Ast** parser_struct_expression_statements();

// Compares a token's text against an operator or keyword. Literal tokens are
// never matched: their text is payload, not syntax, so a string literal like
// "." must not be mistaken for the selector operator.
int token_is(TokenInfo tok, char* ref){
    if (tok.data == NULL) return 0;
    switch (tok.type) {
        case TOKEN_STRING:
        case TOKEN_CHAR:
        case TOKEN_INTEGER:
        case TOKEN_FLOAT:
        case TOKEN_HEXADECIMAL:
        case TOKEN_BINARY:
        case TOKEN_ATTRIBUTE:
            return 0;
        default:
            break;
    }
    if (strcmp(tok.data, ref) == 0){
        return 1;
    }
    return 0;
}

static TokenInfo token_prev(){
    size_t i = parser.current;
    while (i > 0) {
        i--;
        if (parser.tokens[i].type != TOKEN_NEWLINE) return parser.tokens[i];
    }
    TokenInfo empty = {0};
    return empty;
}

static int token_can_end_expression(TokenInfo tok){
    if (tok.data == NULL) return 0;
    switch (tok.type) {
        case TOKEN_IDENTIFIER:
        case TOKEN_INTEGER:
        case TOKEN_FLOAT:
        case TOKEN_STRING:
        case TOKEN_CHAR:
        case TOKEN_BOOLEAN:
        case TOKEN_NIL:
            return 1;
        default:
            if (token_is(tok, ")") || token_is(tok, "]") || token_is(tok, "}"))
                return 1;
            return 0;
    }
}

static int newline_terminates_expression(void){
    if (!parser.newline) return 0;
    return token_can_end_expression(token_prev());
}

static int is_assignment_operator(TokenInfo tok){
    return token_is(tok, "=")  || token_is(tok, "+=") || token_is(tok, "-=") ||
           token_is(tok, "*=") || token_is(tok, "/=") || token_is(tok, "%=");
}

int get_token_precedence(TokenInfo parser) {
    if (parser.data == NULL) return 0;
    if (token_is(parser, "as")) {
        return 0;
    }
    if (token_is(parser, "or")) {
        return 1;
    }
    if (token_is(parser, "and")) {
        return 2;
    }
    if (token_is(parser, "==") || token_is(parser, "!=")) {
        return 3;
    }
    if (token_is(parser, "<")  || token_is(parser, ">") ||
        token_is(parser, "<=") || token_is(parser, ">=")) {
        return 4;
    }
    if (token_is(parser, "+")  || token_is(parser, "-")) {
        return 5;
    }
    if (token_is(parser, "*")  || token_is(parser, "/") || token_is(parser, "%")) {
        return 6;
    }
    return 0;
}

Ast* parser_binary_expression_precedence(int min_precedence) {
    Ast* res = parser_primary_expression();

    if (token_is(token_current(), ";")) return res;

    if (res == NULL) {
        if (token_current().data != NULL) {
            TokenInfo prev = token_prev();
            if (prev.data != NULL && get_token_precedence(prev) > 0) {
                log_source_error(prev.source, ERROR_SYNTAX, "Expected expression after operator");
            }
            else if (!token_is(token_current(), "}") &&
                     !token_is(token_current(), ")") &&
                     !token_is(token_current(), "]") &&
                     !token_is(token_current(), ";")) {
                log_source_error(token_current().source, ERROR_SYNTAX, "Expected an operand");
            }
        }
        return NULL;
    }

    while (1) {
        if (newline_terminates_expression()) {
            break;
        }
        TokenInfo op_token = token_current();
        if (token_is(op_token, "as")) {
            token_next();
            Ast* type = parse_type();
            Ast* cast_node = Ast_create(AST_CAST_EXPRESSION);
            cast_node->cast_expression.type     = type;
            cast_node->cast_expression.argument = res;
            cast_node->source = op_token.source;
            res = cast_node;
            continue;
        }

        int precedence = get_token_precedence(op_token);

        if (precedence < min_precedence || precedence == 0) {
            break;
        }

        char* op_str = token_current().data;
        token_next();

        Ast* right = parser_binary_expression_precedence(precedence + 1);
        if (right == NULL) {
            log_source_error(op_token.source, ERROR_SYNTAX, "Expected expression after operator");
            break;
        }

        Ast* binary_node = Ast_create(AST_BINARY_EXPRESSION);
        binary_node->binary_expression.left     = res;
        binary_node->binary_expression.operator = op_str;
        binary_node->binary_expression.right    = right;
        binary_node->source = op_token.source;

        res = binary_node;
    }

    return res;
}

Ast* parser_binary_expression() {
    return parser_binary_expression_precedence(0);
}

Ast* parser_array_literal(Ast* node) {
    token_next();
    Ast* array_literal = Ast_create(AST_LITERAL_ARRAY);
    Ast** arguments = vector_create();

    push_expr_context(EXPR_ALLOW_STRUCT_LITERAL);
    while (!token_is(token_current(), "]") && !token_end()) {
        Ast* arg = parser_binary_expression();
        if (arg != NULL) {
            vector_add(&arguments, arg);
        } else {
            token_next();
        }
        if (token_is(token_current(), ",")) token_next();
    }
    pop_expr_context();

    if (token_is(token_current(), "]")) {
        token_next();
    }

    array_literal->literal_array.arguments = arguments;
    return array_literal;
}

Ast* parser_call_expression(Ast* node) {
    token_next();

    Ast* call_node = Ast_create(AST_CALL_EXPRESSION);
    call_node->call_expression.callee = node;
    call_node->source = node->source;

    Ast** args = vector_create();

    push_expr_context(EXPR_ALLOW_STRUCT_LITERAL);
    while (!token_is(token_current(), ")") && !token_end()) {
        Ast* arg = parser_binary_expression();
        if (arg != NULL) {
            vector_add(&args, arg);
        } else {
            token_next();
        }
        if (token_is(token_current(), ",")) token_next();
    }
    pop_expr_context();

    token_next();
    call_node->call_expression.arguments = args;
    return call_node;
}

Ast* parser_compound_literal(Ast* object) {
    token_next();

    Ast* compound = Ast_create(AST_LITERAL_COMPOUND);
    compound->literal_compound.type = object;
    compound->source = object->source;

    Ast** args = vector_create();

    push_expr_context(EXPR_ALLOW_STRUCT_LITERAL);
    while (!token_is(token_current(), "}") && !token_end()) {
        Ast* arg = parser_binary_expression();
        if (arg != NULL) {
            vector_add(&args, arg);
        } else {
            token_next();
        }
        if (token_is(token_current(), ",")) token_next();
    }
    pop_expr_context();

    compound->literal_compound.arguments = args;

    if (token_is(token_current(), "}")) {
        token_next();
    }
    return compound;
}

Ast* parser_postfix_expression(Ast* node) {
    while (1) {

        if (newline_terminates_expression()) {
            break;
        }
        // Field Selector: (.member)
        if (token_is(token_current(), ".")) {
            node = parser_selector_expression(node);
        }
        // Compound / Struct Literal: ({ ... })
        else if (token_is(token_current(), "{")) {
            if (current_expr_context() == EXPR_DISALLOW_STRUCT_LITERAL) {
                break;
            }
            node = parser_compound_literal(node);
        }
        // Function Call: ( ... )
        else if (token_is(token_current(), "(")) {
            token_next();

            Ast* call_expression = Ast_create(AST_CALL_EXPRESSION);
            call_expression->call_expression.callee = node;
            call_expression->source = node->source;

            Ast** args = vector_create();

            push_expr_context(EXPR_ALLOW_STRUCT_LITERAL);
            while (!token_is(token_current(), ")") && !token_end()) {
                Ast* arg = parser_binary_expression();
                if (arg != NULL) {
                    vector_add(&args, arg);
                } else {
                    token_next();
                }
                if (token_is(token_current(), ",")) token_next();
            }
            pop_expr_context();

            call_expression->call_expression.arguments = args;

            if (token_is(token_current(), ")")) token_next();
            node = call_expression;
        }
        // Index / Slice Expression: [index]
        else if (token_is(token_current(), "[")) {
            SourceSite bracket_site = token_current().source;
            token_next();
            
            Ast* expr = Ast_create(AST_INDEX_EXPRESSION);
            expr->index_expression.object = node;
            expr->source = bracket_site;
            
            if (token_current().type == TOKEN_FLOAT) {
                log_source_error(token_current().source, ERROR_SYNTAX, "type 'f32' is not valid as an array subscript");
            } 
            else if (token_current().type == TOKEN_STRING) {
                log_source_error(token_current().source, ERROR_SYNTAX, "type '*u8' is not valid as an array subscript");
            } 
            else if (token_current().type == TOKEN_BOOLEAN) {
                log_source_error(token_current().source, ERROR_SYNTAX, "type 'bool' is not valid as an array subscript");
            }

            else {
                push_expr_context(EXPR_ALLOW_STRUCT_LITERAL);
                Ast* idx = parser_binary_expression();
                if (token_current().type == TOKEN_RANGE) {
                    token_next();
                    Ast* end = parser_binary_expression();
                    Ast* range = Ast_create(AST_RANGE_EXPRESSION);
                    range->range_expression.start = idx;
                    range->range_expression.end   = end;
                    range->source = idx->source;
                    idx = range;
                }
                expr->index_expression.index = idx;
                pop_expr_context();
            }

            if (token_is(token_current(), "]")) {
                token_next();
            }
            else {
                log_source_error(token_current().source, ERROR_SYNTAX, "Expected ']'");
            }
            node = expr;
        }
        // Postfix Increment / Decrement: (++, --)
        else if (token_is(token_current(), "++") || token_is(token_current(), "--")) {
            SourceSite op_site = token_current().source;
            Ast* postfix = Ast_create(AST_POSTFIX_EXPRESSION);
            postfix->postfix_expression.operator = token_current().data;
            postfix->postfix_expression.argument = node;
            postfix->source = op_site;
            token_next();
            node = postfix;
        }
        else {
            break;
        }
    }
    return node;
}

Ast* parser_import_statement() {
    SourceSite import_site = token_current().source;
    token_next();

    Ast* import_path = NULL;
    if (token_current().type == TOKEN_STRING) {
        import_path = Ast_create(AST_LITERAL_STRING);
        import_path->literal_string.value = token_current().data;
        import_path->source = token_current().source;
        token_next();
    }
    else {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected path");
    }

    Ast* import_node = Ast_create(AST_IMPORT_STATEMENT);
    import_node->source = import_site;
    if (import_path) import_node->source = import_path->source;
    import_node->import_statement.path = import_path;
    return import_node;
}

Ast* parser_goto_statement() {
    SourceSite goto_site = token_current().source;
    token_next();
    
    Ast* goto_node = Ast_create(AST_GOTO_STATEMENT);
    goto_node->source = goto_site;
    goto_node->goto_statement.label = Ast_create_identifier(token_current().data);
    token_next();
    if (token_is(token_current(), ";")) {
       token_next();
    }
    return goto_node;
}

Ast* pareser_label_statement() {
    SourceSite label_site = token_current().source;

    Ast* label_node = Ast_create(AST_LABEL_STATEMENT);
    label_node->source = label_site;
    label_node->label_statement.name = Ast_create_identifier(token_current().data);
    token_next();
    if (token_is(token_current(), ":")) {
        token_next();
    }
    return label_node;
}

// Types that can only be spelled as a type expression, i.e. never as a plain
// function reference. `*Foo` is a pointer type, `[]u8` a slice, `[N]u8` an
// array and `(i32) -> i32` a function type.
static bool using_target_is_type_syntax(TokenInfo tok) {
    return token_is(tok, "*") || token_is(tok, "[") || token_is(tok, "(");
}

Ast* parser_using_declaration() {
    SourceSite using_site = token_current().source;
    token_next();

    Ast* alias_name = NULL;
    if (token_current().type == TOKEN_IDENTIFIER) {
        alias_name = Ast_create(AST_IDENTIFIER);
        alias_name->identifier.name = token_current().data;
        alias_name->source = token_current().source;
        token_next();
    }
    else {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected identifier after 'using'");
        return NULL;
    }

    int has_equals = token_is(token_current(), "=");
    int is_type_syntax = has_equals || using_target_is_type_syntax(token_current());

    Ast* using_node = Ast_create(AST_USING_DECLARATION);
    using_node->source = using_site;
    add_attributes(using_node);

    Ast* assigned_type = NULL;
    if (has_equals) token_next();

    if (is_type_syntax) {
        assigned_type = parse_type();
        if (assigned_type == NULL) return NULL;
    }
    else {
        assigned_type = parser_binary_expression();
        if (assigned_type == NULL) {
            log_source_error(token_current().source, ERROR_SYNTAX, "Expected type or function name after 'using'");
            return NULL;
        }
    }

    if (token_is(token_current(), ";")) {
        token_next();
    }
    else if (!parser.newline && !token_is(token_current(), "}")) {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected ';' after using declaration");
    }

    if (is_reserved_builtin_name(identifier_name(alias_name))) {
        char str[256];
        snprintf(str, sizeof(str), "cannot declare alias '%s': name is reserved for a builtin", identifier_name(alias_name));
        log_source_error(alias_name->source, ERROR_UNDEFINED, str);
    }

    using_node->using_declaration.name      = alias_name;
    using_node->using_declaration.target    = assigned_type;
    using_node->using_declaration.ambiguous = !is_type_syntax;
    return using_node;
}

char unescape_char(const char* src) {
    if (!src) return '\0';
    size_t len = strlen(src);
    if (len < 1) return '\0';

    char c = src[0];
    if (c == '\\' && len >= 2) {
        switch (src[1]) {
            case 'n': return '\n';
            case 't': return '\t';
            case 'r': return '\r';
            case '0': return '\0';
            case '\\': return '\\';
            case '\'': return '\'';
            case '"': return '"';
            case 'a': return '\a';
            case 'b': return '\b';
            case 'f': return '\f';
            case 'v': return '\v';
            default:   return src[1];
        }
    }
    return c;
}

Ast* parser_primary_expression() {
    TokenInfo token = token_current();
    if (token.data == NULL) return NULL;

    Ast* res = NULL;

    if (token.type == TOKEN_IDENTIFIER) {
        res = Ast_create(AST_IDENTIFIER);
        res->source = token.source;
        res->identifier.name = token_current().data;
        token_next();
    }
    else if (token.type == TOKEN_INTEGER || token.type == TOKEN_FLOAT) {
        res = (token.type == TOKEN_INTEGER)
            ? Ast_create(AST_LITERAL_INTEGER)
            : Ast_create(AST_LITERAL_FLOAT);
        res->source = token.source;
        if (token.type == TOKEN_INTEGER) {
            res->literal_integer.value = atol(token_current().data);
        } else {
            res->literal_float.value = atof(token_current().data);
        }
        token_next();
    }
    else if (token.type == TOKEN_STRING) {
        res = Ast_create(AST_LITERAL_STRING);
        res->source = token.source;
        res->literal_string.value = token_current().data;
        res->literal_string.length       = strlen(token_current().data);
        token_next();
    }
    else if (token.type == TOKEN_CHAR) {
        res = Ast_create(AST_LITERAL_CHAR);
        res->source = token.source;
        res->literal_char.value = unescape_char(token_current().data);
        token_next();
    }
    else if (token.type == TOKEN_BOOLEAN) {
        res = Ast_create(AST_LITERAL_BOOLEAN);
        res->source = token.source;
        res->literal_boolean.value = token_is(token, "true") ? 1 : 0;
        token_next();
    }
    else if (token.type == TOKEN_HEXADECIMAL) {
        res = Ast_create(AST_LITERAL_HEXADECIMAL);
        res->source = token.source;
        res->literal_hexadecimal.value = token.data;
        token_next();
    }
    else if (token.type == TOKEN_NIL) {
        res = Ast_create(AST_LITERAL_NIL);
        res->source = token.source;
        token_next();
    }
    else if (token.type == TOKEN_BINARY) {
        res = Ast_create(AST_LITERAL_BINARY);
        res->source = token.source;
        res->literal_binary.value = token.data;
        token_next();
    }
    else if (token_is(token, "-") || token_is(token, "!") || token_is(token, "&")) {
        res = Ast_create(AST_UNARY_EXPRESSION);
        res->source = token.source;
        res->unary_expression.operator = token_current().data;
        token_next();
        res->unary_expression.argument = parser_primary_expression();
    }
    else if (token_is(token, "*")) {
        res = Ast_create(AST_STAR_EXPRESSION);
        res->source = token.source;
        token_next();
        res->star_expression.object = parser_primary_expression();
    }
    else if (token_is(token, "[")) {
        res = parser_array_literal(NULL);
    }
    else if (token_is(token, "(")) {
        token_next();
        push_expr_context(EXPR_ALLOW_STRUCT_LITERAL);
        res = parser_binary_expression();
        pop_expr_context();
        if (token_is(token_current(), ")")) {
            token_next();
            if (!parser.newline && token_is(token_current(), "(")) {
                Ast* pre = res;
                res = parser_call_expression(pre);
            }
        }
    }

    if (res != NULL) {
        res = parser_postfix_expression(res);
    }

    return res;
}

static bool is_expression_start(TokenInfo parser)
{
    return parser.type == TOKEN_IDENTIFIER ||
           parser.type == TOKEN_INTEGER ||
           parser.type == TOKEN_FLOAT ||
           parser.type == TOKEN_STRING ||
           parser.type == TOKEN_CHAR ||
           parser.type == TOKEN_BOOLEAN ||
           parser.type == TOKEN_HEXADECIMAL ||
           parser.type == TOKEN_BINARY ||
           parser.type == TOKEN_NIL ||
           token_is(parser, "(") ||
           token_is(parser, "-") ||
           token_is(parser, "!") ||
           token_is(parser, "*") ||
           token_is(parser, "&") ||
           token_is(parser, "[");
}

static Ast* parser_assignment_expression(Ast* left) {
    SourceSite op_site = token_current().source;
    char* op = token_current().data;
    token_next();

    Ast* assignment_node = Ast_create(AST_ASSIGNMENT_EXPRESSION);
    assignment_node->assignment_expression.left = left;
    assignment_node->assignment_expression.operator = op;
    assignment_node->assignment_expression.right = parser_binary_expression();
    assignment_node->source = op_site;

    if (token_is(token_current(), ";")) {
        token_next();
    }
    else if (!parser.newline && !token_is(token_current(), "}")) {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected ';'");
    }
    return assignment_node;
}

Ast* parser_expression_statement() {
    Ast* lhs = parser_binary_expression();
    if (lhs == NULL) return NULL;

    if (!newline_terminates_expression() && is_assignment_operator(token_current())) {
        return parser_assignment_expression(lhs);
    }
    if (token_is(token_current(), ";")) {
        token_next();
    }
    else if (!parser.newline && !token_is(token_current(), "}")) {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected ';'");
    }

    return lhs;
}

Ast* parser_break_statement() {
    Ast* break_node = Ast_create(AST_BREAK_STATEMENT);
    token_next();
    if (token_is(token_current(), ";")) {
        token_next();
    }
    else if (!parser.newline && !token_is(token_current(), "}")) {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected ';'");
    }
    return break_node;
}

Ast* parser_continue_statement() {
    Ast* continue_node = Ast_create(AST_CONTINUE_STATEMENT);
    token_next();
    if (token_is(token_current(), ";")) {
        token_next();
    }
    else if (!parser.newline && !token_is(token_current(), "}")) {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected ';'");
    }
    return continue_node;
}

Ast* parser_statement() {
    TokenInfo token = token_current();
    if (token.data == NULL) return NULL;

    else if (token.type == TOKEN_IMPORT) {
        return parser_import_statement();
    }
    else if (token.type == TOKEN_USING) {
        return parser_using_declaration();
    }
    else if (token.type == TOKEN_DEF) {
        return parser_function_declaration();
    }
    else if (token.type == TOKEN_VAR || token.type == TOKEN_LET) {
        return parser_variable_declaration();
    }
    else if (token.type == TOKEN_LOOP) {
        return parser_infinity_loop_statement();
    }
    else if (token.type == TOKEN_FOR) {
        return parser_range_loop_statement();
    }
    else if (token.type == TOKEN_IF) {
        return parser_if_statement();
    }
    else if (token.type == TOKEN_STRUCT) {
        return parser_struct_declaration();
    }
    else if (token.type == TOKEN_ENUM) {
        return parser_enum_declaration();
    }
    else if (token.type == TOKEN_RETURN) {
        return parser_return_statement();
    }
    else if (token.type == TOKEN_GOTO) {
        return parser_goto_statement();
    }
    else if (token.type == TOKEN_BREAK) {
        return parser_break_statement();
    }
    else if (token.type == TOKEN_CONTINUE) {
        return parser_continue_statement();
    }
    else if (token.type == TOKEN_IDENTIFIER) {
        TokenInfo next = token_seek(parser.current + 1);
        if (token_is(next, ":")) {
            return pareser_label_statement();
        }
        return parser_expression_statement();
    }
    return parser_expression_statement();
}

Ast* parse_type() {

    // Function type: (T, T) -> T
    if (token_is(token_current(), "(")) {
        SourceSite fn_site = token_current().source;
        token_next();

        Ast** fn_params = vector_create();
        while (!token_is(token_current(), ")") && !token_end()) {
            Ast* param_type = parse_type();
            if (param_type != NULL) {
                vector_add(&fn_params, param_type);
            }
            if (token_is(token_current(), ",")) {
                token_next();
            } else if (!token_is(token_current(), ")")) {
                log_source_error(token_current().source, ERROR_SYNTAX, "Expected ',' or ')' in function type parameter list");
                break;
            }
        }

        if (token_is(token_current(), ")")) {
            token_next();
        }

        Ast* fn_return = NULL;
        if (token_current().type != TOKEN_ARROW) {
            log_source_error(token_current().source, ERROR_SYNTAX, "Expected '->' after function type parameter list");
            return NULL;
        }
        token_next();
        fn_return = parse_type();

        Ast* fn_type = Ast_create(AST_FUNCTION_TYPE);
        fn_type->source = fn_site;
        fn_type->function_type.params = fn_params;
        fn_type->function_type.return_type = fn_return;
        return fn_type;
    }

    // Type: (T)
    if (token_current().type == TOKEN_IDENTIFIER) {
        Ast* type_node = Ast_create(AST_IDENTIFIER);
        type_node->identifier.name = token_current().data;
        type_node->source = token_current().source;
        token_next();
        
        while (token_is(token_current(), ".")) {
            token_next();
            if (token_current().type == TOKEN_IDENTIFIER) {
                Ast* member = Ast_create(AST_IDENTIFIER);
                member->identifier.name = token_current().data;
                member->source = token_current().source;
                Ast* selector = Ast_create(AST_SELECTOR_EXPRESSION);
                selector->selector_expression.object = type_node;
                selector->selector_expression.member = member;
                selector->selector_expression.field_index = 0;
                selector->source = member->source;
                type_node = selector;
                token_next();
            } else {
                log_source_error(token_current().source, ERROR_SYNTAX, "Expected identifier after '.'");
                break;
            }
        }
        return type_node;
    }

    {
        // Pointer: (*T)
        if (token_is(token_current(), "*")) {
            SourceSite star_site = token_current().source;
            token_next();
            Ast* ptr_node = Ast_create(AST_STAR_EXPRESSION);
            ptr_node->source = star_site;
            ptr_node->star_expression.object = parse_type();
            return ptr_node;
        }

        // Array: ([]T, [N]T, [*]T)
        if (token_is(token_current(), "[")) {
            SourceSite bracket_site = token_current().source;
            token_next();
            
            if (token_is(token_current(), "*")) {
                token_next();
                if (!token_is(token_current(), "]")) {
                    log_source_error(token_current().source, ERROR_SYNTAX, "Expected ']' after '*'");
                    return NULL;
                }
                token_next();
                Ast* elem_type = parse_type();
                Ast* ptr_node = Ast_create(AST_STAR_EXPRESSION);
                ptr_node->source = bracket_site;
                ptr_node->star_expression.object = elem_type;
                return ptr_node;
            }

            Ast* size_expr = NULL;

            if (!token_is(token_current(), "]")) {
                size_expr = parser_binary_expression();
            }

            if (!token_is(token_current(), "]")) {
                log_source_error(token_current().source, ERROR_SYNTAX, "Expected ']' after array size or slice definition");
                return NULL;
            }
            token_next();

            Ast* elem_type = parse_type();

            if (size_expr != NULL) {
                size_t length = 0;
                if (size_expr->type == AST_LITERAL_INTEGER) {
                    length = (size_t)size_expr->literal_integer.value;
                } else {
                    log_source_error(size_expr->source, ERROR_SYNTAX, "Array size must be a constant integer");
                }
                Ast* array_node = Ast_create(AST_ARRAY_TYPE);
                array_node->source = bracket_site;
                array_node->array_type.length       = length;
                array_node->array_type.type = elem_type;
                return array_node;
            } else {
                Ast* slice_node = Ast_create(AST_SLICE_TYPE);
                slice_node->source = bracket_site;
                slice_node->slice_type.type = elem_type;
                return slice_node;
            }
        }
    }

    log_source_error(token_current().source, ERROR_SYNTAX, "Expected type definition");
    return NULL;
}

Ast* parser_block_statement(){ 
    Ast** statements = vector_create();
    token_next();
    while (!token_is(token_current(), "}") && !token_end()) {
        Ast* stmt = parser_statement();
        if (stmt != NULL) {
            vector_add(&statements, stmt);
        } else {
            token_next();
        }
    }
    if (token_is(token_current(), "}")) {
        token_next();
    }
    Ast* block = Ast_create(AST_BLOCK_STATEMENT);
    block->block.statements = statements;
    return block;
}

Ast* parser_field() {
    if (token_current().type != TOKEN_IDENTIFIER) {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected field name");
        return NULL;
    }

    Ast* field_name = Ast_create(AST_IDENTIFIER);
    field_name->identifier.name = token_current().data;
    SourceSite field_site = token_current().source;
    field_name->source = field_site;
    token_next();

    if (!token_is(token_current(), ":")) {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected ':' after field name");
        return NULL;
    }
    token_next();
    
    Ast* field_type = parse_type();

    Ast* field_node = Ast_create(AST_FIELD_DECLARATION);
    field_node->source = field_site;
    field_node->field_declaration.name = field_name;
    field_node->field_declaration.type = field_type;
    
    if (token_is(token_current(), ";")) {
        token_next();
    }
    else if (!parser.newline) {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected ';'");
    }

    return field_node;
}

Ast** parser_struct_fields(){ 
    token_next();
    Ast** statements = vector_create();
    while (!token_is(token_current(), "}") && !token_end()) {
        Ast* stmt = parser_field();
        if (stmt == NULL) {
            token_next();
        }
        else if (stmt->type == AST_ASSIGNMENT_EXPRESSION) {
            log_source_error(stmt->source, ERROR_SYNTAX, "Assignment is not allowed in a struct body");
            token_next();
        }
        else if (stmt->type == AST_CALL_EXPRESSION) {
            log_source_error(stmt->source, ERROR_SYNTAX, "Function call is not allowed in a struct body");
            token_next();
        }
        else if (stmt->type == AST_STRUCT_DECLARATION) {
            log_source_error(stmt->source, ERROR_SYNTAX, "struct is not allowed in a struct body");
            token_next();
        }
        else {
            vector_add(&statements, stmt);
        }
    }
    if (token_is(token_current(), "}")) {
        token_next();
    }
    return statements;
}


Ast* parser_selector_expression(Ast* object) {
    if (!token_is(token_current(), ".")) {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected '.' in selector expression");
        return NULL;
    }

    token_next();

    if (token_current().type != TOKEN_IDENTIFIER) {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected field name after '.'");
        return NULL;
    }

    Ast* selector = Ast_create(AST_SELECTOR_EXPRESSION);

    Ast* member = Ast_create(AST_IDENTIFIER);
    member->identifier.name = token_current().data;
    member->source = token_current().source;

    selector->source = member->source;
    selector->selector_expression.object = object;
    selector->selector_expression.member = member;

    token_next();

    return selector;
}

Ast* parser_range_expression() {
    Ast* start = parser_binary_expression();
    if (!start) {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected start expression for range");
    }

    if (token_current().type == TOKEN_RANGE) {
        token_next();
    }
    else {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected '..'");
    }

    Ast* end  = parser_binary_expression();
    if (!end) {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected expression after '..'");
    }

    Ast* range = Ast_create(AST_RANGE_EXPRESSION);
    range->range_expression.start = start;
    range->range_expression.end   = end;
    range->source = start->source;
    return range;
}

Ast* parser_infinity_loop_statement() {
    SourceSite loop_site = token_current().source;
    token_next();

    if (!token_is(token_current(), "{")) {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected '{' after 'loop'");
        return NULL;
    }

    Ast* loop = Ast_create(AST_LOOP_STATEMENT);
    loop->loop_statement.iterator = NULL;
    loop->loop_statement.sequence = NULL;
    loop->loop_statement.body     = parser_block_statement();
    loop->source = loop_site;

    return loop;
}

Ast* parser_range_loop_statement() {
    SourceSite loop_site = token_current().source;
    token_next();
    Ast* loop_iterator = NULL;

    if (token_current().type == TOKEN_IDENTIFIER) {
        Ast* identifier = Ast_create(AST_IDENTIFIER);
        identifier->identifier.name = token_current().data;
        
        if (is_reserved_builtin_name(identifier_name(identifier))) {
            char str[256];
            snprintf(str, sizeof(str), "cannot declare variable '%s': name is reserved for a builtin", identifier_name(identifier));
            log_source_error(identifier->source, ERROR_UNDEFINED, str); 
        }
        
        token_next();

        if (token_current().type != TOKEN_IN) {
            log_source_error(token_current().source, ERROR_SYNTAX, "Expected 'in' after identifier");
            return NULL;
        }

        token_next();

        loop_iterator = Ast_create(AST_FIELD_DECLARATION);
        loop_iterator->field_declaration.name = identifier;
        loop_iterator->field_declaration.type = NULL;
    }

    push_expr_context(EXPR_DISALLOW_STRUCT_LITERAL);
    Ast* loop_sequence = parser_range_expression();
    pop_expr_context();
    if (!loop_sequence) {
        return NULL;
    }

    if (!token_is(token_current(), "{")) {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected '{' to start loop body");
        return NULL;
    }

    Ast* loop_body = parser_block_statement();

    Ast* loop = Ast_create(AST_LOOP_STATEMENT);
    loop->loop_statement.iterator = loop_iterator;
    loop->loop_statement.sequence = loop_sequence;
    loop->loop_statement.body     = loop_body;
    loop->source = loop_site;

    return loop;
}

Ast* parser_if_statement() {
    token_next();

    Ast* if_node = Ast_create(AST_IF_STATEMENT);
    if_node->if_statement.test       = NULL;
    if_node->if_statement.consequent = NULL;
    if_node->if_statement.alternate  = NULL;

    push_expr_context(EXPR_DISALLOW_STRUCT_LITERAL);
    Ast* test = parser_binary_expression();
    pop_expr_context();
    if (!test) {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected condition after 'if'");
        return NULL;
    }

    Ast* consequent = NULL;
    if (token_is(token_current(), "{")) {
        consequent = parser_block_statement();
    } else {
        Ast* stmt = parser_statement();
        if (!stmt) {
            log_source_error(token_current().source, ERROR_SYNTAX, "Expected '{' or statement after 'if' condition");
            return NULL;
        }
        consequent = Ast_create(AST_BLOCK_STATEMENT);
        consequent->block.statements = vector_create();
        vector_add(&consequent->block.statements, stmt);
    }

    Ast* alternate = NULL;
    if (token_current().type == TOKEN_ELSE) {
        token_next();

        if (token_current().type == TOKEN_IF) {
            alternate = parser_if_statement();
        } else if (token_is(token_current(), "{")) {
            alternate = parser_block_statement();
        } else {
            Ast* stmt = parser_statement();
            if (!stmt) {
                log_source_error(token_current().source, ERROR_SYNTAX, "Expected '{', 'if', or statement after 'else'");
                return NULL;
            }
            alternate = Ast_create(AST_BLOCK_STATEMENT);
            alternate->block.statements = vector_create();
            vector_add(&alternate->block.statements, stmt);
        }
    }

    if_node->if_statement.test       = test;
    if_node->if_statement.consequent = consequent;
    if_node->if_statement.alternate  = alternate;

    return if_node;
}

Ast* parser_enum_declaration() {
    SourceSite enum_site = token_current().source;
    token_next();

    Ast* enum_name = NULL;
    Ast* enum_type = NULL;

    if (token_current().type != TOKEN_IDENTIFIER) {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected enum name");
        return NULL;
    }
    enum_name = Ast_create_identifier(token_current().data);
    enum_name->source = token_current().source;
    token_next();
    
    if (token_is(token_current(), ":")) {
        token_next();
        if (token_current().type != TOKEN_IDENTIFIER) {
            log_source_error(token_current().source, ERROR_SYNTAX, "Expected enum type");
            return NULL;
        }
        enum_type = parse_type();
    }

if (!token_is(token_current(), "{")) {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected '{'");
        return NULL;
    }
    token_next();

    Ast** enum_members = vector_create();

    while (!token_is(token_current(), "}") && !token_end()) {
        if (token_current().type != TOKEN_IDENTIFIER) {
            log_source_error(token_current().source, ERROR_SYNTAX, "Expected member identifier in enum");
            break;
        }

        Ast* enumerator = Ast_create(AST_ENUMERATOR);
        enumerator->source = token_current().source;
        enumerator->enumerator.name = Ast_create_identifier(token_current().data);
        enumerator->enumerator.name->source = token_current().source;
        enumerator->enumerator.value = NULL;
        token_next();
        if (token_is(token_current(), "=")) {
            token_next();
            if (token_current().type != TOKEN_INTEGER) {
                log_source_error(token_current().source, ERROR_SYNTAX, "Expected integer after '='");
            } else {
                Ast* value = Ast_create(AST_LITERAL_INTEGER);
                value->source = token_current().source;
                value->literal_integer.value = atol(token_current().data);
                enumerator->enumerator.value = value;
                token_next();
            }
        }

        vector_add(&enum_members, enumerator);
        if (token_is(token_current(), ";")) {
            token_next();
        } else if (!token_is(token_current(), "}")) {
            log_source_error(token_current().source, ERROR_SYNTAX, "Expected ';' or '}'");
            break;
        }
    }

    if (token_is(token_current(), "}")) {
        token_next();
    } else {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected '}'");
    }

    Ast* enum_node = Ast_create(AST_ENUM_DECLARATION);
    enum_node->source = enum_site;
    enum_node->enum_declaration.name = enum_name;
    enum_node->enum_declaration.type = enum_type;
    enum_node->enum_declaration.members = enum_members;

    return enum_node;
}

Ast* parser_struct_declaration() {
    SourceSite struct_site = token_current().source;
    Ast* struct_node = Ast_create(AST_STRUCT_DECLARATION);
    struct_node->source = struct_site;
    add_attributes(struct_node);
    token_next();
    TokenInfo tok_struct_name = token_current();

    Ast* struct_name = Ast_create(AST_IDENTIFIER);
    struct_name->source = tok_struct_name.source;
    if (tok_struct_name.type == TOKEN_IDENTIFIER) {
        struct_name->identifier.name = tok_struct_name.data;
    }
    else {
        log_source_error(struct_name->source, ERROR_SYNTAX, "Invalid struct identifier");
    }
    
    token_next();
    Ast** fields = vector_create();
    if (token_is(token_current(), "{")) {
        Ast** parsed_fields = parser_struct_fields();
        fields = parsed_fields;
    } 
    else if (token_is(token_current(), ";")) {
        token_next();
    }
    else if (!parser.newline && !token_end() && !token_is(token_current(), "}")) {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected '{'");
    }

    if (is_reserved_builtin_name(identifier_name(struct_name))) {
        char str[256];
        snprintf(str, sizeof(str), "cannot declare struct '%s': name is reserved for a builtin", identifier_name(struct_name));
        log_source_error(struct_name->source, ERROR_UNDEFINED, str); 
    }

    struct_node->struct_declaration.name   = struct_name;
    struct_node->struct_declaration.fields = fields;

    return struct_node;
}

Ast* parser_variable_declaration() {
    bool constant = token_current().type == TOKEN_LET;
    Ast* variable_node = Ast_create(AST_VARIABLE_DECLARATION);
    add_attributes(variable_node);
    token_next();

    Ast* variable_name = Ast_create(AST_IDENTIFIER);

    if (token_current().type == TOKEN_IDENTIFIER) {
        variable_name->identifier.name = token_current().data;
        variable_name->source = token_current().source;
    }
    else {
        log_source_error(token_current().source, ERROR_SYNTAX, "Invalid variable identifier");
    }

    token_next();
    if (!token_is(token_current(), ":") && !token_is(token_current(), "=")) {
        log_source_error(token_current().source, ERROR_SYNTAX, "Invalid variable identifier");
    }
    
    Ast* variable_type = NULL;
    if (token_is(token_current(), ":")) {
        token_next();
        variable_type = parse_type();
    }
    else
    {
        variable_type = NULL;
    }

    Ast* variable_initializer = NULL;
    if (token_is(token_current(), "=")) {
        if (is_expression_start(token_next()))
        {
            variable_initializer = parser_binary_expression();
            if (token_current().type == TOKEN_RANGE) {
                token_next();
                Ast* end = parser_binary_expression();
                Ast* range = Ast_create(AST_RANGE_EXPRESSION);
                range->range_expression.start = variable_initializer;
                range->range_expression.end   = end;
                variable_initializer = range;
            }
        }
        else {
            log_source_error(token_current().source, ERROR_SYNTAX, "Invalid variable identifier");
        }
    }
    else if (token_is(token_current(), ";")) {
        variable_initializer = NULL;
    }

    if (is_reserved_builtin_name(identifier_name(variable_name))) {
        char str[256];
        snprintf(str, sizeof(str), "cannot declare variable '%s': name is reserved for a builtin", identifier_name(variable_name));
        log_source_error(variable_name->source, ERROR_UNDEFINED, str); 
    }

    variable_node->source = variable_name->source;
    variable_node->variable_declaration.is_constant = constant;
    variable_node->variable_declaration.type        = variable_type;
    variable_node->variable_declaration.name        = variable_name;
    variable_node->variable_declaration.initializer = variable_initializer;

    return variable_node;
}

static Ast* parser_function_overload_declaration(Ast* node) {
    Ast* overload  = node;
    overload->type = AST_FUNCTION_OVERLOAD_DECLARATION; 

    Ast** names = vector_create();

    while (!token_is(token_current(), "}")) {
        if (token_current().type != TOKEN_IDENTIFIER) {
            log_source_error(
                token_current().source,
                ERROR_SYNTAX,
                "Expected function name"
            );
            return NULL;
        }

        Ast* identifier = Ast_create(AST_IDENTIFIER);
        identifier->source = token_current().source;
        identifier->identifier.name = token_current().data;

        vector_add(&names, identifier);

        token_next();
        if (token_is(token_current(), ";")) {
            token_next();
        }
        else if (!parser.newline) {
            log_source_error(token_current().source, ERROR_SYNTAX, "Expected ';'");
        }
    }

    token_next();

    Ast* body = Ast_create(AST_BLOCK_STATEMENT);
    body->block.statements = names;

    overload->function_overload_declaration.body = body;

    return overload;
}

Ast* parser_function_declaration() {
    SourceSite keyword_site = token_current().source;

    Ast* function = Ast_create(AST_FUNCTION_DEFINITION);
    function->source = keyword_site;
    add_attributes(function);

    token_next(); 
    TokenInfo tok_function_name = token_current();
    
    Ast* function_name = Ast_create(AST_IDENTIFIER);
    function_name->source = tok_function_name.source;


    if (tok_function_name.type == TOKEN_IDENTIFIER) {
        function_name->identifier.name = tok_function_name.data;
    } 
    else {
        log_source_error(tok_function_name.source, ERROR_SYNTAX, "Invalid function identifier");
    }

    /* Assigned before the overload-set dispatch below, which returns early. */
    function->function_declaration.name = function_name;

    token_next();

    if (token_is(token_current(), "=")) {
        log_source_error(token_current().source, ERROR_SYNTAX,
               "function alias must be declared with 'using', e.g. 'using name target'");
        return NULL;
    }

    Ast** function_params = vector_create();

    if (token_is(token_current(), "{")) {
        token_next();
        return parser_function_overload_declaration(function);
    }
    else if (token_is(token_current(), "(")) {
        token_next();
    }
    else {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected '(' or '{'");
    }

    int has_ellipsis = 0;

    while (!token_is(token_current(), ")")) {
        if (token_is(token_current(), "...")) {
            has_ellipsis = 1;
            token_next();
            break;
        }

        Ast* param_pat   = NULL;
        Ast* param_type  = NULL;

        if (token_current().type == TOKEN_IDENTIFIER) {
            param_pat = Ast_create(AST_IDENTIFIER);
            param_pat->identifier.name = token_current().data;
            token_next();
        } else {
            log_source_error(token_current().source, ERROR_SYNTAX, "Invalid param identifier");
        }

        if (!token_is(token_current(), ":")) {
            log_source_error(token_current().source, ERROR_SYNTAX, "Expected ':'");
        }
        token_next();
        param_type = parse_type();

        Ast* param = Ast_create(AST_PARAMETER_DECLARATION);
        param->parameter_declaration.name          = param_pat;
        param->parameter_declaration.type          = param_type;
        
        vector_add(&function_params, param);

        if (token_is(token_current(), ",")) {
            token_next();
        } else if (!token_is(token_current(), ")")) {
            log_source_error(token_current().source, ERROR_SYNTAX, "Expected ',' or ')'");
        }
    }

    token_next();

    Ast* function_output = NULL;

    if (token_current().type == TOKEN_ARROW) {
        token_next();
        function_output = parse_type();
    }

    Ast* function_body = NULL;
    if (token_is(token_current(), "{")) {
        function_body = parser_block_statement();
    }

    if (is_reserved_builtin_name(identifier_name(function_name))) {
        char str[512];
        snprintf(str, sizeof(str), "cannot declare function '%s': name is reserved for a builtin", identifier_name(function_name));
        log_source_error(function_name->source, ERROR_UNDEFINED, str); 
    }

    if (function_body != NULL) {
        function->type = AST_FUNCTION_DEFINITION;
    } else {
        function->type = AST_FUNCTION_DECLARATION;
    }

    function->function_declaration.name         = function_name;
    function->function_declaration.params       = function_params;
    function->function_declaration.type         = function_output;
    function->function_declaration.body         = function_body;
    function->function_declaration.has_ellipsis = has_ellipsis;
    function->function_declaration.is_defined   = (function_body != NULL);
    function->function_declaration.is_called    = false;

    return function;
}

Ast* parser_return_statement() {
    Ast* return_node = Ast_create(AST_RETURN_STATEMENT);
    return_node->source = token_current().source;

    token_next();

    Ast* value = NULL;
    if (!token_is(token_current(), ";") &&
        !token_end() &&
        !parser.newline &&
        !token_is(token_current(), "}") &&
        !token_is(token_current(), ")") &&
        !token_is(token_current(), "]")) {
        value = parser_binary_expression();
    }

    if (token_is(token_current(), ";")) {
        token_next();
    }
    else if (!parser.newline && !token_is(token_current(), "}")) {
        log_source_error(token_current().source, ERROR_SYNTAX, "Expected ';'");
    }

    return_node->return_statement.value = value;
    return return_node;
}

Ast* parser_tokens(TokenInfo* token_infos) {
    parser = (TokenParser){0};
    parser.tokens     = token_infos;
    parser.attributes = vector_create();
    parser.size       = vector_size(token_infos);
    token_first();

    Ast* root  = Ast_create(AST_ROOT);
    root->block.statements = vector_create();

    while (!token_end()) {
        TokenInfo token = token_current();
        if (token.data == NULL) break;

        Ast* sta = parser_statement();
        if (sta != NULL) {
            vector_add(&root->block.statements, sta);
        } else {
            token_next();
        }
    }

    return root;
}
