/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Threads that make the first Blosc calls of the process at the same time,
  without blosc2_init(), must initialize the library once and all find it
  complete; the same after blosc2_destroy().  Without that, each of them ran
  the whole initialization (global context, mutexes and registries set up
  again while the others used them), which hung or crashed.  The threads also
  call blosc2_list_compressors() first, which must give each of them the
  whole list.

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdio.h>
#include <string.h>

#include "threading.h"
#include "test_common.h"

#define NTHREADS 8
#define NITEMS 4096

typedef struct {
  int tid;
  int rc;
  char compressors[256];
} worker_state;

static blosc2_pthread_mutex_t gate_mutex;
static blosc2_pthread_cond_t gate_cv;
static int gate_open = 0;

/* Each of them initializes the library */
enum first_call {
  CONTEXTS_2_THREADS,  /* lazy initialization of the shared thread pool */
  GLOBAL_CONTEXT,
  GETTER,
  NKINDS
};

static int first_calls(enum first_call kind, const int32_t *src, int32_t *dest, uint8_t *chunk,
                       int32_t chunk_size) {
  const int32_t nbytes = NITEMS * (int32_t)sizeof(int32_t);
  int cbytes, dbytes;
  if (kind == CONTEXTS_2_THREADS) {
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.typesize = sizeof(int32_t);
    cparams.nthreads = 2;
    cparams.blocksize = 1024;
    blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
    dparams.nthreads = 2;
    blosc2_context *cctx = blosc2_create_cctx(cparams);
    blosc2_context *dctx = blosc2_create_dctx(dparams);
    cbytes = blosc2_compress_ctx(cctx, src, nbytes, chunk, chunk_size);
    dbytes = cbytes > 0 ? blosc2_decompress_ctx(dctx, chunk, cbytes, dest, nbytes) : -1;
    blosc2_free_ctx(cctx);
    blosc2_free_ctx(dctx);
  }
  else if (kind == GLOBAL_CONTEXT) {
    cbytes = blosc2_compress(5, BLOSC_SHUFFLE, sizeof(int32_t), src, nbytes, chunk, chunk_size);
    dbytes = cbytes > 0 ? blosc2_decompress(chunk, cbytes, dest, nbytes) : -1;
  }
  else {
    return blosc2_get_nthreads() >= 1 ? 0 : -1;
  }
  return (cbytes > 0 && dbytes == nbytes && memcmp(src, dest, nbytes) == 0) ? 0 : -1;
}

static void* first_call_worker(void *arg) {
  worker_state *state = (worker_state*)arg;
  const int32_t chunk_size = NITEMS * (int32_t)sizeof(int32_t) + BLOSC2_MAX_OVERHEAD;
  int32_t *src = malloc(NITEMS * sizeof(int32_t));
  int32_t *dest = malloc(NITEMS * sizeof(int32_t));
  uint8_t *chunk = malloc(chunk_size);
  for (int i = 0; i < NITEMS; i++) {
    src[i] = i * (state->tid + 1);
  }

  blosc2_pthread_mutex_lock(&gate_mutex);
  while (!gate_open) {
    blosc2_pthread_cond_wait(&gate_cv, &gate_mutex);
  }
  blosc2_pthread_mutex_unlock(&gate_mutex);

  /* Copy at once, as a caller that uses the string right away */
  strncpy(state->compressors, blosc2_list_compressors(), sizeof(state->compressors) - 1);
  state->rc = first_calls((enum first_call)(state->tid % NKINDS), src, dest, chunk, chunk_size);
  free(src);
  free(dest);
  free(chunk);
  return NULL;
}

static int run_threads(const char *when) {
  blosc2_pthread_t threads[NTHREADS];
  worker_state states[NTHREADS];
  int failed = 0;

  memset(states, 0, sizeof(states));
  gate_open = 0;
  for (int t = 0; t < NTHREADS; t++) {
    states[t].tid = t;
    if (blosc2_pthread_create(&threads[t], NULL, first_call_worker, &states[t]) != 0) {
      printf("Cannot create thread %d\n", t);
      return 1;
    }
  }
  blosc2_pthread_mutex_lock(&gate_mutex);
  gate_open = 1;
  blosc2_pthread_cond_broadcast(&gate_cv);
  blosc2_pthread_mutex_unlock(&gate_mutex);

  for (int t = 0; t < NTHREADS; t++) {
    blosc2_pthread_join(threads[t], NULL);
  }
  const char *compressors = blosc2_list_compressors();
  for (int t = 0; t < NTHREADS; t++) {
    if (states[t].rc != 0) {
      printf("%s: thread %d got a wrong result\n", when, t);
      failed = 1;
    }
    if (strcmp(states[t].compressors, compressors) != 0) {
      printf("%s: thread %d got the compressors \"%s\" instead of \"%s\"\n", when, t,
             states[t].compressors, compressors);
      failed = 1;
    }
  }
  return failed;
}

int main(void) {
  int failed;

  blosc2_pthread_mutex_init(&gate_mutex, NULL);
  blosc2_pthread_cond_init(&gate_cv, NULL);

  /* No Blosc call before the threads: their first calls are concurrent */
  failed = run_threads("first initialization");
  if (strncmp(blosc2_list_compressors(), "blosclz,lz4,lz4hc", 17) != 0) {
    printf("Unexpected list of compressors: %s\n", blosc2_list_compressors());
    failed = 1;
  }
  blosc2_destroy();
  failed |= run_threads("after blosc2_destroy()");
  blosc2_destroy();

  blosc2_pthread_cond_destroy(&gate_cv);
  blosc2_pthread_mutex_destroy(&gate_mutex);

  if (failed) {
    printf("test_init_threads FAILED\n");
    return 1;
  }
  printf("test_init_threads: %d threads made their first Blosc calls at once, twice: OK\n", NTHREADS);
  return 0;
}
