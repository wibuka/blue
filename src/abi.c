#include "abi.h"
#include "llvm-c/Core.h"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"

#define EIGHTBYTE_SIZE 8ULL

static int g_word_bits = 64;

int get_bitness(void) {
    return g_word_bits;
}

static LLVMTargetDataRef  target_data       = NULL;
static AbiKind            abi_kind          = ABI_UNKNOWN;
static char*              native_datalayout = NULL;

AbiKind abi_kind_for_triple(const char* triple) {
    if (!triple) return ABI_UNKNOWN;

    if (strstr(triple, "x86_64") || strstr(triple, "amd64")) {
        if (strstr(triple, "windows") ||
            strstr(triple, "win32")   ||
            strstr(triple, "mingw")   ||
            strstr(triple, "msvc")) {
            return ABI_MS_X64;
        }
        return ABI_SYSV_AMD64;
    }

    if (strstr(triple, "aarch64") ||
        strstr(triple, "arm64")) {
        return ABI_AAPCS64;
    }

    if (strstr(triple, "i386") ||
        strstr(triple, "i486") ||
        strstr(triple, "i586") ||
        strstr(triple, "i686")) {
        return ABI_SYSV_X86;
    }

    return ABI_UNKNOWN;
}

const char* abi_native_datalayout(void) {
    return native_datalayout;
}

uint64_t abi_pointer_bits(void) {
    if (!target_data) return 0;
    return (uint64_t)LLVMPointerSize(target_data) * 8;
}

LLVMTypeRef abi_word_type(LLVMContextRef ctx) {
    uint64_t bits = abi_pointer_bits();
    return bits == 32 ? LLVMInt32TypeInContext(ctx)
                      : LLVMInt64TypeInContext(ctx);
}

void abi_init_native(void) {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();

    char *triple = LLVMGetDefaultTargetTriple();
    LLVMTargetRef target;
    char *error = NULL;

    if (LLVMGetTargetFromTriple(triple, &target, &error)) {
        LLVMDisposeMessage(error);
        LLVMDisposeMessage(triple);
        return;
    }

    LLVMTargetMachineRef tm = LLVMCreateTargetMachine(
        target, triple, "generic", "", LLVMCodeGenLevelDefault,
        LLVMRelocPIC, LLVMCodeModelDefault
    );

    char *dl = LLVMCopyStringRepOfTargetData(LLVMCreateTargetDataLayout(tm));

    if (native_datalayout) 
    native_datalayout = c_alloc(strlen(dl) + 1);
    if (native_datalayout) strcpy(native_datalayout, dl);

    abi_set_target(abi_kind_for_triple(triple), LLVMCreateTargetData(dl));
    {
        int bits = (int)abi_pointer_bits();
        if (bits == 32 || bits == 64) g_word_bits = bits;
    }

    LLVMDisposeMessage(dl);
    LLVMDisposeTargetMachine(tm);
    LLVMDisposeMessage(triple);
}

void abi_set_target(AbiKind kind, LLVMTargetDataRef dl) {
    abi_kind = kind;
    if (target_data) LLVMDisposeTargetData(target_data);
    target_data = dl;
}

void abi_shutdown(void) {
    if (target_data) {
        LLVMDisposeTargetData(target_data);
        target_data = NULL;
    }
    if (native_datalayout) {
        native_datalayout = NULL;
    }
    abi_kind = ABI_UNKNOWN;
}

static void chunk_add(AbiInfo* info, uint64_t offset, uint64_t size, bool is_float) {
    if (info->chunk_count >= (int)(sizeof(info->chunks) / sizeof(info->chunks[0]))) {
        return;
    }
    info->chunks[info->chunk_count].offset   = offset;
    info->chunks[info->chunk_count].size     = size;
    info->chunks[info->chunk_count].is_float = is_float;
    info->chunk_count++;
}

static unsigned g_sysv_class[2];
static int      g_sysv_eights = 0;

static void sysv_merge_class(unsigned* mask, unsigned incoming) {
    if (*mask == 0)        { *mask = incoming; return; }
    if (*mask == incoming) return;
    *mask = 1;
}

static int sysv_is_hfa(LLVMTargetDataRef td, LLVMTypeRef ty, LLVMTypeRef* out_elem) {
    LLVMTypeKind kind = LLVMGetTypeKind(ty);
    if (kind == LLVMFloatTypeKind || kind == LLVMDoubleTypeKind) {
        *out_elem = ty;
        return 1;
    }
    if (kind == LLVMStructTypeKind) {
        unsigned n = LLVMCountStructElementTypes(ty);
        if (n == 0) return 0;
        LLVMTypeRef elems[16];
        LLVMTypeRef* elist = n <= 16 ? elems : c_alloc(sizeof(LLVMTypeRef) * n);
        LLVMGetStructElementTypes(ty, n <= 16 ? elems : elist);
        LLVMTypeRef common = NULL;
        int ok = 1;
        for (unsigned i = 0; i < n; i++) {
            LLVMTypeRef sub;
            if (!sysv_is_hfa(td, n <= 16 ? elems[i] : elist[i], &sub)) { ok = 0; break; }
            if (!common) common = sub;
            else if (common != sub) { ok = 0; break; }
        }
        if (n > 16)
        (void)td;
        if (!ok) return 0;
        *out_elem = common;
        return 1;
    }
    if (kind == LLVMArrayTypeKind) {
        LLVMTypeRef sub;
        if (!sysv_is_hfa(td, LLVMGetElementType(ty), &sub)) return 0;
        *out_elem = sub;
        return 1;
    }
    return 0;
}

static uint64_t eightbyte_scalar_width(unsigned cls, uint64_t isize) {
    if (cls == 2) {
        return isize <= 4 ? 4 : 8;
    }
    if (isize <= 1) return 1;
    if (isize <= 2) return 2;
    if (isize <= 4) return 4;
    return 8;
}

static void sysv_classify_scalar(unsigned cls, uint64_t offset, uint64_t size, int is_sse) {
    uint64_t start = offset;
    while (size > 0) {
        uint64_t eb = start / EIGHTBYTE_SIZE;
        if (eb >= 2) return;
        if ((int)(eb + 1) > g_sysv_eights) g_sysv_eights = (int)(eb + 1);

        sysv_merge_class(&g_sysv_class[eb], is_sse ? 2 : 1);

        uint64_t consumed = EIGHTBYTE_SIZE - (start % EIGHTBYTE_SIZE);
        if (consumed > size) consumed = size;
        start += consumed;
        size  -= consumed;
    }
}

static void sysv_classify(LLVMTargetDataRef td, LLVMTypeRef ty, uint64_t offset) {
    LLVMTypeKind kind = LLVMGetTypeKind(ty);

    switch (kind) {
        case LLVMIntegerTypeKind:
        case LLVMPointerTypeKind:
            sysv_classify_scalar(1, offset, LLVMABISizeOfType(td, ty), 0);
            return;

        case LLVMFloatTypeKind:
        case LLVMDoubleTypeKind:
            sysv_classify_scalar(2, offset, LLVMABISizeOfType(td, ty), 1);
            return;

        case LLVMVectorTypeKind:
            sysv_classify_scalar(2, offset, LLVMABISizeOfType(td, ty), 1);
            return;

        case LLVMArrayTypeKind: {
            LLVMTypeRef el = LLVMGetElementType(ty);
            uint64_t n = LLVMGetArrayLength2(ty);
            uint64_t el_size = LLVMABISizeOfType(td, el);
            for (uint64_t i = 0; i < n; i++) {
                sysv_classify(td, el, offset + i * el_size);
                if (g_sysv_eights > 2) return;
            }
            return;
        }

        case LLVMStructTypeKind: {
            unsigned n = LLVMCountStructElementTypes(ty);
            LLVMTypeRef elems[16];
            LLVMTypeRef* elist = n <= 16 ? elems : c_alloc(sizeof(LLVMTypeRef) * n);
            LLVMGetStructElementTypes(ty, elist);

            for (unsigned i = 0; i < n; i++) {
                uint64_t field_offset = LLVMOffsetOfElement(td, ty, i);
                sysv_classify(td, elist[i], offset + field_offset);
                if (g_sysv_eights > 2) break;
            }

            if (n > 16)
            return;
        }

        default:
            sysv_classify_scalar(1, offset, LLVMABISizeOfType(td, ty), 0);
            return;
    }
}

// MS x64
static void classify_ms_x64(LLVMTypeRef type, AbiInfo* out) {
    LLVMTypeKind kind = LLVMGetTypeKind(type);
    if (kind != LLVMStructTypeKind || LLVMIsOpaqueStruct(type)) {
        out->cls = ABI_CLASS_NOT_STRUCT;
        return;
    }
    uint64_t size  = LLVMABISizeOfType(target_data, type);
    unsigned align = LLVMABIAlignmentOfType(target_data, type);
    out->size  = size;
    out->align = align ? align : 1;

    if (size == 1 || size == 2 || size == 4 || size == 8) {
        out->cls = ABI_CLASS_INTEGER;
        chunk_add(out, 0, size, false);
    } else {
        out->cls = ABI_CLASS_MEMORY;
    }
}

// SysV AMD64
static void classify_sysv_amd64(LLVMTypeRef type, AbiInfo* out) {
    LLVMTypeKind kind = LLVMGetTypeKind(type);
    if (kind != LLVMStructTypeKind || LLVMIsOpaqueStruct(type)) {
        out->cls = ABI_CLASS_NOT_STRUCT;
        return;
    }

    uint64_t size  = LLVMABISizeOfType(target_data, type);
    unsigned align = LLVMABIAlignmentOfType(target_data, type);
    out->size  = size;
    out->align = align ? align : 1;

    g_sysv_class[0] = 0;
    g_sysv_class[1] = 0;
    g_sysv_eights   = 0;
    sysv_classify(target_data, type, 0);

    if (size > 16 || g_sysv_eights > 2) {
        out->cls = ABI_CLASS_MEMORY;
        return;
    }

    LLVMTypeRef elem = NULL;
    if (size > 0 && size % EIGHTBYTE_SIZE == 0 &&
        sysv_is_hfa(target_data, type, &elem)) {
        uint64_t fsize = LLVMABISizeOfType(target_data, elem);
        if (fsize == 0) fsize = 4;
        uint64_t count = size / fsize;
        if (count >= 1 && count <= 4) {
            out->cls = ABI_CLASS_SSE;
            // Split HFAs into 8-byte eightbyte chunks (max 2 chunks for 16 bytes)
            for (uint64_t i = 0; i < size; i += EIGHTBYTE_SIZE) {
                uint64_t chunk_size = (size - i >= EIGHTBYTE_SIZE) ? EIGHTBYTE_SIZE : (size - i);
                chunk_add(out, i, chunk_size, true);
            }
            return;
        }
    }

    if (g_sysv_eights == 0) g_sysv_eights = 1;
    for (int i = 0; i < g_sysv_eights && i < 2; i++) {
        unsigned cls = g_sysv_class[i];
        if (cls == 0) cls = 1;
        uint64_t isize = EIGHTBYTE_SIZE;
        if (i == g_sysv_eights - 1) {
            uint64_t rem = size - (uint64_t)i * EIGHTBYTE_SIZE;
            if (rem > 0 && rem < EIGHTBYTE_SIZE) isize = rem;
        }
        chunk_add(out, (uint64_t)i * EIGHTBYTE_SIZE,
                    eightbyte_scalar_width(cls, isize), (cls == 2));
    }

    out->cls   = (out->chunk_count > 0 && out->chunks[0].is_float)
        ? ABI_CLASS_SSE : ABI_CLASS_INTEGER;
}

static void classify_sysv_x86(LLVMTypeRef type, AbiInfo* out) {
    LLVMTypeKind kind = LLVMGetTypeKind(type);
    if (kind != LLVMStructTypeKind || LLVMIsOpaqueStruct(type)) {
        out->cls = ABI_CLASS_NOT_STRUCT;
        return;
    }
    uint64_t size  = LLVMABISizeOfType(target_data, type);
    unsigned align = LLVMABIAlignmentOfType(target_data, type);
    out->size  = size;
    out->align = align ? align : 1;

    if (size == 1 || size == 2 || size == 4 || size == 8) {
        out->cls = ABI_CLASS_INTEGER;
        chunk_add(out, 0, size, false);
    } else {
        out->cls = ABI_CLASS_MEMORY;
    }
}

static void classify_aapcs64(LLVMTypeRef type, AbiInfo* out) {
    LLVMTypeKind kind = LLVMGetTypeKind(type);
    if (kind != LLVMStructTypeKind || LLVMIsOpaqueStruct(type)) {
        out->cls = ABI_CLASS_NOT_STRUCT;
        return;
    }
    uint64_t size  = LLVMABISizeOfType(target_data, type);
    unsigned align = LLVMABIAlignmentOfType(target_data, type);
    out->size  = size;
    out->align = align ? align : 1;
    if (size <= 16) {
        out->cls = ABI_CLASS_INTEGER;
        chunk_add(out, 0, size < 8 ? size : 8, false);
        if (size > 8) chunk_add(out, 8, size - 8, false);
    } else {
        out->cls = ABI_CLASS_MEMORY;
    }
}

AbiInfo abi_analyze(LLVMContextRef ctx, LLVMTypeRef type) {
    AbiInfo info;
    memset(&info, 0, sizeof(info));
    (void)ctx;
    if (!target_data || !type) {
        info.cls = ABI_CLASS_NOT_STRUCT;
        return info;
    }
    switch (abi_kind) {
        case ABI_SYSV_AMD64: classify_sysv_amd64(type, &info); break;
        case ABI_SYSV_X86:   classify_sysv_x86(type, &info);    break;
        case ABI_MS_X64:     classify_ms_x64(type, &info);      break;
        case ABI_AAPCS64:    classify_aapcs64(type, &info);     break;
        default:
            classify_sysv_amd64(type, &info);
            break;
    }
    return info;
}

int abi_param_count(const AbiInfo* info) {
    switch (info->cls) {
        case ABI_CLASS_MEMORY:   return 1;
        case ABI_CLASS_NOT_STRUCT: return 1;
        default:                 return info->chunk_count > 0 ? info->chunk_count : 1;
    }
}

bool abi_is_byval(LLVMContextRef ctx, LLVMTypeRef type) {
    AbiInfo info = abi_analyze(ctx, type);
    return info.cls == ABI_CLASS_MEMORY;
}

bool abi_is_sret(LLVMContextRef ctx, LLVMTypeRef type) {
    AbiInfo info = abi_analyze(ctx, type);
    return info.cls == ABI_CLASS_MEMORY;
}
