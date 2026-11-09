/*
 * Copyright (c) 2026 Blosc Development Team <blosc@blosc.org>
 * License: BSD 3-Clause (see LICENSE.txt)
 *
 * Idle contexts may be released concurrently with global pool teardown.
 * No compression/decompression or user callbacks run during destruction.
 */
#include "test_common.h"
#include "threading.h"
#include "context.h"

#define NRELEASERS 8
#define ROUNDS 200
#define NBYTES 8192

static blosc2_pthread_mutex_t gate_mutex;
static blosc2_pthread_cond_t gate_cv;
static int ready, go;

static void *release_context(void *arg) {
  blosc2_pthread_mutex_lock(&gate_mutex);
  ++ready;
  blosc2_pthread_cond_broadcast(&gate_cv);
  while (!go) {
    blosc2_pthread_cond_wait(&gate_cv, &gate_mutex);
  }
  blosc2_pthread_mutex_unlock(&gate_mutex);
  blosc2_free_ctx((blosc2_context *)arg);
  return NULL;
}

int main(void) {
  uint8_t input[NBYTES], compressed[NBYTES + BLOSC2_MAX_OVERHEAD];
  for (int i = 0; i < NBYTES; ++i) {
    input[i] = (uint8_t)(i / 16);
  }
  blosc2_pthread_mutex_init(&gate_mutex, NULL);
  blosc2_pthread_cond_init(&gate_cv, NULL);
  for (int round = 0; round < ROUNDS; ++round) {
    blosc2_pthread_t threads[NRELEASERS];
    blosc2_context *contexts[NRELEASERS + 1];
    blosc2_init();
    for (int i = 0; i <= NRELEASERS; ++i) {
      blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
      cp.nthreads = 2 + i % 3;
      cp.blocksize = 1024;
      contexts[i] = blosc2_create_cctx(cp);
      if (contexts[i] == NULL ||
          blosc2_compress_ctx(contexts[i], input, NBYTES, compressed, sizeof(compressed)) <= 0) {
        return 1;
      }
#if !defined(_WIN32)
      if (contexts[i]->thread_pool == NULL) {
        return 1;  // ensure the shared-pool release path is actually exercised
      }
#endif
    }
    ready = go = 0;
    for (int i = 0; i < NRELEASERS; ++i) {
      if (blosc2_pthread_create(&threads[i], NULL, release_context, contexts[i]) != 0) {
        return 1;
      }
    }
    blosc2_pthread_mutex_lock(&gate_mutex);
    while (ready != NRELEASERS) {
      blosc2_pthread_cond_wait(&gate_cv, &gate_mutex);
    }
    go = 1;
    blosc2_pthread_cond_broadcast(&gate_cv);
    blosc2_pthread_mutex_unlock(&gate_mutex);
    blosc2_destroy();
    // Alternate late releases before and after reinitializing the registry.
    if (round % 2) {
      blosc2_init();
    }
    for (int i = 0; i < NRELEASERS; ++i) {
      blosc2_pthread_join(threads[i], NULL);
    }
    // A context that outlives teardown must be able to attach to a fresh pool.
    blosc2_init();
    if (blosc2_compress_ctx(contexts[NRELEASERS], input, NBYTES, compressed, sizeof(compressed)) <= 0) {
      return 1;
    }
    blosc2_free_ctx(contexts[NRELEASERS]);
    blosc2_destroy();
  }
  blosc2_pthread_cond_destroy(&gate_cv);
  blosc2_pthread_mutex_destroy(&gate_mutex);
  puts("Concurrent pool release/teardown and stale-context reuse: OK");
  return 0;
}
