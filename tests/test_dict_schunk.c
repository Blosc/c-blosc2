/*
  Copyright (C) 2015  Francesc Alted
  http://blosc.org
  License: BSD (see LICENSE.txt)

*/

#include <stdio.h>
#include "test_common.h"

#define CHUNKSIZE (200 * 1000)
#define NCHUNKS 100
#define NTHREADS 4

/* Global vars */
int tests_run = 0;
size_t blocksize;
int use_dict;

static char* test_dict() {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  int64_t nbytes, cbytes;
  blosc2_cparams cparams = BLOSC_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;
  size_t nchunks;

  /* Initialize the Blosc compressor */
  blosc_init();

  /* Create a super-chunk container */
  cparams.compcode = BLOSC_ZSTD;
  cparams.use_dict = use_dict;
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS;
  cparams.blocksize = blocksize;
  dparams.nthreads = NTHREADS;
  schunk = blosc2_new_schunk(cparams, dparams);

  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk * CHUNKSIZE;
    }
    nchunks = blosc2_append_buffer(schunk, isize, data);
    mu_assert("ERROR: incorrect nchunks value", nchunks == (nchunk + 1));
  }

  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  float cratio = nbytes / (float)cbytes;
  if (!use_dict) {
    printf("[blocksize: %zd KB] cratio w/o dict: %.2f\n", blocksize, cratio);
    switch (blocksize) {
      case 4 * KB:
        mu_assert("ERROR: No dict does not reach expected compression ratio",
                  10 * cbytes < nbytes);
        break;
      case 32 * KB:
        mu_assert("ERROR: No dict does not reach expected compression ratio",
                  70 * cbytes < nbytes);
        break;
      case 256 * KB:
        mu_assert("ERROR: No dict does not reach expected compression ratio",
                  190 * cbytes < nbytes);
        break;
      default:
        mu_assert("ERROR: No dict does not reach expected compression ratio",
                  170 * cbytes < nbytes);
    }
  } else {
    printf("[blocksize: %zd KB] cratio with dict: %.2f\n", blocksize, cratio);
    switch (blocksize) {
      case 4 * KB:
        mu_assert("ERROR: Dict does not reach expected compression ratio",
                  40 * cbytes < nbytes);
        break;
      case 32 * KB:
        mu_assert("ERROR: Dict does not reach expected compression ratio",
                  70 * cbytes < nbytes);
        break;
      case 256 * KB:
        mu_assert("ERROR: Dict does not reach expected compression ratio",
                  190 * cbytes < nbytes);
        break;
      default:
        mu_assert("ERROR: Dict does not reach expected compression ratio",
                  120 * cbytes < nbytes);
    }
  }

  /* Retrieve and decompress the chunks (0-based count) */
  for (size_t nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    dsize = blosc2_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly.", dsize >= 0);
    for (int i = 0; i < CHUNKSIZE; i++) {
      mu_assert("ERROR: bad roundtrip",
                data_dest[i] == i + nchunk * CHUNKSIZE);
    }
  }

  /* Free resources */
  blosc2_destroy_schunk(schunk);
  /* Destroy the Blosc environment */
  blosc_destroy();

  return EXIT_SUCCESS;
}


static char *all_tests() {
  blocksize = 4 * KB;    // page size
  use_dict = 0;
  mu_run_test(test_dict);
  use_dict = 1;
  mu_run_test(test_dict);

  blocksize = 32 * KB;   // L1 cache size
  use_dict = 0;
  mu_run_test(test_dict);
  use_dict = 1;
  mu_run_test(test_dict);

  blocksize = 256 * KB;   // L2 cache size
  use_dict = 0;
  mu_run_test(test_dict);
  use_dict = 1;
  mu_run_test(test_dict);

  blocksize = 0;         // automatic size
  use_dict = 0;
  mu_run_test(test_dict);
  use_dict = 1;
  mu_run_test(test_dict);

  return EXIT_SUCCESS;
}


int main(int argc, char **argv) {
  char *result;

  printf("STARTING TESTS for %s", argv[0]);

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
