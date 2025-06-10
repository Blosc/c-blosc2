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

typedef struct {
    int nchunks;
    int64_t start;
    int64_t stop;
    char* urlpath;
    bool contiguous;
    bool shorter_last_chunk;
} test_data;

test_data tdata;

typedef struct {
    int nchunks;
    int64_t start;
    int64_t stop;
    bool shorter_last_chunk;
} test_ndata;

test_ndata tndata[] = {
        {10, 0, 10 * CHUNKSIZE, false}, //whole schunk
        {5,  3, 200, false}, //piece of 1 block
        {33, 5, 679, false}, // blocks of same chunk
        {12,  129 * 100, 134 * 100 * 3, false}, // blocks of different chunks
        {2, 200 * 100, CHUNKSIZE * 2, false}, // 1 chunk
        {5, 0, CHUNKSIZE * 5 + 200 * 100 + 300, true}, // last chunk shorter
        {2, 10, CHUNKSIZE * 2 + 400, true}, // start != 0, last chunk shorter
};

typedef struct {
    bool contiguous;
    char *urlpath;
} test_storage;

test_storage tstorage[] = {
        {false, NULL},  // memory - schunk
        {true, NULL},  // memory - cframe
        {true, "test_get_slice_buffer.b2frame"}, // disk - cframe
        {false, "test_get_slice_buffer.b2frame"}, // disk - sframe
};


static char* test_get_slice_buffer(void) {
  static int32_t data[CHUNKSIZE];
  int32_t *data_;
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
  cparams.blocksize = 0;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams,
                            .urlpath=tdata.urlpath, .contiguous=tdata.contiguous};
  schunk = blosc2_schunk_new(&storage);

  // Feed it with data
  if (!tdata.shorter_last_chunk) {
    for (int nchunk = 0; nchunk < tdata.nchunks; nchunk++) {
      for (int i = 0; i < CHUNKSIZE; i++) {
        data[i] = i + nchunk * CHUNKSIZE;
      }
      int64_t nchunks_ = blosc2_schunk_append_buffer(schunk, data, isize);
      mu_assert("ERROR: bad append in frame", nchunks_ > 0);
    }
  }
  else {
    data_ = malloc(sizeof(int32_t) * tdata.stop);
    for (int i = 0; i < tdata.stop; i++) {
      data_[i] = i;
    }
    for (int nchunk = 0; nchunk < tdata.nchunks; nchunk++) {
      int64_t nchunks_ = blosc2_schunk_append_buffer(schunk, data_ + nchunk * CHUNKSIZE, isize);
      mu_assert("ERROR: bad append in frame", nchunks_ > 0);
    }
    int64_t nchunks_ = blosc2_schunk_append_buffer(schunk, data_ + tdata.nchunks * CHUNKSIZE,
                                                   (tdata.stop % CHUNKSIZE) * sizeof(int32_t));
    mu_assert("ERROR: bad append in frame", nchunks_ > 0);
  }

  // Get slice
  int32_t *buffer = malloc((tdata.stop - tdata.start) * schunk->typesize);
  rc = blosc2_schunk_get_slice_buffer(schunk, tdata.start, tdata.stop, buffer);
  mu_assert("ERROR: cannot get slice correctly.", rc >= 0);
  if (tdata.shorter_last_chunk) {
    for (int64_t i = tdata.start; i < (tdata.stop - tdata.start); ++i) {
      mu_assert("ERROR: bad roundtrip get slice",
                buffer[i - tdata.start] == data_[i]);
    }
    free(data_);
  }
  else {
    // Check that the data has been decompressed correctly
    for (int64_t i = 0; i < (tdata.stop - tdata.start); ++i) {
      mu_assert("ERROR: bad roundtrip get slice",
                buffer[i] == ((i + tdata.start) % CHUNKSIZE) + (i + tdata.start)/CHUNKSIZE * CHUNKSIZE);
    }
  }


  /* Free resources */
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(tdata.urlpath);
  /* Destroy the Blosc environment */
  blosc2_destroy();

  free(buffer);

  return EXIT_SUCCESS;
}

static char *all_tests(void) {
  for (int i = 0; i < (int) ARRAY_SIZE(tstorage); ++i) {
    for (int j = 0; j < (int) ARRAY_SIZE(tndata); ++j) {
      tdata.contiguous = tstorage[i].contiguous;
      tdata.urlpath = tstorage[i].urlpath;
      tdata.nchunks = tndata[j].nchunks;
      tdata.start = tndata[j].start;
      tdata.stop = tndata[j].stop;
      tdata.shorter_last_chunk = tndata[j].shorter_last_chunk;
      mu_run_test(test_get_slice_buffer);
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
