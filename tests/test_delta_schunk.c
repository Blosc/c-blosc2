/*
  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdio.h>
#include "test_common.h"

#define SIZE (500 * 1000)
#define NCHUNKS 100
#define NTHREADS 4


int main(void) {
  int32_t *data = malloc(SIZE * sizeof(int32_t));;
  int32_t *data_dest = malloc(SIZE * sizeof(int32_t));
  int32_t isize = SIZE * sizeof(int32_t);
  int dsize;
  int64_t nbytes, cbytes;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;
  int32_t nchunks;

  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  /* Initialize the Blosc compressor */
  install_blosc_callback_test(); /* optionally install callback test */
  blosc_init();

  /* Create a super-chunk container */
  cparams.filters[0] = BLOSC_DELTA;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_BITSHUFFLE;
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams};
  schunk = blosc2_schunk_new(&storage);

  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (int i = 0; i < SIZE; i++) {
      data[i] = i * nchunk;
    }
    nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    if (nchunks != (nchunk + 1)){
      free(data);
      free(data_dest);
      return EXIT_FAILURE;
    }
  }

  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  if (cbytes > nbytes) {
    free(data);
    free(data_dest);
    return EXIT_FAILURE;
  }

  /* Retrieve and decompress the chunks (0-based count) */
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    if (dsize < 0) {
      free(data);
      free(data_dest);
      return EXIT_FAILURE;
    }
    for (int i = 0; i < SIZE; i++) {
      if (data_dest[i] != i  * nchunk) {
        fprintf(stderr, "First error in: %u, %d, %d\n",
                nchunk, i, data_dest[i]);
        free(data);
        free(data_dest);
        return EXIT_FAILURE;
      }
    }
  }

  /* Free resources */
  blosc2_schunk_free(schunk);
  /* Destroy the Blosc environment */
  blosc_destroy();

  free(data);
  free(data_dest);

  return EXIT_SUCCESS;
}
