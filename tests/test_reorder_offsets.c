/*
  Copyright (C) 2020- The Blosc Development Team <blosc@blosc.org>
  http://blosc.org
  License: BSD (see LICENSE.txt)

  Creation date: 2020-09-22

  See LICENSE.txt for details about copyright and rights to use.
*/

#include <stdio.h>
#include "test_common.h"

#define CHUNKSIZE (200 * 1000)
#define NTHREADS (2)

/* Global vars */
int tests_run = 0;
int nchunks;
bool serialized;
char* filename;


static char* test_reorder_offsets(void) {
  int32_t data[CHUNKSIZE];
  int32_t data_dest[CHUNKSIZE];
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;

  /* Initialize the Blosc compressor */
  blosc_init();

  /* Create a super-chunk container */
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  blosc2_frame *frame;
  if (serialized == true) {
    frame = blosc2_new_frame(filename);
  } else {
    frame = NULL;
  }
  schunk = blosc2_new_schunk(cparams, dparams, frame);

  // Feed it with data
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk * CHUNKSIZE;
    }
    int nchunks_ = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: bad append in frame", nchunks_ > 0);
  }

  int *offsets_order = malloc(sizeof(int) * nchunks);
  for (int i = 0; i < nchunks; ++i) {
    offsets_order[i] = (i + 3) % nchunks;
  }
  int err = blosc2_schunk_reorder_offsets(schunk, offsets_order);
  mu_assert("ERROR: can not reorder chunks", err >= 0);

  // Check that the chunks have been decompressed correctly
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly.", dsize >= 0);
    for (int i = 0; i < CHUNKSIZE; i++) {
      mu_assert("ERROR: bad roundtrip",data_dest[i] == i + (offsets_order[nchunk]) * CHUNKSIZE);
    }
  }

  /* Free resources */
  free(offsets_order);
  blosc2_free_schunk(schunk);
  if (serialized == true) {
    blosc2_free_frame(frame);
  }
  /* Destroy the Blosc environment */
  blosc_destroy();

  return EXIT_SUCCESS;
}

static char *all_tests(void) {

  nchunks = 5;
  serialized = false;
  filename = NULL;
  mu_run_test(test_reorder_offsets);

  nchunks = 13;
  serialized = false;
  filename = NULL;
  mu_run_test(test_reorder_offsets);

  /*
  nchunks = 44;
  serialized = true;
  filename = NULL;
  mu_run_test(test_reorder_offsets);

  nchunks = 13;
  serialized = true;
  filename = NULL;
  mu_run_test(test_reorder_offsets);


  nchunks = 23;
  serialized = true;
  filename = "test_reorder_offsets.b2frame";
  mu_run_test(test_reorder_offsets);

  nchunks = 13;
  serialized = true;
  filename = "test_reorder_offsets2.b2frame";
  mu_run_test(test_reorder_offsets);
*/
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
