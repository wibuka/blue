#include "abi.h"
#include "llvm-c/Target.h"

static LLVMTypeRef pointer_to_for_module(LLVMModuleRef module, LLVMTypeRef hint) {
    LLVMContextRef ctx = NULL;
    if (hint) ctx = LLVMGetTypeContext(hint);
    else if (module) ctx = LLVMGetModuleContext(module);
    else { assert(module || hint); ctx = module ? LLVMGetModuleContext(module) : LLVMGetTypeContext(hint); }
    return LLVMPointerTypeInContext(ctx, 0);
}

LLVMTypeRef abi_chunk_type(LLVMTypeRef elem, const AbiChunkInfo* chunk) {
    LLVMContextRef ctx = LLVMGetTypeContext(elem);
    (void)chunk;
    if (LLVMGetTypeKind(elem) == LLVMFloatTypeKind) {
        return LLVMFloatTypeInContext(ctx);
    }
    if (LLVMGetTypeKind(elem) == LLVMDoubleTypeKind) {
        return LLVMDoubleTypeInContext(ctx);
    }
    if (chunk && chunk->is_float) {
        return chunk->size <= 4 ? LLVMFloatTypeInContext(ctx)
                                : LLVMDoubleTypeInContext(ctx);
    }
    uint64_t sz = chunk ? chunk->size : 8;
    if (sz <= 1)  return LLVMInt8TypeInContext(ctx);
    if (sz <= 2)  return LLVMInt16TypeInContext(ctx);
    if (sz <= 4)  return LLVMInt32TypeInContext(ctx);
    return LLVMInt64TypeInContext(ctx);
}

LLVMTypeRef abi_ir_return_type(LLVMContextRef ctx, const AbiInfo* info) {
    if (info->cls == ABI_CLASS_MEMORY) {
        return LLVMVoidTypeInContext(ctx);
    }
    if (info->chunk_count == 1) {
        LLVMTypeRef dummy = LLVMInt64TypeInContext(ctx);
        return abi_chunk_type(dummy, &info->chunks[0]);
    }
    if (info->chunk_count == 2) {
        LLVMTypeRef elems[2];
        LLVMTypeRef dummy = LLVMInt64TypeInContext(ctx);
        elems[0] = abi_chunk_type(dummy, &info->chunks[0]);
        elems[1] = abi_chunk_type(dummy, &info->chunks[1]);
        return LLVMStructTypeInContext(ctx, elems, 2, false);
    }
    return LLVMVoidTypeInContext(ctx);
}

static LLVMValueRef abi_byte_gep_impl(LLVMBuilderRef builder, LLVMModuleRef module, LLVMValueRef ptr, uint64_t bytes) {
    LLVMContextRef ctx = module ? LLVMGetModuleContext(module) : LLVMGetTypeContext(LLVMTypeOf(ptr));
    LLVMTypeRef i8  = LLVMInt8TypeInContext(ctx);
    LLVMTypeRef i64 = LLVMInt64TypeInContext(ctx);
    LLVMValueRef off = LLVMConstInt(i64, bytes, false);
    LLVMTypeRef pti8 = pointer_to_for_module(module, i8);
    LLVMValueRef base = LLVMBuildBitCast(builder, ptr, pti8, "");
    return LLVMBuildGEP2(builder, i8, base, &off, 1, "");
}

LLVMValueRef abi_chunk_address(LLVMBuilderRef builder, LLVMModuleRef module, LLVMTypeRef struct_ty, LLVMValueRef ptr, uint64_t offset) {
    LLVMTargetDataRef td = LLVMGetModuleDataLayout(module);
    unsigned nf = LLVMCountStructElementTypes(struct_ty);
    for (unsigned i = 0; i < nf; i++) {
        uint64_t field_off = LLVMOffsetOfElement(td, struct_ty, i);
        if (field_off == offset) {
            return LLVMBuildStructGEP2(builder, struct_ty, ptr, i, "");
        }
    }
    return abi_byte_gep_impl(builder, module, ptr, offset);
}

AbiValue abi_load_chunks_from_ptr(LLVMBuilderRef builder, LLVMModuleRef module, LLVMTypeRef struct_ty, const AbiInfo *info, LLVMValueRef ptr) {
    AbiValue out = { { NULL, NULL, NULL, NULL }, 0 };
    if (info->cls == ABI_CLASS_MEMORY) {
        out.chunks[0] = ptr;
        out.count = 1;
        return out;
    }
    for (int i = 0; i < info->chunk_count && i < 4; i++) {
        LLVMTypeRef cty = abi_chunk_type(struct_ty, &info->chunks[i]);
        LLVMValueRef addr = abi_chunk_address(builder, module, struct_ty, ptr, info->chunks[i].offset);
        LLVMTypeRef pt = pointer_to_for_module(module, cty);
        addr = LLVMBuildBitCast(builder, addr, pt, "");
        out.chunks[i] = LLVMBuildLoad2(builder, cty, addr, "");
    }
    out.count = info->chunk_count;
    return out;
}

AbiValue abi_extract_chunks_from_value(LLVMBuilderRef builder, LLVMModuleRef module, LLVMTypeRef struct_ty, const AbiInfo *info, LLVMValueRef struct_val) {
    AbiValue out = { { NULL, NULL, NULL, NULL }, 0 };
    if (info->cls == ABI_CLASS_MEMORY) {
        LLVMValueRef temp = LLVMBuildAlloca(builder, struct_ty, "byval.tmp");
        LLVMBuildStore(builder, struct_val, temp);
        out.chunks[0] = temp;
        out.count = 1;
        return out;
    }
    LLVMTypeRef val_ty = LLVMTypeOf(struct_val);
    if (LLVMGetTypeKind(val_ty) == LLVMStructTypeKind) {
        unsigned nf = LLVMCountStructElementTypes(struct_ty);
        if ((int)nf == info->chunk_count && nf <= 4) {
            LLVMTargetDataRef td = LLVMGetModuleDataLayout(module);
            int can_extract = 1;
            for (int i = 0; i < info->chunk_count; i++) {
                uint64_t field_off = LLVMOffsetOfElement(td, struct_ty, i);
                if (field_off != info->chunks[i].offset) { can_extract = 0; break; }
            }
            if (can_extract) {
                for (int i = 0; i < info->chunk_count; i++) {
                    out.chunks[i] = LLVMBuildExtractValue(builder, struct_val, i, "");
                }
                out.count = info->chunk_count;
                return out;
            }
        }
        if (info->chunk_count == 1) {
            LLVMTypeRef sole = abi_chunk_type(struct_ty, &info->chunks[0]);
            if (val_ty == sole) {
                out.chunks[0] = struct_val;
                out.count = 1;
                return out;
            }
            LLVMTargetDataRef td = LLVMGetModuleDataLayout(module);
            uint64_t struct_sz = LLVMABISizeOfType(td, struct_ty);
            uint64_t chunk_sz = LLVMABISizeOfType(td, sole);
            if (struct_sz == chunk_sz) {
                LLVMValueRef slot = LLVMBuildAlloca(builder, struct_ty, "abi.coerce");
                LLVMBuildStore(builder, struct_val, slot);
                LLVMValueRef addr = abi_chunk_address(builder, module, struct_ty, slot, info->chunks[0].offset);
                LLVMTypeRef pt = pointer_to_for_module(module, sole);
                addr = LLVMBuildBitCast(builder, addr, pt, "");
                out.chunks[0] = LLVMBuildLoad2(builder, sole, addr, "");
                out.count = 1;
                return out;
            }
        }
    } else if (info->chunk_count == 1) {
        LLVMTypeRef sole = abi_chunk_type(struct_ty, &info->chunks[0]);
        if (val_ty == sole) {
            out.chunks[0] = struct_val;
            out.count = 1;
            return out;
        }
    }
    LLVMValueRef slot = LLVMBuildAlloca(builder, struct_ty, "abi.arg");
    LLVMBuildStore(builder, struct_val, slot);
    for (int i = 0; i < info->chunk_count && i < 4; i++) {
        LLVMTypeRef cty = abi_chunk_type(struct_ty, &info->chunks[i]);
        LLVMValueRef addr = abi_chunk_address(builder, module, struct_ty, slot, info->chunks[i].offset);
        LLVMTypeRef pt = pointer_to_for_module(module, cty);
        addr = LLVMBuildBitCast(builder, addr, pt, "");
        out.chunks[i] = LLVMBuildLoad2(builder, cty, addr, "");
    }
    out.count = info->chunk_count;
    return out;
}

AbiValue abi_pull_chunks(LLVMBuilderRef builder, LLVMModuleRef module, LLVMTypeRef struct_ty, const AbiInfo *info, LLVMValueRef struct_val) {
    return abi_extract_chunks_from_value(builder, module, struct_ty, info, struct_val);
}

LLVMValueRef abi_rebuild_struct(LLVMBuilderRef builder, LLVMModuleRef module, LLVMTypeRef struct_ty, const AbiInfo *info, const AbiValue *input) {
    if (info->cls == ABI_CLASS_MEMORY) {
        return LLVMBuildLoad2(builder, struct_ty, input->chunks[0], "");
    }
    LLVMValueRef slot = LLVMBuildAlloca(builder, struct_ty, "abi.ret");
    if (info->chunk_count == 1) {
        LLVMTypeRef cty = abi_chunk_type(struct_ty, &info->chunks[0]);
        LLVMValueRef addr = abi_chunk_address(builder, module, struct_ty, slot, info->chunks[0].offset);
        LLVMTypeRef pt = pointer_to_for_module(module, cty);
        addr = LLVMBuildBitCast(builder, addr, pt, "");
        LLVMBuildStore(builder, input->chunks[0], addr);
    } else {
        for (int i = 0; i < info->chunk_count && i < 4; i++) {
            LLVMTypeRef cty = abi_chunk_type(struct_ty, &info->chunks[i]);
            LLVMValueRef addr = abi_chunk_address(builder, module, struct_ty, slot, info->chunks[i].offset);
            LLVMTypeRef pt = pointer_to_for_module(module, cty);
            addr = LLVMBuildBitCast(builder, addr, pt, "");
            LLVMBuildStore(builder, input->chunks[i], addr);
        }
    }
    return LLVMBuildLoad2(builder, struct_ty, slot, "");
}
