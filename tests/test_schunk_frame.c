/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
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
int contiguous = false;
bool copy = false;
bool special_chunks = false;


static char* test_schunk_cframe(void) {
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int32_t *data = malloc(isize);
  int32_t *data_zeros = calloc(CHUNKSIZE, sizeof(int32_t));
  int32_t *data_dest = malloc(isize);
  int dsize;
  blosc2_schunk* schunk;

  /* Initialize the Blosc compressor */
  blosc2_init();

  /* Create a super-chunk container */
  blosc2_storage storage = {.contiguous=contiguous};
  schunk = blosc2_schunk_new(&storage);

  // Feed it with data
  int64_t nchunks_;
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    if (special_chunks && nchunk >= 2) {
      nchunks_ = blosc2_schunk_append_buffer(schunk, data_zeros, isize);
    }
    else {
      for (int i = 0; i < CHUNKSIZE; i++) {
        data[i] = i + nchunk * CHUNKSIZE;
      }
      nchunks_ = blosc2_schunk_append_buffer(schunk, data, isize);
    }
    mu_assert("ERROR: bad append in frame", nchunks_ > 0);
  }

  // Get a memory frame out of the schunk
  uint8_t* cframe;
  bool cframe_needs_free;
  int64_t len = blosc2_schunk_to_buffer(schunk, &cframe, &cframe_needs_free);
  mu_assert("Error in getting a frame buffer", len > 0);

  // ...and another schunk backed by the contiguous frame buffer
  blosc2_schunk* schunk2 = blosc2_schunk_from_buffer(cframe, len, copy);

  if (!copy) {
    // Now store frame in a file
    len = blosc2_schunk_to_file(schunk2, "test_file.b2frame");
    mu_assert("Error in storing a frame buffer", len > 0);
  }

  // Free completely the schunks
  blosc2_schunk_free(schunk);
  if (!copy) {
    blosc2_schunk_free(schunk2);
    // ...and open a new one back
    schunk = blosc2_schunk_open("test_file.b2frame");
  }
  else {
    schunk = schunk2;
  }

  // Check that the chunks have been decompressed correctly
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly.", dsize >= 0);
    if (special_chunks && nchunk >= 2) {
      for (int i = 0; i < CHUNKSIZE; i++) {
        mu_assert("ERROR: bad roundtrip",data_dest[i] == 0);
      }
    }
    else {
      for (int i = 0; i < CHUNKSIZE; i++) {
        mu_assert("ERROR: bad roundtrip",data_dest[i] == i + nchunk * CHUNKSIZE);
      }
    }
  }

  /* Free resources */
  free(data);
  free(data_zeros);
  free(data_dest);
  blosc2_schunk_free(schunk);
  if (cframe_needs_free) {
    free(cframe);
  }
  /* Destroy the Blosc environment */
  blosc2_destroy();

  return EXIT_SUCCESS;
}

static char *all_tests(void) {
  nchunks = 0;
  contiguous = true;
  mu_run_test(test_schunk_cframe);

  nchunks = 0;
  contiguous = false;
  mu_run_test(test_schunk_cframe);

  nchunks = 1;
  contiguous = false;
  mu_run_test(test_schunk_cframe);

  nchunks = 10;
  contiguous = true;
  mu_run_test(test_schunk_cframe);

  nchunks = 5;
  contiguous = true;
  copy = true;
  special_chunks = true;
  mu_run_test(test_schunk_cframe);

  nchunks = 5;
  contiguous = true;
  copy = false;
  special_chunks = true;
  mu_run_test(test_schunk_cframe);


  nchunks = 5;
  contiguous = false;
  copy = true;
  special_chunks = true;
  mu_run_test(test_schunk_cframe);

  nchunks = 5;
  contiguous = false;
  copy = false;
  special_chunks = true;
  mu_run_test(test_schunk_cframe);

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
