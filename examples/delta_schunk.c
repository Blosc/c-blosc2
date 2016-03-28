/*
  Copyright (C) 2015  Francesc Alted
  http://blosc.org
  License: MIT (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  $ gcc -O delta_schunk.c -o delta_schunk -lblosc

  To run:

  $ ./delta_schunk
  Blosc version info: 2.0.0a2 ($Date:: 2015-12-17 #$)
  Compression super-chunk: 200000000 -> 14081961 (14.2x)
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
  blosc2_sparams* sparams = calloc(1, sizeof(blosc2_sparams));
  blosc2_sheader* sheader;
  int i, nchunk, nchunks;

  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  /* Initialize the Blosc compressor */
  blosc_init();
  blosc_set_nthreads(2);

  /* Create a super-chunk container */
  sparams->filters[0] = BLOSC_DELTA;
  sparams->filters[1] = BLOSC_BITSHUFFLE;
  sparams->compressor = BLOSC_LZ4;
  sparams->clevel = 5;
  sheader = blosc2_new_schunk(sparams);

  for (nchunk = 1; nchunk <= NCHUNKS; nchunk++) {

    for (i = 0; i < SIZE; i++) {
      data[i] = i * nchunk;
    }

    nchunks = blosc2_append_buffer(sheader, sizeof(int32_t), isize, data);
    assert(nchunks == nchunk);
  }

  /* Gather some info */
  nbytes = sheader->nbytes;
  cbytes = sheader->cbytes;
  printf("Compression super-chunk: %d -> %d (%.1fx)\n",
         nbytes, cbytes, (1. * nbytes) / cbytes);

  /* Retrieve and decompress the chunks (0-based count) */
  dsize = blosc2_decompress_chunk(sheader, 0, (void*)data_dest, isize);
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
  free(sparams);
  /* Destroy the super-chunk */
  blosc2_destroy_schunk(sheader);
  /* Destroy the Blosc environment */
  blosc_destroy();

  return 0;
}
