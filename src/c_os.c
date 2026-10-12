#include "c_os.h"
#include "c_vector.h"

#if defined(__linux__)
#include <linux/stat.h>
#endif

#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

int read_file(char **str, const char *path) {
    if (!str) return 0;
    FILE *fptr = fopen(path, "rb");
    if (!fptr) return 0;
    fseek(fptr, 0, SEEK_END);
    long file_size = ftell(fptr);
    if (file_size < 0) {
        fclose(fptr);
        return 0;
    }
    fseek(fptr, 0, SEEK_SET);
    char *ptr = (char *)malloc((size_t)file_size + 1);
    if (!ptr) {
        fclose(fptr);
        return 0;
    }
    size_t bytes_read = fread(ptr, 1, (size_t)file_size, fptr);
    ptr[bytes_read] = '\0';
    fclose(fptr);
    *str = ptr;
    return 1;
}

static void fill_file_times(const char *fullpath, FileInfo *info) {
#if defined(__linux__)
    struct statx stx;
    if (statx(AT_FDCWD, fullpath, 0, STATX_BTIME | STATX_MTIME | STATX_ATIME, &stx) == 0) {
        info->modification_time = stx.stx_mtime.tv_sec + stx.stx_mtime.tv_nsec / 1e9;
        info->access_time       = stx.stx_atime.tv_sec + stx.stx_atime.tv_nsec / 1e9;
        if (stx.stx_mask & STATX_BTIME) {
            info->creation_time = stx.stx_btime.tv_sec + stx.stx_btime.tv_nsec / 1e9;
            return;
        }
    }
#endif
    struct stat st;
    if (stat(fullpath, &st) == 0) {
        if (info->creation_time == 0) info->creation_time = st.st_ctime;
        if (info->modification_time == 0) info->modification_time = st.st_mtime;
        if (info->access_time == 0) info->access_time = st.st_atime;
    }
}

int read_directory(FileInfo **vec, const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return 0;

    if (*vec == NULL) *vec = vector_create();
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        FileInfo info = {0};
        char fullpath[PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) info.type = DIRECTORY;
            else if (S_ISREG(st.st_mode)) info.type = REGULAR;
            else info.type = UNKNOWN;
        } else {
            info.type = UNKNOWN;
        }

        info.fullpath = strdup(fullpath);
        info.name = strdup(entry->d_name);

        fill_file_times(fullpath, &info);

        vector_add(vec, info);
    }
    closedir(dir);
    return 1;
}

int read_all_directory(FileInfo **vec, const char *path) {
    FileInfo *infos = NULL;
    if (*vec == NULL) *vec = vector_create();
    if (!read_directory(&infos, path)) {
        vector_free(infos);
        return 0;
    }

    for (size_t i = 0; i < vector_size(infos); i++) {
        FileInfo *info = &infos[i];
        vector_add(vec, *info);

        if (info->type == DIRECTORY) {
            FileInfo *sub_res = NULL;
            if (read_all_directory(&sub_res, info->fullpath)) {
                for (size_t j = 0; j < vector_size(sub_res); j++)
                    vector_add(vec, sub_res[j]);
            }
            vector_free(sub_res);
        }
    }
    vector_free(infos);
    return 1;
}

int write_file(const char *data, const char *path) {
    FILE *file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    if (fputs(data, file) == EOF) {
        fclose(file);
        return 0;
    }

    fclose(file);
    return 1;
}

int path_directory(const char *path, int level, char *buffer) {
    if (!path || !buffer) return 0;

    buffer[0] = '\0';

    size_t len = strlen(path);

    const char *end = strrchr(path, '/');

    if (!end) return 0;

    const char *pos = end;

    for (int i = -1; i > level; i--) {
        const char *p = pos - 1;
        while (p >= path && *p != '/')p--;
        if (p < path) {
            pos = path;
            break;
        }
        pos = p;
    }

    size_t size = (size_t)(pos - path + 1);

    memcpy(buffer, path, size);
    buffer[size] = '\0';
    return 1;
}

int read_line(char **str, const char *path, size_t row) {
    FILE *file = fopen(path, "r");
    if (!file)
        return 0;

    char line[4096];
    size_t current_row = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        current_row++;

        if (current_row == row) {
            size_t len = strlen(line);
            while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
                line[--len] = '\0';
            }

            *str = malloc(len + 1);
            if (!*str) {
                fclose(file);
                return 0;
            }
            memcpy(*str, line, len + 1);
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0;
}
