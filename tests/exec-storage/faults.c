#include "b2nd-exec-storage.h"
#include <stdlib.h>
static size_t allocations, allocation_fail_at, decode_calls, decode_fail_at;
static void *storage_malloc(size_t size) {
    return ++allocations == allocation_fail_at ? NULL : malloc(size);
}
static void *storage_calloc(size_t count, size_t size) {
    return ++allocations == allocation_fail_at ? NULL : calloc(count, size);
}
static int storage_decompress(blosc2_context *ctx, const void *src, int32_t cbytes,
                               void *dst, int32_t capacity) {
    return ++decode_calls == decode_fail_at ? -1 :
        blosc2_decompress_ctx(ctx, src, cbytes, dst, capacity);
}
#define malloc storage_malloc
#define calloc storage_calloc
#define blosc2_decompress_ctx storage_decompress
#include "../../blosc/b2nd-exec-storage.c"
#undef malloc
#undef calloc
#undef blosc2_decompress_ctx
#define STORAGE_FAULT_TEST 1
#include "../installed-miniexpr/exec-storage.c"
