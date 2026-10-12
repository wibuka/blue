
#include "compile.h"
#include "module.h"

#include "llvm-c/TargetMachine.h"
#include "llvm-c/Target.h"
#include "llvm-c/Target.h"

#ifdef _WIN32
#include <io.h>
#endif

#ifdef _WIN32
#endif


static LLVMRelocMode select_reloc_mode(const char *target_triple);

void initialize_targets(void) {
    LLVMInitializeX86TargetInfo();
    LLVMInitializeX86Target();
    LLVMInitializeX86TargetMC();
    LLVMInitializeX86AsmPrinter();
    LLVMInitializeX86AsmParser();

    LLVMInitializeAArch64TargetInfo();
    LLVMInitializeAArch64Target();
    LLVMInitializeAArch64TargetMC();
    LLVMInitializeAArch64AsmPrinter();
    LLVMInitializeAArch64AsmParser();

    LLVMInitializeARMTargetInfo();
    LLVMInitializeARMTarget();
    LLVMInitializeARMTargetMC();
    LLVMInitializeARMAsmPrinter();
    LLVMInitializeARMAsmParser();

    LLVMInitializeWebAssemblyTargetInfo();
    LLVMInitializeWebAssemblyTarget();
    LLVMInitializeWebAssemblyTargetMC();
    LLVMInitializeWebAssemblyAsmPrinter();
    LLVMInitializeWebAssemblyAsmParser();
}

static int append_arg(char *command, size_t capacity, size_t *used, const char *arg, int quote)
{
    int n;

    if (quote) {
#ifdef _WIN32
        n = snprintf(command + *used, capacity - *used," \"%s\"", arg);
#else
        n = snprintf(command + *used, capacity - *used," '%s'", arg);
#endif
    } else {
        n = snprintf(command + *used, capacity - *used," %s", arg);
    }

    if (n < 0 || (size_t)n >= capacity - *used)
        return -1;

    *used += (size_t)n;
    return 0;
}

static int append_option(char *command, size_t capacity, size_t *used, const char *prefix, const char *value)
{
    size_t size = strlen(prefix) + strlen(value) + 1;
    char *arg = malloc(size);

    if (!arg)
        return -1;

    snprintf(arg, size, "%s%s", prefix, value);

    int result = append_arg(command, capacity, used, arg, 0);
    free(arg);
    return result;
}

static int directory_exists(const char *path)
{
#ifdef _WIN32
    return _access(path, 0) == 0;
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

/* Returns the parent directory of out_file into buf (caller-provided),
 * or 1 (no parent dir) if out_file has no directory component. */
static int parent_dir(const char *out_file, char *buf, size_t size)
{
    char *slash;

    if (!out_file || !*out_file)
        return 1;

    strncpy(buf, out_file, size - 1);
    buf[size - 1] = '\0';

    slash = strrchr(buf, '/');
#ifdef _WIN32
    if (!slash) {
        slash = strrchr(buf, '\\');
    }
#endif
    if (!slash)
        return 1;

    *slash = '\0';
    return directory_exists(buf[0] ? buf : "/");
}

int link_object(CLI cli, const char *opt_flag, const char *obj_file)
{
    const char *out_file = cli.output ? cli.output : "a.out";
    const char *clang = "clang";
    char dir[4096];

    if (!parent_dir(out_file, dir, sizeof(dir))) {
        char message[4352];
        snprintf(message, sizeof(message),
                 "Output directory '%s' does not exist",
                 dir[0] ? dir : "/");
        log_error(message);
        return -1;
    }

    char **module_links = get_module_links();
    size_t module_link_count = vector_size(module_links);

    size_t define_count = vector_size(cli.defines);
    size_t lib_count = vector_size(cli.libs);
    size_t lib_path_count = vector_size(cli.libpaths);
    size_t libfile_count = vector_size(cli.libfiles);

    size_t capacity = 4096;

    const char *fixed[] = {
        clang, cli.target, cli.sysroot, opt_flag, obj_file, out_file
    };

    for (size_t i = 0; i < sizeof(fixed) / sizeof(fixed[0]); i++) {
        if (fixed[i])
            capacity += strlen(fixed[i]) * 4 + 64;
    }

    const char **arrays[] = {
        cli.defines, cli.libs, cli.libpaths, cli.libfiles,
        (const char **)module_links
    };

    size_t counts[] = {
        define_count, lib_count, lib_path_count, libfile_count,
        module_link_count
    };

    for (size_t a = 0; a < sizeof(arrays) / sizeof(arrays[0]); a++) {
        for (size_t i = 0; i < counts[a]; i++) {
            if (arrays[a][i])
                capacity += strlen(arrays[a][i]) * 4 + 64;
        }
    }

    char *command = malloc(capacity);
    if (!command) {
        fprintf(stderr, "Error: Out of memory while building link command\n");
        return -1;
    }

    size_t used = 0;
    int n = snprintf(command, capacity, "%s", clang);

    if (n < 0 || (size_t)n >= capacity) {
        free(command);
        return -1;
    }
    used = (size_t)n;

    if (cli.target &&
        append_option(command, capacity, &used, "--target=", cli.target))
        goto command_error;

    if (cli.sysroot) {
        if (append_arg(command, capacity, &used, "--sysroot", 0) ||
            append_arg(command, capacity, &used, cli.sysroot, 1))
            goto command_error;
    }

    if (opt_flag && append_arg(command, capacity, &used, opt_flag, 0))
        goto command_error;

    for (size_t i = 0; i < define_count; i++) {
        if (cli.defines[i] &&
            append_option(command, capacity, &used, "-D", cli.defines[i]))
            goto command_error;
    }

    for (size_t i = 0; i < lib_path_count; i++) {
        if (cli.libpaths[i] &&
            append_option(command, capacity, &used, "-L", cli.libpaths[i]))
            goto command_error;
    }

    if (append_arg(command, capacity, &used, obj_file, 1))
        goto command_error;

    for (size_t i = 0; i < libfile_count; i++) {
        if (cli.libfiles[i] &&
            append_arg(command, capacity, &used, cli.libfiles[i], 1))
            goto command_error;
    }

    for (size_t i = 0; i < lib_count; i++) {
        if (cli.libs[i] &&
            append_option(command, capacity, &used, "-l", cli.libs[i]))
            goto command_error;
    }

    for (size_t i = 0; i < module_link_count; i++) {
        const char *link = module_links[i];
        if (!link)
            continue;

        if (append_arg(command, capacity, &used, link,
                       link[0] != '-'))
            goto command_error;
    }

    if (append_arg(command, capacity, &used, "-o", 0) ||
        append_arg(command, capacity, &used, out_file, 1))
        goto command_error;

    int result = system(command);

    if (result != 0) {
        fprintf(stderr,"Error: Linking failed (command exit status: %d)\n", result);
    }

    free(command);
    return result;

command_error:
    fprintf(stderr, "Error: Failed to construct linker command\n");
    free(command);
    return -1;
}

static LLVMRelocMode select_reloc_mode(const char *triple)
{
    if (!triple)
        return LLVMRelocDefault;

    if (strstr(triple, "linux"))
        return LLVMRelocPIC;

    return LLVMRelocDefault;
}

int compile(CLI cli, LLVMModuleRef module)
{
    char *error = NULL;
    const char *out_file = cli.output ? cli.output : "a.out";
    char *default_triple = NULL;
    const char *target_triple = cli.target;

    /* Resolve target. */
    if (!target_triple) {
        default_triple = LLVMGetDefaultTargetTriple();
        target_triple = default_triple;

        if (!target_triple) {
            fprintf(stderr, "Error: Failed to determine default target\n");
            return 1;
        }
    }

    /* LLVM IR output does not require a target machine. */
    if (cli.emit == CLI_FLAG_OUTPUT_LLVM_IR) {
        LLVMSetTarget(module, target_triple);

        if (LLVMPrintModuleToFile(module, out_file, &error)) {
            fprintf(stderr, "Error: Failed to emit LLVM IR: %s\n",
                    error ? error : "unknown error");

            if (error)
                LLVMDisposeMessage(error);
            if (default_triple)
                LLVMDisposeMessage(default_triple);

            return 1;
        }

        if (default_triple)
            LLVMDisposeMessage(default_triple);

        log_success(out_file);
        return 0;
    }

    /* Initialize LLVM targets. */
    initialize_targets();

    LLVMTargetRef target;

    if (LLVMGetTargetFromTriple(target_triple, &target, &error)) {
        fprintf(stderr, "Error: Unsupported target '%s': %s\n",
                target_triple,
                error ? error : "unknown error");

        if (error)
            LLVMDisposeMessage(error);
        if (default_triple)
            LLVMDisposeMessage(default_triple);

        return 1;
    }

    /* Resolve optimization settings. */
    LLVMCodeGenOptLevel opt_level = LLVMCodeGenLevelDefault;
    const char *opt_flag = "-O0";

    switch (cli.optimization) {
        case CLI_FLAG_O0:
            opt_level = LLVMCodeGenLevelNone;
            opt_flag = "-O0";
            break;

        case CLI_FLAG_O1:
            opt_level = LLVMCodeGenLevelLess;
            opt_flag = "-O1";
            break;

        case CLI_FLAG_O2:
            opt_level = LLVMCodeGenLevelDefault;
            opt_flag = "-O2";
            break;

        case CLI_FLAG_O3:
            opt_level = LLVMCodeGenLevelAggressive;
            opt_flag = "-O3";
            break;

        case CLI_FLAG_OS:
            opt_level = LLVMCodeGenLevelDefault;
            opt_flag = "-Os";
            break;
    }

    LLVMRelocMode reloc_mode = select_reloc_mode(target_triple);

    LLVMTargetMachineRef tm = LLVMCreateTargetMachine(
        target,
        target_triple,
        "generic",
        "",
        opt_level,
        reloc_mode,
        LLVMCodeModelDefault
    );

    if (!tm) {
        fprintf(stderr, "Error: Failed to create target machine for '%s'\n",
                target_triple);

        if (default_triple)
            LLVMDisposeMessage(default_triple);

        return 1;
    }

    LLVMSetTarget(module, target_triple);

    /* Set data layout for the selected target. */
    LLVMTargetDataRef target_data = LLVMCreateTargetDataLayout(tm);

    if (target_data) {
        char *data_layout = LLVMCopyStringRepOfTargetData(target_data);

        if (data_layout) {
            LLVMSetDataLayout(module, data_layout);
            LLVMDisposeMessage(data_layout);
        }

        LLVMDisposeTargetData(target_data);
    }

    /* Emit assembly or object file directly. */
    if (cli.emit == CLI_FLAG_OUTPUT_ASSEMBLY ||
        cli.emit == CLI_FLAG_OUTPUT_OBJECT) {

        LLVMCodeGenFileType file_type =
            cli.emit == CLI_FLAG_OUTPUT_ASSEMBLY
                ? LLVMAssemblyFile
                : LLVMObjectFile;

        if (LLVMTargetMachineEmitToFile(
                tm, module, (char *)out_file, file_type, &error)) {

            fprintf(stderr, "Error: Failed to emit output: %s\n",
                    error ? error : "unknown error");

            if (error)
                LLVMDisposeMessage(error);

            LLVMDisposeTargetMachine(tm);

            if (default_triple)
                LLVMDisposeMessage(default_triple);

            return 1;
        }

        LLVMDisposeTargetMachine(tm);

        if (default_triple)
            LLVMDisposeMessage(default_triple);

        log_success(out_file);
        return 0;
    }

    /* Emit a temporary object file for linking. */
#ifdef _WIN32
    const char *tmp_obj = ".blue_tmp.obj";
#else
    const char *tmp_obj = ".blue_tmp.o";
#endif

    if (LLVMTargetMachineEmitToFile(
            tm, module, (char *)tmp_obj, LLVMObjectFile, &error)) {

        fprintf(stderr, "Error: Failed to emit object file: %s\n",
                error ? error : "unknown error");

        if (error)
            LLVMDisposeMessage(error);

        LLVMDisposeTargetMachine(tm);

        if (default_triple)
            LLVMDisposeMessage(default_triple);

        return 1;
    }

    /* Link using CLI target, sysroot, defines and libraries. */
    int result = link_object(cli, opt_flag, tmp_obj);

    if (remove(tmp_obj) != 0) {
        fprintf(stderr, "Warning: Could not remove temporary object '%s'\n",
                tmp_obj);
    }

    LLVMDisposeTargetMachine(tm);

    if (default_triple)
        LLVMDisposeMessage(default_triple);

    if (result == 0) {
        log_success(out_file);
        return 0;
    }

    return 1;
}
