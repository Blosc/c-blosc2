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
    {1,  0}
};

typedef struct {
  bool contiguous;
  char *urlpath;
}test_storage;

test_storage tstorage[] = {
    {false, NULL},  // memory - schunk
    {true, NULL},  // memory - cframe
    {true, "test_update_chunk.b2frame"}, // disk - cframe
    {false, "test_update_chunk_s.b2frame"}, // disk - sframe
};

int32_t *data;
int32_t *data_dest;

static char* test_update_chunk(void) {
  /* Free resources */
  blosc2_remove_urlpath(tdata.urlpath);

  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;

  /* Create a super-chunk container */
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams,
                            .urlpath = tdata.urlpath,
                            .contiguous = tdata.contiguous};

  schunk = blosc2_schunk_new(&storage);

  // Feed it with data
  for (int nchunk = 0; nchunk < tdata.nchunks; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk * CHUNKSIZE;
    }
    int nchunks_ = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: bad append", nchunks_ > 0);
  }

  // Check that the chunks have been decompressed correctly
  for (int nchunk = 0; nchunk < tdata.nchunks; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly", dsize >= 0);
    for (int i = 0; i < CHUNKSIZE; i++) {
      mu_assert("ERROR: bad roundtrip", data_dest[i] == i + nchunk * CHUNKSIZE);
    }
  }

  for (int i = 0; i < tdata.nupdates; ++i) {
    // Create chunk
    for (int j = 0; j < CHUNKSIZE; ++j) {
      data[j] = i;
    }

    int32_t datasize = sizeof(int32_t) * CHUNKSIZE;
    int32_t chunksize = sizeof(int32_t) * CHUNKSIZE + BLOSC_MAX_OVERHEAD;
    uint8_t *chunk = malloc(chunksize);
    int csize = blosc2_compress_ctx(schunk->cctx, data, datasize, chunk, chunksize);
    mu_assert("ERROR: chunk cannot be compressed", csize >= 0);

    // Update a random position
    int pos = rand() % schunk->nchunks;
    int _nchunks = blosc2_schunk_update_chunk(schunk, pos, chunk, true);
    mu_assert("ERROR: chunk cannot be updated correctly", _nchunks > 0);
    free(chunk);

    // Assert updated chunk
    dsize = blosc2_schunk_decompress_chunk(schunk, pos, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly", dsize >= 0);
    for (int j = 0; j < CHUNKSIZE; j++) {
      int32_t a = data_dest[j];
      mu_assert("ERROR: bad roundtrip", a == i);
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
  for (int i = 0; i < sizeof(tstorage) / sizeof(test_storage); ++i) {
    for (int j = 0; j < sizeof(tndata) / sizeof(test_ndata); ++j) {
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

  install_blosc_callback_test(); /* optionally install callback test */
  blosc_init();

  data = blosc_test_malloc(BUFFER_ALIGN_SIZE, CHUNKSIZE * sizeof(int32_t));
  data_dest = blosc_test_malloc(BUFFER_ALIGN_SIZE, CHUNKSIZE * sizeof(int32_t));

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

  blosc_destroy();

  return result != EXIT_SUCCESS;
}
