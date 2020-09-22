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
int pos;


static char* test_insert_schunk(void) {
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
    int nchunks_ = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: bad append", nchunks_ > 0);
  }

  // Insert chunk in specified position
  for (int i = 0; i < CHUNKSIZE; ++i) {
    data[i] = 0;
  }
  int32_t datasize = sizeof(int32_t) * CHUNKSIZE;
  int32_t chunksize = sizeof(int32_t) * CHUNKSIZE + BLOSC_MAX_OVERHEAD;
  uint8_t *chunk = malloc(chunksize);
  int csize = blosc2_compress_ctx(schunk->cctx, data, datasize, chunk, chunksize);
  mu_assert("ERROR: chunk cannot be compressed", csize >= 0);
  int _nchunks = blosc2_schunk_insert_chunk(schunk, pos, chunk, true);
  mu_assert("ERROR: chunk cannot be inserted correctly", _nchunks > 0);

  // Check that the chunks have been decompressed correctly
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    if (nchunk < pos) {
      dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    } else {
      dsize = blosc2_schunk_decompress_chunk(schunk, nchunk + 1, (void *) data_dest, isize);
    }
    mu_assert("ERROR: chunk cannot be decompressed correctly", dsize >= 0);
    for (int i = 0; i < CHUNKSIZE; i++) {
      mu_assert("ERROR: bad roundtrip", data_dest[i] == i + nchunk * CHUNKSIZE);
    }
  }
  dsize = blosc2_schunk_decompress_chunk(schunk, pos, (void *) data_dest, isize);
  mu_assert("ERROR: chunk cannot be decompressed correctly", dsize >= 0);
  for (int i = 0; i < CHUNKSIZE; i++) {
    mu_assert("ERROR: bad roundtrip", data_dest[i] == 0);
  }

  /* Free resources */
  blosc2_free_schunk(schunk);
  /* Destroy the Blosc environment */
  blosc_destroy();

  return EXIT_SUCCESS;
}

static char *all_tests(void) {

  nchunks = 10;
  pos = 4;
  mu_run_test(test_insert_schunk);

  nchunks = 5;
  pos = 0;
  mu_run_test(test_insert_schunk);

  nchunks = 33;
  pos = 33;
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
