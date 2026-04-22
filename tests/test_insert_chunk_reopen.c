/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "blosc2.h"
#include "test_common.h"

#define CHUNKSIZE 1000
#define NTHREADS 2

int tests_run = 0;

static char* test_insert_chunk_reopen(void) {
  const char* urlpath = "test_insert_chunk_reopen.b2frame";
  blosc2_remove_urlpath(urlpath);
  blosc2_init();

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int64_t);
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;

  blosc2_storage storage = {
      .cparams = &cparams,
      .dparams = &dparams,
      .urlpath = (char*)urlpath,
      .contiguous = true,
  };

  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: could not create schunk", schunk != NULL);

  int64_t data[CHUNKSIZE];
  int64_t dest[CHUNKSIZE];
  int32_t isize = CHUNKSIZE * (int32_t)sizeof(int64_t);

  for (int64_t nchunk = 0; nchunk < 3; ++nchunk) {
    for (int64_t i = 0; i < CHUNKSIZE; ++i) {
      data[i] = i + nchunk * CHUNKSIZE;
    }
    int64_t nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: append failed", nchunks == nchunk + 1);
  }

  for (int64_t i = 0; i < CHUNKSIZE; ++i) {
    data[i] = -1;
  }
  uint8_t chunk[CHUNKSIZE * (int32_t)sizeof(int64_t) + BLOSC2_MAX_OVERHEAD];
  int csize = blosc2_compress_ctx(schunk->cctx, data, isize, chunk, sizeof(chunk));
  mu_assert("ERROR: compression failed", csize > 0);

  int64_t nchunks = blosc2_schunk_insert_chunk(schunk, 1, chunk, true);
  mu_assert("ERROR: insert failed", nchunks == 4);

  blosc2_schunk_free(schunk);

  blosc2_schunk* reopened = blosc2_schunk_open(urlpath);
  mu_assert("ERROR: reopen after insert returned NULL", reopened != NULL);
  mu_assert("ERROR: reopened nchunks mismatch", reopened->nchunks == 4);

  int dsize = blosc2_schunk_decompress_chunk(reopened, 1, dest, isize);
  mu_assert("ERROR: inserted chunk could not be decompressed after reopen", dsize == isize);
  for (int64_t i = 0; i < CHUNKSIZE; ++i) {
    mu_assert("ERROR: inserted chunk payload mismatch after reopen", dest[i] == -1);
  }

  blosc2_schunk_free(reopened);
  blosc2_remove_urlpath(urlpath);
  blosc2_destroy();
  return EXIT_SUCCESS;
}

static char* all_tests(void) {
  mu_run_test(test_insert_chunk_reopen);
  return EXIT_SUCCESS;
}

int main(void) {
  char* result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);
  return result != EXIT_SUCCESS;
}
