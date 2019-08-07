/*
  Copyright (C) 2019  Francesc Alted
  http://blosc.org
  License: BSD (see LICENSE.txt)

  Creation date: 2019-08-06

  See LICENSE.txt for details about copyright and rights to use.
*/

#include <stdio.h>
#include "test_common.h"

#define CHUNKSIZE (200 * 1000)
#define NTHREADS (2)

/* Global vars */
int tests_run = 0;
int nchunks;


static char* test_schunk() {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  int64_t nbytes, cbytes;
  blosc2_cparams cparams = BLOSC_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;

  /* Initialize the Blosc compressor */
  blosc_init();

  /* Create a super-chunk container */
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  schunk = blosc2_new_schunk(cparams, dparams, NULL);

  // Feed it with data
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk * CHUNKSIZE;
    }
    nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: bad append in frame", nchunk >= 0);
  }

  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  if (nchunks > 0) {
    mu_assert("ERROR: bad compression ratio in frame", nbytes > 10 * cbytes);
  }

  // Exercise the metadata retrieval machinery (i.e. no decompression is happening below)
  bool needs_free;
  uint8_t* chunk;
  size_t nbytes_, cbytes_, blocksize;
  nbytes = 0;
  cbytes = 0;
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    dsize = blosc2_schunk_get_chunk(schunk, nchunk, &chunk, &needs_free);
    mu_assert("ERROR: chunk cannot be retrieved correctly.", dsize >= 0);
    blosc_cbuffer_sizes(chunk, &nbytes_, &cbytes_, &blocksize);
    nbytes += nbytes_;
    cbytes += cbytes_;
    if (needs_free) {
      free(chunk);
    }
  }
  mu_assert("ERROR: nbytes is not correct", nbytes == schunk->nbytes);
  mu_assert("ERROR: cbytes is not correct", cbytes == schunk->cbytes);

  // Check that the chunks have been decompressed correctly
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly.", dsize >= 0);
    for (int i = 0; i < CHUNKSIZE; i++) {
      mu_assert("ERROR: bad roundtrip",data_dest[i] == i + nchunk * CHUNKSIZE);
    }
  }

  /* Free resources */
  blosc2_free_schunk(schunk);
  /* Destroy the Blosc environment */
  blosc_destroy();

  return EXIT_SUCCESS;
}

static char *all_tests() {
  nchunks = 0;
  mu_run_test(test_schunk);

  nchunks = 1;
  mu_run_test(test_schunk);

  nchunks = 10;
  mu_run_test(test_schunk);

  return EXIT_SUCCESS;
}


int main() {
  char *result;

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
