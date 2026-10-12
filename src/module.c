#include "log.h"
#ifdef _WIN32
    #include <direct.h>
    #include <io.h>
    #define access _access
    #ifndef R_OK
    #define R_OK 4
    #endif
    typedef unsigned long DWORD;
    extern DWORD GetModuleFileNameA(void* module, char* filename, DWORD size);
#else
    #include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include "ast.h"
#include "tokenizer.h"

#define MAX_SEARCH_DIRS 3
#define MODULE_MANIFEST "module.toml"

#if defined(_WIN32)
#define MODULE_PLATFORM "windows"
#elif defined(__APPLE__)
#define MODULE_PLATFORM "macos"
#else
#define MODULE_PLATFORM "linux"
#endif

typedef enum {
    MODULE_NOT_FOUND,
    MODULE_FILE,
    MODULE_DIRECTORY,
} ModuleKind;

typedef struct {
    ModuleKind kind;
    char* path;
    char* manifest_less;
} ModuleSearch;

static char* copy_string(const char* text) {
    size_t size = strlen(text) + 1;
    char* copy = c_alloc(size);
    if (copy) memcpy(copy, text, size);
    return copy;
}

// Tokens keep a pointer to their source path for the lifetime of the AST, so
// the path handed to the tokenizer must outlive the module load.
static char* retain_module_path(const char* path) {
    static char** retained = NULL;
    if (!retained) retained = vector_create();
    char* copy = copy_string(path);
    if (copy) vector_add(&retained, copy);
    return copy ? copy : (char*)path;
}

static char** loaded_modules = NULL;

static char entry_dir[PATH_MAX];
static bool entry_dir_set = false;

static char** links = NULL;

char** get_module_links(void) {
    if (!links) links = vector_create();
    return links;
}

static void add_module_link(const char* path) {
    char** paths = get_module_links();

    for (size_t i = 0; i < vector_size(paths); i++) {
        if (strcmp(paths[i], path) == 0) return;
    }

    char* path_copy = copy_string(path);
    if (!path_copy) {
        fprintf(stderr, "Error: Out of memory while recording link inputs\n");
        return;
    }
    vector_add(&links, path_copy);
}

static bool is_module_loaded(const char* path) {
    if (!loaded_modules) return false;

    for (size_t i = 0; i < vector_size(loaded_modules); i++) {
        if (strcmp(loaded_modules[i], path) == 0) return true;
    }
    return false;
}

static void mark_module_loaded(const char* path) {
    if (!loaded_modules) {
        loaded_modules = vector_create();
    }
    
    size_t len = strlen(path) + 1;
    char* path_copy = c_alloc(len);
    if (path_copy) {
        memcpy(path_copy, path, len);
        vector_add(&loaded_modules, path_copy);
    }
}

int load_module(Ast* node, const char* source_path);
static void load_module_block(Ast* holder, const char* dir, int* success);
static int process_import_in_holder(Ast* holder, const char* import_path, const char* dir, int* success);

static bool is_readable_file(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return false;
    return access(path, R_OK) == 0;
}

static bool is_directory(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static char* join_path(const char* dir, const char* name) {
    if (!dir || dir[0] == '\0') return copy_string(name);

    
    size_t dir_len = strlen(dir);
    bool needs_slash = (dir_len > 0 && dir[dir_len - 1] != '/' && dir[dir_len - 1] != '\\');

    size_t size = dir_len + (needs_slash ? 1 : 0) + strlen(name) + 1;
    char* path = c_alloc(size);
    if (!path) return NULL;

    if (needs_slash) {
        snprintf(path, size, "%s/%s", dir, name);
    } else {
        snprintf(path, size, "%s%s", dir, name);
    }
    return path;
}

static char* module_file_name(const char* import_path) {
    const char* last_dot = strrchr(import_path, '.');
    const char* last_sep = strrchr(import_path, '/');
    if (!last_sep) last_sep = strrchr(import_path, '\\');
    bool has_extension = (last_dot && (!last_sep || last_dot > last_sep));

    if (has_extension) return copy_string(import_path);

    size_t size = strlen(import_path) + 6;
    char* name = c_alloc(size);
    if (!name) return NULL;

    snprintf(name, size, "%s.blue", import_path);
    return name;
}

static const char* executable_directory(void) {
    static char dir[PATH_MAX];
    static bool resolved = false;

    if (!resolved) {
        resolved = true;

#ifdef _WIN32
        DWORD len = GetModuleFileNameA(NULL, dir, (DWORD)sizeof(dir));

        if (len == 0 || len >= sizeof(dir))
            return NULL;

        dir[len] = '\0';

        char* slash = strrchr(dir, '\\');
        char* forward_slash = strrchr(dir, '/');

        if (!slash || (forward_slash && forward_slash > slash))
            slash = forward_slash;
#else
        ssize_t len = readlink("/proc/self/exe", dir, sizeof(dir) - 1);

        if (len <= 0)
            return NULL;

        dir[len] = '\0';
        char* slash = strrchr(dir, '/');
#endif

        if (!slash)
            return NULL;

        if (slash == dir)
            slash[1] = '\0';
        else
            *slash = '\0';
    }

    return dir;
}

static char* canonicalize_path(const char* path, char* buffer, size_t size) {
#ifdef _WIN32
    (void)size;
    return _fullpath(buffer, path, PATH_MAX);
#else
    (void)size;
    return realpath(path, buffer);
#endif
}

static size_t collect_search_dirs(const char* dir, const char* dirs[MAX_SEARCH_DIRS]) {
    size_t count = 0;

    const char* exe = executable_directory();
    if (exe) {
        bool duplicate = strcmp(exe, dir) == 0;
        if (!duplicate && entry_dir_set && strcmp(exe, entry_dir) == 0) duplicate = true;
        if (!duplicate) dirs[count++] = exe;
    }

    if (count < MAX_SEARCH_DIRS && (exe == NULL || strcmp(dir, exe) != 0)) {
        dirs[count++] = dir;
    }

    if (entry_dir_set && count < MAX_SEARCH_DIRS &&
        strcmp(entry_dir, dir) != 0 &&
        (exe == NULL || strcmp(exe, entry_dir) != 0)) {
        dirs[count++] = entry_dir;
    }

    return count;
}

static bool has_module_manifest(const char* dir_path) {
    char* manifest = join_path(dir_path, MODULE_MANIFEST);
    if (!manifest) return false;

    bool found = is_readable_file(manifest);
    return found;
}

static char* find_module(const char* import_path, const char* name, const char* const* dirs, size_t count, ModuleSearch* found) {
    bool try_directory = strcmp(import_path, name) != 0;
    *found = (ModuleSearch){0};

    for (size_t i = 0; i < count; i++) {
        char* candidate = join_path(dirs[i], name);
        if (!candidate) {
            fprintf(stderr, "Error: Out of memory during module import\n");
            return NULL;
        }
        if (is_readable_file(candidate)) {
            found->kind = MODULE_FILE;
            found->path = candidate;
            return candidate;
        }

        if (!try_directory) continue;

        candidate = join_path(dirs[i], import_path);
        if (!candidate) {
            fprintf(stderr, "Error: Out of memory during module import\n");
            return NULL;
        }
        if (is_directory(candidate)) {
            if (has_module_manifest(candidate)) {
                found->kind = MODULE_DIRECTORY;
                found->path = candidate;
                return candidate;
            }
            if (!found->manifest_less) found->manifest_less = candidate;
            else
            continue;
        }
    }
    return NULL;
}

static void log_search_failure(const char* import_path, const char* const* dirs, size_t count) {
    fprintf(stderr, "Error: Failed to import module '%s' (searched:", import_path);
    for (size_t i = 0; i < count; i++) {
        fprintf(stderr, "%s %s", i ? "," : "", dirs[i][0] ? dirs[i] : ".");
    }
    fprintf(stderr, ")\n");
}

static void log_field_type_error(const char* manifest_path, const char* field, const char* expected) {
    fprintf(stderr, "Error: invalid '%s' entry in %s\n  expected %s\n", field, manifest_path, expected);
}

static int load_manifest_imports(Ast* holder, toml_table_t* manifest, const char* manifest_path, const char* module_dir, int* success) {
    if (!toml_key_exists(manifest, "import")) return 1;

    toml_array_t* imports = toml_array_in(manifest, "import");
    if (!imports) {
        log_field_type_error(manifest_path, "import", "an array of strings");
        *success = 0;
        return 0;
    }

    for (int i = 0; i < toml_array_nelem(imports); i++) {
        toml_datum_t entry = toml_string_at(imports, i);
        if (!entry.ok) {
            fprintf(stderr, "Error: invalid 'import' entry in %s\n  item %d: expected a string\n", manifest_path, i + 1);
            *success = 0;
            return 0;
        }
        if (!process_import_in_holder(holder, entry.u.s, module_dir, success)) return 0;
    }
    return 1;
}

static int load_manifest_links(toml_table_t* manifest, const char* manifest_path, const char* module_dir) {
    toml_table_t* platform = toml_table_in(manifest, "platform");
    if (!platform) return 1;

    toml_table_t* current = toml_table_in(platform, MODULE_PLATFORM);
    if (!current) return 1;

    if (!toml_key_exists(current, "link")) return 1;

    toml_array_t* link = toml_array_in(current, "link");
    if (!link) {
        log_field_type_error(manifest_path, "link", "an array of strings");
        return 0;
    }

    for (int i = 0; i < toml_array_nelem(link); i++) {
        toml_datum_t entry = toml_string_at(link, i);
        if (!entry.ok) {
            fprintf(stderr, "Error: invalid 'link' entry in %s\n  item %d: expected a string\n", manifest_path, i + 1);
            return 0;
        }

        // A raw linker flag (e.g. "-lX11", "-framework Cocoa") is passed
        // through to the link command as-is instead of resolving to a file.
        if (entry.u.s[0] == '-') {
            add_module_link(entry.u.s);
            continue;
        }

        char* full_path = join_path(module_dir, entry.u.s);
        if (!full_path) return 0;

        if (!is_readable_file(full_path)) {
            char message[1024];
            snprintf(message, sizeof(message),"Error: link file not found: %s", full_path);
            log_error(message);
            return 0;
        }

        char canonical[PATH_MAX];
        add_module_link(
            canonicalize_path(full_path, canonical, sizeof(canonical))
                ? canonical
                : full_path
        );
    }
    return 1;
}

static int load_manifest(Ast* holder, const char* manifest_path, const char* module_dir, int* success) {
    char* context = NULL;
    if (!read_file(&context, manifest_path)) {
        fprintf(stderr, "Error: Failed to read %s\n", manifest_path);
        *success = 0;
        return 0;
    }

    char errbuf[256] = {0};
    toml_table_t* manifest = toml_parse(context, errbuf, sizeof(errbuf));
    if (!manifest) {
        fprintf(stderr, "Error: Failed to parse %s: %s\n", manifest_path, errbuf);
        *success = 0;
        return 0;
    }

    int ok = load_manifest_imports(holder, manifest, manifest_path, module_dir, success);
    if (load_manifest_links(manifest, manifest_path, module_dir)) {
        if (!ok) *success = 0;
    } else {
        ok = 0;
        *success = 0;
    }

    toml_free(manifest);
    return ok;
}

static int process_module_directory(Ast* holder, const char* import_path, const char* module_path, int* success) {
    if (is_module_loaded(module_path)) return 1;
    mark_module_loaded(module_path);

    char* manifest_path = join_path(module_path, MODULE_MANIFEST);
    if (!manifest_path) {
        fprintf(stderr, "Error: Out of memory during module import\n");
        *success = 0;
        return 0;
    }

    if (!is_readable_file(manifest_path)) {
        fprintf(stderr, "Error: Failed to import module '%s'\n  %s not found: %s\n", import_path, MODULE_MANIFEST, manifest_path);
        *success = 0;
        return 0;
    }

    int ok = load_manifest(holder, manifest_path, module_path, success);
    return ok;
}

static int process_import_in_holder(Ast* holder, const char* import_path, const char* dir, int* success) {
    char* name = module_file_name(import_path);
    if (!name) {
        fprintf(stderr, "Error: Out of memory during module import\n");
        return 0;
    }

    const char* dirs[MAX_SEARCH_DIRS];
    size_t dir_count = collect_search_dirs(dir, dirs);

    ModuleSearch found;
    char* full_path = find_module(import_path, name, dirs, dir_count, &found);

    if (!full_path) {
        if (found.manifest_less) {
            char* manifest_path = join_path(found.manifest_less, MODULE_MANIFEST);
            fprintf(stderr, "Error: Failed to import module '%s'\n  %s not found: %s\n",
                    import_path, MODULE_MANIFEST, manifest_path ? manifest_path : found.manifest_less);
        } else {
            log_search_failure(import_path, dirs, dir_count);
        }
        *success = 0;
        return 0;
    }

    char canonical[PATH_MAX];
    const char* module_path =
        canonicalize_path(full_path, canonical, sizeof(canonical))
            ? canonical
            : full_path;

    if (found.kind == MODULE_DIRECTORY) {
        int ok = process_module_directory(holder, import_path, module_path, success);
        return ok;
    }

    if (is_module_loaded(module_path)) {
        return 0;
    }

    char* context = NULL;
    if (!read_file(&context, full_path)) {
        fprintf(stderr, "Error: Failed to import module '%s'\n", full_path);
        *success = 0;
        return 0;
    }

    TokenInfo* token_infos = vector_create();
    tokenizer(&token_infos, context, retain_module_path(full_path));

    Ast* imported_ast = parser_tokens(token_infos);

    if (imported_ast) {
        load_module(imported_ast, module_path);
        
        Ast** stmts = imported_ast->block.statements;
        for (size_t i = 0; i < vector_size(stmts); i++) {
            Ast* stmt = stmts[i];
            if (!stmt) continue;
            if (stmt->type == AST_IMPORT_STATEMENT) continue;
            vector_add(&holder->block.statements, stmt);
        }
    }
    return 1;
}

static void load_module_block(Ast* holder, const char* dir, int* success) {
    
    for (size_t i = 0; i < vector_size(holder->block.statements); i++) {
        Ast* stmt = holder->block.statements[i];
        if (!stmt) continue;
        if (stmt->type == AST_IMPORT_STATEMENT) {
            const char* import_path = stmt->import_statement.path->literal_string.value;
            process_import_in_holder(holder, import_path, dir, success);
        }
    }
}

int load_module(Ast* node, const char* source_path) {
    if (!node || node->type != AST_ROOT) return 0;

    bool is_root = !entry_dir_set;
    if (is_root) {
        entry_dir_set = true;
        path_directory(source_path, -1, entry_dir);
    }

    mark_module_loaded(source_path);

    char dir[PATH_MAX];
    path_directory(source_path, -1, dir);

    int success = 1;
    load_module_block(node, dir, &success);

    if (is_root) {
        char* manifest_path = join_path(dir, MODULE_MANIFEST);
        if (manifest_path) {
            if (is_readable_file(manifest_path)) load_manifest(node, manifest_path, dir, &success);
        }
    }
    return success;
}
