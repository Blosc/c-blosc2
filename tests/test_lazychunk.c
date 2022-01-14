/*
  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
*/

#include <stdio.h>
#include "test_common.h"

#define CHUNKSIZE (200 * 1000)
#define BLOCKSIZE (20 * 1000)
#define NBLOCKS (CHUNKSIZE / BLOCKSIZE)

/* Global vars */
int tests_run = 0;
int nchunks;
int clevel;
int16_t nthreads;
uint8_t filter;


static char* test_lazy_chunk(void) {
  int32_t *data = malloc(CHUNKSIZE * sizeof(int32_t));
    int32_t *data_dest = malloc(CHUNKSIZE * sizeof(int32_t));
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  int cbytes;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.filters[5] = filter;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;

  /* Initialize the Blosc compressor */
  blosc_init();

  /* Create a super-chunk container, backed by a frame */
  cparams.typesize = sizeof(int32_t);
  cparams.clevel = clevel;
  cparams.nthreads = nthreads;
  cparams.blocksize = BLOCKSIZE * cparams.typesize;
  dparams.nthreads = nthreads;
  char* urlpath = "test_lazy_chunk.b2frame";
  remove(urlpath);
  blosc2_storage storage = {.contiguous=true, .urlpath=urlpath, .cparams=&cparams, .dparams=&dparams};

  schunk = blosc2_schunk_new(&storage);

  // Feed it with data
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    for (int i = 0; i < NBLOCKS; i++) {
      for (int j = 0; j < BLOCKSIZE; j++) {
        data[j + i * BLOCKSIZE] = j + i * BLOCKSIZE + nchunk * CHUNKSIZE;
      }
    }
    int nchunks_ = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: bad append in frame", nchunks_ > 0);
  }

  /* Gather some info */
  if (nchunks > 0 && clevel > 0) {
    mu_assert("ERROR: bad compression ratio in frame", schunk->nbytes > 10 * schunk->cbytes);
  }

  // Check that blosc2_getitem_ctx works correctly with lazy chunks
  bool needs_free;
  uint8_t* lazy_chunk;
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    cbytes = blosc2_schunk_get_lazychunk(schunk, nchunk, &lazy_chunk, &needs_free);
    for (int i = 0; i < NBLOCKS - 1; i++) {
      memset(data_dest, 0, isize);
      dsize = blosc2_getitem_ctx(schunk->dctx, lazy_chunk, cbytes, i * BLOCKSIZE, BLOCKSIZE * 2, data_dest, isize);
      mu_assert("ERROR: blosc2_getitem_ctx does not work correctly.", dsize >= 0);
      for (int j = 0; j < BLOCKSIZE * 2; j++) {
        mu_assert("ERROR: bad roundtrip (blosc2_getitem_ctx)",
                  data_dest[j] == j + i * BLOCKSIZE + nchunk * CHUNKSIZE);
      }
    }
    if (needs_free) {
      free(lazy_chunk);
    }
  }

  // Check that lazy chunks can be decompressed correctly
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    memset(data_dest, 0, isize);
    cbytes = blosc2_schunk_get_lazychunk(schunk, nchunk, &lazy_chunk, &needs_free);
    mu_assert("ERROR: cannot get lazy chunk.", cbytes > 0);
    dsize = blosc2_decompress_ctx(schunk->dctx, lazy_chunk, cbytes, data_dest, isize);
    if (needs_free) {
      free(lazy_chunk);
    }
    mu_assert("ERROR: chunk cannot be decompressed correctly.", dsize >= 0);
    for (int i = 0; i < NBLOCKS; i++) {
      for (int j = 0; j < BLOCKSIZE; j++) {
        mu_assert("ERROR: bad roundtrip (blosc2_decompress_ctx)",
                  data_dest[j + i * BLOCKSIZE] == j + i * BLOCKSIZE + nchunk * CHUNKSIZE);
      }
    }
  }

  /* Free resources */
  blosc2_schunk_free(schunk);
  /* Destroy the Blosc environment */
  blosc_destroy();

  free(data);
  free(data_dest);

  return EXIT_SUCCESS;
}

static char *all_tests(void) {
  nchunks = 0;
  clevel = 5;
  nthreads = 1;
  filter = BLOSC_SHUFFLE;
  mu_run_test(test_lazy_chunk);

  nchunks = 1;
  clevel = 5;
  nthreads = 2;
  filter = BLOSC_SHUFFLE;
  mu_run_test(test_lazy_chunk);

  nchunks = 1;
  clevel = 0;
  nthreads = 2;
  filter = BLOSC_BITSHUFFLE;
  mu_run_test(test_lazy_chunk);

  nchunks = 10;
  clevel = 5;
  nthreads = 1;
  filter = BLOSC_SHUFFLE;
  mu_run_test(test_lazy_chunk);

  nchunks = 10;
  clevel = 5;
  nthreads = 2;
  filter = BLOSC_BITSHUFFLE;
  mu_run_test(test_lazy_chunk);

  nchunks = 10;
  clevel = 0;
  nthreads = 1;
  filter = BLOSC_SHUFFLE;
  mu_run_test(test_lazy_chunk);

  nchunks = 10;
  clevel = 0;
  nthreads = 2;
  filter = BLOSC_BITSHUFFLE;
  mu_run_test(test_lazy_chunk);

  return EXIT_SUCCESS;
}


int main(void) {
  char *result;

  install_blosc_callback_test(); /* optionally install callback test */
  blosc_init();

  /* Run all the suite */
  result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  blosc_destroy();

  return result != EXIT_SUCCESS;
}
