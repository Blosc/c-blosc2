/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  The per-thread cache of large temporaries (blosc2_tmp_malloc/free): reuse,
  its size limits, and that b2nd slice reads and writes on frames give their
  chunk-sized buffers back to it.  Run under
  AddressSanitizer/LeakSanitizer for the thread-exit and blosc2_destroy()
  cleanup.
*/

#include <stdio.h>
#include "test_common.h"
#include "blosc-private.h"
#include "b2nd.h"

#if !defined(_WIN32)
#include <pthread.h>
#endif

int tests_run = 0;


static char* test_reuse(void) {
#if !defined(_WIN32)
  size_t cap;
  uint8_t *p = blosc2_tmp_malloc(MB, &cap);
  mu_assert("Fresh buffers have the requested size", p != NULL && cap == MB);
  memset(p, 1, MB);
  blosc2_tmp_free(p, cap);
  uint8_t *q = blosc2_tmp_malloc(MB, &cap);
  mu_assert("A freed buffer is handed out again", q == p && cap == MB);
  blosc2_tmp_free(q, cap);
  q = blosc2_tmp_malloc(MB / 2 + 1, &cap);
  mu_assert("A somewhat larger buffer is reused, with its real size", q == p && cap == MB);
  blosc2_tmp_free(q, cap);
  q = blosc2_tmp_malloc(MB / 4, &cap);
  mu_assert("A much larger buffer is not handed out", q != p && cap == MB / 4);
  free(q);  // plain free() is fine for these buffers
  blosc2_tmp_cache_clear();
#endif
  return EXIT_SUCCESS;
}


static char* test_limits(void) {
#if !defined(_WIN32)
  size_t cap;
  // Small buffers are not cached (malloc keeps them cheap anyway).  The checks
  // count cached bytes, not returned addresses: a buffer that the cache frees
  // can come straight back from malloc() at the same address (macOS does that).
  blosc2_tmp_cache_clear();
  uint8_t *p = blosc2_tmp_malloc(KB, &cap);
  blosc2_tmp_free(p, cap);
  mu_assert("Small buffers must not be cached", blosc2_tmp_cache_nbytes() == 0);
  // At most 64 MB stay cached: of three 30 MB buffers, two are kept
  uint8_t *b[3];
  size_t c[3];
  for (int i = 0; i < 3; ++i) {
    b[i] = blosc2_tmp_malloc(30 * MB, &c[i]);
    mu_assert("Cannot allocate", b[i] != NULL);
  }
  for (int i = 0; i < 3; ++i) {
    blosc2_tmp_free(b[i], c[i]);
  }
  mu_assert("No more than 64 MB may stay cached", blosc2_tmp_cache_nbytes() == 60 * MB);
  for (int i = 0; i < 3; ++i) {
    b[i] = blosc2_tmp_malloc(30 * MB, &c[i]);
    mu_assert("Cannot allocate", b[i] != NULL);
  }
  mu_assert("Cached buffers are handed out again", blosc2_tmp_cache_nbytes() == 0);
  for (int i = 0; i < 3; ++i) {
    free(b[i]);
  }
  // Buffers above 32 MB are never cached (a cached one would serve 20 MB)
  p = blosc2_tmp_malloc(40 * MB, &cap);
  blosc2_tmp_free(p, cap);
  mu_assert("Buffers above 32 MB must not be cached", blosc2_tmp_cache_nbytes() == 0);
  p = blosc2_tmp_malloc(20 * MB, &cap);
  mu_assert("Buffers above 32 MB must not be cached", cap == 20 * MB);
  free(p);
#endif
  return EXIT_SUCCESS;
}


#if !defined(_WIN32)
static void *thread_fill_cache(void *arg) {
  (void) arg;
  size_t cap;
  void *p = blosc2_tmp_malloc(4 * MB, &cap);
  blosc2_tmp_free(p, cap);  // stays in this thread's cache until it exits
  return NULL;
}
#endif


static char* test_thread_exit(void) {
#if !defined(_WIN32)
  pthread_t thread;
  mu_assert("Cannot create a thread", pthread_create(&thread, NULL, thread_fill_cache, NULL) == 0);
  mu_assert("Cannot join the thread", pthread_join(thread, NULL) == 0);
  // LeakSanitizer reports the cached buffer if the thread exit did not free it
#endif
  return EXIT_SUCCESS;
}


/* Slice writes and reads on frames hand their chunk-sized temporaries (the
   chunk scratch, the compression destination, the compressed chunk read from
   a file) back to the cache instead of freeing them: freed per call, glibc
   trims them back to the kernel and the next call faults them in again. */
static char* test_slice_buffers_cached(void) {
#if !defined(_WIN32)
  int8_t ndim = 3;
  int64_t shape[] = {96, 256, 256};
  int32_t chunkshape[] = {48, 128, 128};
  int32_t blockshape[] = {16, 32, 32};
  int64_t nitems = 96 * 256 * 256;
  size_t chunk_nbytes = 48 * 128 * 128 * sizeof(uint32_t);
  uint32_t *buf = malloc(nitems * sizeof(uint32_t));
  for (int64_t i = 0; i < nitems; ++i) {
    buf[i] = (uint32_t) i * 2654435761u;  // incompressible: chunks of about their full size
  }
  for (int disk = 0; disk < 2; ++disk) {
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.typesize = sizeof(uint32_t);
    blosc2_storage storage = {.contiguous=true, .cparams=&cparams};
    if (disk) {
      storage.urlpath = "test_tmp_cache.b2nd";
      blosc2_remove_urlpath(storage.urlpath);
    }
    b2nd_context_t *ctx = b2nd_create_ctx(&storage, ndim, shape, chunkshape, blockshape, NULL, 0, NULL, 0);
    b2nd_array_t *array;
    mu_assert("Cannot create the array", b2nd_from_cbuffer(ctx, &array, buf, nitems * sizeof(uint32_t)) >= 0);

    // A partial chunk write: old chunk decompressed into the scratch, new one compressed
    blosc2_tmp_cache_clear();
    int64_t start[] = {1, 1, 1};
    int64_t stop[] = {47, 127, 127};
    int64_t boxshape[] = {46, 126, 126};
    mu_assert("Cannot set the slice",
              b2nd_set_slice_cbuffer(buf, boxshape, 46 * 126 * 126 * sizeof(uint32_t), start, stop, array) >= 0);
    size_t cap1, cap2;
    void *p1 = blosc2_tmp_malloc(chunk_nbytes - 1, &cap1);
    void *p2 = blosc2_tmp_malloc(chunk_nbytes - 1, &cap2);
    mu_assert("set_slice must leave its chunk-sized buffers in the cache",
              cap1 >= chunk_nbytes && cap2 >= chunk_nbytes);
    free(p1);
    free(p2);

    // A partial chunk read (scatter path)
    blosc2_tmp_cache_clear();
    mu_assert("Cannot get the slice",
              b2nd_get_slice_cbuffer(array, start, stop, buf, boxshape, 46 * 126 * 126 * sizeof(uint32_t)) >= 0);
    p1 = blosc2_tmp_malloc(chunk_nbytes - 1, &cap1);
    mu_assert("get_slice must leave its chunk scratch in the cache", cap1 >= chunk_nbytes);
    free(p1);
    if (disk) {
      // ... and the compressed chunk it read from the file
      uint8_t *chunk;
      bool needs_free;
      int cbytes = blosc2_schunk_get_chunk(array->sc, 0, &chunk, &needs_free);
      mu_assert("Cannot get the chunk", cbytes > 0);
      if (needs_free) {
        free(chunk);
      }
      p1 = blosc2_tmp_malloc(cbytes - 1, &cap1);
      mu_assert("The compressed chunk read from the file must be cached", cap1 >= (size_t) cbytes);
      free(p1);
    }
    blosc2_tmp_cache_clear();

    b2nd_free(array);
    b2nd_free_ctx(ctx);
    if (disk) {
      blosc2_remove_urlpath(storage.urlpath);
    }
  }
  free(buf);
#endif
  return EXIT_SUCCESS;
}


static char* all_tests(void) {
  mu_run_test(test_reuse);
  mu_run_test(test_limits);
  mu_run_test(test_thread_exit);
  mu_run_test(test_slice_buffers_cached);
  return EXIT_SUCCESS;
}


int main(void) {
  blosc2_init();
  char* result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);
  blosc2_destroy();
  return result != EXIT_SUCCESS;
}
