#pragma once
#include <stdbool.h>

typedef enum {
    CLI_FLAG_NONE,
    CLI_FLAG_HELP,
    CLI_FLAG_VERSION,
} CLIInfo;

typedef enum {
    CLI_FLAG_OUTPUT_OBJECT,
    CLI_FLAG_OUTPUT_LLVM_IR,
    CLI_FLAG_OUTPUT_ASSEMBLY,
    CLI_FLAG_OUTPUT_EXECUTABLE,
    CLI_FLAG_OUTPUT_AST,
} CLIEmit;

typedef enum {
    CLI_FLAG_O0,
    CLI_FLAG_O1,
    CLI_FLAG_O2,
    CLI_FLAG_O3,
    CLI_FLAG_OS,
} CLIOptimization;


typedef struct CLI {
    CLIInfo         info;
    CLIEmit         emit;
    CLIOptimization optimization;

    const char*  input;
    const char*  output;
    const char*  target;
    const char*  sysroot;
    const char** defines;
    const char** libs;
    const char** libpaths;
    const char** libfiles;
} CLI;

CLI cli_parse(int argc, char** argv);
