/*
  Copyright (C) 2015  Francesc Alted
  http://blosc.org
  License: MIT (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  gcc delta_schunk.c -o delta_schunk -lblosc

  To run:

  $ ./delta_schunk
  Blosc version info: 2.0.0a2 ($Date:: 2015-12-17 #$)
  Compression super-chunk: 60000112 -> 20234528 (3.0x)
  Decompression successful!
  Successful roundtrip!

*/

#include <stdio.h>
#include <assert.h>
#include "blosc.h"

#define SIZE 50 * 1000
#define NCHUNKS 1000


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
  sc_params->compressor = BLOSC_LZ4;
  sc_params->clevel = 5;
  sc_header = blosc2_new_schunk(sc_params);

  for (nchunk = 1; nchunk <= NCHUNKS; nchunk++) {

    for (i = 0; i < SIZE; i++) {
      data[i] = i * nchunk;
    }

    nchunks = blosc2_append_buffer(sc_header, sizeof(int32_t), isize, data);
    assert(nchunks == nchunk);
  }

  /* Gather some info */
  nbytes = sc_header->nbytes;
  cbytes = sc_header->cbytes;
  printf("Compression super-chunk: %d -> %d (%.1fx)\n",
         nbytes, cbytes, (1. * nbytes) / cbytes);

  /* Retrieve and decompress the chunks (0-based count) */
  dsize = blosc2_decompress_chunk(sc_header, 0, (void*)data_dest, isize);
  if (dsize < 0) {
    printf("Decompression error.  Error code: %d\n", dsize);
    return dsize;
  }

  printf("Decompression successful!\n");

  for (i = 0; i < SIZE; i++) {
    if (data_dest[i] != i) {
      printf("Decompressed data differs from original %d, %d, %d!\n", i, data[i], data_dest[i]);
      return -1;
    }
  }

  printf("Successful roundtrip!\n");

  /* Free resources */
  free(sc_params);
  /* Destroy the super-chunk */
  blosc2_destroy_schunk(sc_header);
  /* Destroy the Blosc environment */
  blosc_destroy();

  return 0;
}
