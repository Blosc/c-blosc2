/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdio.h>
#include "test_common.h"

#define CHUNKSIZE 256

int tests_run = 0;

typedef struct {
  bool contiguous;
  char *urlpath;
} test_storage;

static test_storage tstorage[] = {
    {false, NULL},
    {true, NULL},
    {true, "test_schunk_negative_index.b2frame"},
};

static test_storage tdata;

static char *test_negative_chunk_index_rejected(void) {
  int32_t src[CHUNKSIZE];
  int32_t dst[CHUNKSIZE];
  int32_t isize = (int32_t)(sizeof(src));

  blosc2_remove_urlpath(tdata.urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = 1;

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;

  blosc2_storage storage = {
      .contiguous = tdata.contiguous,
      .urlpath = tdata.urlpath,
      .cparams = &cparams,
      .dparams = &dparams,
  };

  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: cannot create schunk", schunk != NULL);

  for (int i = 0; i < CHUNKSIZE; ++i) {
    src[i] = i;
  }
  mu_assert("ERROR: cannot append initial chunk", blosc2_schunk_append_buffer(schunk, src, isize) == 1);

  uint8_t *chunk = NULL;
  bool needs_free = false;
  int rc = blosc2_schunk_get_chunk(schunk, -1, &chunk, &needs_free);
  mu_assert("ERROR: negative index should fail in get_chunk", rc == BLOSC2_ERROR_INVALID_PARAM);

  rc = blosc2_schunk_get_lazychunk(schunk, -1, &chunk, &needs_free);
  mu_assert("ERROR: negative index should fail in get_lazychunk", rc == BLOSC2_ERROR_INVALID_PARAM);

  rc = blosc2_schunk_decompress_chunk(schunk, -1, dst, isize);
  mu_assert("ERROR: negative index should fail in decompress_chunk", rc == BLOSC2_ERROR_INVALID_PARAM);

  int32_t chunk_cap = isize + BLOSC2_MAX_OVERHEAD;
  uint8_t *new_chunk = malloc((size_t)chunk_cap);
  mu_assert("ERROR: cannot allocate compressed chunk", new_chunk != NULL);
  int cbytes = blosc2_compress_ctx(schunk->cctx, src, isize, new_chunk, chunk_cap);
  mu_assert("ERROR: cannot compress replacement chunk", cbytes > 0);

  rc = (int)blosc2_schunk_update_chunk(schunk, -1, new_chunk, true);
  mu_assert("ERROR: negative index should fail in update_chunk", rc == BLOSC2_ERROR_INVALID_PARAM);
  mu_assert("ERROR: update_chunk changed nchunks on failure", schunk->nchunks == 1);

  rc = (int)blosc2_schunk_insert_chunk(schunk, -1, new_chunk, true);
  mu_assert("ERROR: negative index should fail in insert_chunk", rc == BLOSC2_ERROR_INVALID_PARAM);
  mu_assert("ERROR: insert_chunk changed nchunks on failure", schunk->nchunks == 1);

  rc = (int)blosc2_schunk_delete_chunk(schunk, -1);
  mu_assert("ERROR: negative index should fail in delete_chunk", rc == BLOSC2_ERROR_INVALID_PARAM);
  mu_assert("ERROR: delete_chunk changed nchunks on failure", schunk->nchunks == 1);

  free(new_chunk);

  rc = blosc2_schunk_decompress_chunk(schunk, 0, dst, isize);
  mu_assert("ERROR: valid chunk cannot be decompressed after invalid operations", rc == isize);

  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(tdata.urlpath);

  return EXIT_SUCCESS;
}


static char *all_tests(void) {
  for (int i = 0; i < (int)ARRAY_SIZE(tstorage); ++i) {
    tdata = tstorage[i];
    mu_run_test(test_negative_chunk_index_rejected);
  }

  return EXIT_SUCCESS;
}


int main(void) {
  char *result;

  install_blosc_callback_test();
  blosc2_init();

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
