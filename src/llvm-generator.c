#include "llvm-generator.h"
#include "llvm-c/Target.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef struct LoopContext {
    LLVMBasicBlockRef* exit_blocks;
    LLVMBasicBlockRef* latch_blocks;
    int depth;
} LoopContext;

typedef struct StructReference {
    TypeData*   type;
    LLVMTypeRef ref;
} StructReference;

typedef struct LLVMEnvironment {
    LLVMContextRef      context;
    LLVMModuleRef       module;
    LLVMBuilderRef      builder;
    LLVMValueRef        function;
    LoopContext         loop;
    Ast**               function_labels;
    LLVMBasicBlockRef*  function_label_blocks;
    int                 function_label_blocks_count;
} LLVMEnvironment;

static int ast_has_attribute(Ast* node, const char* name);

static LLVMEnvironment env;
static StructReference* struct_references = NULL;
static int private_name_counter = 0;

static char** private_used_names = NULL;
static char** user_defined_names = NULL;

static int is_private_name_taken(const char* cand) {
    if (LLVMGetNamedFunction(env.module, cand)) return 1;
    if (LLVMGetNamedGlobal(env.module, cand)) return 1;
    if (private_used_names) for(size_t i=0;i<vector_size(private_used_names);i++) if(strcmp(private_used_names[i],cand)==0) return 1;
    if (user_defined_names) for(size_t i=0;i<vector_size(user_defined_names);i++) if(strcmp(user_defined_names[i],cand)==0) return 1;
    return 0;
}


static const char* private_mangled_name(const char* kind, const char* name) {
    char base[512];
    if (kind && kind[0]) snprintf(base, sizeof(base), "%s.%s", kind, name);
    else snprintf(base, sizeof(base), "%s", name);

    for (int i = 0; i < 1000; i++) {
        char candidate[544];
        snprintf(candidate, sizeof(candidate), "%s.%d", base, i);
        if (is_private_name_taken(candidate)) continue;

        size_t len = strlen(candidate) + 1;
        char* copy = c_alloc(len);
        if (!copy) return NULL;
        memcpy(copy, candidate, len);
        vector_add(&private_used_names, copy);
        return copy;
    }
    return NULL;
}

static void abi_init_for_module(LLVMModuleRef module);
static int  type_is_struct(TypeData* td);
static AbiInfo abi_info_of_type(LLVMTypeRef type);

static int abi_return_is_sret(TypeData* ret_td);
static LLVMValueRef try_get_struct_ptr(Ast* node, bool is_global);
LLVMTypeRef llvm_struct_declare(Ast* node);
void llvm_struct_define(Ast* node);

static void llvm_internal_error(Ast* node, const char* message) {
    const char* path = (node && node->source.path) ? node->source.path : "<unknown>";
    size_t row = node ? node->source.row : 0;
    size_t col = node ? node->source.col : 0;

    fprintf(
        stderr,
        "%s:%zu:%zu:\n"KRED"Internal error:"KRESET" %s\n",
        path, row, col, message
    );
    exit(EXIT_FAILURE);
}

static TypeData* llvm_require_symbol_type(Ast* node) {
    if (!node->symbol) {
        llvm_internal_error(node, "unresolved expression: no symbol was assigned during semantic analysis");
    }
    return node->symbol->type;
}

static void llvm_init(Ast* root, CLI cli) {
    
    user_defined_names = vector_create();
    private_used_names = vector_create();
    env.context = LLVMContextCreate();
    env.module  = LLVMModuleCreateWithNameInContext(cli.input, env.context);
    env.builder = LLVMCreateBuilderInContext(env.context);

    env.loop.exit_blocks  = vector_create();
    env.loop.latch_blocks = vector_create();
    env.loop.depth        = 0;

    assert(env.context != NULL && env.module != NULL && env.builder != NULL);

    abi_init_for_module(env.module);

    struct_references = vector_create();
}

static void abi_init_for_module(LLVMModuleRef module) {
    abi_init_native();

    const char* dl = abi_native_datalayout();
    if (dl) {
        LLVMSetModuleDataLayout(module, LLVMCreateTargetData(dl));
    }
}

#ifdef DEVELOP
#include "llvm-c/Analysis.h"

static bool llvm_verify(LLVMModuleRef module) {
    char *error = NULL;

    if (LLVMVerifyModule(module, LLVMReturnStatusAction, &error)) {
        fprintf(stderr, KRED "LLVM IR verification failed:\n%s\n" KRESET, error);
        LLVMDisposeMessage(error);
        return false;
    }

    return true;
}


#else
static bool llvm_verify(LLVMModuleRef module) {
    return true;
}
#endif

char* unescape_string(const char* src) {
    size_t len = strlen(src);
    char* out = c_alloc(len + 1);

    if (!out) return NULL;
    size_t j = 0;

    for (size_t i = 0; i < len; i++) {
        if (src[i] != '\\') {
            out[j++] = src[i];
            continue;
        }

        if (i + 1 >= len) {
            out[j++] = '\\';
            break;
        }

        switch (src[++i]) {
            case 'n' : out[j++] = '\n'; break;
            case 't' : out[j++] = '\t'; break;
            case 'r' : out[j++] = '\r'; break;
            case '\\': out[j++] = '\\'; break;
            case '"' : out[j++] = '"' ; break;
            case '\'': out[j++] = '\''; break;

            default:
                out[j++] = '\\';
                out[j++] = src[i];
                break;
        }
    }

    out[j] = '\0';
    return out;
}

static LLVMTypeRef TypeRef_of(TypeData* type) {
    if (!type) {
        return NULL;
    }

    if (is_pointer_type(type)) {
        return LLVMPointerTypeInContext(env.context, 0);
    }

    if (type->base == NIL) {
        return LLVMPointerTypeInContext(env.context, 0);
    }

    switch (type->base) {
        case U0:
            return LLVMVoidTypeInContext(env.context);
        case U8:
        case I8:
            return LLVMInt8TypeInContext(env.context);
        case U16:
        case I16:
            return LLVMInt16TypeInContext(env.context);
        case U32:
        case I32:
            return LLVMInt32TypeInContext(env.context);
        case U64:
        case I64:
            return LLVMInt64TypeInContext(env.context);
        case USIZE:
            return abi_word_type(env.context);
        case U128:
        case I128:
            return LLVMInt128TypeInContext(env.context);
        case F32:
            return LLVMFloatTypeInContext(env.context);
        case F64:
            return LLVMDoubleTypeInContext(env.context);
        case BOOL:
            return LLVMInt1TypeInContext(env.context);
        case TYPEDEF:
            for (size_t i = 0; i < vector_size(struct_references); i++) {
                if (TypeData_match(type, struct_references[i].type)) {
                    return struct_references[i].ref;
                }
            }
            return NULL;

        case ARRAY:
           return LLVMArrayType2(TypeRef_of(type->array.element_type), type->array.length);
        case TYPEINV:
            return LLVMVoidTypeInContext(env.context);
        case SLICE: {
            LLVMTypeRef parts[2] = {
                LLVMPointerTypeInContext(env.context, 0),
                abi_word_type(env.context)
            };
            return LLVMStructTypeInContext(env.context, parts, 2, false);
        }
        case FUNCTION:
            return LLVMPointerTypeInContext(env.context, 0);
        default:
            return NULL;
    }
}

static LLVMTypeRef TypeRef_pointee(TypeData* ptr_type) {
    assert(ptr_type != NULL);
    if (!is_pointer_type(ptr_type)) {
        return TypeRef_of(ptr_type);
    }
    TypeData* pointee = TypeData_retype(ptr_type, ptr_type->pointer_depth - 1);
    assert(pointee != NULL);
    return TypeRef_of(pointee);
}

static void set_reference(Ast* node, LLVMValueRef value, LLVMTypeRef type) {
    if (!node) return;
    if (!node->symbol) return;
    node->symbol->llvm_value = value;
    node->symbol->llvm_type  = type;
}

void llvm_block(Ast* node, bool is_global);
void llvm_statement(Ast* node, bool is_global);
LLVMValueRef llvm_literal(Ast* node);
LLVMValueRef llvm_call_expression(Ast* node, bool is_global);
LLVMValueRef llvm_binary_expression(Ast* node, bool is_global);
LLVMValueRef llvm_expression(Ast* node, bool is_global);
LLVMValueRef llvm_compound_literal(Ast* node, bool is_global);
LLVMValueRef llvm_postfix_expression(Ast* node, bool is_global);
static LLVMValueRef llvm_index_address(Ast* node, bool is_global);
static LLVMValueRef llvm_object_base_address(Ast* object, bool is_global);

void llvm_return_statement(Ast* node, bool is_global) {
    assert(node != NULL && env.function != NULL && env.builder != NULL);

    TypeData* ret_type = TypeData_of(node);

    if (is_void_type(ret_type) || !node->return_statement.value) {
        LLVMBuildRetVoid(env.builder);
        return;
    }

    Ast* ret_expr = node->return_statement.value;
    LLVMValueRef value = NULL;
    int value_from_ptr = 0;
    AbiValue av_pre = { {0}, 0 };
    LLVMTypeRef stype_pre = NULL;
    AbiInfo info_pre;
    if (ret_expr && type_is_struct(ret_type)) {
        stype_pre = TypeRef_of(ret_type);
        info_pre = abi_info_of_type(stype_pre);
        if (info_pre.cls == ABI_CLASS_MEMORY) {
            LLVMValueRef ptr = try_get_struct_ptr(ret_expr, is_global);
            if (ptr) {
                LLVMValueRef loaded = LLVMBuildLoad2(env.builder, stype_pre, ptr, "");
                assert(loaded != NULL);
                LLVMBuildStore(env.builder, loaded, LLVMGetParam(env.function, 0));
            } else {
                value = llvm_expression(ret_expr, is_global);
                assert(value != NULL);
                LLVMBuildStore(env.builder, value, LLVMGetParam(env.function, 0));
            }
            LLVMBuildRetVoid(env.builder);
            return;
        }
        LLVMValueRef ptr = try_get_struct_ptr(ret_expr, is_global);
        if (ptr) {
            av_pre = abi_load_chunks_from_ptr(env.builder, env.module, stype_pre, &info_pre, ptr);
            value_from_ptr = 1;
        }
    }
    if (!value_from_ptr) {
        value = ret_expr
            ? llvm_expression(ret_expr, is_global)
            : (type_is_struct(ret_type)
                ? LLVMConstNull(TypeRef_of(ret_type))
                : LLVMConstInt(TypeRef_of(ret_type), 0, false));
    }

    if (type_is_struct(ret_type)) {
        LLVMTypeRef stype = TypeRef_of(ret_type);
        AbiInfo info = abi_info_of_type(stype);

        if (info.cls == ABI_CLASS_MEMORY) {
            assert(value != NULL);
            LLVMBuildStore(env.builder, value, LLVMGetParam(env.function, 0));
            LLVMBuildRetVoid(env.builder);
            return;
        }

        AbiValue av;
        if (value_from_ptr) av = av_pre;
        else av = abi_pull_chunks(env.builder, env.module, stype, &info, value);
        if (av.count == 1) {
            assert(av.chunks[0] != NULL);
            LLVMBuildRet(env.builder, av.chunks[0]);
            return;
        }
        LLVMTypeRef ret_ty = abi_ir_return_type(env.context, &info);
        assert(ret_ty != NULL);
        if (LLVMGetTypeKind(ret_ty) == LLVMStructTypeKind) {
            assert(av.count == 2);
            assert(av.chunks[0] != NULL && av.chunks[1] != NULL);
            LLVMValueRef agg = LLVMGetUndef(ret_ty);
            agg = LLVMBuildInsertValue(env.builder, agg, av.chunks[0], 0, "");
            agg = LLVMBuildInsertValue(env.builder, agg, av.chunks[1], 1, "");
            LLVMBuildRet(env.builder, agg);
        } else {
            assert(av.chunks[0] != NULL);
            LLVMBuildRet(env.builder, av.chunks[0]);
        }
        return;
    }

    if (is_function_type(ret_type) && value &&
        LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMFunctionTypeKind) {
        value = LLVMBuildBitCast(env.builder, value, TypeRef_of(ret_type), "");
        assert(value != NULL);
    }
    assert(value != NULL);
    LLVMBuildRet(env.builder, value);
}

void llvm_break_statement(Ast* node, bool is_global) {
    (void)node; (void)is_global;
    if (env.loop.depth <= 0) return;
    LLVMBuildBr(env.builder, env.loop.exit_blocks[env.loop.depth - 1]);
}

void llvm_continue_statement(Ast* node, bool is_global) {
    (void)node; (void)is_global;
    if (env.loop.depth <= 0) return;
    LLVMBuildBr(env.builder, env.loop.latch_blocks[env.loop.depth - 1]);
}

static const char* llvm_resolve_variable_name(Ast* node) {
    const char* var_name = identifier_name(node);
    if (node->symbol && node->symbol->name) var_name = node->symbol->name;
    if (ast_has_attribute(node, "@private")) {
        const char* mangled = private_mangled_name("", var_name);
        if (mangled) var_name = mangled;
    }
    return var_name;
}

static LLVMValueRef llvm_const_range_array(TypeData* var_td, Ast* range_node) {
    assert(var_td != NULL && range_node != NULL);
    assert(range_node->range_expression.start != NULL);
    long start_v = 0;
    if (range_node->range_expression.start->type == AST_LITERAL_INTEGER)
        start_v = range_node->range_expression.start->literal_integer.value;
    size_t len = var_td->array.length;
    LLVMTypeRef elem_ty = TypeRef_of(var_td->array.element_type);
    assert(elem_ty != NULL);
    LLVMValueRef* elems = c_alloc(sizeof(LLVMValueRef) * len);
    for (size_t i = 0; i < len; i++) {
        elems[i] = LLVMConstInt(elem_ty, (unsigned long long)(start_v + (long)i), true);
    }
    LLVMValueRef arr = LLVMConstArray2(elem_ty, elems, len);
    assert(arr != NULL);
    return arr;
}

static LLVMValueRef llvm_pointer_from_array_init(Ast* init_node, TypeData* init_td, LLVMTypeRef var_type, bool is_global) {
    LLVMValueRef base = llvm_object_base_address(init_node, is_global);
    assert(base != NULL);
    LLVMTypeRef arr_ty = TypeRef_of(init_td);
    assert(arr_ty != NULL);
    LLVMValueRef idx = LLVMConstInt(LLVMInt64TypeInContext(env.context), 0, false);
    LLVMValueRef data = LLVMBuildGEP2(env.builder, arr_ty, base, &idx, 1, "");
    assert(data != NULL);
    LLVMValueRef casted = LLVMBuildPointerCast(env.builder, data, var_type, "");
    assert(casted != NULL);
    return casted;
}

static LLVMValueRef llvm_slice_from_array_init(Ast* init_node, TypeData* init_td, LLVMTypeRef var_type, bool is_global) {
    LLVMValueRef base = llvm_object_base_address(init_node, is_global);
    assert(base != NULL);
    LLVMTypeRef arr_ty = TypeRef_of(init_td);
    assert(arr_ty != NULL);
    LLVMValueRef idx = LLVMConstInt(LLVMInt64TypeInContext(env.context), 0, false);
    LLVMValueRef data = LLVMBuildGEP2(env.builder, arr_ty, base, &idx, 1, "");
    assert(data != NULL);
    LLVMValueRef ptr = LLVMBuildPointerCast(env.builder, data, LLVMPointerTypeInContext(env.context, 0), "");
    assert(ptr != NULL);
    LLVMValueRef len = LLVMConstInt(abi_word_type(env.context), (unsigned long long)init_td->array.length, false);
    assert(len != NULL);
    LLVMValueRef slice = LLVMGetUndef(var_type);
    assert(slice != NULL);
    slice = LLVMBuildInsertValue(env.builder, slice, ptr, 0, "");
    assert(slice != NULL);
    slice = LLVMBuildInsertValue(env.builder, slice, len, 1, "");
    assert(slice != NULL);
    return slice;
}

static LLVMValueRef llvm_coerce_global_initializer(LLVMValueRef init_val, LLVMTypeRef var_type) {
    if (!init_val || LLVMTypeOf(init_val) == var_type) return init_val;
    LLVMTypeRef it = LLVMTypeOf(init_val);
    if (LLVMGetTypeKind(var_type) == LLVMPointerTypeKind && LLVMGetTypeKind(it) == LLVMFunctionTypeKind) {
        return LLVMConstBitCast(init_val, var_type);
    }
    if (LLVMGetTypeKind(var_type) == LLVMPointerTypeKind && LLVMGetTypeKind(it) == LLVMPointerTypeKind) {
        return LLVMBuildBitCast(env.builder, init_val, var_type, "");
    }
    return init_val;
}

static LLVMValueRef llvm_coerce_local_initializer(LLVMValueRef init, LLVMTypeRef var_type) {
    if (!init) return NULL;
    LLVMTypeRef init_type = LLVMTypeOf(init);
    if (!init_type || init_type == var_type) return init;
    if (LLVMGetTypeKind(var_type) == LLVMIntegerTypeKind && LLVMGetTypeKind(init_type) == LLVMIntegerTypeKind) {
        return LLVMBuildIntCast2(env.builder, init, var_type, true, "");
    }
    if (LLVMGetTypeKind(var_type) == LLVMFloatTypeKind || LLVMGetTypeKind(var_type) == LLVMDoubleTypeKind) {
        return LLVMBuildFPCast(env.builder, init, var_type, "");
    }
    if (LLVMGetTypeKind(var_type) == LLVMPointerTypeKind && LLVMGetTypeKind(init_type) == LLVMPointerTypeKind) {
        return LLVMBuildBitCast(env.builder, init, var_type, "");
    }
    if (LLVMGetTypeKind(var_type) == LLVMPointerTypeKind && LLVMGetTypeKind(init_type) == LLVMFunctionTypeKind) {
        return LLVMBuildBitCast(env.builder, init, var_type, "");
    }
    if (LLVMGetTypeKind(var_type) == LLVMStructTypeKind && init_type) {
        return init;
    }
    return LLVMBuildBitCast(env.builder, init, var_type, "");
}

static LLVMValueRef llvm_build_initializer_value(Ast* var_node, Ast* init_node, LLVMTypeRef var_type, bool is_global) {
    assert(var_node != NULL && init_node != NULL && var_type != NULL);
    TypeData* var_td = TypeData_of(var_node);
    TypeData* init_td = TypeData_of(init_node);
    if (init_node->type == AST_RANGE_EXPRESSION && var_td->base == ARRAY) {
        return llvm_const_range_array(var_td, init_node);
    }
    if (var_td->pointer_depth > 0 && init_td->base == ARRAY) {
        return llvm_pointer_from_array_init(init_node, init_td, var_type, is_global);
    }
    if (var_td->base == SLICE && init_td->base == ARRAY) {
        return llvm_slice_from_array_init(init_node, init_td, var_type, is_global);
    }
    LLVMValueRef init = llvm_expression(init_node, is_global);
    return is_global ? llvm_coerce_global_initializer(init, var_type)
                     : llvm_coerce_local_initializer(init, var_type);
}

static LLVMValueRef llvm_build_local_alloca(
    LLVMTypeRef type,
    const char* name
) {
    assert(env.function != NULL && type != NULL);

    LLVMBasicBlockRef entry =
        LLVMGetEntryBasicBlock(env.function);
    assert(entry != NULL);

    LLVMBuilderRef builder =
        LLVMCreateBuilderInContext(env.context);

    LLVMValueRef first = LLVMGetFirstInstruction(entry);
    if (first) {
        LLVMPositionBuilderBefore(builder, first);
    } else {
        LLVMPositionBuilderAtEnd(builder, entry);
    }

    LLVMValueRef value =
        LLVMBuildAlloca(builder, type, name);
    assert(value != NULL);

    LLVMDisposeBuilder(builder);

    return value;
}

LLVMValueRef llvm_variable_declaration(Ast* node, bool is_global)
{
    if (!node->variable_declaration.is_called)
        return NULL;

    LLVMTypeRef type = TypeRef_of(TypeData_of(node));
    assert(type != NULL);
    Ast* init_node = node->variable_declaration.initializer;
    
    LLVMValueRef init = init_node 
        ? llvm_build_initializer_value(node, init_node, type, is_global) 
        : NULL;
    if (!init) init = LLVMConstNull(type);
    assert(init != NULL);

    LLVMValueRef value;

    if (is_global) {
        value = LLVMAddGlobal(env.module, type, llvm_resolve_variable_name(node));

        if (node->variable_declaration.is_constant)
            LLVMSetGlobalConstant(value, true);

        if (ast_has_attribute(node, "@private"))
            LLVMSetLinkage(value, LLVMPrivateLinkage);

        LLVMSetInitializer(value, init);
    } else {
        if (node->variable_declaration.is_constant && is_numeric_type(TypeData_of(node))) {
            value = init;
        }
        else {
            value = llvm_build_local_alloca(type, identifier_name(node));
            LLVMBuildStore(env.builder, init, value);
        }
    }

    set_reference(node, value, type);
    return value;
}

static int type_is_struct(TypeData* td) {
    return td->base == TYPEDEF && td->pointer_depth == 0;
}

static AbiInfo abi_info_of_type(LLVMTypeRef type) {
    return abi_analyze(env.context, type);
}

static LLVMTypeRef pointer_to(LLVMTypeRef elem) {
    (void)elem;
    return LLVMPointerTypeInContext(env.context, 0);
}

static AbiInfo abi_info_for_param(Ast* param) {
    TypeData* td = TypeData_of(param);
    return type_is_struct(td)
        ? abi_info_of_type(TypeRef_of(td))
        : (AbiInfo){ .cls = ABI_CLASS_NOT_STRUCT };
}

static int abi_lowered_param_count(Ast* param) {
    AbiInfo info = abi_info_for_param(param);
    if (info.cls == ABI_CLASS_NOT_STRUCT) return 1;
    return abi_param_count(&info);
}

static int abi_return_is_sret(TypeData* ret_td) {
    if (!type_is_struct(ret_td)) return 0;
    AbiInfo info = abi_info_of_type(TypeRef_of(ret_td));
    return info.cls == ABI_CLASS_MEMORY;
}

// Processes and attaches AST node attributes (@private, @inline, etc.) to an LLVM function/statement
// 
// Supported Attributes:
//  - @private : Restricts visibility strictly to the current file (file-local scope)
//               Translates to LLVMPrivateLinkage (symbol is hidden from external linkage)
//  - @inline  : Mapped directly to LLVM's "alwaysinline" attribute to force function inlining
//  - Other    : Strips the '@' prefix (e.g., @noinline -> "noinline") and dynamically resolves 
//               the matching LLVM enum attribute.
//
// https://llvm.org/docs/LangRef.html#function-attributes
 
static void llvm_add_attribute(LLVMValueRef statement, const char* attribute) {
    if(!attribute) return;
    if (str_match(attribute, "@private")) {
        LLVMSetLinkage(statement, LLVMPrivateLinkage);
        return;
    }
    if (str_match(attribute, "@inline")) {
        attribute = "alwaysinline";
    }
    else attribute += 1;
    unsigned attrKind = LLVMGetEnumAttributeKindForName(attribute, strlen(attribute));
    if (!attrKind) return;
    LLVMAttributeRef attr = LLVMCreateEnumAttribute(env.context, attrKind, 0);
    LLVMAddAttributeAtIndex(statement, LLVMAttributeFunctionIndex, attr);
}

static int ast_has_attribute(Ast* node, const char* name) {
    if (!node || !node->attributes) return 0;
    for (size_t i = 0; i < vector_size(node->attributes); i++) {
        if (node->attributes[i] && strcmp(node->attributes[i], name) == 0) return 1;
    }
    return 0;
}

static void llvm_enum_declaration(Ast* node) {
    if (!node || node->type != AST_ENUM_DECLARATION) return;
    if (node->symbol && node->symbol->llvm_value) return;

    TypeData* enum_td = TypeData_of(node);
    LLVMTypeRef int_type = TypeRef_of(enum_td);
    if (!int_type) int_type = LLVMInt32TypeInContext(env.context);
    assert(int_type != NULL);
    int is_signed = is_signed_type(enum_td);

    Ast** members = node->enum_declaration.members;
    size_t count  = members ? vector_size(members) : 0;
    for (size_t i = 0; i < count; i++) {
        Ast* mem = members[i];
        if (!mem || !mem->symbol) continue;
        mem->symbol->llvm_value = LLVMConstInt(int_type,
            mem->enumerator.discriminant, is_signed);
        mem->symbol->llvm_type  = int_type;
    }

    if (node->symbol) {
        node->symbol->llvm_value = LLVMConstNull(int_type);
        node->symbol->llvm_type  = int_type;
    }
}

LLVMValueRef llvm_function_declaration(Ast* node, bool is_global) {
    assert(node != NULL);
    (void)is_global;
    
    if (node->type == AST_FUNCTION_DECLARATION && !node->function_declaration.is_defined && !node->function_declaration.is_called) {
        return NULL;
    }

    size_t param_size = vector_size(node->function_declaration.params);
    bool uses_sret = abi_return_is_sret(TypeData_of(node));

    size_t type_cap = param_size + (uses_sret ? 1 : 0) + 1;
    LLVMTypeRef* lowered = c_alloc(sizeof(LLVMTypeRef) * (type_cap ? type_cap : 1));
    size_t lcount = 0;

    LLVMTypeRef ret_type = TypeRef_of(TypeData_of(node));
    LLVMTypeRef sret_struct = NULL;
    if (uses_sret) {
        assert(ret_type != NULL);
        sret_struct = ret_type;
        lowered[lcount++] = pointer_to(NULL);
        ret_type = LLVMVoidTypeInContext(env.context);
    } else if (type_is_struct(TypeData_of(node))) {
        AbiInfo info = abi_info_of_type(ret_type);
        ret_type = abi_ir_return_type(env.context, &info);
    }

    for (size_t i = 0; i < param_size; i++) {
        Ast*      param = node->function_declaration.params[i];
        assert(param != NULL);
        TypeData*  td    = TypeData_of(param);
        if (!type_is_struct(td)) {
            lowered[lcount++] = TypeRef_of(td);
            continue;
        }
        LLVMTypeRef  stype = TypeRef_of(td);
        AbiInfo info = abi_info_of_type(stype);
        if (info.cls == ABI_CLASS_MEMORY) {
            lowered[lcount++] = pointer_to(NULL);
        } else {
            for (int c = 0; c < info.chunk_count && c < 4; c++) {
                lowered[lcount++] = abi_chunk_type(stype, &info.chunks[c]);
            }
        }
    }

    for (size_t i = 0; i < lcount; i++) {
        assert(lowered[i] != NULL);
    }
    assert(ret_type != NULL);

    LLVMTypeRef fnc_type = LLVMFunctionType(
        ret_type, lowered, (unsigned)lcount,
        node->function_declaration.has_ellipsis
    );
    assert(fnc_type != NULL);

    const char* fn_name = identifier_name(node);
    if (node->symbol && node->symbol->name) {
        fn_name = node->symbol->name;
    }
    const char* effective_name = fn_name;
    if (ast_has_attribute(node, "@private")) {
        const char* mangled = private_mangled_name("function", fn_name);
        if (mangled) effective_name = mangled;
    }
    LLVMValueRef function = LLVMGetNamedFunction(env.module, effective_name);
    if (!function) {
        function = LLVMAddFunction(env.module, effective_name, fnc_type);
    }

    LLVMContextRef ctx = env.context;
    unsigned byval_kind = LLVMGetEnumAttributeKindForName("byval", 5);
    unsigned sret_kind  = LLVMGetEnumAttributeKindForName("sret", 4);

    if (uses_sret) {
        LLVMAddAttributeAtIndex(function, 1,
            LLVMCreateTypeAttribute(ctx, sret_kind, sret_struct));
    }

    size_t lparam = uses_sret ? 1 : 0;
    for (size_t i = 0; i < param_size; i++) {
        Ast*     param = node->function_declaration.params[i];
        TypeData* td    = TypeData_of(param);
        if (!type_is_struct(td)) { lparam++; continue; }

        AbiInfo info = abi_info_of_type(TypeRef_of(td));
        if (info.cls != ABI_CLASS_MEMORY) { lparam += info.chunk_count; continue; }

        LLVMAddAttributeAtIndex(function, (LLVMAttributeIndex)(lparam + 1),
            LLVMCreateTypeAttribute(ctx, byval_kind, TypeRef_of(td)));
        lparam++;
    }

    if (node->attributes) {
        for (size_t i = 0; i < vector_size(node->attributes); i++) {
            llvm_add_attribute(function, node->attributes[i]);
        }
    }

    assert(function != NULL);
    set_reference(node, function, fnc_type);
    return function;
}

// Function aliases do not produce their own LLVM function: the alias symbol
// is bound directly to the target function's value, so calls through either
// name produce identical IR.
LLVMValueRef llvm_function_alias(Ast* node, bool is_global) {
    (void)is_global;
    if (!node || !node->symbol) return NULL;

    Ast* target = node->symbol->declaration;
    int hops = 0;
    while (target && target->type == AST_FUNCTION_ALIAS &&
           target->symbol && target->symbol->declaration &&
           hops++ < 64) {
        target = target->symbol->declaration;
    }

    if (!target || (target->type != AST_FUNCTION_DECLARATION &&
                    target->type != AST_FUNCTION_DEFINITION)) {
        return NULL;
    }
    if (!target->symbol || !target->symbol->llvm_value || !target->symbol->llvm_type) {
        return NULL;
    }

    node->symbol->llvm_value = target->symbol->llvm_value;
    node->symbol->llvm_type  = target->symbol->llvm_type;
    return node->symbol->llvm_value;
}

static void get_labels(Ast* node, Ast*** labels) {
    if (!node || !labels) return;
    if (node->type == AST_LABEL_STATEMENT) {
        vector_add(labels, node);
    }
    else if (node->type == AST_BLOCK_STATEMENT) {
        Ast** stmts = node->block.statements;
        for (int i = 0; i < vector_size(stmts); i++) {
            get_labels(stmts[i], labels);
        }
    }
    else if (node->type == AST_IF_STATEMENT) {
        get_labels(node->if_statement.alternate, labels);
        get_labels(node->if_statement.consequent, labels);
    }
    else if (node->type == AST_FUNCTION_DEFINITION) {
        get_labels(node->function_declaration.body, labels);
    }
    else if (node->type == AST_LOOP_STATEMENT) {
        get_labels(node->loop_statement.body, labels);
    }
    return;
}

void llvm_function_body(Ast* node) {
    assert(node != NULL);
    assert(node->symbol);
    assert(node->symbol->llvm_value);
    assert(env.builder != NULL && env.context != NULL);

    LLVMValueRef function = env.function = node->symbol->llvm_value;

    if (node->function_declaration.body) {
        Ast** fstmts = node->function_declaration.body->block.statements;
        for (size_t i = 0; i < vector_size(fstmts); i++) if (fstmts[i]->type == AST_STRUCT_DECLARATION) llvm_struct_declare(fstmts[i]);
        for (size_t i = 0; i < vector_size(fstmts); i++) if (fstmts[i]->type == AST_STRUCT_DECLARATION) llvm_struct_define(fstmts[i]);
    }

    LLVMBasicBlockRef entry_block = LLVMAppendBasicBlockInContext(env.context, function, "entry");
    assert(entry_block != NULL);
    LLVMPositionBuilderAtEnd(env.builder, entry_block);

    bool uses_sret = abi_return_is_sret(TypeData_of(node));
    size_t param_size = vector_size(node->function_declaration.params);

    size_t param_index = uses_sret ? 1 : 0;
    for (size_t i = 0; i < param_size; i++) {
        Ast*     param = node->function_declaration.params[i];
        TypeData* td    = TypeData_of(param);

        if (!type_is_struct(td)) {
            LLVMValueRef val = LLVMGetParam(function, (unsigned)param_index++);
            assert(val != NULL);
            assert(param->symbol != NULL);
            param->symbol->llvm_value = val;
            param->symbol->llvm_type  = LLVMTypeOf(val);
            continue;
        }

        LLVMTypeRef stype = TypeRef_of(td);
        assert(stype != NULL);
        AbiInfo info = abi_info_of_type(stype);

        if (info.cls == ABI_CLASS_MEMORY) {

            LLVMValueRef ptr  = LLVMGetParam(function, (unsigned)param_index++);
            assert(ptr != NULL);
            LLVMValueRef slot = LLVMBuildAlloca(env.builder, stype, "byval.addr");
            assert(slot != NULL);
            LLVMValueRef val  = LLVMBuildLoad2(env.builder, stype, ptr, "");
            assert(val != NULL);
            LLVMBuildStore(env.builder, val, slot);
            param->symbol->llvm_value = slot;
            param->symbol->llvm_type  = stype;
            continue;
        }

        AbiValue av = { {0}, 0 };
        for (int c = 0; c < info.chunk_count && c < 4; c++) {
            av.chunks[c] = LLVMGetParam(function, (unsigned)param_index++);
            assert(av.chunks[c] != NULL);
        }
        av.count = info.chunk_count;

        LLVMValueRef slot = LLVMBuildAlloca(env.builder, stype, "param.addr");
        assert(slot != NULL);
        for (int c = 0; c < av.count; c++) {
            LLVMTypeRef  cty   = abi_chunk_type(stype, &info.chunks[c]);
            LLVMValueRef addr  = abi_chunk_address(env.builder, env.module, stype, slot, info.chunks[c].offset);
            assert(addr != NULL);
            addr = LLVMBuildBitCast(env.builder, addr, pointer_to(cty), "");
            assert(addr != NULL);
            LLVMBuildStore(env.builder, av.chunks[c], addr);
        }
        param->symbol->llvm_value = slot;
        param->symbol->llvm_type  = stype;
    }
    
    Ast** labels = vector_create();
    get_labels(node, &labels);

    LLVMBasicBlockRef* label_blocks = malloc(sizeof(LLVMBasicBlockRef) * vector_size(labels));

    for (int i = 0; i < vector_size(labels); i++) {
        Ast* label = labels[i];
        label_blocks[i] = LLVMAppendBasicBlockInContext(
            env.context,
            function,
            identifier_name(label->label_statement.name)
        );
    }

    env.function_label_blocks       = label_blocks;
    env.function_label_blocks_count = vector_size(labels);

    llvm_block(node->function_declaration.body, false);
    LLVMBasicBlockRef current = LLVMGetInsertBlock(env.builder);
    if (current && !LLVMGetBasicBlockTerminator(current)) {
        TypeData* ret_td = TypeData_of(node);
        if (type_is_struct(ret_td)) {
            AbiInfo info = abi_info_of_type(TypeRef_of(ret_td));
            if (info.cls == ABI_CLASS_MEMORY) {
                LLVMTypeRef ret_stype = TypeRef_of(ret_td);
                assert(ret_stype != NULL);
                LLVMValueRef sret_param = LLVMGetParam(function, 0);
                assert(sret_param != NULL);
                LLVMBuildStore(env.builder, LLVMConstNull(ret_stype),
                               sret_param);
                LLVMBuildRetVoid(env.builder);
            } else {
                LLVMTypeRef  coerce = abi_ir_return_type(env.context, &info);
                assert(coerce != NULL);
                LLVMValueRef zero   = LLVMConstNull(coerce);
                LLVMBuildRet(env.builder, zero);
            }
        } else {
            LLVMTypeRef ret_llvm_ty = TypeRef_of(ret_td);
            assert(ret_llvm_ty != NULL);
            ret_td->base == U0
                ? (void)LLVMBuildRetVoid(env.builder)
                : (void)LLVMBuildRet(env.builder, LLVMConstInt(ret_llvm_ty, 0, 0));
        }
    }

    if (node->function_declaration.body) {
        Ast** stmts = node->function_declaration.body->block.statements;
        for (size_t i = 0; i < vector_size(stmts); i++) {
            Ast* stmt = stmts[i];
            if (stmt->type == AST_FUNCTION_DEFINITION && stmt->symbol && stmt->symbol->llvm_value) {
                llvm_function_body(stmt);
            }
        }
    }
}

LLVMValueRef ensure_i1(LLVMBuilderRef builder, LLVMValueRef val) {
    assert(val != NULL);
    LLVMTypeRef type = LLVMTypeOf(val);
    LLVMTypeKind kind = LLVMGetTypeKind(type);

    if (kind == LLVMIntegerTypeKind && LLVMGetIntTypeWidth(type) == 1) {
        return val;
    }

    if (kind == LLVMIntegerTypeKind) {
        return LLVMBuildICmp(builder, LLVMIntNE, val, LLVMConstNull(type), "tobool");
    }

    if (kind == LLVMPointerTypeKind) {
        return LLVMBuildICmp(builder, LLVMIntNE, val, LLVMConstNull(type), "tobool");
    }

    if (kind == LLVMFloatTypeKind || kind == LLVMDoubleTypeKind || 
        kind == LLVMHalfTypeKind  || kind == LLVMFP128TypeKind) {
        return LLVMBuildFCmp(builder, LLVMRealONE, val, LLVMConstNull(type), "tobool");
    }

    if (kind == LLVMStructTypeKind) {
        LLVMValueRef null_struct = LLVMConstNull(type);
        return LLVMBuildICmp(builder, LLVMIntNE, val, null_struct, "tobool");
    }

    return val;
}

void llvm_if_statement(Ast* node, bool is_global) {
    assert(node != NULL && env.function != NULL);
    LLVMValueRef cond = llvm_expression(node->if_statement.test, false);
    assert(cond != NULL);
    LLVMValueRef cond_i1 = ensure_i1(env.builder, cond);
    assert(cond_i1 != NULL);

    LLVMBasicBlockRef then_block  = LLVMAppendBasicBlockInContext(env.context, env.function, "if.then");
    LLVMBasicBlockRef merge_block = LLVMAppendBasicBlockInContext(env.context, env.function, "if.end");
    assert(then_block != NULL && merge_block != NULL);

    bool has_alternate = (node->if_statement.alternate != NULL);
    LLVMBasicBlockRef else_block = has_alternate
        ? LLVMAppendBasicBlockInContext(env.context, env.function, "if.else")
        : merge_block;
    assert(else_block != NULL);

    LLVMBuildCondBr(env.builder, cond_i1, then_block, else_block);

    LLVMPositionBuilderAtEnd(env.builder, then_block);
    if (node->if_statement.consequent) {
        llvm_block(node->if_statement.consequent, is_global);
    }

    if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(env.builder))) {
        LLVMBuildBr(env.builder, merge_block);
    }

    if (has_alternate) {
        LLVMPositionBuilderAtEnd(env.builder, else_block);

        llvm_statement(node->if_statement.alternate, is_global);

        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(env.builder))) {
            LLVMBuildBr(env.builder, merge_block);
        }
    }

    LLVMPositionBuilderAtEnd(env.builder, merge_block);
}

void push_loop(LLVMBasicBlockRef exit, LLVMBasicBlockRef latch) {
    assert(exit != NULL && latch != NULL);
    size_t index = (size_t)env.loop.depth;

    if (index >= vector_size(env.loop.exit_blocks)) {
        vector_add(&env.loop.exit_blocks, exit);
        vector_add(&env.loop.latch_blocks, latch);
    }
    else {
        env.loop.exit_blocks[index]  = exit;
        env.loop.latch_blocks[index] = latch;
    }

    env.loop.depth++;
}
void pop_loop(void) {
    if (env.loop.depth > 0) {
        env.loop.depth--;
    }
}

void llvm_loop_statement(Ast* node, bool is_global) {
    assert(node != NULL);
    assert(node->loop_statement.body != NULL);
    LLVMBuilderRef b = env.builder;
    LLVMContextRef ctx = env.context;
    LLVMValueRef fn = env.function;
    assert(b != NULL && ctx != NULL && fn != NULL);

    LLVMBasicBlockRef header = LLVMAppendBasicBlockInContext(ctx, fn, "loop.header");
    LLVMBasicBlockRef body   = LLVMAppendBasicBlockInContext(ctx, fn, "loop.body");
    LLVMBasicBlockRef latch  = LLVMAppendBasicBlockInContext(ctx, fn, "loop.latch");
    LLVMBasicBlockRef exit   = LLVMAppendBasicBlockInContext(ctx, fn, "loop.exit");
    assert(header != NULL && body != NULL && latch != NULL && exit != NULL);

    push_loop(exit, latch);
    Ast* seq = node->loop_statement.sequence;
    
    // loop statement
    if (!seq) {
        LLVMBuildBr(b, header);

        LLVMPositionBuilderAtEnd(b, header);
        LLVMBuildBr(b, body);

        LLVMPositionBuilderAtEnd(b, body);
        llvm_statement(node->loop_statement.body, false);

        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(b)))
            LLVMBuildBr(b, latch);

        LLVMPositionBuilderAtEnd(b, latch);
        LLVMBuildBr(b, header);
    } 
    // for statement
    else {
        Ast* iter = node->loop_statement.iterator;
        LLVMTypeRef iter_type = iter ? TypeRef_of(TypeData_of(iter)) : LLVMInt32TypeInContext(ctx);
        const char* iter_name = iter ? identifier_name(iter) : "range.iter";

        LLVMValueRef iter_val = LLVMBuildAlloca(b, iter_type, iter_name);
        assert(iter_val != NULL);
        if (iter) set_reference(iter, iter_val, iter_type);

        LLVMValueRef start = llvm_expression(seq->range_expression.start, false);
        LLVMValueRef end   = llvm_expression(seq->range_expression.end, false);
        assert(start != NULL && end != NULL);

        LLVMBuildStore(b, start, iter_val);
        LLVMBuildBr(b, header);

        LLVMPositionBuilderAtEnd(b, header);
        LLVMValueRef cur = LLVMBuildLoad2(b, iter_type, iter_val, "iter");
        TypeData* iter_td = iter ? TypeData_of(iter) : (TypeData*)(&TYPE_I32_DATA);
        LLVMIntPredicate pred = is_signed_type(iter_td) ? LLVMIntSLT : LLVMIntULT;
        LLVMValueRef cmp = LLVMBuildICmp(b, pred, cur, end, "range.cmp");
        assert(cmp != NULL);
        LLVMBuildCondBr(b, cmp, body, exit);

        LLVMPositionBuilderAtEnd(b, body);
        llvm_statement(node->loop_statement.body, false);
        if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(b)))
            LLVMBuildBr(b, latch);

        LLVMPositionBuilderAtEnd(b, latch);
        LLVMValueRef next = LLVMBuildAdd(b, cur, LLVMConstInt(iter_type, 1, false), "iter.next");
        LLVMBuildStore(b, next, iter_val);
        LLVMBuildBr(b, header);
    }

    pop_loop();
    LLVMPositionBuilderAtEnd(b, exit);
}

static LLVMBasicBlockRef find_label_block(const char* name) {
    for (int i = 0; i < env.function_label_blocks_count; i++) {
        LLVMBasicBlockRef block = env.function_label_blocks[i];
        if (str_match(LLVMGetBasicBlockName(block), name)) {
            return block;
        }
    }
    return NULL;
}

void llvm_goto_statement(Ast* node) {
    LLVMBasicBlockRef label_block = find_label_block(identifier_name(node));
    LLVMBuildBr(env.builder, label_block);
}

void llvm_label_statement(Ast* node) {
    LLVMBasicBlockRef label_block = find_label_block(identifier_name(node));
    LLVMBasicBlockRef current = LLVMGetInsertBlock(env.builder);
    if (current && !LLVMGetBasicBlockTerminator(current) && current != label_block) {
        LLVMBuildBr(env.builder, label_block);
    }
    LLVMPositionBuilderAtEnd(env.builder, label_block);
}

LLVMValueRef llvm_literal(Ast* node) {
    TypeData* type = TypeData_of(node);

    if (is_float_type(type)) {
        if (node->type == AST_LITERAL_INTEGER) {
            double value = (double)node->literal_integer.value;
            return LLVMConstReal(TypeRef_of(type), value);
        }
        if (node->type == AST_LITERAL_FLOAT) {
            return LLVMConstReal(TypeRef_of(type), node->literal_float.value);
        }
    }

    if (is_integer_type(type)) {
        if (node->type == AST_LITERAL_INTEGER) {
            return LLVMConstInt(TypeRef_of(type), node->literal_integer.value, is_signed_type(type));
        }
        if (node->type == AST_LITERAL_HEXADECIMAL) {
            char buf[65];
            strip_hex_literal(node->literal_hexadecimal.value, buf, sizeof(buf));
            return LLVMConstIntOfString(TypeRef_of(type), buf, 16);
        }
        if (node->type == AST_LITERAL_BINARY) {
            char buf[129];
            strip_bin_literal(node->literal_binary.value, buf, sizeof(buf));
            return LLVMConstIntOfString(TypeRef_of(type), buf, 2);
        }
        if (node->type == AST_LITERAL_CHAR) {
            return LLVMConstInt(TypeRef_of(type), (unsigned long long)node->literal_char.value, 0);
        }
        if (node->type == AST_LITERAL_FLOAT) {
            long long value = (long long)node->literal_float.value;
            return LLVMConstInt(TypeRef_of(type), value, is_signed_type(type));
        }
    }

    if (is_boolean_type(type)) {
        if (node->type == AST_LITERAL_BOOLEAN) {
            return LLVMConstInt(TypeRef_of(type), node->literal_boolean.value ? 1 : 0, 0);
        }
    }

    if (node->type == AST_LITERAL_STRING) {
        char* str = unescape_string(node->literal_string.value);
        assert(str != NULL);
        size_t len = strlen(str);

        LLVMValueRef str_val  = LLVMConstStringInContext(env.context, str, len, false);
        LLVMTypeRef  str_type = LLVMTypeOf(str_val);

        LLVMValueRef gvar = LLVMAddGlobal(env.module, str_type, ".str");
        LLVMSetInitializer(gvar, str_val);
        LLVMSetGlobalConstant(gvar, true);
        LLVMSetLinkage(gvar, LLVMPrivateLinkage);
        LLVMSetUnnamedAddr(gvar, true);

        LLVMValueRef indices[] = {
            LLVMConstInt(LLVMInt32TypeInContext(env.context), 0, false),
            LLVMConstInt(LLVMInt32TypeInContext(env.context), 0, false)
        };
        return LLVMConstInBoundsGEP2(str_type, gvar, indices, 2);
    }

    if (node->type == AST_LITERAL_ARRAY) {
        size_t size = vector_size(node->literal_array.arguments);
        assert(size > 0);
        assert(node->literal_array.arguments != NULL);
        LLVMTypeRef type = TypeRef_of(TypeData_of(node->literal_array.arguments[0]));
        assert(type != NULL);
        LLVMValueRef* values = c_alloc(sizeof(LLVMValueRef) * size);
        for (int i = 0; i < size; i++) {
            values[i] = llvm_expression(node->literal_array.arguments[i], false);
            assert(values[i] != NULL);
        }
        return LLVMConstArray2(type, values, size);
    }

    if (node->type == AST_LITERAL_NIL) {
        LLVMTypeRef null_type = TypeRef_of(type);
        if (!null_type) null_type = LLVMPointerTypeInContext(env.context, 0);
        return LLVMConstNull(null_type);
    }

    return NULL;
}

LLVMValueRef llvm_cast_expression(Ast* node, bool is_global) {
    Ast* type = node->cast_expression.type;
    Ast* argm = node->cast_expression.argument;
    assert(type != NULL && argm != NULL);

    TypeData* type_td = TypeData_of(type);
    TypeData* argm_td = TypeData_of(argm);

    LLVMTypeRef  target_type = TypeRef_of(type_td);
    if (!target_type) target_type = LLVMPointerTypeInContext(env.context, 0);

    // `nil` has no meaningful value of its own: lower `nil as T` directly as
    // a null of the target type instead of lowering an untyped nil first.
    if (argm && argm->type == AST_LITERAL_NIL) {
        return LLVMConstNull(target_type);
    }

    LLVMValueRef value = llvm_expression(argm, is_global);
    if (!value) {
        return LLVMConstNull(target_type);
    }

    if (LLVMTypeOf(value) == target_type) {
        return value;
    }

    // Every conversion below is emitted with the instruction builder, which has
    // no insertion block at global scope. A global initializer must already be
    // a constant, so a cast that changes its representation cannot be built.
    if (is_global) {
        llvm_internal_error(node, "cast in a global initializer must not change the type");
    }

    // Struct -> Struct
    if (type_is_struct(type_td) && type_is_struct(argm_td)) {
        if (type_td->struct_def == argm_td->struct_def) {
            return value;
        }
        
        LLVMTypeRef src_type = TypeRef_of(argm_td);
        assert(src_type != NULL);
        LLVMValueRef slot = LLVMBuildAlloca(env.builder, src_type, "");
        assert(slot != NULL);
        LLVMBuildStore(env.builder, value, slot);
        
        LLVMValueRef casted_ptr = LLVMBuildBitCast(env.builder, slot, LLVMPointerType(target_type, 0), "bitcast");
        assert(casted_ptr != NULL);
        LLVMValueRef loaded = LLVMBuildLoad2(env.builder, target_type, casted_ptr, "load");
        assert(loaded != NULL);
        return loaded;
    }
    
    // Struct -> Scalar
    if (type_is_struct(argm_td) && !type_is_struct(type_td)) {
        AbiInfo info = abi_info_of_type(TypeRef_of(argm_td));
        
        if (info.cls == ABI_CLASS_MEMORY) {
            LLVMTypeRef struct_type = TypeRef_of(argm_td);
            assert(struct_type != NULL);
            LLVMValueRef slot = LLVMBuildAlloca(env.builder, struct_type, "");
            assert(slot != NULL);
            LLVMBuildStore(env.builder, value, slot);
            LLVMValueRef ptr = LLVMBuildBitCast(env.builder, slot, LLVMPointerType(target_type, 0), "bitcast");
            assert(ptr != NULL);
            LLVMValueRef loaded = LLVMBuildLoad2(env.builder, target_type, ptr, "load");
            assert(loaded != NULL);
            return loaded;
        } 
        else if (info.chunk_count > 0) {
            if (info.chunk_count == 1) {
                return value;
            }
            LLVMValueRef extracted = LLVMBuildExtractValue(env.builder, value, 0, "extract");
            assert(extracted != NULL);
            return extracted;
        }
    }
    
    // Scalar -> Struct
    if (!type_is_struct(argm_td) && type_is_struct(type_td)) {
        AbiInfo info = abi_info_of_type(target_type);
        
        if (info.cls == ABI_CLASS_MEMORY) {
            LLVMValueRef slot = LLVMBuildAlloca(env.builder, target_type, "");
            assert(slot != NULL);
            LLVMValueRef ptr = LLVMBuildBitCast(env.builder, slot, 
                LLVMPointerType(LLVMTypeOf(value), 0), "bitcast");
            assert(ptr != NULL);
            LLVMBuildStore(env.builder, value, ptr);
            LLVMValueRef loaded = LLVMBuildLoad2(env.builder, target_type, slot, "load");
            assert(loaded != NULL);
            return loaded;
        } 
        else {
            AbiValue av = { {0}, 0 };
            av.chunks[0] = value;
            av.count = 1;
            LLVMValueRef rebuilt = abi_rebuild_struct(env.builder, env.module, target_type, &info, &av);
            assert(rebuilt != NULL);
            return rebuilt;
        }
    }

    // Pointer -> Pointer
    if (argm_td->pointer_depth > 0 && type_td->pointer_depth > 0) {
        if (LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMPointerTypeKind) {
            return LLVMBuildBitCast(env.builder, value, target_type, "ptrcast");
        }
    }

    // Function pointer <-> data pointer (and function -> function): all lower
    // to opaque `ptr`, so convert with a bitcast. Semantic analysis owns
    // signature compatibility; codegen only preserves the address value.
    if ((is_function_type(argm_td) || is_function_type(type_td)) &&
        (is_function_type(argm_td) || argm_td->pointer_depth > 0) &&
        (is_function_type(type_td) || type_td->pointer_depth > 0)) {
        LLVMTypeKind value_kind = LLVMGetTypeKind(LLVMTypeOf(value));
        if (value_kind == LLVMPointerTypeKind || value_kind == LLVMFunctionTypeKind) {
            return LLVMBuildBitCast(env.builder, value, target_type, "fnptrcast");
        }
    }

    // Integer -> Integer
    if (is_integer_type(argm_td) && is_integer_type(type_td)) {
        unsigned src_bits = LLVMGetIntTypeWidth(LLVMTypeOf(value));
        unsigned dst_bits = LLVMGetIntTypeWidth(target_type);
        
        if (src_bits < dst_bits) {
            return is_signed_type(argm_td) 
                ? LLVMBuildSExt(env.builder, value, target_type, "sext")
                : LLVMBuildZExt(env.builder, value, target_type, "zext");
        } else if (src_bits > dst_bits) {
            return LLVMBuildTrunc(env.builder, value, target_type, "trunc");
        }
    }

    // Float -> Float
    if (is_float_type(argm_td) && is_float_type(type_td)) {
        unsigned src_bits = LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMFloatTypeKind ? 32 : 64;
        unsigned dst_bits = LLVMGetTypeKind(target_type) == LLVMFloatTypeKind ? 32 : 64;
        
        if (src_bits < dst_bits) {
            return LLVMBuildFPExt(env.builder, value, target_type, "fpext");
        } else if (src_bits > dst_bits) {
            return LLVMBuildFPTrunc(env.builder, value, target_type, "fptrunc");
        }
    }

    // Integer -> Float
    if (is_integer_type(argm_td) && is_float_type(type_td)) {
        return is_signed_type(argm_td)
            ? LLVMBuildSIToFP(env.builder, value, target_type, "sitofp")
            : LLVMBuildUIToFP(env.builder, value, target_type, "uitofp");
    }

    // Float -> Integer
    if (is_float_type(argm_td) && is_integer_type(type_td)) {
        return is_signed_type(type_td)
            ? LLVMBuildFPToSI(env.builder, value, target_type, "fptosi")
            : LLVMBuildFPToUI(env.builder, value, target_type, "fptoui");
    }

    // Boolean -> Integer
    if (is_boolean_type(argm_td) && is_integer_type(type_td)) {
        return LLVMBuildZExt(env.builder, ensure_i1(env.builder, value), target_type, "zext");
    }

    // Integer -> Boolean
    if (is_integer_type(argm_td) && is_boolean_type(type_td)) {
        return ensure_i1(env.builder, value);
    }

    // Pointer -> Integer
    if (argm_td->pointer_depth > 0 && is_integer_type(type_td)) {
        if (LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMPointerTypeKind) {
            return LLVMBuildPtrToInt(env.builder, value, target_type, "ptrtoint");
        }
    }

    // Integer -> Pointer
    if (is_integer_type(argm_td) && type_td->pointer_depth > 0) {
        return LLVMBuildIntToPtr(env.builder, value, target_type, "inttoptr");
    }

    return value;
}

LLVMValueRef llvm_identifier(Ast* node, bool is_global) {
    if (!node->symbol || !node->symbol->declaration || !node->symbol->declaration->symbol) return NULL;
    Symbol* sym = node->symbol->declaration->symbol;
    if (!sym->llvm_value || !sym->llvm_type) return NULL;

    bool is_memory = LLVMIsAAllocaInst(sym->llvm_value) || LLVMIsAGlobalVariable(sym->llvm_value);

    if (!is_memory) return sym->llvm_value;

    // A global initializer must be a constant and there is no enclosing
    // function to emit a load into, so reuse the referenced global's own
    // initializer instead of building a load.
    if (is_global) {
        if (!LLVMIsAGlobalVariable(sym->llvm_value)) {
            llvm_internal_error(node, "cannot read a local value in a global initializer");
        }
        return LLVMGetInitializer(sym->llvm_value);
    }

    return LLVMBuildLoad2(env.builder, sym->llvm_type, sym->llvm_value, "");
}

LLVMValueRef llvm_selector_address(Ast* node, bool is_global) {
    Ast* object = node->selector_expression.object;
    assert(object != NULL);
    
    if (object->type == AST_IDENTIFIER && object->symbol && object->symbol->declaration && object->symbol->declaration->type == AST_ENUM_DECLARATION) {
        return LLVMConstNull(LLVMPointerTypeInContext(env.context, 0));
    }

    LLVMValueRef base;
    if (object->type == AST_SELECTOR_EXPRESSION) {
        base = llvm_selector_address(object, is_global);
    } else if (object->type == AST_INDEX_EXPRESSION) {
        base = llvm_index_address(object, is_global);
    } else {
        assert(object->symbol != NULL && object->symbol->declaration != NULL);
        Symbol* sym = object->symbol->declaration->symbol;
        assert(sym && sym->llvm_value);
        base = sym->llvm_value;

        bool is_memory = LLVMIsAAllocaInst(base) || LLVMIsAGlobalVariable(base);
        if (!is_memory) {
            LLVMValueRef slot = LLVMBuildAlloca(env.builder, LLVMTypeOf(base), "");
            LLVMBuildStore(env.builder, base, slot);
            base = slot;
        }
    }

    TypeData* obj_td = object->symbol->type;
    bool is_ptr = is_pointer_type(obj_td) && obj_td->pointer_depth > 0;
    LLVMTypeRef  struct_type = is_ptr
        ? TypeRef_pointee(obj_td)
        : TypeRef_of(obj_td);
    assert(struct_type != NULL);
    int index                = node->selector_expression.field_index;
    assert(index >= 0);
    
    if (is_ptr) {
        LLVMValueRef ptr_value = LLVMBuildLoad2(env.builder, LLVMPointerTypeInContext(env.context, 0), base, "");
        assert(ptr_value != NULL);
        base = ptr_value;
    }
    
    LLVMValueRef gep = LLVMBuildStructGEP2(env.builder, struct_type, base, index, "");
    assert(gep != NULL);
    return gep;
}

LLVMValueRef llvm_selector_expression(Ast* node, bool is_global) {
    Ast* object = node->selector_expression.object;
    if (object->type == AST_IDENTIFIER && object->symbol && object->symbol->declaration && object->symbol->declaration->type == AST_ENUM_DECLARATION) {
        Ast* member_decl = node->symbol ? node->symbol->declaration : NULL;
        if (member_decl && member_decl->symbol && member_decl->symbol->llvm_value) {
            return member_decl->symbol->llvm_value;
        }
        return LLVMConstNull(LLVMInt32TypeInContext(env.context));
    }
    // A field selected directly on a temporary value (e.g. `Point{1, 2}.x`).
    // Evaluate the object to a value and extract the field from it.
    if (object->type == AST_LITERAL_COMPOUND || object->type == AST_CALL_EXPRESSION) {
        if (is_global) {
            llvm_internal_error(node, "field of a temporary value in a global initializer is not supported");
        }
        LLVMValueRef value = llvm_expression(object, is_global);
        assert(value != NULL);
        int index = node->selector_expression.field_index;
        assert(index >= 0);
        LLVMValueRef field = LLVMBuildExtractValue(env.builder, value, (unsigned)index, "");
        assert(field != NULL);
        return field;
    }
    LLVMValueRef addr = llvm_selector_address(node, is_global);
    assert(addr != NULL);
    LLVMTypeRef  type = TypeRef_of(llvm_require_symbol_type(node));
    assert(type != NULL);
    LLVMValueRef loaded = LLVMBuildLoad2(env.builder, type, addr, "");
    assert(loaded != NULL);
    return loaded;
}

LLVMValueRef llvm_unary_expression(Ast* node, bool is_global) {
    assert(node != NULL);
    char* op = node->unary_expression.operator;
    assert(op != NULL);
    LLVMValueRef arg = llvm_expression(node->unary_expression.argument, is_global);
    assert(arg != NULL);

    if (strcmp(op, "!") == 0) {
        return LLVMBuildNot(env.builder, ensure_i1(env.builder, arg), "lnot");
    }

    if (strcmp(op, "-") == 0) {
        if (is_float_type(TypeData_of(node))) {
            return LLVMBuildFNeg(env.builder, arg, "fneg");
        }
        return LLVMBuildNeg(env.builder, arg, "neg");
    }

    if (strcmp(op, "&") == 0) {
        Ast* inner = node->unary_expression.argument;
        assert(inner != NULL);

        if (inner->type == AST_SELECTOR_EXPRESSION) {
            LLVMValueRef value = llvm_selector_address(inner, is_global);
            assert(value != NULL);
            return value;
        }

        if (inner->type == AST_INDEX_EXPRESSION) {
            LLVMValueRef value = llvm_index_address(inner, is_global);
            assert(value != NULL);
            return value;
        }

        assert(inner->symbol != NULL);
        assert(inner->symbol->declaration != NULL);
        assert(inner->symbol->declaration->symbol != NULL);
        LLVMValueRef value = inner->symbol->declaration->symbol->llvm_value;
        assert(value != NULL);

        return value;
    }
    
    assert(false);
    return NULL;
}

LLVMValueRef llvm_deref_expression(Ast* node, bool is_global) {
    LLVMValueRef ptr = llvm_expression(node->star_expression.object, is_global);
    assert(ptr != NULL);
    TypeData* pointee = llvm_require_symbol_type(node);
    assert(pointee != NULL);
    LLVMTypeRef pointee_ty = TypeRef_of(pointee);
    assert(pointee_ty != NULL);
    LLVMValueRef loaded = LLVMBuildLoad2(env.builder, pointee_ty, ptr, "deref");
    assert(loaded != NULL);
    return loaded;
}

static LLVMValueRef llvm_object_base_address(Ast* object, bool is_global) {
    assert(object != NULL);
    if (object->type == AST_INDEX_EXPRESSION) {
        LLVMValueRef addr = llvm_index_address(object, is_global);
        return addr;
    }
    if (object->type == AST_SELECTOR_EXPRESSION) {
        return llvm_selector_address(object, is_global);
    }
    if (object->type == AST_STAR_EXPRESSION) {
        return llvm_expression(object->star_expression.object, is_global);
    }

    assert(object->symbol != NULL && object->symbol->declaration != NULL);
    Symbol* sym = object->symbol->declaration->symbol;
    assert(sym && sym->llvm_value);
    LLVMValueRef base = sym->llvm_value;

    bool is_memory = LLVMIsAAllocaInst(base) || LLVMIsAGlobalVariable(base);
    if (!is_memory) {
        LLVMValueRef slot = LLVMBuildAlloca(env.builder, LLVMTypeOf(base), "");
        LLVMBuildStore(env.builder, base, slot);
        base = slot;
    }
    return base;
}

static LLVMValueRef llvm_index_address(Ast* node, bool is_global) {
    Ast* object_node = node->index_expression.object;
    Ast* index_node  = node->index_expression.index;
    assert(object_node != NULL && index_node != NULL);

    LLVMValueRef index = llvm_expression(index_node, is_global);
    assert(index != NULL);
    TypeData* object_td = TypeData_of(object_node);

    LLVMTypeRef i64 = LLVMInt64TypeInContext(env.context);
    LLVMValueRef idx = LLVMBuildIntCast2(env.builder, index, i64, true, "");
    assert(idx != NULL);

    if (object_td->base == ARRAY) {
        LLVMTypeRef array_type = TypeRef_of(object_td);
        assert(array_type != NULL);
        LLVMValueRef base = llvm_object_base_address(object_node, is_global);
        assert(base != NULL);
        LLVMValueRef indices[] = {
            LLVMConstInt(i64, 0, false),
            idx
        };
        LLVMValueRef gep = LLVMBuildGEP2(env.builder, array_type, base, indices, 2, "");
        assert(gep != NULL);
        return gep;
    }

    if (object_td->base == SLICE) {
        TypeData* elem_td = object_td->slice.element_type;
        assert(elem_td != NULL);
        LLVMTypeRef elem_type = TypeRef_of(elem_td);
        assert(elem_type != NULL);
        LLVMValueRef slice = llvm_expression(object_node, is_global);
        assert(slice != NULL);
        LLVMValueRef data = LLVMBuildExtractValue(env.builder, slice, 0, "");
        assert(data != NULL);
        LLVMValueRef gep = LLVMBuildGEP2(env.builder, elem_type, data, &idx, 1, "");
        assert(gep != NULL);
        return gep;
    }

    LLVMTypeRef elem_type = TypeRef_pointee(object_td);
    assert(elem_type != NULL);
    LLVMValueRef ptr = llvm_expression(object_node, is_global);
    assert(ptr != NULL);
    LLVMValueRef gep = LLVMBuildGEP2(env.builder, elem_type, ptr, &idx, 1, "");
    assert(gep != NULL);
    return gep;
}

LLVMValueRef llvm_index_expression(Ast* node, bool is_global) {
    Ast* idx_node = node->index_expression.index;
    if (idx_node && idx_node->type == AST_RANGE_EXPRESSION) {
        Ast* object_node = node->index_expression.object;
        TypeData* object_td = TypeData_of(object_node);
        LLVMTypeRef usize = abi_word_type(env.context);
        LLVMValueRef s = llvm_expression(idx_node->range_expression.start, is_global);
        LLVMValueRef e = llvm_expression(idx_node->range_expression.end, is_global);
        assert(s != NULL && e != NULL);
        LLVMValueRef s64 = LLVMBuildIntCast2(env.builder, s, usize, true, "");
        LLVMValueRef e64 = LLVMBuildIntCast2(env.builder, e, usize, true, "");
        assert(s64 != NULL && e64 != NULL);
        LLVMValueRef len = LLVMBuildSub(env.builder, e64, s64, "slice.len");
        assert(len != NULL);
        LLVMValueRef data_ptr = NULL;
        if (object_td->base == ARRAY) {
            LLVMTypeRef arr_ty = TypeRef_of(object_td);
            assert(arr_ty != NULL);
            LLVMValueRef base = llvm_object_base_address(object_node, is_global);
            assert(base != NULL);
            LLVMValueRef indices[] = { LLVMConstInt(usize, 0, false), s64 };
            data_ptr = LLVMBuildGEP2(env.builder, arr_ty, base, indices, 2, "");
            assert(data_ptr != NULL);
            data_ptr = LLVMBuildPointerCast(env.builder, data_ptr, LLVMPointerTypeInContext(env.context, 0), "");
            assert(data_ptr != NULL);
        } else if (object_td->base == SLICE) {
            LLVMValueRef slice = llvm_expression(object_node, is_global);
            assert(slice != NULL);
            LLVMValueRef data = LLVMBuildExtractValue(env.builder, slice, 0, "");
            assert(data != NULL);
            TypeData* elem_td = object_td->slice.element_type;
            assert(elem_td != NULL);
            LLVMTypeRef elem_type = TypeRef_of(elem_td);
            assert(elem_type != NULL);
            data_ptr = LLVMBuildGEP2(env.builder, elem_type, data, &s64, 1, "");
            assert(data_ptr != NULL);
        } else if (is_pointer_type(object_td)) {
            LLVMTypeRef elem_type = TypeRef_pointee(object_td);
            assert(elem_type != NULL);
            LLVMValueRef ptr = llvm_expression(object_node, is_global);
            assert(ptr != NULL);
            data_ptr = LLVMBuildGEP2(env.builder, elem_type, ptr, &s64, 1, "");
            assert(data_ptr != NULL);
        } else {
            return NULL;
        }
        assert(data_ptr != NULL);
        LLVMTypeRef slice_ty = TypeRef_of(llvm_require_symbol_type(node));
        assert(slice_ty != NULL);
        LLVMValueRef slice_val = LLVMGetUndef(slice_ty);
        assert(slice_val != NULL);
        slice_val = LLVMBuildInsertValue(env.builder, slice_val, data_ptr, 0, "");
        assert(slice_val != NULL);
        slice_val = LLVMBuildInsertValue(env.builder, slice_val, len, 1, "");
        assert(slice_val != NULL);
        return slice_val;
    }
    LLVMTypeRef result_type = TypeRef_of(llvm_require_symbol_type(node));
    assert(result_type != NULL);
    LLVMValueRef addr = llvm_index_address(node, is_global);
    assert(addr != NULL);
    LLVMValueRef loaded = LLVMBuildLoad2(env.builder, result_type, addr, "");
    assert(loaded != NULL);
    return loaded;
}

LLVMValueRef llvm_compound_literal(Ast* node, bool is_global) {
    if (!node->symbol || !node->symbol->declaration) return NULL;

    Ast* decl = node->symbol->declaration;
    int hops = 0;
    while (decl && decl->type == AST_FUNCTION_ALIAS &&
           decl->symbol && decl->symbol->declaration &&
           hops++ < 64) {
        decl = decl->symbol->declaration;
    }

    if (!decl || decl->type != AST_STRUCT_DECLARATION) return NULL;

    Ast** args = node->literal_compound.arguments;
    size_t arg_size = vector_size(args);
    LLVMTypeRef struct_type = decl->symbol->llvm_type;
    if (!struct_type) return NULL;

    if (is_global) {
        LLVMValueRef* const_args = NULL;
        if (arg_size > 0) {
            const_args = c_alloc(sizeof(LLVMValueRef) * arg_size);
            for (size_t i = 0; i < arg_size; i++) {
                const_args[i] = llvm_expression(args[i], true);
                assert(const_args[i] != NULL);
            }
        }
        return LLVMConstNamedStruct(struct_type, const_args, (unsigned)arg_size);
    }

    LLVMValueRef struct_val = LLVMGetUndef(struct_type);
    for (size_t i = 0; i < arg_size; i++) {
        LLVMValueRef arg_val = llvm_expression(args[i], false);
        assert(arg_val != NULL);
        struct_val = LLVMBuildInsertValue(env.builder, struct_val, arg_val, (unsigned)i, "");
    }
    return struct_val;
}

LLVMValueRef llvm_expression(Ast* node, bool is_global) {
    assert(node != NULL);
    switch (node->type) {
        case AST_LITERAL_INTEGER:
        case AST_LITERAL_FLOAT:
        case AST_LITERAL_BOOLEAN:
        case AST_LITERAL_NIL:
        case AST_LITERAL_CHAR:
        case AST_LITERAL_STRING:
        case AST_LITERAL_ARRAY:
        case AST_LITERAL_HEXADECIMAL:
        case AST_LITERAL_BINARY:
            return llvm_literal(node);
        case AST_CAST_EXPRESSION:
            return llvm_cast_expression(node, is_global);
        case AST_IDENTIFIER:
            return llvm_identifier(node, is_global);
        case AST_CALL_EXPRESSION:
            return llvm_call_expression(node, is_global);
        case AST_LITERAL_COMPOUND:
            return llvm_compound_literal(node, is_global);
        case AST_BINARY_EXPRESSION:
            return llvm_binary_expression(node, is_global);
        case AST_UNARY_EXPRESSION:
            return llvm_unary_expression(node, is_global);
        case AST_POSTFIX_EXPRESSION:
            return llvm_postfix_expression(node, is_global);
        case AST_STAR_EXPRESSION:
            return llvm_deref_expression(node, is_global);
        case AST_SELECTOR_EXPRESSION:
            return llvm_selector_expression(node, is_global);
        case AST_INDEX_EXPRESSION:
            return llvm_index_expression(node, is_global);
        default:
            assert(false);
            return NULL;
    }
}

typedef LLVMValueRef (*BinArithFn)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef, const char*);

typedef struct {
    const char* op;
    BinArithFn  int_fn;
    BinArithFn  float_fn;
} ArithOpEntry;

static const ArithOpEntry ARITH_OPS[] = {
    { "+",  LLVMBuildAdd,  LLVMBuildFAdd },
    { "-",  LLVMBuildSub,  LLVMBuildFSub },
    { "*",  LLVMBuildMul,  LLVMBuildFMul },
    { "/",  LLVMBuildSDiv, LLVMBuildFDiv },
    { "%",  LLVMBuildSRem, LLVMBuildFRem },
    { "+=", LLVMBuildAdd,  LLVMBuildFAdd },
    { "-=", LLVMBuildSub,  LLVMBuildFSub },
    { "*=", LLVMBuildMul,  LLVMBuildFMul },
    { "/=", LLVMBuildSDiv, LLVMBuildFDiv },
    { "%=", LLVMBuildSRem, LLVMBuildFRem },
};

static LLVMValueRef llvm_build_pointer_arith(const char* op, LLVMValueRef lhs, LLVMValueRef rhs, TypeData* type) {
    LLVMTypeRef elem_type = TypeRef_pointee(type);
    assert(elem_type != NULL);

    LLVMValueRef base  = lhs;
    LLVMValueRef index = rhs;
    if (LLVMGetTypeKind(LLVMTypeOf(lhs)) != LLVMPointerTypeKind &&
        LLVMGetTypeKind(LLVMTypeOf(rhs)) == LLVMPointerTypeKind) {
        base  = rhs;
        index = lhs;
    }

    index = LLVMBuildIntCast2(env.builder, index, LLVMInt64TypeInContext(env.context), true, "");
    assert(index != NULL);

    if (op[0] == '-') {
        index = LLVMBuildNeg(env.builder, index, "");
        assert(index != NULL);
    }

    LLVMValueRef result = LLVMBuildGEP2(env.builder, elem_type, base, &index, 1, "");
    assert(result != NULL);
    return result;
}

static LLVMValueRef llvm_build_arith(const char* op, LLVMValueRef lhs, LLVMValueRef rhs, TypeData* type) {
    if (is_pointer_type(type) &&
        (strcmp(op, "+") == 0 || strcmp(op, "+=") == 0 ||
         strcmp(op, "-") == 0 || strcmp(op, "-=") == 0)) {
        return llvm_build_pointer_arith(op, lhs, rhs, type);
    }

    bool is_float = is_float_type(type);
    for (size_t i = 0; i < ARRAY_SIZE(ARITH_OPS); i++) {
        if (strcmp(op, ARITH_OPS[i].op) == 0) {
            return is_float
                ? ARITH_OPS[i].float_fn(env.builder, lhs, rhs, "")
                : ARITH_OPS[i].int_fn(env.builder, lhs, rhs, "");
        }
    }
    return NULL;
}

typedef struct {
    const char*       op;
    LLVMIntPredicate  int_pred;
    LLVMRealPredicate float_pred;
} CmpOpEntry;

static const CmpOpEntry CMP_OPS[] = {
    { "==", LLVMIntEQ,  LLVMRealOEQ },
    { "!=", LLVMIntNE,  LLVMRealONE },
    { "<",  LLVMIntSLT, LLVMRealOLT },
    { "<=", LLVMIntSLE, LLVMRealOLE },
    { ">",  LLVMIntSGT, LLVMRealOGT },
    { ">=", LLVMIntSGE, LLVMRealOGE },
};

LLVMValueRef llvm_binary_expression(Ast* node, bool is_global) {
    LLVMValueRef lhs = llvm_expression(node->binary_expression.left, is_global);
    LLVMValueRef rhs = llvm_expression(node->binary_expression.right, is_global);
    assert(lhs != NULL && rhs != NULL);

    const char* op = node->binary_expression.operator;

    LLVMValueRef arith = llvm_build_arith(op, lhs, rhs, TypeData_of(node));
    if (arith != NULL) {
        return arith;
    }

    // Comparisons produce bool, so the operand type (not the node type)
    // decides between integer and float comparison. Semantic guarantees both
    // operands share the same type category.
    bool is_float = is_float_type(TypeData_of(node->binary_expression.left));

    for (size_t i = 0; i < ARRAY_SIZE(CMP_OPS); i++) {
        if (strcmp(op, CMP_OPS[i].op) == 0) {
            return is_float
                ? LLVMBuildFCmp(env.builder, CMP_OPS[i].float_pred, lhs, rhs, "")
                : LLVMBuildICmp(env.builder, CMP_OPS[i].int_pred, lhs, rhs, "");
        }
    }

    if (strcmp(op, "and") == 0) {
        return LLVMBuildAnd(env.builder, lhs, rhs, "");
    }

    if (strcmp(op, "or") == 0) {
        return LLVMBuildOr(env.builder, lhs, rhs, "");
    }

    return NULL;
}

// Applies C default argument promotions for variadic arguments:
// e.g:
//      int printf(const char *format, ...);
//                └───── fixed ─────┘ └─ variadic ─┘
//
// f32 -> f64, signed i8/i16 -> i32, unsigned u8/u16 -> i32.
// Pointer and already-promoted types are passed through unchanged.
static LLVMValueRef promote_vararg_argument(LLVMValueRef value, TypeData* arg_td) {
    assert(value != NULL && arg_td != NULL);
    if (arg_td->pointer_depth > 0) {
        return value;
    }
    switch (arg_td->base) {
        case F32:
            return LLVMBuildFPExt(env.builder, value, LLVMDoubleTypeInContext(env.context), "");
        case I8:
        case I16:
            return LLVMBuildSExt(env.builder, value, LLVMInt32TypeInContext(env.context), "");
        case U8:
        case U16:
            return LLVMBuildZExt(env.builder, value, LLVMInt32TypeInContext(env.context), "");
        default:
            break;
    }

    LLVMTypeKind kind = LLVMGetTypeKind(LLVMTypeOf(value));
    if (kind == LLVMFloatTypeKind) {
        return LLVMBuildFPExt(env.builder, value, LLVMDoubleTypeInContext(env.context), "");
    }
    return value;
}

LLVMValueRef llvm_sizeof(Ast* node) {
    assert(node != NULL);
    LLVMTypeRef type = TypeRef_of(TypeData_of(node));
    assert(type != NULL);
    LLVMTargetDataRef td = LLVMGetModuleDataLayout(env.module);
    uint64_t size = LLVMABISizeOfType(td, type);
    return LLVMConstInt(abi_word_type(env.context), size, false);
}

static LLVMValueRef try_get_struct_ptr(Ast* node, bool is_global) {
    if (!node) return NULL;
    if (node->type == AST_IDENTIFIER) {
        if (!node->symbol || !node->symbol->declaration || !node->symbol->declaration->symbol) return NULL;
        Symbol* sym = node->symbol->declaration->symbol;
        LLVMValueRef v = sym->llvm_value;
        if (!v) return NULL;
        if (LLVMIsAAllocaInst(v) || LLVMIsAGlobalVariable(v)) return v;
        return NULL;
    }
    if (node->type == AST_SELECTOR_EXPRESSION) {
        return llvm_selector_address(node, is_global);
    }
    if (node->type == AST_INDEX_EXPRESSION) {
        TypeData* td = TypeData_of(node);
        if (type_is_struct(td)) return llvm_index_address(node, is_global);
        return NULL;
    }
    if (node->type == AST_STAR_EXPRESSION) {
        return llvm_expression(node->star_expression.object, is_global);
    }
    return NULL;
}

static LLVMTypeRef llvm_function_type_from_type_data(TypeData* fn_td) {
    assert(fn_td != NULL);
    size_t n = fn_td->function.param_count;
    TypeData* ret_td = fn_td->function.return_type;

    bool uses_sret = abi_return_is_sret(ret_td);
    size_t type_cap = n + (uses_sret ? 1 : 0) + 4;
    LLVMTypeRef* lowered = c_alloc(sizeof(LLVMTypeRef) * (type_cap ? type_cap : 1));
    size_t lcount = 0;

    LLVMTypeRef ret_type = TypeRef_of(ret_td);
    if (uses_sret) {
        assert(ret_type != NULL);
        lowered[lcount++] = pointer_to(NULL);
        ret_type = LLVMVoidTypeInContext(env.context);
    } else if (type_is_struct(ret_td)) {
        AbiInfo info = abi_info_of_type(ret_type);
        ret_type = abi_ir_return_type(env.context, &info);
    }

    for (size_t i = 0; i < n; i++) {
        TypeData* td = fn_td->function.param_types[i];
        assert(td != NULL);
        if (!type_is_struct(td)) {
            lowered[lcount++] = TypeRef_of(td);
            continue;
        }
        LLVMTypeRef stype = TypeRef_of(td);
        assert(stype != NULL);
        AbiInfo info = abi_info_of_type(stype);
        if (info.cls == ABI_CLASS_MEMORY) {
            lowered[lcount++] = pointer_to(NULL);
        } else {
            for (int c = 0; c < info.chunk_count && c < 4; c++) {
                lowered[lcount++] = abi_chunk_type(stype, &info.chunks[c]);
            }
        }
    }

    for (size_t i = 0; i < lcount; i++) {
        assert(lowered[i] != NULL);
    }
    assert(ret_type != NULL);

    LLVMTypeRef fnc_type = LLVMFunctionType(ret_type, lowered, (unsigned)lcount, false);
    assert(fnc_type != NULL);
    return fnc_type;
}

static LLVMValueRef llvm_coerce_function_arg(LLVMValueRef v) {
    if (v && LLVMTypeOf(v) &&
        LLVMGetTypeKind(LLVMTypeOf(v)) == LLVMFunctionTypeKind) {
        return LLVMBuildBitCast(env.builder, v, LLVMPointerTypeInContext(env.context, 0), "");
    }
    return v;
}

LLVMValueRef llvm_call_expression(Ast* node, bool is_global) {
    assert(node != NULL);
    Ast* callee = node->call_expression.callee;
    Ast** args  = node->call_expression.arguments;
    const char* iname = identifier_name(node);

    // Handle builtin function calls (e.g: sizeof,...)
    // Note: More builtins will be added in the future. ᕕ( ᐛ )ᕗ 
    if (iname && strcmp(iname,"sizeof") == 0) {
        assert(args != NULL && vector_size(args) > 0);
        return llvm_sizeof(args[0]);
    }

    size_t arg_size = vector_size(args);

    if (!node->symbol || !node->symbol->declaration) return NULL;
    Ast* decl = node->symbol->declaration;
    int hops = 0;
    while (decl && decl->type == AST_FUNCTION_ALIAS &&
           decl->symbol && decl->symbol->declaration &&
           hops++ < 64) {
        decl = decl->symbol->declaration;
    }

    bool is_indirect = decl->type != AST_FUNCTION_DECLARATION &&
                       decl->type != AST_FUNCTION_DEFINITION;

    if (!is_indirect) {
        if (!decl->symbol) return NULL;
        if (!decl->symbol->llvm_value) return NULL;
        if (!decl->symbol->llvm_type) return NULL;
    }

    LLVMValueRef fnc_value;
    LLVMTypeRef  fnc_type;
    if (is_indirect) {
        fnc_value = llvm_expression(callee, is_global);
        assert(fnc_value != NULL);
        fnc_type  = llvm_function_type_from_type_data(TypeData_of(callee));
        assert(fnc_type != NULL);
    } else {
        fnc_value = decl->symbol->llvm_value;
        fnc_type  = decl->symbol->llvm_type;
    }
    assert(fnc_value != NULL && fnc_type != NULL);

    size_t fixed_param_count = is_indirect ? 0 : vector_size(decl->function_declaration.params);
    int has_ellipsis = is_indirect ? 0 : decl->function_declaration.has_ellipsis;

    TypeData* ret_td = TypeData_of(node);
    bool ret_struct = type_is_struct(ret_td);
    bool ret_sret   = false;
    AbiInfo ret_info;
    LLVMTypeRef ret_stype = NULL;
    if (ret_struct) {
        ret_stype = TypeRef_of(ret_td);
        ret_info  = abi_info_of_type(ret_stype);
        ret_sret  = (ret_info.cls == ABI_CLASS_MEMORY);
    }

    size_t cap = arg_size + (ret_sret ? 1 : 0) + 8;
    LLVMValueRef* llvm_args = c_alloc(sizeof(LLVMValueRef) * (cap ? cap : 1));
    size_t lcount = 0;

    LLVMValueRef sret_buffer = NULL;
    if (ret_sret) {
        sret_buffer = LLVMBuildAlloca(env.builder, ret_stype, "sret");
        assert(sret_buffer != NULL);
        llvm_args[lcount++] = sret_buffer;
    }

    // Lower Arguments according to Target ABI Rules
    for (size_t i = 0; i < arg_size; i++) {
        bool is_variadic = has_ellipsis && i >= fixed_param_count;
        TypeData* arg_td = TypeData_of(args[i]);
        if (!type_is_struct(arg_td)) {
            LLVMValueRef arg_val = llvm_expression(args[i], is_global);
            assert(arg_val != NULL);
            if (is_variadic) {
                arg_val = llvm_coerce_function_arg(promote_vararg_argument(arg_val, arg_td));
            } else {
                arg_val = llvm_coerce_function_arg(arg_val);
            }
            llvm_args[lcount++] = arg_val;
            continue;
        }
        if (is_variadic) {
            LLVMValueRef arg_val = llvm_expression(args[i], is_global);
            assert(arg_val != NULL);
            llvm_args[lcount++] = llvm_coerce_function_arg(promote_vararg_argument(arg_val, arg_td));
            continue;
        }

        LLVMTypeRef arg_stype = TypeRef_of(arg_td);
        assert(arg_stype != NULL);
        AbiInfo info = abi_info_of_type(arg_stype);
        if (info.cls == ABI_CLASS_MEMORY) {
            LLVMValueRef ptr = try_get_struct_ptr(args[i], is_global);
            if (ptr) {
                llvm_args[lcount++] = ptr;
            } else {
                LLVMValueRef arg_val = llvm_expression(args[i], is_global);
                assert(arg_val != NULL);
                AbiValue av = abi_pull_chunks(env.builder, env.module, arg_stype, &info, arg_val);
                llvm_args[lcount++] = av.chunks[0];
            }
            continue;
        }
        LLVMValueRef ptr = try_get_struct_ptr(args[i], is_global);
        AbiValue av;
        if (ptr) {
            av = abi_load_chunks_from_ptr(env.builder, env.module, arg_stype, &info, ptr);
        } else {
            LLVMValueRef arg_val = llvm_expression(args[i], is_global);
            assert(arg_val != NULL);
            av = abi_extract_chunks_from_value(env.builder, env.module, arg_stype, &info, arg_val);
        }
        for (int c = 0; c < av.count && c < 2; c++) {
            assert(av.chunks[c] != NULL);
            llvm_args[lcount++] = av.chunks[c];
        }
    }

    LLVMValueRef value = LLVMBuildCall2(env.builder, fnc_type, fnc_value, llvm_args, (unsigned)lcount, "");
    assert(value != NULL);

    if (!ret_struct) {
        return value;
    }
    if (ret_sret) {
        LLVMValueRef loaded = LLVMBuildLoad2(env.builder, ret_stype, sret_buffer, "");
        assert(loaded != NULL);
        return loaded;
    }

    AbiValue av = { {0}, 0 };
    if (ret_info.chunk_count == 2 &&
        LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMStructTypeKind) {
        av.chunks[0] = LLVMBuildExtractValue(env.builder, value, 0, "");
        av.chunks[1] = LLVMBuildExtractValue(env.builder, value, 1, "");
        av.count = 2;
    } else {
        av.chunks[0] = value;
        av.count = 1;
    }
    return abi_rebuild_struct(env.builder, env.module, ret_stype, &ret_info, &av);
}

LLVMTypeRef llvm_struct_declare(Ast* node) {
    assert(node != NULL);
    assert(node->struct_declaration.name != NULL);
    const char* struct_name = identifier_name(node->struct_declaration.name);
    if (node->symbol && node->symbol->name) {
        struct_name = node->symbol->name;
    }
    assert(struct_name != NULL);

    char str[512];
    if (ast_has_attribute(node, "@private")) {
        snprintf(str, sizeof(str), "struct.%s", struct_name);
    } else if (strchr(struct_name, '.')) {
        const char* dot = strchr(struct_name, '.');
        size_t parent_len = dot - struct_name;
        char parent[256];
        if (parent_len >= sizeof(parent)) parent_len = sizeof(parent)-1;
        memcpy(parent, struct_name, parent_len);
        parent[parent_len] = '\0';
        const char* base = dot + 1;
        snprintf(str, sizeof(str), "%s.struct.%s", parent, base);
    } else {
        snprintf(str, sizeof(str), "struct.%s", struct_name);
    }

    LLVMTypeRef struct_type = LLVMStructCreateNamed(env.context, str);
    assert(struct_type != NULL);

    assert(node->symbol != NULL);
    StructReference ref = { node->symbol->type, struct_type };
    vector_add(&struct_references, ref);
    
    set_reference(node , NULL, struct_type);
    
    return struct_type;
}

void llvm_struct_define(Ast* node) {
    assert(node != NULL && node->symbol != NULL);
    Ast** fields = node->struct_declaration.fields;
    size_t field_size = vector_size(fields);

    if (field_size == 0) return;
    assert(fields != NULL);

    LLVMTypeRef struct_type = TypeRef_of(node->symbol->type);
    assert(struct_type != NULL);

    LLVMTypeRef* field_types = c_alloc(sizeof(LLVMTypeRef) * field_size);
    for (size_t i = 0; i < field_size; i++) {
        field_types[i] = TypeRef_of(TypeData_of(fields[i]));
        assert(field_types[i] != NULL);
    }

    LLVMStructSetBody(struct_type, field_types, field_size, false);
    
}

LLVMValueRef llvm_postfix_expression(Ast* node, bool is_global) {
    assert(node != NULL);
    Ast* arg = node->postfix_expression.argument;
    const char* op = node->postfix_expression.operator;
    assert(arg != NULL && op != NULL);

    LLVMValueRef addr = llvm_object_base_address(arg, is_global);
    assert(addr != NULL);
    TypeData* td = TypeData_of(node);
    LLVMTypeRef ty = TypeRef_of(td);
    assert(ty != NULL);
    LLVMValueRef old_value = LLVMBuildLoad2(env.builder, ty, addr, "");
    assert(old_value != NULL);

    if (is_pointer_type(td)) {
        LLVMValueRef one = LLVMConstInt(LLVMInt64TypeInContext(env.context), 1, true);
        LLVMValueRef updated = strcmp(op, "++") == 0
            ? llvm_build_pointer_arith("+", old_value, one, td)
            : llvm_build_pointer_arith("-", old_value, one, td);
        LLVMBuildStore(env.builder, updated, addr);
        return old_value;
    }

    LLVMValueRef one = is_float_type(td)
        ? LLVMConstReal(ty, 1.0)
        : LLVMConstInt(ty, 1, is_signed_type(td));

    LLVMValueRef updated = (strcmp(op, "++") == 0)
        ? (is_float_type(td) ? LLVMBuildFAdd(env.builder, old_value, one, "")
                             : LLVMBuildAdd(env.builder, old_value, one, ""))
        : (is_float_type(td) ? LLVMBuildFSub(env.builder, old_value, one, "")
                             : LLVMBuildSub(env.builder, old_value, one, ""));

    LLVMBuildStore(env.builder, updated, addr);
    return old_value;
}

LLVMValueRef llvm_assignment_expression(Ast* node, bool is_global) {
    assert(node != NULL);
    Ast* left  = node->assignment_expression.left;
    Ast* right = node->assignment_expression.right;
    assert(left != NULL && right != NULL);
    const char* op = node->assignment_expression.operator
        ? node->assignment_expression.operator : "=";

    LLVMValueRef left_value = llvm_object_base_address(left, is_global);
    assert(left_value != NULL);

    TypeData* left_td  = TypeData_of(left);
    TypeData* right_td = TypeData_of(right);
    LLVMValueRef right_value;

    if (left_td->base == SLICE && right_td->base == ARRAY) {
        LLVMTypeRef slice_type = TypeRef_of(left_td);
        LLVMValueRef base = llvm_object_base_address(right, is_global);
        LLVMValueRef idx = LLVMConstInt(LLVMInt64TypeInContext(env.context), 0, false);
        LLVMValueRef data = LLVMBuildGEP2(env.builder, TypeRef_of(right_td), base, &idx, 1, "");
        LLVMValueRef ptr = LLVMBuildPointerCast(env.builder, data,
            LLVMPointerTypeInContext(env.context, 0), "");
        LLVMValueRef len = LLVMConstInt(abi_word_type(env.context),
            (unsigned long long)right_td->array.length, false);
        right_value = LLVMGetUndef(slice_type);
        right_value = LLVMBuildInsertValue(env.builder, right_value, ptr, 0, "");
        right_value = LLVMBuildInsertValue(env.builder, right_value, len, 1, "");
    }
    else {
        right_value = llvm_expression(right, is_global);
    }

    if (is_function_type(left_td) && right_value &&
        LLVMGetTypeKind(LLVMTypeOf(right_value)) == LLVMFunctionTypeKind) {
        right_value = LLVMBuildBitCast(env.builder, right_value, TypeRef_of(left_td), "");
    }

    if (strcmp(op, "=") != 0) {
        LLVMValueRef lhs_value = LLVMBuildLoad2(env.builder, TypeRef_of(left_td), left_value, "");
        assert(lhs_value != NULL);
        right_value = llvm_build_arith(op, lhs_value, right_value, left_td);
        if (right_value == NULL) return NULL;
    }

    assert(right_value != NULL);
    LLVMBuildStore(env.builder, right_value, left_value);

    return right_value;
}


void llvm_statement(Ast* stmt, bool is_global) {
    assert(stmt != NULL);
    switch (stmt->type) {

        case AST_BLOCK_STATEMENT:
            llvm_block(stmt, is_global);
            break;
        case AST_VARIABLE_DECLARATION:
            llvm_variable_declaration(stmt, is_global);
            break;
        case AST_RETURN_STATEMENT:
            llvm_return_statement(stmt, is_global);
            break;
        case AST_CALL_EXPRESSION:
            llvm_call_expression(stmt, is_global);
            break;
        case AST_LITERAL_COMPOUND:
            llvm_compound_literal(stmt, is_global);
            break;
        case AST_ASSIGNMENT_EXPRESSION:
            llvm_assignment_expression(stmt, is_global);
            break;
        case AST_POSTFIX_EXPRESSION:
            llvm_postfix_expression(stmt, is_global);
            break;
        case AST_IF_STATEMENT:
            llvm_if_statement(stmt, is_global);
            break;
        case AST_LOOP_STATEMENT:
            llvm_loop_statement(stmt, is_global);
            break;
        case AST_BREAK_STATEMENT:
            llvm_break_statement(stmt, is_global);
            break;
        case AST_CONTINUE_STATEMENT:
            llvm_continue_statement(stmt, is_global);
            break;
        case AST_FUNCTION_DECLARATION:
        case AST_FUNCTION_DEFINITION:
            llvm_function_declaration(stmt, is_global);
            break;
        case AST_FUNCTION_ALIAS:
            llvm_function_alias(stmt, is_global);
            break;
        case AST_USING_DECLARATION:
            break;
        case AST_ENUM_DECLARATION:
            llvm_enum_declaration(stmt);
            break;
        case AST_GOTO_STATEMENT:
            llvm_goto_statement(stmt);
            break;
        case AST_LABEL_STATEMENT:
            llvm_label_statement(stmt);
            break;
        default:
            break;
    }
}

void llvm_block(Ast* node, bool is_global) {
    assert(node != NULL);
    Ast** stmts = node->block.statements;

    for (size_t i = 0; i < vector_size(stmts); i++) {
        Ast* stmt = stmts[i];
        LLVMBasicBlockRef current = LLVMGetInsertBlock(env.builder);
        if (current && 
                LLVMGetBasicBlockTerminator(current) && 
                    stmt->type != AST_LABEL_STATEMENT) 
                        continue;

        llvm_statement(stmt, is_global);
    }
}

void llvm_root(Ast* node) {
    assert(node != NULL);
    Ast** stmts = node->block.statements;
    size_t count = vector_size(stmts);

    for (size_t i = 0; i < count; i++) {
        Ast* stmt = stmts[i];
        if (stmt->type == AST_ENUM_DECLARATION) {
            llvm_enum_declaration(stmt);
        }
    }

    for (size_t i = 0; i < count; i++) {
        Ast* stmt = stmts[i];
        if (stmt->type == AST_STRUCT_DECLARATION) {
            llvm_struct_declare(stmt);
        }
    }

    for (size_t i = 0; i < count; i++) {
        Ast* stmt = stmts[i];
        if (stmt->type == AST_STRUCT_DECLARATION) {
            llvm_struct_define(stmt);
        }
    }

    for (size_t i = 0; i < count; i++) {
        Ast* stmt = stmts[i];
        switch (stmt->type) {
            case AST_FUNCTION_DECLARATION:
            case AST_FUNCTION_DEFINITION:
                llvm_function_declaration(stmt, true);
                break;
            default:
                break;
        }
    }

    // Resolve function aliases to their targets. This must run after all
    // function declarations so that target LLVM values already exist.
    for (size_t i = 0; i < count; i++) {
        Ast* stmt = stmts[i];
        if (stmt->type == AST_FUNCTION_ALIAS) {
            llvm_function_alias(stmt, true);
        }
    }

    for (size_t i = 0; i < count; i++) {
        Ast* stmt = stmts[i];
        switch (stmt->type) {
            case AST_VARIABLE_DECLARATION:
                llvm_variable_declaration(stmt, true);
                break;
            default:
                break;
        }
    }

    for (size_t i = 0; i < count; i++) {
        Ast* stmt = stmts[i];
        if (stmt->type == AST_FUNCTION_DEFINITION) {
            llvm_function_body(stmt);
        }
    }
}

LLVMModuleRef llvm_generate(Ast* root, CLI cli) {
    llvm_init(root, cli);

    if (root->type != AST_ROOT) {
        exit(EXIT_FAILURE);
    }

    llvm_root(root);

    if (!llvm_verify(env.module)) {
        assert(false);
    }

    LLVMDisposeBuilder(env.builder);

    return env.module;
}
