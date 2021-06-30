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

test_ndata tndata[] = {{10, 1},
                       {5,  3},
                       {33, 5},
                       {1,  0},
                       {12, 24},
                       {0, 3},
                       {0, 0}
};

typedef struct {
  bool contiguous;
  char *urlpath;
}test_storage;

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

  int32_t *data = malloc(CHUNKSIZE * sizeof(int32_t));
  int32_t *data_dest = malloc(CHUNKSIZE * sizeof(int32_t));
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;

  /* Initialize the Blosc compressor */
  blosc_init();

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
    int nchunks_ = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: bad append", nchunks_ > 0);
  }

  for (int i = 0; i < tdata.ninsertions; ++i) {
    // Create chunk
    for (int j = 0; j < CHUNKSIZE; ++j) {
      data[j] = i;
    }
    int32_t datasize = sizeof(int32_t) * CHUNKSIZE;
    int32_t chunksize = sizeof(int32_t) * CHUNKSIZE + BLOSC_MAX_OVERHEAD;
    uint8_t *chunk = malloc(chunksize);
    int csize = blosc2_compress_ctx(schunk->cctx, data, datasize, chunk, chunksize);
    mu_assert("ERROR: chunk cannot be compressed", csize >= 0);

    // Insert in a random position
    int pos = rand() % (schunk->nchunks + 1);
    int _nchunks = blosc2_schunk_insert_chunk(schunk, pos, chunk, tdata.copy);
    mu_assert("ERROR: chunk cannot be inserted correctly", _nchunks > 0);

    // Check that the inserted chunk can be decompressed correctly
    dsize = blosc2_schunk_decompress_chunk(schunk, pos, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly", dsize >= 0);
    for (int j = 0; j < CHUNKSIZE; j++) {
      int32_t a = data_dest[j];
      int32_t b = a + 1;
      mu_assert("ERROR: bad roundtrip", a == i);
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
  blosc_destroy();

  free(data);
  free(data_dest);

  return EXIT_SUCCESS;
}

static char *all_tests(void) {

  for (int i = 0; i < sizeof(tstorage) / sizeof(test_storage); ++i) {
    for (int j = 0; j < sizeof(tndata) / sizeof(test_ndata); ++j) {
      for (int k = 0; k < sizeof(tcopy) / sizeof(bool); ++k) {

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
