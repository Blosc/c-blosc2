/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  Regression test: the temporaries of the decompression threads have one
  block plus 4 * typesize bytes per region, and the block of a lazy chunk is
  read into the last region.  A thread that first works on a chunk with a
  small typesize must grow them for a later chunk with the same blocksize
  and a larger typesize, or reading its (split, incompressible) block of
  blocksize + 4 * typesize bytes overruns them.
*/

#include <stdio.h>
#include "test_common.h"

#define URLPATH "lazychunk_typesize_threads.b2frame"
#define TYPESIZE1 255
#define BLOCKSIZE (TYPESIZE1 * 16)
#define CHUNKSIZE (2 * BLOCKSIZE)

int tests_run = 0;

static char* test_lazychunk_typesize_threads(void) {
  uint8_t *data0 = malloc(CHUNKSIZE);
  uint8_t *data1 = malloc(CHUNKSIZE);
  uint8_t *dest = malloc(CHUNKSIZE);
  uint32_t seed = 1;
  for (int i = 0; i < CHUNKSIZE; i++) {
    data0[i] = (uint8_t)(i % 7);
    seed = seed * 1103515245u + 12345u;
    // First block incompressible, second one all zeros
    data1[i] = i < BLOCKSIZE ? (uint8_t)(seed >> 16) : 0;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  cparams.blocksize = BLOCKSIZE;
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.splitmode = BLOSC_ALWAYS_SPLIT;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_NOFILTER;
  blosc2_storage storage = {.urlpath = URLPATH, .contiguous = true, .cparams = &cparams};
  blosc2_remove_urlpath(URLPATH);
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: cannot create schunk", schunk != NULL);
  mu_assert("ERROR: cannot append chunk 0",
            blosc2_schunk_append_buffer(schunk, data0, CHUNKSIZE) == 1);

  // Same blocksize, but a typesize of 255: the first block is stored as
  // 255 raw streams, i.e. blocksize + 4 * 255 bytes
  cparams.typesize = TYPESIZE1;
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  int32_t chunk_size = CHUNKSIZE + BLOSC2_MAX_OVERHEAD;
  uint8_t *chunk = malloc(chunk_size);
  int cbytes = blosc2_compress_ctx(cctx, data1, CHUNKSIZE, chunk, chunk_size);
  blosc2_free_ctx(cctx);
  mu_assert("ERROR: cannot compress chunk 1", cbytes > 0 && cbytes < CHUNKSIZE);
  mu_assert("ERROR: cannot append chunk 1",
            blosc2_schunk_append_chunk(schunk, chunk, true) == 2);
  blosc2_schunk_free(schunk);

  // Reopen, so that chunks are read lazily, and decompress with threads
  schunk = blosc2_schunk_open(URLPATH);
  mu_assert("ERROR: cannot open schunk", schunk != NULL);
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 2;
  dparams.schunk = schunk;
  blosc2_free_ctx(schunk->dctx);
  schunk->dctx = blosc2_create_dctx(dparams);

  int rc = blosc2_schunk_get_slice_buffer(schunk, 0, CHUNKSIZE, dest);
  mu_assert("ERROR: cannot get slice of chunk 0", rc == 0);
  mu_assert("ERROR: wrong data in chunk 0", memcmp(dest, data0, CHUNKSIZE) == 0);
  rc = blosc2_schunk_get_slice_buffer(schunk, CHUNKSIZE, 2 * CHUNKSIZE, dest);
  mu_assert("ERROR: cannot get slice of chunk 1", rc == 0);
  mu_assert("ERROR: wrong data in chunk 1", memcmp(dest, data1, CHUNKSIZE) == 0);

  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(URLPATH);
  free(chunk);
  free(dest);
  free(data1);
  free(data0);

  return EXIT_SUCCESS;
}

static char *all_tests(void) {
  mu_run_test(test_lazychunk_typesize_threads);

  return EXIT_SUCCESS;
}

int main(void) {
  char *result;

  install_blosc_callback_test(); /* optionally install callback test */
  blosc2_init();

  /* Run all the suite */
  result = all_tests();
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
