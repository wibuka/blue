#include "cli.h"

static const char* consume_value(
    int argc,
    char** argv,
    int* index,
    const char* arg,
    const char* option,
    size_t option_len
) {
    if (arg[option_len] == '=') {
        const char* value = arg + option_len + 1;

        if (*value != '\0') {
            return value;
        }
    } else if (arg[option_len] == '\0' && *index + 1 < argc) {
        const char* value = argv[*index + 1];

        if (value[0] != '-') {
            (*index)++;
            return value;
        }
    }

    fprintf(stderr, "Error: %s requires a value\n", option);
    exit(EXIT_FAILURE);
}

static int has_extension(const char* path, const char* ext) {
    const char* base = path;
    const char* dot;

    /* Ignore directory components. */
    for (const char* p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }

    dot = strrchr(base, '.');

    if (!dot) {
        return 0;
    }

    while (*dot && *ext) {
        if (tolower((unsigned char)*dot) !=
            tolower((unsigned char)*ext)) {
            return 0;
        }

        dot++;
        ext++;
    }

    return *dot == '\0' && *ext == '\0';
}

static int is_library_file(const char* path) {
    return has_extension(path, ".a") ||
           has_extension(path, ".lib") ||
           has_extension(path, ".so") ||
           has_extension(path, ".dylib");
}

static void require_value(
    int argc,
    char** argv,
    int* index,
    const char* option,
    const char** value
) {
    if (*index + 1 >= argc || argv[*index + 1][0] == '\0') {
        fprintf(stderr, "Error: %s requires a value\n", option);
        exit(EXIT_FAILURE);
    }

    *value = argv[++(*index)];
}

CLI cli_parse(int argc, char** argv) {
    CLI cli = {
        .info = CLI_FLAG_NONE,
        .emit = CLI_FLAG_OUTPUT_EXECUTABLE,
        .optimization = CLI_FLAG_O0,

        .input   = NULL,
        .output  = NULL,

        .target  = NULL,
        .sysroot = NULL,

        .defines  = vector_create(),
        .libs     = vector_create(),
        .libfiles = vector_create(),
        .libpaths = vector_create(),
    };

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        const char* value = NULL;

        /* Help and version */
        if (strcmp(arg, "-h") == 0 ||
            strcmp(arg, "--help") == 0) {
            cli.info = CLI_FLAG_HELP;
        } else if (strcmp(arg, "-v") == 0 ||
                   strcmp(arg, "--version") == 0) {
            cli.info = CLI_FLAG_VERSION;
        }

        /* Target */
        else if (strncmp(arg, "--target", 8) == 0 &&
                 (arg[8] == '=' || arg[8] == '\0')) {
            cli.target = consume_value(
                argc, argv, &i, arg, "--target", 8
            );
        }

        /* Sysroot */
        else if (strncmp(arg, "--sysroot", 9) == 0 &&
                 (arg[9] == '=' || arg[9] == '\0')) {
            cli.sysroot = consume_value(
                argc, argv, &i, arg, "--sysroot", 9
            );
        }

        /* Preprocessor definitions: -DNAME, -D NAME */
        else if (strcmp(arg, "-D") == 0) {
            require_value(argc, argv, &i, "-D", &value);
            vector_add(&cli.defines, value);
        } else if (strncmp(arg, "-D", 2) == 0 &&
                   arg[2] != '\0') {
            vector_add(&cli.defines, arg + 2);
        }

        /* LLVM IR: check before library flags */
        else if (strcmp(arg, "--emit-llvm") == 0 ||
                 strcmp(arg, "-ll") == 0) {
            cli.emit = CLI_FLAG_OUTPUT_LLVM_IR;
        }

        /* Libraries: -lraylib, -l raylib */
        else if (strcmp(arg, "-l") == 0) {
            require_value(argc, argv, &i, "-l", &value);
            vector_add(&cli.libs, value);
        } else if (strncmp(arg, "-l", 2) == 0 &&
                   arg[2] != '\0') {
            vector_add(&cli.libs, arg + 2);
        }

        /* Library search paths: -Lpath, -L path */
        else if (strcmp(arg, "-L") == 0) {
            require_value(argc, argv, &i, "-L", &value);
            vector_add(&cli.libpaths, value);
        } else if (strncmp(arg, "-L", 2) == 0 &&
                   arg[2] != '\0') {
            vector_add(&cli.libpaths, arg + 2);
        }

        /* Output modes */
        else if (strcmp(arg, "-s") == 0 ||
                 strcmp(arg, "-S") == 0) {
            cli.emit = CLI_FLAG_OUTPUT_ASSEMBLY;
        } else if (strcmp(arg, "-c") == 0) {
            cli.emit = CLI_FLAG_OUTPUT_OBJECT;
        }

        /* Optimization */
        else if (strcmp(arg, "-O0") == 0) {
            cli.optimization = CLI_FLAG_O0;
        } else if (strcmp(arg, "-O1") == 0) {
            cli.optimization = CLI_FLAG_O1;
        } else if (strcmp(arg, "-O2") == 0) {
            cli.optimization = CLI_FLAG_O2;
        } else if (strcmp(arg, "-O3") == 0) {
            cli.optimization = CLI_FLAG_O3;
        } else if (strcmp(arg, "-Os") == 0) {
            cli.optimization = CLI_FLAG_OS;
        }

        /* Output file */
        else if (strcmp(arg, "-o") == 0) {
            require_value(argc, argv, &i, "-o", &cli.output);
        }

        /* AST output */
        else if (strcmp(arg, "-ast") == 0 ||
                 strcmp(arg, "--ast") == 0) {
            cli.emit = CLI_FLAG_OUTPUT_AST;

            if (i + 1 < argc &&
                has_extension(argv[i + 1], ".ast")) {
                cli.output = argv[++i];
            }
        } else if (strncmp(arg, "--ast=", 6) == 0) {
            if (arg[6] == '\0') {
                fprintf(stderr, "Error: --ast requires a filename\n");
                exit(EXIT_FAILURE);
            }

            cli.emit = CLI_FLAG_OUTPUT_AST;
            cli.output = arg + 6;
        } else if (strncmp(arg, "-ast=", 5) == 0) {
            if (arg[5] == '\0') {
                fprintf(stderr, "Error: -ast requires a filename\n");
                exit(EXIT_FAILURE);
            }

            cli.emit = CLI_FLAG_OUTPUT_AST;
            cli.output = arg + 5;
        }

        /* Explicit library files */
        else if (is_library_file(arg)) {
            vector_add(&cli.libfiles, arg);
        }

        /* Unknown options */
        else if (arg[0] == '-') {
            fprintf(stderr, "Error: Unknown option: %s\n", arg);
            exit(EXIT_FAILURE);
        }

        /* Input file */
        else {
            if (cli.input != NULL) {
                fprintf(stderr,
                        "Error: Multiple input files: '%s' and '%s'\n",
                        cli.input, arg);
                exit(EXIT_FAILURE);
            }

            cli.input = arg;
        }
    }

    if (cli.info != CLI_FLAG_NONE) {
        return cli;
    }

    if (!cli.input) {
        fprintf(stderr, "Error: No input file specified\n");
        exit(EXIT_FAILURE);
    }

    if (!cli.output && cli.emit != CLI_FLAG_OUTPUT_AST) {
        fprintf(stderr,
                "Error: No output file specified, use -o <file>\n");
        exit(EXIT_FAILURE);
    }

    return cli;
}
