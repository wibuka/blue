#pragma once

typedef struct SourceSite {
    const char* path;
    size_t row;
    size_t col;
} SourceSite;

typedef enum {
    TOKEN_UNKNOW,       TOKEN_IDENTIFIER,
    TOKEN_PACKAGE,      TOKEN_IMPORT,
    TOKEN_AS,           TOKEN_VAR,
    TOKEN_CONST,        TOKEN_DEF,
    TOKEN_STRUCT,       TOKEN_ENUM,
    TOKEN_RETURN,       TOKEN_IF,
    TOKEN_ELSE,         TOKEN_CHAR,
    TOKEN_STRING,       TOKEN_INTEGER,
    TOKEN_FLOAT,        TOKEN_BOOLEAN,
    TOKEN_SEMICOLON,    TOKEN_SYMBOL,
    TOKEN_ELLIPSIS,     TOKEN_ARROW,
    TOKEN_NEWLINE,      TOKEN_AND,TOKEN_OR,
    TOKEN_IN,           TOKEN_COMPARISON,
    TOKEN_NOT,          TOKEN_NOT_EQUAL,
    TOKEN_CONTINUE,     TOKEN_BREAK,
    TOKEN_FOR,          TOKEN_LOOP,
    TOKEN_NIL,          TOKEN_USING,
    TOKEN_ATTRIBUTE,    TOKEN_ADDRESS,
    TOKEN_RANGE,        TOKEN_COMMENT,
    TOKEN_HEXADECIMAL,  TOKEN_BINARY,
    TOKEN_GOTO
}TokenType;

typedef struct {
    char* data;
    TokenType type;
    SourceSite source;
}TokenInfo;

int tokenizer(TokenInfo** vec, char* context, const char* path);

const char *token_type_name(TokenType type);
