#pragma once

typedef enum {
    UNKNOWN,
    REGULAR,
    DIRECTORY,
} FileType;

typedef struct {
    char *fullpath;
    char *name;
    FileType type;
    double creation_time;
    double modification_time;
    double access_time;
} FileInfo;


int read_line(char **str, const char *path, size_t row);
int read_file(char **str, const char *path);
int read_directory(FileInfo **vec, const char *path);
int read_all_directory(FileInfo **vec, const char *path);
int write_file(const char* data, const char *path);

int path_directory(const char *path, int level, char *buffer);
