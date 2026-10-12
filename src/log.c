#include "log.h"

char log_description[2048];

void log_source_error(SourceSite source, ErrorType error_type, const char *message) {
    char *code = NULL;

    const char *path = source.path ? source.path : "<unknown>";

    if (source.path) {
        read_line(&code, source.path, source.row);
    }

    fprintf(
        stderr,
        "%s:%zu:%zu:\n"KRED"Error:"KRESET" %s\n"
        "%4zu | %s\n"
        "     | ",
        path,
        source.row,
        source.col,
        message,
        source.row,
        code ? code : ""
    );

    for (size_t i = 1; i < source.col; i++)
        fputc(' ', stderr);

    fprintf(stderr, "^\n");

    if (error_type == ERROR_SYNTAX) exit(EXIT_FAILURE);
}

void log_error(const char *message) {
    fprintf(stderr, KRED"Error:"KRESET" %s\n", message);
}

void log_success(const char *message) {
    fprintf(stderr, KGRN "Build succeeded: %s" KRESET "\n", message);
}

#include "ast_explorer.h"
#ifdef DEVELOP
void log_Ast(Ast* node) {
    printf(KRED "\nAST EXPLORER:" KRESET "\n");
    ast_explorer(node);
    printf("\n");
}

void log_TypeData(TypeData* type) {
    printf(KRED "\nTYPEDATA EXPLORER:" KRESET "\n");
    printf("base         : %s\n", typeBase_to_string(type->base));
    printf("identity     : %s\n", type->identity);
    printf("pointer_depth: %d\n", type->pointer_depth);
    printf("\n");
}

void log_Symbol(Symbol* symbol) {
    printf(KRED "\nSYMBOL EXPLORER:" KRESET "\n");
    printf("name       : %s\n", symbol->name);
    printf("type       : %s\n", TypeData_to_string(symbol->type));
    printf("llvm_type  : %p\n", symbol->llvm_type);
    printf("llvm_value : %p\n", symbol->llvm_value);
    printf("description: %p\n", symbol->declaration);
    printf("\n");
}
#else
void log_Symbol(Symbol* symbol) {}
void log_Ast(Ast* node)  {}
void log_TypeData(TypeData* type) {}
#endif
    
