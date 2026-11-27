#include "b2nd-exec-storage.h"
#include "threading.h"
#include <assert.h>
#include <stdlib.h>
static size_t allocations, allocation_fail_at, decode_calls, decode_fail_at;
static size_t mutex_inits, mutex_fail_at, live_mutexes;
static blosc2_pthread_mutex_t *mutexes[B2ND_EXEC_MAX_INPUTS];
static int storage_mutex_init(blosc2_pthread_mutex_t *mutex, const void *attr) {
    if (++mutex_inits == mutex_fail_at) return -1;
#ifdef _WIN32
    (void)attr;
    blosc2_pthread_mutex_init(mutex, NULL);
    int rc = 0;
#else
    int rc = blosc2_pthread_mutex_init(mutex, attr);
#endif
    if (rc == 0) {
        assert(live_mutexes < B2ND_EXEC_MAX_INPUTS);
        mutexes[live_mutexes++] = mutex;
    }
    return rc;
}
static int storage_mutex_destroy(blosc2_pthread_mutex_t *mutex) {
    size_t i = 0;
    while (i < live_mutexes && mutexes[i] != mutex) i++;
    /* Cleanup must never touch an uninitialized or already destroyed mutex. */
    assert(i < live_mutexes);
    mutexes[i] = mutexes[--live_mutexes];
    blosc2_pthread_mutex_destroy(mutex);
    return 0;
}
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
#undef blosc2_pthread_mutex_init
#undef blosc2_pthread_mutex_destroy
#define blosc2_pthread_mutex_init storage_mutex_init
#define blosc2_pthread_mutex_destroy storage_mutex_destroy
#include "../../blosc/b2nd-exec-storage.c"
#undef malloc
#undef calloc
#undef blosc2_decompress_ctx
#undef blosc2_pthread_mutex_init
#undef blosc2_pthread_mutex_destroy
#define STORAGE_FAULT_TEST 1
#include "../installed-miniexpr/exec-storage.c"
