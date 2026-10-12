#include "tokenizer.h"
#include "semantic.h"
#include "ast_explorer.h"
#include "llvm-generator.h"
#include "compile.h"

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

static int file_exists(const char *path) {
    if (!path || !*path) return 0;
#if defined(_WIN32)
    return _access(path, 0) == 0;
#else
    return access(path, F_OK) == 0;
#endif
}

char** flags;

int main(int argc, char** argv) {
    CLI cli = cli_parse(argc, argv);

    if (cli.info == CLI_FLAG_VERSION) {
        printf("blue %s\n", BLUE_VERSION_STRING);
        return 0;
    }

    if (cli.info == CLI_FLAG_HELP) {
        printf(
            "Usage: blue [options] <input>\n"
            "\n"
            "Options:\n"
            "  -o, --output <file>       Output file path\n"
            "  -c                        Compile to object (.o)\n"
            "  -s                        Emit assembly (.s)\n"
            "  -ll                       Emit LLVM IR (.ll)\n"
            "  --ast=<file>              Emit JSON AST dump\n"
            "  -O0 | -O1 | -O2 | -O3 | -Os  Optimization level\n"
            "  -D <name>                 Preprocessor definition\n"
            "  --target <triple>         Target triple\n"
            "  --sysroot <path>          Sysroot path\n"
            "  --emit-llvm               Emit LLVM IR to stdout\n"
            "  -h, --help                Show this help\n"
            "  -v, --version             Show version\n"
        );
        return 0;
    }

    if (!cli.input) {
        log_error("No input file specified.");
        return 1;
    }

    if (!file_exists(cli.input)) {
        char message[1024];
        snprintf(message, sizeof(message), "%s does not exist", cli.input);
        log_error(message);
        return 1;
    }

    char* context = NULL;
    if (!read_file(&context, cli.input)) {
        char message[1024];
        snprintf(message, sizeof(message),"Could not open source file '%s'", cli.input);
        log_error(message);
        return 1;
    }

    TokenInfo* token_infos = vector_create();
    tokenizer(&token_infos, context, cli.input);

    Ast_build(); 

    Ast* ast = parser_tokens(token_infos);
    if (cli.emit == CLI_FLAG_OUTPUT_AST) {
        if (ast_dump(ast, &cli)) return 1;
        return 0;
    }

    if (!load_module(ast, cli.input)) return 1;

    abi_init_native();
    semantic(ast, builtin_declarations);
    LLVMModuleRef module = llvm_generate(ast, cli);
    int status = compile(cli, module);
    LLVMContextRef ctx = LLVMGetModuleContext(module);
    LLVMDisposeModule(module);
    LLVMContextDispose(ctx);
    return status;
}
