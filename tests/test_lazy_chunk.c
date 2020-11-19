/*
  Copyright (C) 2020 The Blosc Developers
  http://blosc.org
  License: BSD (see LICENSE.txt)

  Creation date: 2020-11-19

  See LICENSE.txt for details about copyright and rights to use.
*/

#include <stdio.h>
#include "test_common.h"

#define CHUNKSIZE (200 * 1000)
#define NTHREADS (2)

/* Global vars */
int tests_run = 0;
int nchunks;


static char* test_lazy_chunk(void) {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  int64_t nbytes, cbytes;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;

  /* Initialize the Blosc compressor */
  blosc_init();

  /* Create a super-chunk container, backed by a frame */
  cparams.typesize = sizeof(int32_t);
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.sequential=true, .path="test_lazy_chunk.b2frame", .cparams=&cparams, .dparams=&dparams};
  schunk = blosc2_schunk_new(storage);

  // Feed it with data
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk * CHUNKSIZE;
    }
    int nchunks_ = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: bad append in frame", nchunks_ > 0);
  }

  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  if (nchunks > 0) {
    mu_assert("ERROR: bad compression ratio in frame", nbytes > 10 * cbytes);
  }

  // Check that lazy chunks can be decompressed correctly
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    memset(data_dest, 0, CHUNKSIZE * sizeof(int32_t));
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly.", dsize >= 0);
    for (int i = 0; i < CHUNKSIZE; i++) {
      mu_assert("ERROR: bad roundtrip",data_dest[i] == i + nchunk * CHUNKSIZE);
    }
  }

  // Check that blosc2_getitem_ctx works correctly with lazy chunks
  bool needs_free;
  uint8_t* lazy_chunk;
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    memset(data_dest, 0, CHUNKSIZE * sizeof(int32_t));
    cbytes  = blosc2_schunk_get_chunk(schunk, nchunk, &lazy_chunk, &needs_free);
    dsize = blosc2_getitem_ctx(schunk->dctx, lazy_chunk, cbytes, nchunk, nchunk * 100, data_dest);
    mu_assert("ERROR: blosc2_getitem_ctx does not work correctly.", dsize >= 0);
    for (int i = nchunk; i < nchunk * 100; i++) {
      mu_assert("ERROR: bad roundtrip",data_dest[i - nchunk] == i + nchunk * CHUNKSIZE);
    }
    if (needs_free) {
      free(lazy_chunk);
    }
  }

  /* Free resources */
  blosc2_schunk_free(schunk);
  /* Destroy the Blosc environment */
  blosc_destroy();

  return EXIT_SUCCESS;
}

static char *all_tests(void) {
  nchunks = 0;
  mu_run_test(test_lazy_chunk);

  nchunks = 1;
  mu_run_test(test_lazy_chunk);

  nchunks = 10;
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
