#pragma once
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* cJSON type flags */
#define cJSON_Invalid 0
#define cJSON_False  (1 << 0)
#define cJSON_True   (1 << 1)
#define cJSON_NULL   (1 << 2)
#define cJSON_Number (1 << 3)
#define cJSON_String (1 << 4)
#define cJSON_Array  (1 << 5)
#define cJSON_Object (1 << 6)
#define cJSON_Raw    (1 << 7)

#define cJSON_IsReference 256
#define cJSON_StringIsConst 512

typedef struct cJSON {
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;

    int type;

    char *valuestring;
    int valueint;
    double valuedouble;

    char *string; /* key for object members */
} cJSON;

/* Creation */
cJSON *cJSON_CreateNull(void);
cJSON *cJSON_CreateTrue(void);
cJSON *cJSON_CreateFalse(void);
cJSON *cJSON_CreateBool(bool b);
cJSON *cJSON_CreateNumber(double num);
cJSON *cJSON_CreateString(const char *string);
cJSON *cJSON_CreateRaw(const char *raw);
cJSON *cJSON_CreateArray(void);
cJSON *cJSON_CreateObject(void);
cJSON *cJSON_CreateIntArray(const int *numbers, int count);
cJSON *cJSON_CreateFloatArray(const float *numbers, int count);
cJSON *cJSON_CreateDoubleArray(const double *numbers, int count);
cJSON *cJSON_CreateStringArray(const char **strings, int count);

/* Add item */
void cJSON_AddItemToArray(cJSON *array, cJSON *item);
void cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item);
void cJSON_AddItemToObjectCS(cJSON *object, const char *string, cJSON *item);
void cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item);
void cJSON_AddItemReferenceToObject(cJSON *object, const char *string, cJSON *item);

#define cJSON_AddNullToObject(object, name) cJSON_AddItemToObject(object, name, cJSON_CreateNull())
#define cJSON_AddTrueToObject(object, name) cJSON_AddItemToObject(object, name, cJSON_CreateTrue())
#define cJSON_AddFalseToObject(object, name) cJSON_AddItemToObject(object, name, cJSON_CreateFalse())
#define cJSON_AddBoolToObject(object, name, b) cJSON_AddItemToObject(object, name, cJSON_CreateBool(b))
#define cJSON_AddNumberToObject(object, name, n) cJSON_AddItemToObject(object, name, cJSON_CreateNumber(n))
#define cJSON_AddStringToObject(object, name, s) cJSON_AddItemToObject(object, name, cJSON_CreateString(s))
#define cJSON_AddRawToObject(object, name, s) cJSON_AddItemToObject(object, name, cJSON_CreateRaw(s))

/* Helpers to detach / delete */
cJSON *cJSON_DetachItemViaPointer(cJSON *parent, cJSON * const item);
void cJSON_DeleteItemFromArray(cJSON *array, int which);
void cJSON_DeleteItemFromObject(cJSON *object, const char *string);
void cJSON_DeleteItemFromObjectCaseSensitive(cJSON *object, const char *string);

/* Update */
void cJSON_ReplaceItemInArray(cJSON *array, int which, cJSON *newitem);
void cJSON_ReplaceItemViaPointer(cJSON *parent, cJSON *item, cJSON *replacement);
void cJSON_ReplaceItemInObject(cJSON *object, const char *string, cJSON *newitem);

/* Duplicate */
cJSON *cJSON_Duplicate(const cJSON *item, bool recurse);

/* Parse / Print */
char *cJSON_Print(const cJSON *item);
char *cJSON_PrintUnformatted(const cJSON *item);
char *cJSON_PrintBuffered(const cJSON *item, int prebuffer, bool fmt);
int cJSON_PrintToFile(const cJSON *item, const char *path, bool fmt);

/* Delete */
void cJSON_Delete(cJSON *item);

/* Get */
int cJSON_GetArraySize(const cJSON *array);
cJSON *cJSON_GetArrayItem(const cJSON *array, int index);
cJSON *cJSON_GetObjectItem(const cJSON * const object, const char * const string);
cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON * const object, const char * const string);
bool cJSON_HasObjectItem(const cJSON *object, const char *string);

/* Type checks */
bool cJSON_IsInvalid(const cJSON * const item);
bool cJSON_IsFalse(const cJSON * const item);
bool cJSON_IsTrue(const cJSON * const item);
bool cJSON_IsBool(const cJSON * const item);
bool cJSON_IsNull(const cJSON * const item);
bool cJSON_IsNumber(const cJSON * const item);
bool cJSON_IsString(const cJSON * const item);
bool cJSON_IsArray(const cJSON * const item);
bool cJSON_IsObject(const cJSON * const item);
bool cJSON_IsRaw(const cJSON * const item);

const char *cJSON_GetErrorPtr(void);
void cJSON_SetNumberValue(cJSON *item, double number);
void cJSON_SetValuestring(cJSON *item, const char *valuestring);

#ifdef __cplusplus
}
#endif
