/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdio.h>
#include "test_common.h"

#define CHUNKSIZE (200 * 1000)
#define NTHREADS (2)

/* Global vars */
int tests_run = 0;


typedef struct {
  int nchunks;
  int ndeletes;
  char* urlpath;
  bool contiguous;
} test_data;

test_data tdata;

typedef struct {
  int nchunks;
  int ndeletes;
} test_ndata;

test_ndata tndata[] = {
    {10, 1},
    {5,  3},
    {33, 5},
    {1,  0},
    {12, 12},
    {1, 1},
    {0, 0},
};

typedef struct {
  bool contiguous;
  char *urlpath;
} test_storage;

test_storage tstorage[] = {
    {false, NULL},  // memory - schunk
    {true, NULL},  // memory - cframe
    {true, "test_delete_chunk.b2frame"}, // disk - cframe
    {false, "test_delete_chunk_s.b2frame"}, // disk - sframe
};

bool tcopy[] = {
    true,
    false
};

static char* test_delete_chunk(void) {
  /* Free resources */
  blosc2_remove_urlpath(tdata.urlpath);

  int32_t *data = malloc(CHUNKSIZE * sizeof(int32_t));
  int32_t *data_dest = malloc(CHUNKSIZE * sizeof(int32_t));
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;

  /* Initialize the Blosc compressor */
  blosc2_init();

  /* Create a super-chunk container */
  cparams.typesize = sizeof(int32_t);
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
    mu_assert("ERROR: bad append", nchunks_ > 0);
  }

  if (tdata.nchunks >= 2) {
    // Check that no file is removed if a special value chunk is deleted in a sframe
    int64_t _nchunks = blosc2_schunk_delete_chunk(schunk, 1);
    mu_assert("ERROR: chunk 1 cannot be deleted correctly", _nchunks >= 0);
    _nchunks = blosc2_schunk_delete_chunk(schunk, 0);
    mu_assert("ERROR: chunk 0 cannot be deleted correctly", _nchunks >= 0);
  }

  for (int i = 0; i < tdata.ndeletes - 2; ++i) {
    // Delete in a random position
    int64_t pos = rand() % (schunk->nchunks);
    int64_t nchunks_old = schunk->nchunks;
    if (pos != nchunks_old - 1) {
      dsize = blosc2_schunk_decompress_chunk(schunk, pos + 1, (void *) data, isize);
      mu_assert("ERROR: chunk cannot be decompressed correctly", dsize >= 0);
    }

    int64_t _nchunks = blosc2_schunk_delete_chunk(schunk, pos);
    mu_assert("ERROR: chunk cannot be deleted correctly", _nchunks >= 0);

    if (pos != nchunks_old - 1) {
      dsize = blosc2_schunk_decompress_chunk(schunk, pos, (void *) data_dest, isize);
      // Check that the inserted chunk can be decompressed correctly
      mu_assert("ERROR: chunk cannot be decompressed correctly", dsize >= 0);

      for (int j = 0; j < CHUNKSIZE; j++) {
        int32_t a = data[j];
        int32_t b = data_dest[j];
        mu_assert("ERROR: bad roundtrip", a == b);
      }
    }

    mu_assert("ERROR: chunk is not deleted", nchunks_old - 1 == schunk->nchunks);
  }

  // Check that the chunks have been decompressed correctly
  for (int nchunk = 0; nchunk < schunk->nchunks; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly", dsize >= 0);
  }


  /* Free resources */
  blosc2_remove_urlpath(storage.urlpath);

  blosc2_schunk_free(schunk);
  /* Destroy the Blosc environment */
  blosc2_destroy();

  free(data);
  free(data_dest);

  return EXIT_SUCCESS;
}

static char *all_tests(void) {

  for (int i = 0; i < (int) ARRAY_SIZE(tstorage); ++i) {
    for (int j = 0; j < (int) ARRAY_SIZE(tndata); ++j) {

      tdata.contiguous = tstorage[i].contiguous;
      tdata.urlpath = tstorage[i].urlpath;
      tdata.nchunks = tndata[j].nchunks;
      tdata.ndeletes = tndata[j].ndeletes;
      mu_run_test(test_delete_chunk);

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
