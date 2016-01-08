/*
  Copyright (C) 2015  Francesc Alted
  http://blosc.org
  License: MIT (see LICENSE.txt)

*/

#include <stdio.h>
#include "test_common.h"

#define SIZE 500 * 1000
#define NCHUNKS 10


int main() {
  static int32_t data[SIZE];
  static int32_t data_dest[SIZE];
  int isize = SIZE * sizeof(int32_t);
  int dsize;
  int32_t nbytes, cbytes;
  schunk_params* sc_params = calloc(1, sizeof(schunk_params));
  schunk_header* sc_header;
  int i, nchunk, nchunks;

  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  /* Initialize the Blosc compressor */
  blosc_init();
  blosc_set_nthreads(2);

  /* Create a super-chunk container */
  sc_params->filters[0] = BLOSC_DELTA;
  sc_params->filters[1] = BLOSC_BITSHUFFLE;
  sc_params->compressor = BLOSC_BLOSCLZ;
  sc_params->clevel = 5;
  sc_header = blosc2_new_schunk(sc_params);

  for (nchunk = 1; nchunk <= NCHUNKS; nchunk++) {
    for (i = 0; i < SIZE; i++) {
      data[i] = i * nchunk;
    }
    nchunks = blosc2_append_buffer(sc_header, sizeof(int32_t), isize, data);
    if (nchunks != nchunk) return EXIT_FAILURE;
  }

  /* Gather some info */
  nbytes = sc_header->nbytes;
  cbytes = sc_header->cbytes;
  if (cbytes > nbytes) {
    return EXIT_FAILURE;
  }

  /* Retrieve and decompress the chunks (0-based count) */
  dsize = blosc2_decompress_chunk(sc_header, 0, (void*)data_dest, isize);
  if (dsize < 0) {
    return EXIT_FAILURE;
  }

  for (i = 0; i < SIZE; i++) {
    if (data_dest[i] != i) {
      return EXIT_FAILURE;
    }
  }

  /* Free resources */
  free(sc_params);
  /* Destroy the super-chunk */
  blosc2_destroy_schunk(sc_header);
  /* Destroy the Blosc environment */
  blosc_destroy();

  return EXIT_SUCCESS;
}
