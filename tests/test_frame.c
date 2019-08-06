/*
  Copyright (C) 2019  Francesc Alted
  http://blosc.org
  License: BSD (see LICENSE.txt)

  Creation date: 2019-08-06

  See LICENSE.txt for details about copyright and rights to use.
*/

#include <stdio.h>
#include <stdbool.h>
#include "test_common.h"

#define CHUNKSIZE (200 * 1000)
#define NTHREADS (2)
#define MIN_FRAME_LEN (74)  // the minimum frame length as of now

/* Global vars */
int nchunks_[] = {0, 1, 2, 10};
int tests_run = 0;
int nchunks;
bool free_new;
bool sparse_schunk;
char *fname;


static char* test_frame() {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  int64_t nbytes, cbytes;
  blosc2_cparams cparams = BLOSC_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;

  /* Initialize the Blosc compressor */
  blosc_init();

  /* Create a frame container */
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  blosc2_frame* frame = blosc2_new_frame(fname);
  schunk = blosc2_new_schunk(cparams, dparams, frame);

  if (!sparse_schunk) {
    if (free_new) {
      if (fname != NULL) {
        blosc2_free_schunk(schunk);
        blosc2_free_frame(frame);
        frame = blosc2_frame_from_file(fname);
        schunk = blosc2_schunk_from_frame(frame, sparse_schunk);
      } else {
        blosc2_free_schunk(schunk);
        schunk = blosc2_schunk_from_frame(frame, sparse_schunk);
      }
    }
  }

  // Feed it with data
  int _nchunks = 0;
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk * CHUNKSIZE;
    }
    _nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: bad append in frame", nchunk >= 0);
  }
  mu_assert("ERROR: wrong number of append chunks", _nchunks == nchunks);

  if (!sparse_schunk) {
    mu_assert("ERROR: frame->len must be larger or equal than schunk->cbytes",
              frame->len >= schunk->cbytes + MIN_FRAME_LEN);
  }

  if (!sparse_schunk) {
    if (free_new) {
      if (fname != NULL) {
        blosc2_free_schunk(schunk);
        blosc2_free_frame(frame);
        frame = blosc2_frame_from_file(fname);
        schunk = blosc2_schunk_from_frame(frame, sparse_schunk);
      } else {
        blosc2_free_schunk(schunk);
        schunk = blosc2_schunk_from_frame(frame, sparse_schunk);
      }
    }
  }

  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  if (nchunks > 0) {
    mu_assert("ERROR: bad compression ratio in frame", nbytes > 10 * cbytes);
  }

  // Check that the chunks have been decompressed correctly
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly.", dsize >= 0);
    for (int i = 0; i < CHUNKSIZE; i++) {
      mu_assert("ERROR: bad roundtrip",data_dest[i] == i + nchunk * CHUNKSIZE);
    }
  }

  /* Free resources */
  blosc2_free_schunk(schunk);
  blosc2_free_frame(frame);
  /* Destroy the Blosc environment */
  blosc_destroy();

  return EXIT_SUCCESS;
}


static char *all_tests() {

  // Iterate over all different parameters
  char buf[256];
  for (int i = 0; i < (int)sizeof(nchunks_) / (int)sizeof(int); i++) {
    nchunks = nchunks_[i];
    for (int ifree_new = 0; ifree_new < 2; ifree_new++) {
      for (int isparse_schunk = 0; isparse_schunk < 2; isparse_schunk++) {
        free_new = (bool) ifree_new;
        sparse_schunk = (bool) isparse_schunk;
        fname = NULL;
        mu_run_test(test_frame);
        snprintf(buf, sizeof(buf), "test_frame_nc%d.b2frame", nchunks);
        fname = buf;
        mu_run_test(test_frame);
      }
    }
  }

  return EXIT_SUCCESS;
}


int main() {
  char *result;

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
