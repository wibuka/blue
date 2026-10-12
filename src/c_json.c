#include "c_json.h"
/* Use system malloc for cJSON to avoid Boehm GC interaction with c_vector interior pointers */
#ifdef malloc
#undef malloc
#endif
#ifdef realloc
#undef realloc
#endif
#ifdef free
#undef free
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>

static const char *ep = NULL;

const char *cJSON_GetErrorPtr(void) { return ep; }

static cJSON *cJSON_New_Item(void) {
    cJSON *node = (cJSON*)malloc(sizeof(cJSON));
    if (node) memset(node, 0, sizeof(cJSON));
    return node;
}

static char *cJSON_strdup(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char *copy = (char*)malloc(len);
    if (!copy) return NULL;
    memcpy(copy, str, len);
    return copy;
}

/* Creation */
cJSON *cJSON_CreateNull(void) {
    cJSON *item = cJSON_New_Item();
    if (item) item->type = cJSON_NULL;
    return item;
}
cJSON *cJSON_CreateTrue(void) {
    cJSON *item = cJSON_New_Item();
    if (item) item->type = cJSON_True;
    return item;
}
cJSON *cJSON_CreateFalse(void) {
    cJSON *item = cJSON_New_Item();
    if (item) item->type = cJSON_False;
    return item;
}
cJSON *cJSON_CreateBool(bool b) { return b ? cJSON_CreateTrue() : cJSON_CreateFalse(); }
cJSON *cJSON_CreateNumber(double num) {
    cJSON *item = cJSON_New_Item();
    if (item) {
        item->type = cJSON_Number;
        item->valuedouble = num;
        item->valueint = (int)num;
    }
    return item;
}
cJSON *cJSON_CreateString(const char *string) {
    cJSON *item = cJSON_New_Item();
    if (item) {
        item->type = cJSON_String;
        item->valuestring = cJSON_strdup(string);
    }
    return item;
}
cJSON *cJSON_CreateRaw(const char *raw) {
    cJSON *item = cJSON_New_Item();
    if (item) {
        item->type = cJSON_Raw;
        item->valuestring = cJSON_strdup(raw);
    }
    return item;
}
cJSON *cJSON_CreateArray(void) {
    cJSON *item = cJSON_New_Item();
    if (item) item->type = cJSON_Array;
    return item;
}
cJSON *cJSON_CreateObject(void) {
    cJSON *item = cJSON_New_Item();
    if (item) item->type = cJSON_Object;
    return item;
}

/* Add helpers */
static void suffix_object(cJSON *prev, cJSON *item) {
    prev->next = item;
    item->prev = prev;
}

void cJSON_AddItemToArray(cJSON *array, cJSON *item) {
    if (!array || !item) return;
    if (!array->child) {
        array->child = item;
    } else {
        cJSON *c = array->child;
        while (c->next) c = c->next;
        suffix_object(c, item);
    }
}
void cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item) {
    if (!object || !string || !item) return;
    if (item->string) free(item->string);
    item->string = cJSON_strdup(string);
    cJSON_AddItemToArray(object, item);
}
void cJSON_AddItemToObjectCS(cJSON *object, const char *string, cJSON *item) {
    cJSON_AddItemToObject(object, string, item);
}
void cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item) {
    if (item) item->type |= cJSON_IsReference;
    cJSON_AddItemToArray(array, item);
}
void cJSON_AddItemReferenceToObject(cJSON *object, const char *string, cJSON *item) {
    if (item) item->type |= cJSON_IsReference;
    cJSON_AddItemToObject(object, string, item);
}

/* Delete */
void cJSON_Delete(cJSON *item) {
    if (!item) return;
    cJSON *next;
    for (cJSON *c = item->child; c; ) {
        next = c->next;
        cJSON_Delete(c);
        c = next;
    }
    if (!(item->type & cJSON_IsReference) && item->valuestring) free(item->valuestring);
    if (item->string) free(item->string);
    free(item);
}

cJSON *cJSON_DetachItemViaPointer(cJSON *parent, cJSON * const item) {
    if (!parent || !item) return NULL;
    if (item->prev) item->prev->next = item->next;
    if (item->next) item->next->prev = item->prev;
    if (item == parent->child) parent->child = item->next;
    item->prev = item->next = NULL;
    return item;
}
void cJSON_DeleteItemFromArray(cJSON *array, int which) {
    cJSON *c = cJSON_GetArrayItem(array, which);
    if (c) { cJSON_DetachItemViaPointer(array, c); cJSON_Delete(c); }
}
void cJSON_DeleteItemFromObject(cJSON *object, const char *string) {
    cJSON_DeleteItemFromObjectCaseSensitive(object, string);
}
void cJSON_DeleteItemFromObjectCaseSensitive(cJSON *object, const char *string) {
    cJSON *c = cJSON_GetObjectItemCaseSensitive(object, string);
    if (c) { cJSON_DetachItemViaPointer(object, c); cJSON_Delete(c); }
}

/* Replace */
void cJSON_ReplaceItemViaPointer(cJSON *parent, cJSON *item, cJSON *replacement) {
    if (!parent || !item || !replacement) return;
    replacement->next = item->next;
    replacement->prev = item->prev;
    if (replacement->next) replacement->next->prev = replacement;
    if (replacement->prev) replacement->prev->next = replacement;
    if (parent->child == item) parent->child = replacement;
    item->next = item->prev = NULL;
    cJSON_Delete(item);
}
void cJSON_ReplaceItemInArray(cJSON *array, int which, cJSON *newitem) {
    cJSON *c = cJSON_GetArrayItem(array, which);
    if (c) cJSON_ReplaceItemViaPointer(array, c, newitem);
}
void cJSON_ReplaceItemInObject(cJSON *object, const char *string, cJSON *newitem) {
    cJSON *c = cJSON_GetObjectItemCaseSensitive(object, string);
    if (c) {
        newitem->string = cJSON_strdup(string);
        cJSON_ReplaceItemViaPointer(object, c, newitem);
    }
}

/* Duplicate */
cJSON *cJSON_Duplicate(const cJSON *item, bool recurse) {
    if (!item) return NULL;
    cJSON *newitem = cJSON_New_Item();
    if (!newitem) return NULL;
    newitem->type = item->type & (~cJSON_IsReference);
    newitem->valueint = item->valueint;
    newitem->valuedouble = item->valuedouble;
    if (item->valuestring) newitem->valuestring = cJSON_strdup(item->valuestring);
    if (item->string) newitem->string = cJSON_strdup(item->string);
    if (recurse && item->child) {
        cJSON *child = item->child;
        cJSON *prev = NULL;
        while (child) {
            cJSON *dup = cJSON_Duplicate(child, true);
            if (!dup) { cJSON_Delete(newitem); return NULL; }
            if (!newitem->child) newitem->child = dup;
            else { prev->next = dup; dup->prev = prev; }
            prev = dup;
            child = child->next;
        }
    }
    return newitem;
}

/* Get */
int cJSON_GetArraySize(const cJSON *array) {
    if (!array || !(array->type & cJSON_Array)) return 0;
    int size = 0;
    for (cJSON *c = array->child; c; c = c->next) size++;
    return size;
}
cJSON *cJSON_GetArrayItem(const cJSON *array, int index) {
    if (!array) return NULL;
    cJSON *c = array->child;
    while (c && index > 0) { index--; c = c->next; }
    return c;
}
cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON * const object, const char * const string) {
    if (!object || !(object->type & cJSON_Object) || !string) return NULL;
    for (cJSON *c = object->child; c; c = c->next) {
        if (c->string && strcmp(c->string, string) == 0) return c;
    }
    return NULL;
}
cJSON *cJSON_GetObjectItem(const cJSON * const object, const char * const string) {
    return cJSON_GetObjectItemCaseSensitive(object, string);
}
bool cJSON_HasObjectItem(const cJSON *object, const char *string) {
    return cJSON_GetObjectItem(object, string) != NULL;
}

/* Type checks */
bool cJSON_IsInvalid(const cJSON * const item) { return item && (item->type & 0xFF) == cJSON_Invalid; }
bool cJSON_IsFalse(const cJSON * const item) { return item && (item->type & 0xFF) == cJSON_False; }
bool cJSON_IsTrue(const cJSON * const item) { return item && (item->type & 0xFF) == cJSON_True; }
bool cJSON_IsBool(const cJSON * const item) { return item && (item->type & (cJSON_True | cJSON_False)) != 0; }
bool cJSON_IsNull(const cJSON * const item) { return item && (item->type & 0xFF) == cJSON_NULL; }
bool cJSON_IsNumber(const cJSON * const item) { return item && (item->type & 0xFF) == cJSON_Number; }
bool cJSON_IsString(const cJSON * const item) { return item && (item->type & 0xFF) == cJSON_String; }
bool cJSON_IsArray(const cJSON * const item) { return item && (item->type & 0xFF) == cJSON_Array; }
bool cJSON_IsObject(const cJSON * const item) { return item && (item->type & 0xFF) == cJSON_Object; }
bool cJSON_IsRaw(const cJSON * const item) { return item && (item->type & 0xFF) == cJSON_Raw; }

void cJSON_SetNumberValue(cJSON *item, double number) {
    if (!item) return;
    item->valuedouble = number;
    item->valueint = (int)number;
}
void cJSON_SetValuestring(cJSON *item, const char *valuestring) {
    if (!item) return;
    if (item->valuestring) free(item->valuestring);
    item->valuestring = cJSON_strdup(valuestring);
}

/* Print */
typedef struct {
    char *buffer;
    size_t length;
    size_t offset;
    bool fmt;
    int depth;
} printbuffer;

static void ensure(printbuffer *p, size_t need) {
    if (p->offset + need + 1 >= p->length) {
        size_t new_len = p->length ? p->length * 2 : 256;
        while (new_len < p->offset + need + 1) new_len *= 2;
        char *new_buf = (char*)realloc(p->buffer, new_len);
        if (!new_buf) return;
        p->buffer = new_buf;
        p->length = new_len;
    }
}
static void pb_append(printbuffer *p, const char *str, size_t len) {
    if (!str || !p) return;
    ensure(p, len);
    if (!p->buffer) return;
    memcpy(p->buffer + p->offset, str, len);
    p->offset += len;
    p->buffer[p->offset] = '\0';
}
static void pb_append_str(printbuffer *p, const char *str) {
    if (str) pb_append(p, str, strlen(str));
}
static void pb_append_char(printbuffer *p, char c) {
    ensure(p, 1);
    if (!p->buffer) return;
    p->buffer[p->offset++] = c;
    p->buffer[p->offset] = '\0';
}
static void pb_indent(printbuffer *p) {
    if (!p->fmt) return;
    pb_append_char(p, '\n');
    for (int i = 0; i < p->depth; i++) pb_append_str(p, "  ");
}

static void print_string(const char *str, printbuffer *p) {
    pb_append_char(p, '"');
    if (!str) { pb_append_char(p, '"'); return; }
    for (const char *c = str; *c; c++) {
        switch (*c) {
            case '"': pb_append_str(p, "\\\""); break;
            case '\\': pb_append_str(p, "\\\\"); break;
            case '\b': pb_append_str(p, "\\b"); break;
            case '\f': pb_append_str(p, "\\f"); break;
            case '\n': pb_append_str(p, "\\n"); break;
            case '\r': pb_append_str(p, "\\r"); break;
            case '\t': pb_append_str(p, "\\t"); break;
            default:
                if ((unsigned char)*c < 32) {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned int)*c);
                    pb_append_str(p, buf);
                } else {
                    pb_append_char(p, *c);
                }
                break;
        }
    }
    pb_append_char(p, '"');
}

static void print_value(const cJSON *item, printbuffer *p);

static void print_array(const cJSON *item, printbuffer *p) {
    pb_append_char(p, '[');
    if (item->child) {
        p->depth++;
        bool first = true;
        for (cJSON *c = item->child; c; c = c->next) {
            if (!first) pb_append_char(p, ',');
            if (p->fmt) {
                pb_append_char(p, '\n');
                for (int i = 0; i < p->depth; i++) pb_append_str(p, "  ");
            }
            print_value(c, p);
            first = false;
        }
        p->depth--;
        if (p->fmt) {
            pb_append_char(p, '\n');
            for (int i = 0; i < p->depth; i++) pb_append_str(p, "  ");
        }
    }
    pb_append_char(p, ']');
}

static void print_object(const cJSON *item, printbuffer *p) {
    pb_append_char(p, '{');
    if (item->child) {
        p->depth++;
        bool first = true;
        for (cJSON *c = item->child; c; c = c->next) {
            if (!first) pb_append_char(p, ',');
            if (p->fmt) {
                pb_append_char(p, '\n');
                for (int i = 0; i < p->depth; i++) pb_append_str(p, "  ");
            }
            print_string(c->string ? c->string : "", p);
            pb_append_char(p, ':');
            if (p->fmt) pb_append_char(p, ' ');
            print_value(c, p);
            first = false;
        }
        p->depth--;
        if (p->fmt) {
            pb_append_char(p, '\n');
            for (int i = 0; i < p->depth; i++) pb_append_str(p, "  ");
        }
    }
    pb_append_char(p, '}');
}

static void print_value(const cJSON *item, printbuffer *p) {
    if (!item) { pb_append_str(p, "null"); return; }
    switch (item->type & 0xFF) {
        case cJSON_NULL: pb_append_str(p, "null"); break;
        case cJSON_False: pb_append_str(p, "false"); break;
        case cJSON_True: pb_append_str(p, "true"); break;
        case cJSON_Number: {
            char buf[64];
            double d = item->valuedouble;
            // use enough precision, remove trailing zeros if needed
            if (fabs(d) < 1e-6 || fabs(d) > 1e9) {
                snprintf(buf, sizeof(buf), "%.17g", d);
            } else {
                snprintf(buf, sizeof(buf), "%.15g", d);
            }
            // ensure .0 for integers to stay numbers? not needed
            pb_append_str(p, buf);
            break;
        }
        case cJSON_String:
        case cJSON_Raw:
            if (item->type & cJSON_Raw) pb_append_str(p, item->valuestring ? item->valuestring : "");
            else print_string(item->valuestring ? item->valuestring : "", p);
            break;
        case cJSON_Array:
            print_array(item, p);
            break;
        case cJSON_Object:
            print_object(item, p);
            break;
        default: pb_append_str(p, "null"); break;
    }
}

static char *print(const cJSON *item, bool fmt) {
    printbuffer p = {0};
    p.length = 1024;
    p.buffer = (char*)malloc(p.length);
    if (!p.buffer) return NULL;
    p.buffer[0] = '\0';
    p.fmt = fmt;
    p.depth = 0;
    print_value(item, &p);
    return p.buffer;
}

char *cJSON_Print(const cJSON *item) { return print(item, true); }
char *cJSON_PrintUnformatted(const cJSON *item) { return print(item, false); }
char *cJSON_PrintBuffered(const cJSON *item, int prebuffer, bool fmt) {
    (void)prebuffer;
    return print(item, fmt);
}
int cJSON_PrintToFile(const cJSON *item, const char *path, bool fmt) {
    if (!item || !path) return 0;
    char *out = print(item, fmt);
    if (!out) return 0;
    FILE *f = fopen(path, "w");
    if (!f) { free(out); return 0; }
    fputs(out, f);
    fclose(f);
    free(out);
    return 1;
}

/* Convenience creators for arrays */
cJSON *cJSON_CreateIntArray(const int *numbers, int count) {
    cJSON *a = cJSON_CreateArray();
    for (int i=0;i<count;i++) cJSON_AddItemToArray(a, cJSON_CreateNumber(numbers[i]));
    return a;
}
cJSON *cJSON_CreateFloatArray(const float *numbers, int count) {
    cJSON *a = cJSON_CreateArray();
    for (int i=0;i<count;i++) cJSON_AddItemToArray(a, cJSON_CreateNumber(numbers[i]));
    return a;
}
cJSON *cJSON_CreateDoubleArray(const double *numbers, int count) {
    cJSON *a = cJSON_CreateArray();
    for (int i=0;i<count;i++) cJSON_AddItemToArray(a, cJSON_CreateNumber(numbers[i]));
    return a;
}
cJSON *cJSON_CreateStringArray(const char **strings, int count) {
    cJSON *a = cJSON_CreateArray();
    for (int i=0;i<count;i++) cJSON_AddItemToArray(a, cJSON_CreateString(strings[i]));
    return a;
}
