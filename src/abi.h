#pragma once

typedef struct LLVMOpaqueTargetData *LLVMTargetDataRef;
typedef struct LLVMOpaqueType *LLVMTypeRef;
typedef struct LLVMOpaqueContext *LLVMContextRef;
typedef struct LLVMOpaqueBuilder *LLVMBuilderRef;
typedef struct LLVMOpaqueModule *LLVMModuleRef;
typedef struct LLVMOpaqueValue *LLVMValueRef;

typedef enum {
    ABI_UNKNOWN,
    ABI_SYSV_AMD64,
    ABI_SYSV_X86,
    ABI_MS_X64,
    ABI_AAPCS64,
} AbiKind;

typedef enum {
    ABI_CLASS_NOT_STRUCT,
    ABI_CLASS_INTEGER,
    ABI_CLASS_SSE,
    ABI_CLASS_MEMORY,
} AbiClass;

typedef struct AbiChunkInfo {
    uint64_t    offset;
    uint64_t    size;
    bool        is_float;
    LLVMTypeRef type;
} AbiChunkInfo;

typedef struct AbiInfo {
    AbiClass    cls;
    uint64_t    size;
    unsigned    align;
    AbiChunkInfo chunks[4];
    int         chunk_count;
} AbiInfo;

int get_bitness(void);

void abi_set_target(AbiKind kind, LLVMTargetDataRef dl);
void abi_shutdown(void);

AbiKind abi_kind_for_triple(const char *triple);
const char *abi_native_datalayout(void);
void abi_init_native(void);

#define EIGHTBYTE_SIZE 8ULL

uint64_t abi_pointer_bits(void);
LLVMTypeRef abi_word_type(LLVMContextRef ctx);

AbiInfo abi_analyze(LLVMContextRef ctx, LLVMTypeRef type);

int  abi_param_count(const AbiInfo *info);
bool abi_is_byval(LLVMContextRef ctx, LLVMTypeRef type);
bool abi_is_sret(LLVMContextRef ctx, LLVMTypeRef type);

LLVMTypeRef abi_chunk_type(LLVMTypeRef elem, const AbiChunkInfo *chunk);
LLVMTypeRef abi_ir_return_type(LLVMContextRef ctx, const AbiInfo *info);
LLVMValueRef abi_coerce_value(LLVMBuilderRef builder, LLVMTypeRef to, LLVMValueRef value);

typedef struct AbiValue {
    LLVMValueRef chunks[4];
    int          count;
} AbiValue;

LLVMValueRef abi_chunk_address(LLVMBuilderRef builder, LLVMModuleRef module, LLVMTypeRef struct_ty, LLVMValueRef ptr, uint64_t offset);
AbiValue abi_load_chunks_from_ptr(LLVMBuilderRef builder, LLVMModuleRef module, LLVMTypeRef struct_ty, const AbiInfo *info, LLVMValueRef ptr);
AbiValue abi_extract_chunks_from_value(LLVMBuilderRef builder, LLVMModuleRef module, LLVMTypeRef struct_ty, const AbiInfo *info, LLVMValueRef struct_val);
AbiValue abi_pull_chunks(LLVMBuilderRef builder, LLVMModuleRef module, LLVMTypeRef struct_ty, const AbiInfo *info, LLVMValueRef struct_val);
LLVMValueRef abi_rebuild_struct(LLVMBuilderRef builder, LLVMModuleRef module, LLVMTypeRef struct_ty, const AbiInfo *info, const AbiValue *input);
