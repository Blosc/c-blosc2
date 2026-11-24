/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Threads that make the first (bit)shuffle call of the process at the same
  time, as the worker threads of a compression or decompression job do,
  must all find the shuffle implementation complete, not a NULL function
  pointer.  An unsynchronized lazy initialization opens a window of a few
  instructions, so this test finds it reliably only under ThreadSanitizer.

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdio.h>
#include <string.h>

#include "threading.h"
#include "test_common.h"

#define NTHREADS 16
#define TYPESIZE 4
#define NBYTES (TYPESIZE * 1024)

typedef int32_t (*shuffle_fn)(const int32_t, const int32_t, const void*, void*);

/* Each thread starts with one of the four entry points, so all of them race
   for the first initialization; the inverse checks the result. */
static const struct {
  shuffle_fn first;
  shuffle_fn inverse;
} ops[] = {
  {blosc2_shuffle, blosc2_unshuffle},
  {blosc2_unshuffle, blosc2_shuffle},
  {blosc2_bitshuffle, blosc2_bitunshuffle},
  {blosc2_bitunshuffle, blosc2_bitshuffle},
};
#define NOPS (int)(sizeof(ops) / sizeof(ops[0]))

typedef struct {
  int tid;
  int rc;
} worker_state;

static blosc2_pthread_mutex_t gate_mutex;
static blosc2_pthread_cond_t gate_cv;
static int gate_open = 0;

static void* first_call_worker(void *arg) {
  worker_state *state = (worker_state*)arg;
  uint8_t src[NBYTES], shuffled[NBYTES], back[NBYTES];
  for (int i = 0; i < NBYTES; i++) {
    src[i] = (uint8_t)(i * 7 + state->tid);
  }

  blosc2_pthread_mutex_lock(&gate_mutex);
  while (!gate_open) {
    blosc2_pthread_cond_wait(&gate_cv, &gate_mutex);
  }
  blosc2_pthread_mutex_unlock(&gate_mutex);

  int32_t rc1 = ops[state->tid % NOPS].first(TYPESIZE, NBYTES, src, shuffled);
  int32_t rc2 = ops[state->tid % NOPS].inverse(TYPESIZE, NBYTES, shuffled, back);
  state->rc = (rc1 == NBYTES && rc2 == NBYTES && memcmp(src, back, NBYTES) == 0) ? 0 : -1;
  return NULL;
}

int main(void) {
  blosc2_pthread_t threads[NTHREADS];
  worker_state states[NTHREADS];
  int failed = 0;

  /* No blosc2_init() and no other shuffle call before the threads: the
     first call of the process must be the concurrent one. */
  blosc2_pthread_mutex_init(&gate_mutex, NULL);
  blosc2_pthread_cond_init(&gate_cv, NULL);
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
    if (states[t].rc != 0) {
      printf("Thread %d got a wrong result\n", t);
      failed = 1;
    }
  }
  blosc2_pthread_cond_destroy(&gate_cv);
  blosc2_pthread_mutex_destroy(&gate_mutex);

  if (failed) {
    printf("test_shuffle_init_threads FAILED\n");
    return 1;
  }
  printf("test_shuffle_init_threads: %d threads made their first shuffle call at once: OK\n", NTHREADS);
  return 0;
}
