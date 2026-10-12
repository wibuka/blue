#pragma once

typedef struct Ast Ast;

extern const char* TYPE_U0_STRING;
extern const char* TYPE_U8_STRING;
extern const char* TYPE_U16_STRING;
extern const char* TYPE_U32_STRING;
extern const char* TYPE_U64_STRING;
extern const char* TYPE_U128_STRING;

extern const char* TYPE_I8_STRING;
extern const char* TYPE_I16_STRING;
extern const char* TYPE_I32_STRING;
extern const char* TYPE_I64_STRING;
extern const char* TYPE_I128_STRING;

extern const char* TYPE_F32_STRING;
extern const char* TYPE_F64_STRING;

extern const char* TYPE_USIZE_STRING;
extern const char* TYPE_BOOL_STRING;

extern const char* TYPE_NIL_STRING;

extern const char* TYPE_TYPEDEF_STRING;

extern const char* TYPE_FUNCTION_STRING;

typedef enum {
    TYPEINV = 0,
    U0      = 1,
    U8      = 2,
    U16     = 3,
    U32     = 4,
    U64     = 5,
    U128    = 6,
    I8      = 8,
    I16     = 9,
    I32     = 10,
    I64     = 11,
    I128    = 12,
    F32     = 13,
    F64     = 14,
    USIZE   = 15,
    BOOL    = 16,
    NIL     = 17,
    ARRAY   = 18,
    SLICE   = 19,
    TYPEDEF = 20,
    TYPEVAR = 21,
    FUNCTION = 22,
} TypeBase;


typedef struct StructDef StructDef;
typedef struct StructField StructField;
typedef struct Scope Scope;
typedef struct TypeData TypeData;

typedef struct TypeData {
    TypeBase    base;
    uint8_t     pointer_depth;
    const char* identity;
    StructDef*  struct_def;
    union {
        struct {
            size_t length;
            struct TypeData* element_type;
        } array;

        struct {
            struct TypeData* element_type;
        } slice;

        struct {
            struct TypeData** param_types;
            size_t            param_count;
            struct TypeData*  return_type;
        } function;
    };
} TypeData;

typedef struct StructField {
    const char* name;
    TypeData*   type;
    size_t      index;
} StructField;

typedef struct StructDef {
    const char*   name;
    struct Ast*   owner;
    StructField*  fields;
    size_t        field_count;
} StructDef;

typedef struct Scope {
    Ast** nodes;
    struct Scope* parent;
} Scope;

extern const TypeData TYPE_U0_DATA;
extern const TypeData TYPE_U8_DATA;
extern const TypeData TYPE_U16_DATA;
extern const TypeData TYPE_U32_DATA;
extern const TypeData TYPE_U64_DATA;
extern const TypeData TYPE_U128_DATA;

extern const TypeData TYPE_I8_DATA;
extern const TypeData TYPE_I16_DATA;
extern const TypeData TYPE_I32_DATA;
extern const TypeData TYPE_I64_DATA;
extern const TypeData TYPE_I128_DATA;

extern const TypeData TYPE_F32_DATA;
extern const TypeData TYPE_F64_DATA;

extern const TypeData TYPE_USIZE_DATA;
extern const TypeData TYPE_BOOL_DATA;

extern const TypeData TYPEINV_DATA;

static inline int is_numeric_base(TypeBase tb) {
    return tb == U8  || tb == U16 || tb == U32 || tb == U64 || tb == U128 ||
           tb == I8  || tb == I16 || tb == I32 || tb == I64 || tb == I128 ||
           tb == F32 || tb == F64 || tb == USIZE;
}

static inline int is_integer_base(TypeBase tb) {
    return tb == U8 || tb == U16 || tb == U32 || tb == U64 || tb == U128 ||
           tb == I8 || tb == I16 || tb == I32 || tb == I64 || tb == I128 ||
           tb == USIZE;
}

static inline int is_unsigned_base(TypeBase tb) {
    return tb == U8 || tb == U16 || tb == U32 || tb == U64 || tb == U128 || tb == USIZE;
}

static inline int is_signed_base(TypeBase tb) {
    return tb == I8 || tb == I16 || tb == I32 || tb == I64 || tb == I128;
}

static inline int TypeBase_bit_width(TypeBase tb) {
    switch (tb) {
        case U8:
        case I8:  return 8;
        case U16:
        case I16: return 16;
        case U32:
        case I32: return 32;
        case U64:
        case I64: return 64;
        case USIZE: return get_bitness();
        case U128:
        case I128: return 128;
        default: return 0;
    }
}

static inline int is_float_base(TypeBase tb) {
    return tb == F32 || tb == F64;
}

static inline int is_bool_base(TypeBase tb) {
    return tb == BOOL;
}

static inline int is_primitive_base(TypeBase tb) {
    switch (tb) {
        case TYPEDEF:
        case FUNCTION:
            return 0;
        default:
            return 1;
    }
}

static inline int is_primitive_type(TypeData* td) {
    if (!td || td->base == TYPEDEF) return 0;
    return 1;
}

static inline bool is_numeric_type(TypeData* td) {
    return td != NULL && td->pointer_depth == 0 && is_numeric_base(td->base);
}

static inline bool is_integer_type(TypeData* td) {
    return td != NULL && td->pointer_depth == 0 && is_integer_base(td->base);
}

static inline bool is_unsigned_type(TypeData* td) {
    return td != NULL && td->pointer_depth == 0 && is_unsigned_base(td->base);
}

static inline bool is_signed_type(TypeData* td) {
    return td != NULL && td->pointer_depth == 0 && is_signed_base(td->base);
}

static inline bool is_float_type(TypeData* td) {
    return td != NULL && td->pointer_depth == 0 && is_float_base(td->base);
}

static inline bool is_boolean_type(TypeData* td) {
    return td != NULL && td->pointer_depth == 0 && is_bool_base(td->base);
}

static inline bool is_pointer_type(TypeData* td) {
    return td != NULL && td->pointer_depth > 0;
}

static inline bool is_void_type(TypeData* td) {
    return td != NULL && td->pointer_depth == 0 && td->base == U0;
}

static inline bool is_void_pointer(TypeData* td) {
    return td != NULL && td->pointer_depth > 0 && td->base == U0;
}

static inline bool is_polymorphic(TypeData* td) {
    return td != NULL && td->base == TYPEVAR;
}

static inline bool is_function_type(TypeData* td) {
    return td != NULL && td->pointer_depth == 0 && td->base == FUNCTION;
}

int has_TypeBase(const char* str);
int TypeData_match(TypeData* a, TypeData* b);

const char* TypeData_to_string(TypeData* td);
const char* typeBase_to_string(TypeBase base);

TypeData* TypeData_of(Ast* decl);

TypeData* TypeData_primitive(TypeBase base);
TypeData* TypeData_typeinv(void);
TypeData* TypeData_pointer(TypeData* base);
TypeData* TypeData_retype(TypeData* src, uint8_t pointer_depth);
TypeData* TypeData_array(TypeData* element_type, size_t length);
TypeData* TypeData_slice(TypeData* element_type);
TypeData* TypeData_function(TypeData** param_types, size_t param_count, TypeData* return_type);
TypeData* TypeData_intern(TypeBase base, uint8_t pointer_depth, const char* identity, StructDef* struct_def);

StructDef* register_struct_skeleton(Ast* node);
StructDef* register_struct_skeleton_qualified(Ast* node, const char* qualified_name);
void fill_struct_fields(StructDef* def, Ast* node);
TypeBase find_TypeBase(const char* str);

