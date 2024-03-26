/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdio.h>
#include "test_common.h"

#define CHUNKSIZE (200 * 1000)
#define NCHUNKS 10
#define NTHREADS 4

/* Global vars */
int tests_run = 0;
int blocksize;
int use_dict;

static char* test_dict(void) {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  int64_t nbytes, cbytes;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;
  int64_t nchunks;
  blosc_timestamp_t last, current;
  double cttotal, dttotal;

  /* Initialize the Blosc compressor */
  blosc2_init();

  /* Create a super-chunk container */
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_ZSTD;
  cparams.use_dict = use_dict;
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS;
  cparams.blocksize = blocksize;
  cparams.splitmode = BLOSC_FORWARD_COMPAT_SPLIT;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams};
  schunk = blosc2_schunk_new(&storage);

  // Feed it with data
  blosc_set_timestamp(&last);
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk * CHUNKSIZE;
    }
    nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: incorrect nchunks value", nchunks == (nchunk + 1));
  }
  blosc_set_timestamp(&current);
  cttotal = blosc_elapsed_secs(last, current);

  /* Retrieve and decompress the chunks */
  blosc_set_timestamp(&last);
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: Decompression error.", dsize > 0);
  }
  blosc_set_timestamp(&current);
  dttotal = blosc_elapsed_secs(last, current);

  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  float cratio =(float)nbytes / (float)cbytes;
  float cspeed = (float)nbytes / ((float)cttotal * MB);
  float dspeed = (float)nbytes / ((float)dttotal * MB);
  if (tests_run == 0) printf("\n");
  if (blocksize > 0) {
    printf("[blocksize: %d KB] ", blocksize / 1024);
  } else {
    printf("[blocksize: automatic] ");
  }
  if (!use_dict) {
    printf("cratio w/o dict: %.1fx (compr @ %.1f MB/s, decompr @ %.1f MB/s)\n",
            cratio, cspeed, dspeed);
    switch (blocksize) {
      case 1 * KB:
        mu_assert("ERROR: No dict does not reach expected compression ratio",
                  3 * cbytes < nbytes);
        break;
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
    printf("cratio with dict: %.1fx (compr @ %.1f MB/s, decompr @ %.1f MB/s)\n",
           cratio, cspeed, dspeed);
    switch (blocksize) {
      case 1 * KB:
        mu_assert("ERROR: Dict does not reach expected compression ratio",
                  8 * cbytes < nbytes);
        break;
      case 4 * KB:
        mu_assert("ERROR: Dict does not reach expected compression ratio",
                  15 * cbytes < nbytes);
        break;
      case 32 * KB:
        mu_assert("ERROR: Dict does not reach expected compression ratio",
                  100 * cbytes < nbytes);
        break;
      case 256 * KB:
        mu_assert("ERROR: Dict does not reach expected compression ratio",
                  180 * cbytes < nbytes);
        break;
      default:
        mu_assert("ERROR: Dict does not reach expected compression ratio",
                  180 * cbytes < nbytes);
    }
  }

  // Check that the chunks have been decompressed correctly
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly.", dsize >= 0);
    for (int i = 0; i < CHUNKSIZE; i++) {
      mu_assert("ERROR: bad roundtrip",
                data_dest[i] == i + nchunk * CHUNKSIZE);
    }
  }

  /* Free resources */
  blosc2_schunk_free(schunk);
  /* Destroy the Blosc environment */
  blosc2_destroy();

  return EXIT_SUCCESS;
}


static char *all_tests(void) {
  blocksize = 1 * KB;    // really tiny
  use_dict = 0;
  mu_run_test(test_dict);
  use_dict = 1;
  mu_run_test(test_dict);

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


int main(void) {
  char *result;

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
