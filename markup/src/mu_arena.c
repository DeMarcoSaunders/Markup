#include "markup/mu_core.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct MuArena {
    unsigned char *base;
    size_t size;
    size_t used;
};

MuArena *mu_arena_create(size_t capacity) {
    MuArena *a = (MuArena *)calloc(1, sizeof(MuArena));
    if (!a) return NULL;
    a->base = (unsigned char *)malloc(capacity);
    if (!a->base) {
        free(a);
        return NULL;
    }
    a->size = capacity;
    a->used = 0;
    return a;
}

void mu_arena_destroy(MuArena *a) {
    if (!a) return;
    free(a->base);
    free(a);
}

void mu_arena_reset(MuArena *a) {
    if (a) a->used = 0;
}

void *mu_arena_alloc(MuArena *a, size_t size, size_t align) {
    if (!a || !size) return NULL;
    uintptr_t p = (uintptr_t)(void *)a->base + a->used;
    uintptr_t aligned = (p + (uintptr_t)(align - 1)) & ~((uintptr_t)(align - 1));
    size_t offset = (size_t)(aligned - (uintptr_t)(void *)a->base);
    if (offset + size > a->size) return NULL;
    void *ptr = a->base + offset;
    a->used = offset + size;
    memset(ptr, 0, size);
    return ptr;
}
