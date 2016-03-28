/*
  Copyright (C) 2015  Francesc Alted
  http://blosc.org
  License: MIT (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  $ gcc simple_schunk.c -o schunk -lblosc

  To run:

  $ ./schunk
  Blosc version info: 2.0.0a1 ($Date:: 2015-07-30 #$)
  Compression: 4000000 -> 158788 (25.2x)
  destsize: 4000000
  Decompression succesful!
  Succesful roundtrip!

*/

#include <stdio.h>
#include <assert.h>
#include "blosc.h"

#define SIZE 1000*1000

int main() {
  static float data[SIZE];
  static float data_dest[SIZE];
  int isize = SIZE * sizeof(float), osize = SIZE * sizeof(float);
  int dsize, csize;
  blosc2_sparams sparams = BLOSC_SPARAMS_DEFAULTS;
  blosc2_sheader* sheader;
  int i, nchunks;

  for (i = 0; i < SIZE; i++) {
    data[i] = i;
  }

  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  /* Initialize the Blosc compressor */
  blosc_init();

  /* Compress with clevel=5 and shuffle active  */
  csize = blosc_compress(5, BLOSC_SHUFFLE, sizeof(float),
    isize, data, data_dest, osize);
  if (csize == 0) {
    printf("Buffer is uncompressible.  Giving up.\n");
    return 1;
  }
  else if (csize < 0) {
    printf("Compression error.  Error code: %d\n", csize);
    return csize;
  }

  printf("Compression: %d -> %d (%.1fx)\n", isize, csize, (1. * isize) / csize);

  /* Create a super-chunk container */
  sparams.filters[0] = BLOSC_DELTA;
  sparams.filters[1] = BLOSC_SHUFFLE;
  sheader = blosc2_new_schunk(&sparams);

  /* Now append a couple of chunks */
  nchunks = blosc2_append_buffer(sheader, sizeof(float), isize, data);
  assert(nchunks == 1);
  nchunks = blosc2_append_buffer(sheader, sizeof(float), isize, data);
  assert(nchunks == 2);

  /* Retrieve and decompress the chunks (0-based count) */
  dsize = blosc2_decompress_chunk(sheader, 0, (void*)data_dest, isize);
  if (dsize < 0) {
    printf("Decompression error.  Error code: %d\n", dsize);
    return dsize;
  }
  dsize = blosc2_decompress_chunk(sheader, 1, (void*)data_dest, isize);
  if (dsize < 0) {
    printf("Decompression error.  Error code: %d\n", dsize);
    return dsize;
  }

  printf("Decompression succesful!\n");

  for (i = 0; i < SIZE; i++) {
    if (data[i] != data_dest[i]) {
      printf("i, values: %d, %f, %f\n", i, data[i], data_dest[i]);
      printf("Decompressed data differs from original!\n");
      return -1;
    }
  }

  printf("Succesful roundtrip!\n");

  /* Free resources */
  /* Destroy the super-chunk */
  blosc2_destroy_schunk(sheader);
  /* Destroy the Blosc environment */
  blosc_destroy();

  return 0;
}
