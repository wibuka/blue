#include "c_string.h"

int str_contains(char *src, char *dest)
{
    if (src == NULL || dest == NULL) return 0;
    if (*dest == '\0') return 0;
    return (strstr(src, dest) != NULL) ? 1 : 0;
}

int str_match(const char *str1,const char *str2){
    if (strcmp(str1, str2) == 0) {
        return 1;
    }
    return 0;
}

int str_ends_with(char *src, char *suffix)
{
    if (src == NULL || suffix == NULL) return 0;
    size_t src_len = strlen(src);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > src_len) return 0;
    return strcmp(src + src_len - suffix_len, suffix) == 0;
}

char* str_cut(const char* str, size_t start, size_t end) {
    if (str == NULL) return NULL;
    size_t len = strlen(str);
    if (end > len || start > end) return NULL;
    size_t cut_len = end - start;
    char* result = (char*)malloc(cut_len + 1);
    if (result == NULL) return NULL;
    memcpy(result, str + start, cut_len);
    result[cut_len] = '\0';
    return result;
}
