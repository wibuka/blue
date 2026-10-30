#include "tokenizer.h"

int is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

int is_symbol(char c){
    switch (c){
        case '.': case ',': case '`':
        case '+': case '-': case '*':
        case '/': case '=': case '%':
        case '&': case ';': case '!':
        case '<': case '>': case '(':
        case ')': case '{': case '}':
        case '[': case ']': case ':':
        
        return 1;
    }
    return 0;
}

int same_char(const char a,const char b) {
    if (a == b) return 1;
    return 0;
}

int is_number(const char c) {
    if (c >= '0' && c <= '9') return 1;
    return 0;
}

int is_int(const char* str) {
    if (str == NULL || *str == '\0') return 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (!is_number(str[i])) {
            return 0;
        }
    }
    return 1;
}

int is_float(const char* str){
    if (str == NULL || *str == '\0') return 0;
    int has_dot = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '.') {
            has_dot = 1;
            continue;
        }
        if (!is_number(str[i])) {
            return 0;
        }
    }
    return has_dot;
}

#define MATCH(kw, tok)  if (str_match(str, kw)) return TOKEN_##tok
TokenType get_token_type(const char* str) {
    if (is_int(str))   return TOKEN_INTEGER;
    if (is_float(str)) return TOKEN_FLOAT;

    MATCH("var",      VAR);       
    MATCH("const",    CONST);
    MATCH("def",      DEF);
    MATCH("struct",   STRUCT);
    MATCH("enum",     ENUM);
    MATCH("using",    USING);
    MATCH("import",   IMPORT);

    MATCH("if",       IF);
    MATCH("else",     ELSE);
    MATCH("for",      FOR);
    MATCH("loop",     LOOP);
    MATCH("goto",     GOTO);
    MATCH("break",    BREAK);
    MATCH("continue", CONTINUE);
    MATCH("return",   RETURN);

    MATCH("and",      AND);  
    MATCH("or",       OR);
    MATCH("not",      NOT);
    MATCH("in",       IN);
    MATCH("as",       AS);

    MATCH("nil",      NIL);
    MATCH("true",     BOOLEAN);
    MATCH("false",    BOOLEAN);

    return TOKEN_IDENTIFIER;
}
#undef MATCH

const char *token_type_name(TokenType type) {
    switch (type) {
#define T(x) case TOKEN_##x: return "TOKEN_" #x
    T(IF);
    T(IN);          T(OR);
    T(AS);          T(DEF);
    T(CONST);       T(VAR);          T(FOR);          T(NIL);
    T(AND);         T(CHAR);         T(ELSE);         T(LOOP);         T(FLOAT);
    T(RANGE);       T(ARROW);        T(BREAK);        T(IMPORT);       T(RETURN);       T(STRING);
    T(STRUCT);      T(SYMBOL);       T(UNKNOW);       T(ADDRESS);      T(NEWLINE);      T(BOOLEAN);      T(INTEGER);
    T(PACKAGE);     T(CONTINUE);     T(ELLIPSIS);     T(SEMICOLON);    T(IDENTIFIER);   T(USING);        T(ENUM);           T(GOTO);

#undef T
    default: return "UNKNOWN_TOKEN";
    }
}

static void add_token(TokenInfo** vec, char* data, TokenType type, size_t row, size_t col, const char* path) {
    TokenInfo info;
    info.data        = data;
    info.type        = type;
    info.source.row  = row;
    info.source.col  = col;
    info.source.path = path;

    vector_add(vec, info);
}

static size_t lex_hexadecimal(char* context, size_t len, size_t i, size_t line, size_t col, TokenInfo** vec, const char* path) {
    size_t start = i;
    for (i += 2; i < len && isxdigit((unsigned char)context[i]); i++);
    if (i == start + 2) i = start + 1;
    add_token(vec, str_cut(context, start, i), TOKEN_HEXADECIMAL, line, col, path);
    return i;
}

static size_t lex_binary(char* context, size_t len, size_t i, size_t line, size_t col, TokenInfo** vec, const char* path) {
    size_t start = i;
    for (i += 2; i < len && (context[i] == '0' || context[i] == '1'); i++);
    if (i == start + 2) i = start + 1;
    add_token(vec, str_cut(context, start, i), TOKEN_BINARY, line, col, path);
    return i;
}

static size_t lex_string(char* context, size_t len, size_t i, size_t line, size_t col, TokenInfo** vec, const char* path) {
    (void)len;
    i++;
    size_t start = i;
    while (i < len && context[i] != '"') {
        i += (context[i] == '\\' && i + 1 < len) ? 2 : 1;
    }
    add_token(vec, str_cut(context, start, i), TOKEN_STRING, line, col, path);
    if (i < len && context[i] == '"') i++;
    return i;
}

static size_t lex_char(char* context, size_t len, size_t i, size_t line, size_t col, TokenInfo** vec, const char* path) {
    (void)len;
    i++;
    size_t start = i;
    while (i < len && context[i] != '\'') {
        i += (context[i] == '\\' && i + 1 < len) ? 2 : 1;
    }
    add_token(vec, str_cut(context, start, i), TOKEN_CHAR, line, col, path);
    if (i < len && context[i] == '\'') i++;
    return i;
}

static size_t lex_newline(char* context, size_t len, size_t i, size_t line, size_t col, TokenInfo** vec, const char* path) {
    (void)len;
    i++;
    while (i < len && context[i] == '\n') i++;
    add_token(vec, "\n", TOKEN_NEWLINE, line, col, path);
    return i;
}

static size_t lex_symbol(char* context, size_t len, size_t i, size_t line, size_t col, TokenInfo** vec, const char* path) {
    (void)len;
    size_t start = i;
    i++;
    {
        // address: "&"
        if (context[i - 1] == '&') {
            add_token(vec, str_cut(context, start, i), TOKEN_ADDRESS, line, col, path);
            return i;
        }
        // token_arrow "->"
        if (i < len && context[i] == '>' && context[i - 1] == '-') {
            i++;
            add_token(vec, str_cut(context, start, i), TOKEN_ARROW, line, col, path);
            return i;
        }
        // ellipsis: "..."
        if (context[i - 1] == '.' && i + 1 < len && context[i] == '.' && context[i + 1] == '.') {
            i += 2;
            add_token(vec, str_cut(context, start, i), TOKEN_ELLIPSIS, line, col, path);
            return i;
        }
        // range operator: ".."
        if (context[i - 1] == '.' && context[i] == '.') {
            i += 1;
            add_token(vec, str_cut(context, start, i), TOKEN_RANGE, line, col, path);
            return i;
        }
    }
    // Equality and comparison operators.
    if (i < len && ((context[i - 1] == '=' && context[i] == '=') ||
                    (context[i - 1] == '!' && context[i] == '=') ||
                    (context[i - 1] == '<' && context[i] == '=') ||
                    (context[i - 1] == '>' && context[i] == '='))) {
        i++;
        add_token(vec, str_cut(context, start, i), TOKEN_SYMBOL, line, col, path);
        return i;
    }
    // Compound arithmetic and increment/decrement operators
    if (i < len && ((context[i - 1] == '+' && (context[i] == '+'  || context[i] == '=')) ||
                    (context[i - 1] == '-' && (context[i] == '-'  || context[i] == '=')) ||
                    (context[i - 1] == '*' && context[i]  == '=') ||
                    (context[i - 1] == '/' && context[i]  == '=') ||
                    (context[i - 1] == '%' && context[i]  == '='))) {
        i++;
        add_token(vec, str_cut(context, start, i), TOKEN_SYMBOL, line, col, path);
        return i;
    }
    // symbol
    add_token(vec, str_cut(context, start, i), TOKEN_SYMBOL, line, col, path);
    return i;
}

static size_t lex_comment(char *c, size_t n, size_t i) {
    if (i + 2 < n && c[i] == '/' && c[i+1] == '*') {
        for (i += 2; i + 1 < n && (c[i] != '*' || c[i+1] != '/'); i++);
        return i + 1 < n ? i + 2 : n;
    }
    for (i += 2; i < n && c[i] != '\n'; i++);
    return i;
}

static size_t lex_statement_terminator(char* context, size_t len, size_t i, size_t line, size_t col, TokenInfo** vec, const char* path) {
    (void)len;
    size_t start = i;
    i++;
    add_token(vec, str_cut(context, start, i), TOKEN_SEMICOLON, line, col, path);
    return i;
}

static size_t lex_word(char* context, size_t len, size_t i, size_t line, size_t col, TokenInfo** vec, const char* path) {
    (void)len;
    size_t start = i;

    if (is_number(context[i])) {
        while (i < len) {
            char c = context[i];
            if (is_number(c)) { i++; continue; }
            if (c == '.' &&
                i + 1 < len &&
                is_number(context[i + 1])) { i++; continue; }
            break;
        }
    } 
    else {
        while (i < len) {
            char c = context[i];
            if (is_whitespace(c) || c == '"' || c == '\'') break;
            if (is_symbol(c)) break;
            i++;
        }
    }

    char* sub = str_cut(context, start, i);
    add_token(vec, sub, get_token_type(sub), line, col, path);
    return i;
}

static size_t lex_attribute(char* context, size_t len, size_t i, size_t line,size_t col, TokenInfo** vec, const char* path) 
{
    (void)len;
    size_t start = i++;
    while (i < len && !is_whitespace(context[i])) i++;
    add_token(vec, str_cut(context, start, i), TOKEN_ATTRIBUTE, line, col, path);
    return i;
}

int tokenizer(TokenInfo** vec, char* context, const char* path) {
    if (!context) return 0;
    if (*vec == NULL) *vec = vector_create();

    size_t len  = strlen(context);
    size_t line = 1;
    size_t col  = 1;
    size_t i    = 0;

    while (i < len) {
        char c = context[i];
        size_t old_i = i;

        if (c == '/' && i + 1 < len) {
            if (context[i + 1] == '/' || context[i + 1] == '*') {
                i = lex_comment(context, len, i);
                col = 1;
                continue;
            }
        }
 
        if (c == '\n')        { i = lex_newline(context, len, i, line, col, vec, path); line += (i - old_i); col = 1;    continue; }
        if (c == ';')         { i = lex_statement_terminator(context, len, i, line, col, vec, path); col += (i - old_i); continue; }
        // IMPORTANT: Keep this before lex_word() and other token readers.
        // Whitespace is a token delimiter and must be consumed first.
        if (is_whitespace(c)) { i++; col++; continue; } 
        {
            if (c == '@')         { i = lex_attribute(context, len, i, line, col, vec, path); col += (i - old_i); continue; }       
            if (c == '"')         { i = lex_string(context, len, i, line, col, vec, path);    col += (i - old_i); continue; }
            if (c == '\'')        { i = lex_char(context, len, i, line, col, vec, path);      col += (i - old_i); continue; }
            if (is_symbol(c))     { i = lex_symbol(context, len, i, line, col, vec, path);    col += (i - old_i); continue; }

            if (c == '0' && i + 1 < len && (context[i + 1] == 'x' || context[i + 1] == 'X')) { 
                i = lex_hexadecimal(context, len, i, line, col, vec, path); 
                col += i - old_i; 
                continue;
            }
            if (c == '0' && i + 1 < len && (context[i + 1] == 'b' || context[i + 1] == 'B')) {
                i = lex_binary(context, len, i, line, col, vec, path);
                col += i - old_i;
                continue;
            }
        }
        i = lex_word(context, len, i, line, col, vec, path);
        col += (i - old_i);
    }
    return 1;
}
