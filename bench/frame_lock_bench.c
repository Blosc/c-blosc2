/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Benchmark for the opt-in cross-process frame locking
  (blosc2_stdio_params.locking): measures the per-operation overhead of the
  sidecar lock by running the same workload on a disk-based sparse frame with
  locking disabled and enabled.

  To run:

  $ ./frame_lock_bench
  locking=off  get_lazychunk:    23731 ns  decompress:    64382 ns  update:   264792 ns
  locking=on   get_lazychunk:    24303 ns  decompress:    65273 ns  update:   265234 ns
  overhead:    get_lazychunk:    +2.4 %    decompress:     +1.4 %   update:     +0.2 %
*/

#include <stdio.h>
#include <stdlib.h>
#include <blosc2.h>

#define CHUNKSIZE (100 * 1000)   /* items per chunk (int64_t) */
#define NCHUNKS (50)
#define NREADS (20 * 1000)
#define NUPDATES (500)
#define URLPATH "frame_lock_bench.b2frame"


static double elapsed_ns_per_op(blosc_timestamp_t t0, int nops) {
  blosc_timestamp_t t1;
  blosc_set_timestamp(&t1);
  return blosc_elapsed_nsecs(t0, t1) / nops;
}


/* Run the workload with or without locking; fills results[3] with ns/op for
   get_lazychunk, decompress_chunk and update_chunk (with a special chunk). */
static int run(bool locking, double results[3]) {
  blosc2_stdio_params ioparams = {.locking = true};
  blosc2_io io = {.id = BLOSC2_IO_FILESYSTEM, .name = "filesystem", .params = &ioparams};
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int64_t);
  blosc2_storage storage = {.contiguous = false, .urlpath = URLPATH, .cparams = &cparams};
  if (locking) {
    storage.io = &io;
  }
  blosc2_remove_urlpath(URLPATH);

  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    return -1;
  }
  int64_t *data = malloc(CHUNKSIZE * sizeof(int64_t));
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = nchunk * CHUNKSIZE + i;
    }
    if (blosc2_schunk_append_buffer(schunk, data, CHUNKSIZE * sizeof(int64_t)) < 0) {
      return -1;
    }
  }
  blosc2_schunk_free(schunk);
  schunk = locking ? blosc2_schunk_open_udio(URLPATH, &io) : blosc2_schunk_open(URLPATH);
  if (schunk == NULL) {
    return -1;
  }

  uint8_t *chunk;
  bool needs_free;
  blosc_timestamp_t t0;

  // get_lazychunk (one untimed warm-up sweep first)
  for (int pass = 0; pass < 2; pass++) {
    blosc_set_timestamp(&t0);
    for (int i = 0; i < NREADS; i++) {
      int csize = blosc2_schunk_get_lazychunk(schunk, i % NCHUNKS, &chunk, &needs_free);
      if (csize < 0) {
        return -1;
      }
      if (needs_free) {
        free(chunk);
      }
    }
    results[0] = elapsed_ns_per_op(t0, NREADS);
  }

  // decompress_chunk
  int64_t *dest = malloc(CHUNKSIZE * sizeof(int64_t));
  const int ndecs = NREADS / 10;
  blosc_set_timestamp(&t0);
  for (int i = 0; i < ndecs; i++) {
    if (blosc2_schunk_decompress_chunk(schunk, i % NCHUNKS, dest,
                                       CHUNKSIZE * sizeof(int64_t)) < 0) {
      return -1;
    }
  }
  results[1] = elapsed_ns_per_op(t0, ndecs);

  // update_chunk with an UNINIT special chunk (the cache-eviction primitive)
  uint8_t uninit[BLOSC_EXTENDED_HEADER_LENGTH];
  if (blosc2_chunk_uninit(*schunk->storage->cparams, CHUNKSIZE * sizeof(int64_t),
                          uninit, sizeof(uninit)) < 0) {
    return -1;
  }
  blosc_set_timestamp(&t0);
  for (int i = 0; i < NUPDATES; i++) {
    if (blosc2_schunk_update_chunk(schunk, i % NCHUNKS, uninit, true) < 0) {
      return -1;
    }
  }
  results[2] = elapsed_ns_per_op(t0, NUPDATES);

  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(URLPATH);
  free(data);
  free(dest);
  return 0;
}


int main(void) {
  blosc2_init();

  double off[3], on[3];
  if (run(false, off) < 0 || run(true, on) < 0) {
    printf("Error running the frame lock benchmark!\n");
    return -1;
  }

  printf("locking=off  get_lazychunk: %8.0f ns  decompress: %8.0f ns  update: %8.0f ns\n",
         off[0], off[1], off[2]);
  printf("locking=on   get_lazychunk: %8.0f ns  decompress: %8.0f ns  update: %8.0f ns\n",
         on[0], on[1], on[2]);
  printf("overhead:    get_lazychunk: %+7.1f %%   decompress: %+7.1f %%  update: %+7.1f %%\n",
         100.0 * (on[0] - off[0]) / off[0],
         100.0 * (on[1] - off[1]) / off[1],
         100.0 * (on[2] - off[2]) / off[2]);

  blosc2_destroy();
  return 0;
}
