#include "type.h"

const char* TYPE_U0_STRING     = "u0";
const char* TYPE_U8_STRING     = "u8";
const char* TYPE_U16_STRING    = "u16";
const char* TYPE_U32_STRING    = "u32";
const char* TYPE_U64_STRING    = "u64";
const char* TYPE_U128_STRING   = "u128";
const char* TYPE_I8_STRING     = "i8";
const char* TYPE_I16_STRING    = "i16";
const char* TYPE_I32_STRING    = "i32";
const char* TYPE_I64_STRING    = "i64";
const char* TYPE_I128_STRING   = "i128";
const char* TYPE_F32_STRING    = "f32";
const char* TYPE_F64_STRING    = "f64";
const char* TYPE_USIZE_STRING  = "usize";
const char* TYPE_BOOL_STRING   = "bool";
const char* TYPE_NIL_STRING    = "nil";
const char* TYPE_TYPEDEF_STRING = "typedef";
const char* TYPE_FUNCTION_STRING = "def";

const TypeData TYPE_U0_DATA    = { U0,    0, "u0",    NULL };
const TypeData TYPE_U8_DATA    = { U8,    0, "u8",    NULL };
const TypeData TYPE_U16_DATA   = { U16,   0, "u16",   NULL };
const TypeData TYPE_U32_DATA   = { U32,   0, "u32",   NULL };
const TypeData TYPE_U64_DATA   = { U64,   0, "u64",   NULL };
const TypeData TYPE_U128_DATA  = { U128,  0, "u128",  NULL };
const TypeData TYPE_I8_DATA    = { I8,    0, "i8",    NULL };
const TypeData TYPE_I16_DATA   = { I16,   0, "i16",   NULL };
const TypeData TYPE_I32_DATA   = { I32,   0, "i32",   NULL };
const TypeData TYPE_I64_DATA   = { I64,   0, "i64",   NULL };
const TypeData TYPE_I128_DATA  = { I128,  0, "i128",  NULL };
const TypeData TYPE_F32_DATA   = { F32,   0, "f32",   NULL };
const TypeData TYPE_F64_DATA   = { F64,   0, "f64",   NULL };
const TypeData TYPE_USIZE_DATA = { USIZE, 0, "usize", NULL };
const TypeData TYPE_BOOL_DATA  = { BOOL,  0, "bool",  NULL };
const TypeData TYPEINV_DATA    = { TYPEINV, 0, NULL,  NULL };

static TypeData** type_registry = NULL;
StructDef** struct_defs = NULL;


static int type_identity_equal(const TypeData* a, const TypeData* b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->pointer_depth != b->pointer_depth || a->base != b->base) return 0;

    switch (a->base) {
        case ARRAY:
            if (a->array.length != b->array.length) return 0;
            return type_identity_equal(a->array.element_type, b->array.element_type);
        case SLICE:
            return type_identity_equal(a->slice.element_type, b->slice.element_type);
        case FUNCTION:
            if (a->function.param_count != b->function.param_count) return 0;
            if (!type_identity_equal(a->function.return_type, b->function.return_type)) return 0;
            for (size_t i = 0; i < a->function.param_count; i++) {
                if (!type_identity_equal(a->function.param_types[i], b->function.param_types[i])) {
                    return 0;
                }
            }
            return 1;
        case TYPEDEF:
            return a->struct_def == b->struct_def;
        default:
            return 1;
    }
}

static TypeData* type_intern(TypeData value) {
    if (!type_registry) {
        type_registry = vector_create();
    }
    
    size_t count = vector_size(type_registry);
    for (size_t i = 0; i < count; i++) {
        if (type_identity_equal(type_registry[i], &value)) {
            return type_registry[i];
        }
    }

    TypeData* td = c_alloc(sizeof(TypeData));
    assert(td && "Out of memory for TypeData allocation");
    *td = value;

    if (td->base == ARRAY && td->array.element_type) {
        td->array.element_type = type_intern(*td->array.element_type);
    } else if (td->base == SLICE && td->slice.element_type) {
        td->slice.element_type = type_intern(*td->slice.element_type);
    } else if (td->base == FUNCTION) {
        if (td->function.return_type) {
            td->function.return_type = type_intern(*td->function.return_type);
        }
        if (td->function.param_types) {
            for (size_t i = 0; i < td->function.param_count; i++) {
                td->function.param_types[i] = type_intern(*td->function.param_types[i]);
            }
        }
    }

    vector_add(&type_registry, td);
    return td;
}


TypeData* TypeData_intern(TypeBase base, uint8_t pointer_depth, const char* identity, StructDef* struct_def) {
    TypeData value = {
        .base = base,
        .pointer_depth = pointer_depth,
        .identity = identity,
        .struct_def = struct_def
    };
    return type_intern(value);
}

TypeData* TypeData_primitive(TypeBase base) {
    return TypeData_intern(base, 0, typeBase_to_string(base), NULL);
}

TypeData* TypeData_typeinv(void) {
    return (TypeData*)(&TYPEINV_DATA);
}

TypeData* TypeData_pointer(TypeData* base) {
    if (!base) return TypeData_intern(U0, 1, NULL, NULL);

    TypeData value = *base;
    value.pointer_depth++;
    return type_intern(value);
}

TypeData* TypeData_retype(TypeData* src, uint8_t pointer_depth) {
    if (!src) return NULL;
    TypeData value = *src;
    value.pointer_depth = pointer_depth;
    return type_intern(value);
}

TypeData* TypeData_array(TypeData* element_type, size_t length) {
    TypeData val = {
        .base = ARRAY,
        .pointer_depth = 0,
        .identity = NULL,
        .struct_def = NULL,
        .array = {
            .element_type = element_type,
            .length = length
        }
    };
    return type_intern(val);
}

TypeData* TypeData_slice(TypeData* element_type) {
    TypeData val = {
        .base = SLICE,
        .pointer_depth = 0,
        .identity = NULL,
        .struct_def = NULL,
        .slice = {
            .element_type = element_type
        }
    };
    return type_intern(val);
}

TypeData* TypeData_function(TypeData** param_types, size_t param_count, TypeData* return_type) {
    if (!return_type) return_type = (TypeData*)(&TYPE_U0_DATA);
    TypeData val = {
        .base = FUNCTION,
        .pointer_depth = 0,
        .identity = NULL,
        .struct_def = NULL,
        .function = {
            .param_types  = param_types,
            .param_count  = param_count,
            .return_type  = return_type
        }
    };
    return type_intern(val);
}

TypeBase find_TypeBase(const char* str) {
    assert(str != NULL);
    if (strcmp(str, TYPE_U0_STRING)    == 0) return U0;
    if (strcmp(str, TYPE_U8_STRING)    == 0) return U8;
    if (strcmp(str, TYPE_U16_STRING)   == 0) return U16;
    if (strcmp(str, TYPE_U32_STRING)   == 0) return U32;
    if (strcmp(str, TYPE_U64_STRING)   == 0) return U64;
    if (strcmp(str, TYPE_U128_STRING)  == 0) return U128;
    if (strcmp(str, TYPE_I8_STRING)    == 0) return I8;
    if (strcmp(str, TYPE_I16_STRING)   == 0) return I16;
    if (strcmp(str, TYPE_I32_STRING)   == 0) return I32;
    if (strcmp(str, TYPE_I64_STRING)   == 0) return I64;
    if (strcmp(str, TYPE_I128_STRING)  == 0) return I128;
    if (strcmp(str, TYPE_F32_STRING)   == 0) return F32;
    if (strcmp(str, TYPE_F64_STRING)   == 0) return F64;
    if (strcmp(str, TYPE_USIZE_STRING) == 0) return USIZE;
    if (strcmp(str, TYPE_BOOL_STRING)  == 0) return BOOL;
    return TYPEDEF;
}

int TypeData_match(TypeData* a, TypeData* b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->pointer_depth != b->pointer_depth) return 0;

    if (a->pointer_depth > 0 && (a->base == U0 || b->base == U0)) return 1;

    if (a->base != b->base) return 0;
    if (a->base == TYPEDEF) {
        return a->struct_def == b->struct_def;
    }
    if (a->base == FUNCTION) {
        if (a->function.param_count != b->function.param_count) return 0;
        if (!TypeData_match(a->function.return_type, b->function.return_type)) return 0;
        for (size_t i = 0; i < a->function.param_count; i++) {
            if (!TypeData_match(a->function.param_types[i], b->function.param_types[i])) {
                return 0;
            }
        }
        return 1;
    }
    return 1;
}

const char* typeBase_to_string(TypeBase base) {
    switch (base) {
        case U0:      return TYPE_U0_STRING;
        case U8:      return TYPE_U8_STRING;
        case U16:     return TYPE_U16_STRING;
        case U32:     return TYPE_U32_STRING;
        case U64:     return TYPE_U64_STRING;
        case U128:    return TYPE_U128_STRING;
        case I8:      return TYPE_I8_STRING;
        case I16:     return TYPE_I16_STRING;
        case I32:     return TYPE_I32_STRING;
        case I64:     return TYPE_I64_STRING;
        case I128:    return TYPE_I128_STRING;
        case F32:     return TYPE_F32_STRING;
        case F64:     return TYPE_F64_STRING;
        case USIZE:   return TYPE_USIZE_STRING;
        case BOOL:    return TYPE_BOOL_STRING;
        case TYPEDEF: return TYPE_TYPEDEF_STRING;
        case FUNCTION: return TYPE_FUNCTION_STRING;
        default:      return "unknown";
    }
}

const char* TypeData_to_string(TypeData* td) {
    static char buffers[16][256];
    static int buf_index = 0;
    
    char* buffer = buffers[buf_index];
    buf_index = (buf_index + 1) % 16;

    if (!td) {
        snprintf(buffer, 256, "(null)");
        return buffer;
    }

    size_t pos = 0;
    while (pos < td->pointer_depth && pos < 250) {
        buffer[pos++] = '*';
    }
    
    if (td->base == ARRAY) {
        const char* elem = td->array.element_type ? TypeData_to_string(td->array.element_type) : "unknown";
        
        char elem_copy[128];
        strncpy(elem_copy, elem, sizeof(elem_copy)-1);
        elem_copy[sizeof(elem_copy)-1] = '\0';
        snprintf(buffer + pos, 256 - pos, "[%zu]%s", td->array.length, elem_copy);
        return buffer;
    }
    if (td->base == SLICE) {
        const char* elem = td->slice.element_type ? TypeData_to_string(td->slice.element_type) : "unknown";
        char elem_copy[128];
        strncpy(elem_copy, elem, sizeof(elem_copy)-1);
        elem_copy[sizeof(elem_copy)-1] = '\0';
        snprintf(buffer + pos, 256 - pos, "[]%s", elem_copy);
        return buffer;
    }
    if (td->base == FUNCTION) {
        const char* ret  = td->function.return_type ? TypeData_to_string(td->function.return_type) : TYPE_U0_STRING;
        char ret_copy[128];
        strncpy(ret_copy, ret, sizeof(ret_copy)-1);
        ret_copy[sizeof(ret_copy)-1] = '\0';

        snprintf(buffer + pos, 256 - pos, "%s(", (pos > 0) ? "*" : "");
        pos = strlen(buffer);
        for (size_t i = 0; i < td->function.param_count && pos < 250; i++) {
            const char* pt = td->function.param_types[i]
                ? TypeData_to_string(td->function.param_types[i]) : "unknown";
            char pt_copy[128];
            strncpy(pt_copy, pt, sizeof(pt_copy)-1);
            pt_copy[sizeof(pt_copy)-1] = '\0';
            if (i > 0 && pos < 253) {
                snprintf(buffer + pos, 256 - pos, ", ");
                pos = strlen(buffer);
            }
            if (pos < 253) {
                snprintf(buffer + pos, 256 - pos, "%s", pt_copy);
            }
            pos = strlen(buffer);
        }
        snprintf(buffer + pos, 256 - pos, ") -> %s", ret_copy);
        return buffer;
    }
    
    const char* type_name = td->identity ? td->identity : typeBase_to_string(td->base);
    snprintf(buffer + pos, 256 - pos, "%s", type_name ? type_name : "unknown");

    return buffer;
}

static StructDef* struct_for_owner(Ast* node) {
    if (!struct_defs || !node) return NULL;
    size_t count = vector_size(struct_defs);
    for (size_t i = 0; i < count; i++) {
        if (struct_defs[i]->owner == node) return struct_defs[i];
    }
    return NULL;
}

StructDef* register_struct_skeleton(Ast* node) {
    if (!node || node->type != AST_STRUCT_DECLARATION) return NULL;

    if (!struct_defs) struct_defs = vector_create();

    StructDef* existing = struct_for_owner(node);
    if (existing) return existing;

    const char* struct_name = identifier_name(node->struct_declaration.name);

    StructDef* def = calloc(1, sizeof(StructDef));
    if (!def) return NULL;

    def->name = struct_name;
    def->owner = node;
    def->field_count = vector_size(node->struct_declaration.fields);
    def->fields = NULL;
    
    vector_add(&struct_defs, def);
    return def;
}

StructDef* register_struct_skeleton_qualified(Ast* node, const char* qualified_name) {
    if (!node || node->type != AST_STRUCT_DECLARATION) return NULL;
    if (!struct_defs) struct_defs = vector_create();
    StructDef* existing = struct_for_owner(node);
    if (existing) return existing;
    StructDef* def = calloc(1, sizeof(StructDef));
    if (!def) return NULL;
    def->name = qualified_name;
    def->owner = node;
    def->field_count = vector_size(node->struct_declaration.fields);
    def->fields = NULL;
    vector_add(&struct_defs, def);
    return def;
}

void fill_struct_fields(StructDef* def, Ast* node) {
    if (!def || !node || def->fields != NULL) return;

    size_t field_count = def->field_count;
    if (field_count == 0) return;

    def->fields = calloc(field_count, sizeof(StructField));
    if (!def->fields) return;

    for (size_t i = 0; i < field_count; i++) {
        Ast* field = node->struct_declaration.fields[i];
        def->fields[i] = (StructField){
            .name  = identifier_name(field->field_declaration.name),
            .type  = TypeData_of(field),
            .index = i
        };
    }
}

TypeData* TypeData_of(Ast* node) {
    if (!node) return TypeData_typeinv();
    if (node->symbol) return node->symbol->type;

    switch (node->type) {
        case AST_LITERAL_NIL:     return TypeData_intern(NIL, 0, TYPE_NIL_STRING, NULL);
        case AST_LITERAL_FLOAT:   return (TypeData*)(&TYPE_F32_DATA);
        case AST_LITERAL_INTEGER: return (TypeData*)(&TYPE_I32_DATA);
        case AST_LITERAL_BOOLEAN: return (TypeData*)(&TYPE_BOOL_DATA);
        case AST_LITERAL_STRING:  return TypeData_intern(U8, 1, TYPE_U8_STRING, NULL);

        case AST_UNARY_EXPRESSION: return TypeData_of(node->unary_expression.argument);
        case AST_RETURN_STATEMENT: return TypeData_of(node->return_statement.value);
        case AST_CAST_EXPRESSION:  return TypeData_of(node->cast_expression.type);
        case AST_LITERAL_COMPOUND: return TypeData_of(node->literal_compound.type);
        default: break;
    }

    Ast* type_node = NULL;
    switch (node->type) {
        case AST_VARIABLE_DECLARATION:  type_node = node->variable_declaration.type;  break;
        case AST_FIELD_DECLARATION:     type_node = node->field_declaration.type;     break;
        case AST_PARAMETER_DECLARATION: type_node = node->parameter_declaration.type; break;
        case AST_IDENTIFIER:            
        case AST_STAR_EXPRESSION:       
        case AST_ARRAY_TYPE:           
        case AST_POLYMORPHIC:
        case AST_SLICE_TYPE:            
        case AST_FUNCTION_TYPE:         type_node = node; break;
        default:
            if (!is_function_node(node)) return TypeData_typeinv();
            if (!node->function_declaration.type) return (TypeData*)(&TYPE_U0_DATA);
            type_node = node->function_declaration.type;
            break;
    }

    uint8_t pointer_depth = 0;
    while (type_node && type_node->type == AST_STAR_EXPRESSION) {
        pointer_depth++;
        type_node = type_node->star_expression.object;
    }

    if (!type_node) return TypeData_typeinv();

    if (type_node->type == AST_ARRAY_TYPE) {
        TypeData val = {
            .base = ARRAY,
            .pointer_depth = pointer_depth,
            .array = {
                .element_type = TypeData_of(type_node->array_type.type),
                .length = type_node->array_type.length
            }
        };
        return type_intern(val);
    }

    if (type_node->type == AST_SLICE_TYPE) {
        TypeData val = {
            .base = SLICE,
            .pointer_depth = pointer_depth,
            .slice = {
                .element_type = TypeData_of(type_node->slice_type.type)
            }
        };
        return type_intern(val);
    }

    if (type_node->type == AST_FUNCTION_TYPE) {
        Ast** params  = type_node->function_type.params;
        size_t n      = params ? vector_size(params) : 0;
        TypeData** ptds = NULL;
        if (n > 0) {
            ptds = c_alloc(n * sizeof(TypeData*));
            for (size_t i = 0; i < n; i++) {
                ptds[i] = TypeData_of(params[i]);
            }
        }
        TypeData* ret = type_node->function_type.return_type
            ? TypeData_of(type_node->function_type.return_type)
            : (TypeData*)(&TYPE_U0_DATA);
        TypeData val = {
            .base = FUNCTION,
            .pointer_depth = pointer_depth,
            .function = {
                .param_types = ptds,
                .param_count = n,
                .return_type = ret
            }
        };
        return type_intern(val);
    }

    if (type_node->type == AST_IDENTIFIER) {
        const char* name = type_node->identifier.name;
        TypeBase base = find_TypeBase(name);
        if (base == TYPEDEF) {
            return TypeData_typeinv();
        }

        return TypeData_intern(base, pointer_depth, name, NULL);
    }

    if (type_node->type == AST_SELECTOR_EXPRESSION) {
        return TypeData_typeinv();
    }

    if (type_node->type == AST_POLYMORPHIC) {
        return TypeData_intern(TYPEVAR, pointer_depth,type_node->polymorphic.name, NULL);
    }

    return TypeData_typeinv();
}
