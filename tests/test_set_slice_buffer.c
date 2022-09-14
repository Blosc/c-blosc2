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

typedef struct {
    int nchunks;
    int64_t start;
    int64_t stop;
    char* urlpath;
    bool contiguous;
} test_data;

test_data tdata;

typedef struct {
    int nchunks;
    int64_t start;
    int64_t stop;
} test_ndata;

test_ndata tndata[] = {
        {10, 0, 10 * CHUNKSIZE}, //whole schunk
        {5,  3, 200}, //piece of 1 block
        {33, 5, 679}, // blocks of same chunk
        {12,  129 * 100, 134 * 100 * 3}, // blocks of diferent chunks
        {3, 200 * 100, CHUNKSIZE * 3}, // 2 chunks
};

typedef struct {
    bool contiguous;
    char *urlpath;
}test_storage;

test_storage tstorage[] = {
        {false, NULL},  // memory - schunk
        {true, NULL},  // memory - cframe
        {true, "test_set_slice_buffer.b2frame"}, // disk - cframe
        {false, "test_set_slice_buffer.b2frame"}, // disk - sframe
};

static char* test_set_slice_buffer(void) {
  static int32_t data[CHUNKSIZE];
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int rc;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;

  /* Initialize the Blosc compressor */
  blosc2_init();

  /* Create a super-chunk container */
  blosc2_remove_urlpath(tdata.urlpath);
  cparams.typesize = sizeof(int32_t);
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams,
                            .urlpath=tdata.urlpath, .contiguous=tdata.contiguous};
  schunk = blosc2_schunk_new(&storage);

  // Feed it with data
  for (int nchunk = 0; nchunk < tdata.nchunks; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk * CHUNKSIZE;
    }
    int64_t nchunks_ = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: bad append in frame", nchunks_ > 0);
  }

  // Set slice
  int32_t *buffer = malloc((tdata.stop - tdata.start) * schunk->typesize);
  for (int i = 0; i < (tdata.stop - tdata.start); ++i) {
    buffer[i] = i + tdata.nchunks * CHUNKSIZE;
  }
  rc = blosc2_schunk_set_slice_buffer(schunk, tdata.start, tdata.stop, buffer);
  mu_assert("ERROR: cannot set slice correctly.", rc >= 0);
  int32_t *res = malloc((tdata.stop - tdata.start) * schunk->typesize);
  // Check that the data has been updated correctly
  rc = blosc2_schunk_get_slice_buffer(schunk, tdata.start, tdata.stop, res);
  mu_assert("ERROR: cannot get slice correctly.", rc >= 0);
  for (int64_t i = 0; i < (tdata.stop - tdata.start); ++i) {
    mu_assert("ERROR: bad roundtrip",buffer[i] == res[i]);
  }

  /* Free resources */
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(tdata.urlpath);
  /* Destroy the Blosc environment */
  blosc2_destroy();

  free(buffer);
  free(res);

  return EXIT_SUCCESS;
}

static char *all_tests(void) {
  for (int i = 0; i < (int) (sizeof(tstorage) / sizeof(test_storage)); ++i) {
    for (int j = 0; j < (int) (sizeof(tndata) / sizeof(test_ndata)); ++j) {
      tdata.contiguous = tstorage[i].contiguous;
      tdata.urlpath = tstorage[i].urlpath;
      tdata.nchunks = tndata[j].nchunks;
      tdata.start = tndata[j].start;
      tdata.stop = tndata[j].stop;
      mu_run_test(test_set_slice_buffer);
    }
  }

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
