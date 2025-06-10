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
  int ninsertions;
  char* urlpath;
  bool contiguous;
  bool copy;
} test_data;

test_data tdata;

typedef struct {
  int nchunks;
  int ninsertions;
} test_ndata;

test_ndata tndata[] = {
    {10, 1},
    {5,  3},
    {33, 5},
    {1,  0},
    {12, 24},
    {0, 3},
    {0, 0},
    // {25000,  0},  // this tests super-chunks with more than 2**32 entries, but it takes too long
};

typedef struct {
  bool contiguous;
  char *urlpath;
} test_storage;

test_storage tstorage[] = {
    {false, NULL},  // memory - schunk
    {true, NULL},  // memory - cframe
    {true, "test_insert_chunk.b2frame"}, // disk - cframe
    {false, "test_insert_chunk_s.b2frame"}, // disk - sframe
};

bool tcopy[] = {
    true,
    false
};

static char* test_insert_chunk(void) {
  /* Free resources */
  blosc2_remove_urlpath(tdata.urlpath);

  int64_t *data = malloc(CHUNKSIZE * sizeof(int64_t));
  int64_t *data_dest = malloc(CHUNKSIZE * sizeof(int64_t));
  int32_t isize = CHUNKSIZE * sizeof(int64_t);
  int dsize;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;

  /* Initialize the Blosc compressor */
  blosc2_init();

  /* Create a super-chunk container */
  cparams.typesize = sizeof(int64_t);
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams,
                            .urlpath=tdata.urlpath, .contiguous=tdata.contiguous};
  schunk = blosc2_schunk_new(&storage);

  // Feed it with data
  for (int64_t nchunk = 0; nchunk < tdata.nchunks; nchunk++) {
    for (int64_t i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk * CHUNKSIZE;
    }
    int64_t nchunks_ = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: bad append", nchunks_ > 0);
  }

  // Check that the chunks can be decompressed correctly
  for (int64_t nchunk = 0; nchunk < tdata.nchunks; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly", dsize >= 0);
    for (int64_t i = 0; i < CHUNKSIZE; i++) {
      mu_assert("ERROR: bad roundtrip 1", data_dest[i] == i + nchunk * CHUNKSIZE);
    }
  }

  for (int i = 0; i < tdata.ninsertions; ++i) {
    // Create chunk
    for (int j = 0; j < CHUNKSIZE; ++j) {
      data[j] = i - 1;
    }
    int32_t datasize = sizeof(int64_t) * CHUNKSIZE;
    int32_t chunksize = sizeof(int64_t) * CHUNKSIZE + BLOSC2_MAX_OVERHEAD;
    uint8_t *chunk = malloc(chunksize);
    int csize = blosc2_compress_ctx(schunk->cctx, data, datasize, chunk, chunksize);
    mu_assert("ERROR: chunk cannot be compressed", csize >= 0);

    // Insert in a random position
    int64_t pos = rand() % (schunk->nchunks + 1);
    int64_t _nchunks = blosc2_schunk_insert_chunk(schunk, pos, chunk, tdata.copy);
    mu_assert("ERROR: chunk cannot be inserted correctly", _nchunks > 0);

    // Check that the inserted chunk can be decompressed correctly
    dsize = blosc2_schunk_decompress_chunk(schunk, pos, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly", dsize >= 0);
    for (int j = 0; j < CHUNKSIZE; j++) {
      int64_t a = data_dest[j];
      mu_assert("ERROR: bad roundtrip2", a == i - 1);
    }
    if (i == 0) {
      // Check that the other chunks have not changed
      for (int nchunk = 0; nchunk < pos; nchunk++) {
        dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
        mu_assert("ERROR: chunk cannot be decompressed correctly.", dsize >= 0);
        for (int j = 0; j < CHUNKSIZE; j++) {
          mu_assert("ERROR: bad roundtrip3",data_dest[j] == j + nchunk * CHUNKSIZE);
        }
      }
      for (int nchunk = pos + 1; nchunk < tdata.nchunks + 1; nchunk++) {
        dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
        mu_assert("ERROR: chunk cannot be decompressed correctly.", dsize >= 0);
        for (int j = 0; j < CHUNKSIZE; j++) {
          mu_assert("ERROR: bad roundtrip4",data_dest[j] == j + (nchunk - 1) * CHUNKSIZE);
        }
      }

    }

    // Free allocated chunk
    if (tdata.copy) {
      free(chunk);
    }
  }

  // Check that the chunks have been decompressed correctly
  for (int nchunk = 0; nchunk < schunk->nchunks; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly", dsize >= 0);
  }

  /* Free resources */
  if (!storage.contiguous && storage.urlpath != NULL) {
    blosc2_remove_dir(storage.urlpath);
  }
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
      for (int k = 0; k < (int) ARRAY_SIZE(tcopy); ++k) {

        tdata.contiguous = tstorage[i].contiguous;
        tdata.urlpath = tstorage[i].urlpath;
        tdata.nchunks = tndata[j].nchunks;
        tdata.ninsertions = tndata[j].ninsertions;
        tdata.copy = tcopy[k];
        mu_run_test(test_insert_chunk);
      }
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
