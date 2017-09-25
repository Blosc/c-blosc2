/*
  Copyright (C) 2015  Francesc Alted
  http://blosc.org
  License: BSD (see LICENSE.txt)

*/

#include <stdio.h>
#include "test_common.h"

#define SIZE (500 * 1000)
#define NCHUNKS 100
#define NTHREADS 4


int main() {
  static int32_t data[SIZE];
  static int32_t data_dest[SIZE];
  size_t isize = SIZE * sizeof(int32_t);
  int dsize;
  int64_t nbytes, cbytes;
  blosc2_cparams cparams = BLOSC_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;
  size_t nchunks;

  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  /* Initialize the Blosc compressor */
  blosc_init();

  /* Create a super-chunk container */
  cparams.filters[0] = BLOSC_DELTA;
  cparams.filters[BLOSC_MAX_FILTERS - 1] = BLOSC_BITSHUFFLE;
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS;
  schunk = blosc2_new_schunk(cparams, dparams);

  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (int i = 0; i < SIZE; i++) {
      data[i] = i * nchunk;
    }
    nchunks = blosc2_append_buffer(schunk, isize, data);
    if (nchunks != (nchunk + 1)) return EXIT_FAILURE;
  }

  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  if (cbytes > nbytes) {
    return EXIT_FAILURE;
  }

  /* Retrieve and decompress the chunks (0-based count) */
  for (size_t nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    dsize = blosc2_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    if (dsize < 0) {
      return EXIT_FAILURE;
    }
    for (int i = 0; i < SIZE; i++) {
      if (data_dest[i] != i  * nchunk) {
        fprintf(stderr, "First error in: %zu, %d, %d\n",
                nchunk, i, data_dest[i]);
        return EXIT_FAILURE;
      }
    }
  }

  /* Free resources */
  blosc2_destroy_schunk(schunk);
  /* Destroy the Blosc environment */
  blosc_destroy();

  return EXIT_SUCCESS;
}
