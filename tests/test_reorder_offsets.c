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
  char* urlpath;
  bool contiguous;
} test_data;

test_data tdata;

typedef struct {
  bool contiguous;
  char *urlpath;
}test_storage;

test_storage tstorage[] = {
    {false, NULL},  // memory - schunk
    {true, NULL},  // memory - frame
    {true, "test_reorder_offsets.b2frame"}, // disk - cframe
    {false, "test_reorder_offsets_s.b2frame"}, // disk - sframe
};

int32_t tnchunks[] = {5, 12, 24, 33, 1};

int32_t *data;
int32_t *data_dest;

static char* test_reorder_offsets(void) {
  /* Free resources */
  blosc2_remove_urlpath(tdata.urlpath);

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
  blosc2_storage storage = {.contiguous=tdata.contiguous, .urlpath=tdata.urlpath, .cparams=&cparams, .dparams=&dparams};
  schunk = blosc2_schunk_new(&storage);

  // Feed it with data
  for (int nchunk = 0; nchunk < tdata.nchunks; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk * CHUNKSIZE;
    }
    int nchunks_ = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: bad append in frame", nchunks_ > 0);
  }

  int *offsets_order = malloc(sizeof(int) * tdata.nchunks);
  for (int i = 0; i < tdata.nchunks; ++i) {
    offsets_order[i] = (i + 3) % tdata.nchunks;
  }
  int err = blosc2_schunk_reorder_offsets(schunk, offsets_order);
  mu_assert("ERROR: can not reorder chunks", err >= 0);

  // Check that the chunks have been decompressed correctly
  for (int nchunk = 0; nchunk < tdata.nchunks; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly.", dsize >= 0);
    for (int i = 0; i < CHUNKSIZE; i++) {
      mu_assert("ERROR: bad roundtrip",data_dest[i] == i + (offsets_order[nchunk]) * CHUNKSIZE);
    }
  }

  /* Free resources */
  if (!storage.contiguous && storage.urlpath != NULL) {
    blosc2_remove_dir(storage.urlpath);
  }
  free(offsets_order);
  blosc2_schunk_free(schunk);

  /* Destroy the Blosc environment */
  blosc_destroy();

  return EXIT_SUCCESS;
}

static char *all_tests(void) {

  for (int i = 0; i < sizeof(tstorage) / sizeof(test_storage); ++i) {
    for (int j = 0; j < sizeof(tnchunks) / sizeof(int32_t); ++j) {

      tdata.contiguous = tstorage[i].contiguous;
      tdata.urlpath = tstorage[i].urlpath;
      tdata.nchunks = tnchunks[j];

      mu_run_test(test_reorder_offsets);

    }
  }

  return EXIT_SUCCESS;
}

#define BUFFER_ALIGN_SIZE   32

int main(void) {
  data = blosc_test_malloc(BUFFER_ALIGN_SIZE, CHUNKSIZE * sizeof(int32_t));
  data_dest = blosc_test_malloc(BUFFER_ALIGN_SIZE, CHUNKSIZE * sizeof(int32_t));

  install_blosc_callback_test(); /* optionally install callback test */
  blosc_init();

  /* Run all the suite */
  char *result = all_tests();
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
