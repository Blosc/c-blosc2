/*
  Copyright (C) 2020- The Blosc Development Team <blosc@blosc.org>
  http://blosc.org
  License: BSD (see LICENSE.txt)

  Creation date: 2020-09-23

  See LICENSE.txt for details about copyright and rights to use.
*/

#include <stdio.h>
#include "test_common.h"

#define CHUNKSIZE (200 * 1000)
#define NTHREADS (2)

/* Global vars */
int tests_run = 0;
int nchunks;
int n_insertions;
bool copy;

static char* test_insert_schunk(void) {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;

  /* Initialize the Blosc compressor */
  blosc_init();

  /* Create a super-chunk container */
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams};
  schunk = blosc2_schunk_new(storage);

  // Feed it with data
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk * CHUNKSIZE;
    }
    int nchunks_ = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: bad append", nchunks_ > 0);
  }

  for (int i = 0; i < n_insertions; ++i) {
    // Create chunk
    for (int j = 0; j < CHUNKSIZE; ++j) {
      data[j] = i;
    }
    int32_t datasize = sizeof(int32_t) * CHUNKSIZE;
    int32_t chunksize = sizeof(int32_t) * CHUNKSIZE + BLOSC_MAX_OVERHEAD;
    uint8_t *chunk = malloc(chunksize);
    int csize = blosc2_compress_ctx(schunk->cctx, data, datasize, chunk, chunksize);
    mu_assert("ERROR: chunk cannot be compressed", csize >= 0);

    // Insert in a random position
    int pos = rand() % schunk->nchunks;
    int _nchunks = blosc2_schunk_insert_chunk(schunk, pos, chunk, copy);
    mu_assert("ERROR: chunk cannot be inserted correctly", _nchunks > 0);

    // Check that the inserted chunk can be decompressed correctly
    dsize = blosc2_schunk_decompress_chunk(schunk, pos, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly", dsize >= 0);
    for (int j = 0; j < CHUNKSIZE; j++) {
      mu_assert("ERROR: bad roundtrip", data_dest[j] == i);
    }
    // Free allocated chunk
    if (copy) {
      free(chunk);
    }
  }

  // Check that the chunks have been decompressed correctly
  for (int nchunk = 0; nchunk < schunk->nchunks; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly", dsize >= 0);
  }


  /* Free resources */
  blosc2_schunk_free(schunk);
  /* Destroy the Blosc environment */
  blosc_destroy();

  return EXIT_SUCCESS;
}

static char *all_tests(void) {

  nchunks = 10;
  n_insertions = 1;
  copy = true;
  mu_run_test(test_insert_schunk);

  nchunks = 5;
  n_insertions = 3;
  copy = true;
  mu_run_test(test_insert_schunk);

  nchunks = 33;
  n_insertions = 5;
  copy = false;
  mu_run_test(test_insert_schunk);

  nchunks = 12;
  n_insertions = 24;
  copy = true;
  mu_run_test(test_insert_schunk);


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
