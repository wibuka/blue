#pragma once

#define KRED   "\x1B[31m"
#define KGRN   "\x1B[32m"
#define KYEL   "\x1B[33m"
#define KBLU   "\x1B[34m"
#define KMAG   "\x1B[35m"
#define KCYN   "\x1B[36m"
#define KWHT   "\x1B[37m"
#define KRESET "\x1B[0m"

typedef enum {
    ERROR_UNKNOWN,
    ERROR_MISMATCH,
    ERROR_UNDEFINED,
    ERROR_INVALID,
    ERROR_SYNTAX,
    ERROR_ARGUMENT,
    ERROR_RETURN,
    ERROR_INTERNAL
} ErrorType;

extern char log_description[2048];

void log_source_error(SourceSite source, ErrorType error_type, const char *message);
void log_error(const char *message);
void log_success(const char *message);

void log_Symbol(Symbol* symbol);
void log_Ast(Ast* node);
void log_TypeData(TypeData* type);
