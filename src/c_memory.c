#include "c_memory.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#define ARENA_DEFAULT_CAPACITY ((size_t)64 * 1024)
#define ARENA_MIN_BLOCK_CAPACITY ((size_t)4096)

typedef struct ArenaAllocation {
    size_t size;
    max_align_t alignment_guard;
} ArenaAllocation;

typedef struct ArenaBlock {
    struct ArenaBlock *next;
    size_t capacity;
    size_t used;
    max_align_t alignment_guard;
    unsigned char data[];
} ArenaBlock;

struct Arena {
    ArenaBlock *first;
    ArenaBlock *current;
    size_t initial_capacity;
};

static Arena *g_current_arena;

static int add_overflow_size(size_t a, size_t b, size_t *out) {
    if (a > SIZE_MAX - b) {
        return 1;
    }
    *out = a + b;
    return 0;
}

static int align_up_size(size_t value, size_t alignment, size_t *out) {
    size_t remainder = value % alignment;
    size_t padding = remainder ? alignment - remainder : 0;
    return add_overflow_size(value, padding, out);
}

static ArenaBlock *arena_block_create(size_t capacity) {
    size_t total;
    if (add_overflow_size(offsetof(ArenaBlock, data), capacity, &total)) {
        return NULL;
    }

    ArenaBlock *block = (ArenaBlock *)malloc(total);
    if (!block) {
        return NULL;
    }

    block->next = NULL;
    block->capacity = capacity;
    block->used = 0;
    return block;
}

static ArenaBlock *arena_find_block(Arena *arena, size_t required,
                                    size_t *aligned_used) {
    for (ArenaBlock *block = arena->current; block; block = block->next) {
        size_t aligned;
        if (!align_up_size(block->used, _Alignof(max_align_t), &aligned)) {
            if (aligned <= block->capacity &&
                required <= block->capacity - aligned) {
                *aligned_used = aligned;
                arena->current = block;
                return block;
            }
        }
    }

    size_t capacity = arena->initial_capacity;
    if (capacity < ARENA_MIN_BLOCK_CAPACITY) {
        capacity = ARENA_MIN_BLOCK_CAPACITY;
    }
    if (capacity < required) {
        capacity = required;
    }

    ArenaBlock *block = arena_block_create(capacity);
    if (!block) {
        return NULL;
    }

    if (!arena->first) {
        arena->first = block;
    } else {
        ArenaBlock *tail = arena->first;
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = block;
    }

    arena->current = block;
    *aligned_used = 0;
    return block;
}

Arena *create_arena_allocator(size_t initial_capacity) {
    Arena *arena = (Arena *)malloc(sizeof(*arena));
    if (!arena) {
        return NULL;
    }

    if (initial_capacity == 0) {
        initial_capacity = ARENA_DEFAULT_CAPACITY;
    }

    arena->first = NULL;
    arena->current = NULL;
    arena->initial_capacity = initial_capacity;

    ArenaBlock *first = arena_block_create(initial_capacity);
    if (!first) {
        free(arena);
        return NULL;
    }

    arena->first = first;
    arena->current = first;

    g_current_arena = arena;
    return arena;
}

Arena *arena_get_current(void) {
    return g_current_arena;
}

void arena_set_current(Arena *arena) {
    g_current_arena = arena;
}

void *c_alloc(size_t size) {
    Arena *arena = g_current_arena;
    if (!arena) {
        arena = create_arena_allocator(ARENA_DEFAULT_CAPACITY);
        if (!arena) {
            return malloc(size == 0 ? 1 : size);
        }
        g_current_arena = arena;
    }

    if (size == 0) {
        size = 1;
    }

    size_t required;
    if (add_overflow_size(sizeof(ArenaAllocation), size, &required)) {
        return NULL;
    }

    size_t aligned_used;
    ArenaBlock *block = arena_find_block(arena, required, &aligned_used);
    if (!block) {
        return NULL;
    }

    ArenaAllocation *header =
        (ArenaAllocation *)(void *)(block->data + aligned_used);
    header->size = size;

    void *result = (void *)(header + 1);
    block->used = aligned_used + required;
    return result;
}

void *c_realloc(void *ptr, size_t size) {
    if (!ptr) {
        return c_alloc(size);
    }

    if (size == 0) {
        return NULL;
    }

    ArenaAllocation *old_header = ((ArenaAllocation *)ptr) - 1;
    size_t old_size = old_header->size;

    void *new_ptr = c_alloc(size);
    if (!new_ptr) {
        return NULL;
    }

    size_t copy_size = old_size < size ? old_size : size;
    memcpy(new_ptr, ptr, copy_size);
    return new_ptr;
}

void c_free(void *ptr) {
    (void)ptr;
}

void arena_reset(Arena *arena) {
    if (!arena) {
        return;
    }

    for (ArenaBlock *block = arena->first; block; block = block->next) {
        block->used = 0;
    }
    arena->current = arena->first;
}

void arena_destroy(Arena *arena) {
    if (!arena) {
        return;
    }

    if (g_current_arena == arena) {
        g_current_arena = NULL;
    }

    ArenaBlock *block = arena->first;
    while (block) {
        ArenaBlock *next = block->next;
        free(block);
        block = next;
    }

    free(arena);
}

