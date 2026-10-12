#pragma once

typedef struct Arena Arena;

Arena *create_arena_allocator(size_t initial_capacity);
Arena *arena_get_current(void);

void *c_alloc(size_t size);
void *c_realloc(void *ptr, size_t size);
void  c_free(void *ptr);

void arena_set_current(Arena *arena);
void arena_reset(Arena *arena);
void arena_destroy(Arena *arena);
