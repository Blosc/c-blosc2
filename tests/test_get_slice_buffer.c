/*
  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
*/

#include <stdio.h>
#include "test_common.h"

#define CHUNKSIZE (200 * 1000)
#define NTHREADS (2)

/* Global vars */
int tests_run = 0;
int nchunks;


static char* test_get_slice_buffer(void) {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int rc;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;

  /* Initialize the Blosc compressor */
  blosc2_init();

  /* Create a super-chunk container */
  cparams.typesize = sizeof(int32_t);
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams};
  schunk = blosc2_schunk_new(&storage);

  // Feed it with data
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk * CHUNKSIZE;
    }
    int64_t nchunks_ = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: bad append in frame", nchunks_ > 0);
  }

  int64_t start = rand() % (schunk->nbytes / schunk->typesize) - 1;
  int64_t stop = rand() % (schunk->nbytes / schunk->typesize) + 1;
  int32_t *buffer = malloc((stop - start) * schunk->typesize);
  rc = blosc2_schunk_get_slice_buffer(schunk, start, stop, buffer);
  mu_assert("ERROR: cannot get slice correctly.", rc >= 0);

  // Check that the chunks have been decompressed correctly
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    rc = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly.", rc >= 0);
    for (int i = 0; i < CHUNKSIZE; i++) {
      mu_assert("ERROR: bad roundtrip decomp",data_dest[i] == i + nchunk * CHUNKSIZE);
    }
  }

  for (int64_t i = 0; i < (stop - start); ++i) {
    mu_assert("ERROR: bad roundtrip get slice",buffer[i] == ((i + start) % CHUNKSIZE) + (i + start)/CHUNKSIZE * CHUNKSIZE);
  }



  /* Free resources */
  blosc2_schunk_free(schunk);
  /* Destroy the Blosc environment */
  blosc2_destroy();

  free(buffer);

  return EXIT_SUCCESS;
}

static char *all_tests(void) {
  nchunks = 1;
  mu_run_test(test_get_slice_buffer);

  nchunks = 5;
  mu_run_test(test_get_slice_buffer);

  nchunks = 10;
  mu_run_test(test_get_slice_buffer);

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
