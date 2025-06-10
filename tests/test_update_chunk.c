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
  int nupdates;
  char* urlpath;
  bool contiguous;
} test_data;

test_data tdata;

typedef struct {
  int nchunks;
  int nupdates;
} test_ndata;

test_ndata tndata[] = {
    {1, 4},
    {10, 4},
    {5,  0},
    {33, 32},
    {1,  0},
    // {25000,  0},  // this tests super-chunks with more than 2**32 entries, but it takes too long
};

typedef struct {
  bool contiguous;
  char *urlpath;
} test_storage;

test_storage tstorage[] = {
    {false, NULL},  // memory - schunk
    {true, NULL},  // memory - cframe
    {true, "test_update_chunk.b2frame"}, // disk - cframe
    {false, "test_update_chunk_s.b2frame"}, // disk - sframe
};

int64_t *data;
int64_t *data_dest;

static char* test_update_chunk(void) {
  /* Free resources */
  blosc2_remove_urlpath(tdata.urlpath);

  int32_t isize = CHUNKSIZE * sizeof(int64_t);
  int dsize;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;

  /* Create a super-chunk container */
  cparams.typesize = sizeof(int64_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams,
                            .urlpath = tdata.urlpath,
                            .contiguous = tdata.contiguous};

  schunk = blosc2_schunk_new(&storage);

  // Feed it with data
  for (int64_t nchunk = 0; nchunk < tdata.nchunks; nchunk++) {
    for (int64_t i = 0; i < CHUNKSIZE; i++) {
      data[i] = nchunk;
    }
    int64_t nchunks_ = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: bad append", nchunks_ > 0);
  }

  // Check that the chunks can be decompressed correctly
  for (int64_t nchunk = 0; nchunk < tdata.nchunks; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly", dsize >= 0);
    for (int64_t i = 0; i < CHUNKSIZE; i++) {
      mu_assert("ERROR: bad roundtrip 1", data_dest[i] == nchunk);
    }
  }

  for (int i = 0; i < tdata.nupdates; ++i) {
    // Create chunk
    for (int j = 0; j < CHUNKSIZE; ++j) {
      data[j] = j + i * CHUNKSIZE;
    }

    int32_t datasize = sizeof(int64_t) * CHUNKSIZE;
    int32_t chunksize = sizeof(int64_t) * CHUNKSIZE + BLOSC2_MAX_OVERHEAD;
    uint8_t *chunk = malloc(chunksize);
    int csize = blosc2_compress_ctx(schunk->cctx, data, datasize, chunk, chunksize);
    mu_assert("ERROR: chunk cannot be compressed", csize >= 0);

    // Update a random position
    int64_t pos = rand() % schunk->nchunks;
    int64_t _nchunks = blosc2_schunk_update_chunk(schunk, pos, chunk, true);
    mu_assert("ERROR: chunk cannot be updated correctly", _nchunks > 0);
    free(chunk);

    // Assert updated chunk
    dsize = blosc2_schunk_decompress_chunk(schunk, pos, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly", dsize >= 0);
    for (int j = 0; j < CHUNKSIZE; j++) {
      int64_t a = data_dest[j];
      mu_assert("ERROR: bad roundtrip 2", a == (j + i * CHUNKSIZE));
    }
    if (i == 0 && tdata.nchunks > 1) {
      if (i != pos) {
        pos = 0;
      }
      else {
        pos = 1;
      }
      dsize = blosc2_schunk_decompress_chunk(schunk, pos, (void *) data_dest, isize);
      mu_assert("ERROR: chunk cannot be decompressed correctly", dsize >= 0);
      for (int j = 0; j < CHUNKSIZE; j++) {
        int64_t a = data_dest[j];
        mu_assert("ERROR: bad roundtrip 3", a == pos);
      }
    }
  }

  // Check that the chunks can be decompressed
  for (int nchunk = 0; nchunk < tdata.nchunks; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly", dsize >= 0);
  }
  /* Free resources */
  if (!storage.contiguous && storage.urlpath != NULL) {
    blosc2_remove_dir(storage.urlpath);
  }
  blosc2_schunk_free(schunk);
  /* Destroy the Blosc environment */

  return EXIT_SUCCESS;
}

static char *all_tests(void) {
  for (int i = 0; i < (int) ARRAY_SIZE(tstorage); ++i) {
    for (int j = 0; j < (int) ARRAY_SIZE(tndata); ++j) {
      tdata.contiguous = tstorage[i].contiguous;
      tdata.urlpath = tstorage[i].urlpath;
      tdata.nchunks = tndata[j].nchunks;
      tdata.nupdates = tndata[j].nupdates;

      mu_run_test(test_update_chunk);
    }
  }

  return EXIT_SUCCESS;
}

#define BUFFER_ALIGN_SIZE   32

int main(void) {
  char *result;

  blosc2_init();
  install_blosc_callback_test(); /* optionally install callback test */

  data = blosc_test_malloc(BUFFER_ALIGN_SIZE, CHUNKSIZE * sizeof(int64_t));
  data_dest = blosc_test_malloc(BUFFER_ALIGN_SIZE, CHUNKSIZE * sizeof(int64_t));

  /* Run all the suite */
  result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  blosc_test_free(data);
  blosc_test_free(data_dest);

  blosc2_destroy();

  return result != EXIT_SUCCESS;
}
