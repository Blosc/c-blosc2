/*
  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating the use of a Blosc from C code.

  To compile this program:

  $ gcc -O simple.c -o simple -lblosc2

  To run:

  $ ./simple
  Blosc version info: 2.0.0a2 ($Date:: 2016-01-08 #$)
  Compression: 40000000 -> 832774 (48.0x)
  Correctly extracted 5 elements from compressed chunk!
  Decompression succesful!
  Succesful roundtrip!

*/

#include <stdio.h>
#include "blosc2.h"

#define SIZE (10 * 1000 * 1000)
#define NTHREADS 2


int main(void) {
  static float data[SIZE];
  static float data_out[SIZE];
  static float data_dest[SIZE];
  float data_subset[5];
  float data_subset_ref[5] = {5, 6, 7, 8, 9};
  size_t isize = SIZE * sizeof(float), osize = SIZE * sizeof(float);
  int dsize = SIZE * sizeof(float), csize;
  int i, ret;

  for (i = 0; i < SIZE; i++) {
    data[i] = (float)i;
  }

  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  /* Initialize the Blosc compressor */
  blosc_init();
  blosc_set_nthreads(NTHREADS);

  /* Compress with clevel=5 and shuffle active  */
  csize = blosc_compress(5, BLOSC_BITSHUFFLE, sizeof(float), isize, data,
                         data_out, osize);
  if (csize == 0) {
    printf("Buffer is uncompressible.  Giving up.\n");
    return 1;
  }
  else if (csize < 0) {
    printf("Compression error.  Error code: %d\n", csize);
    return csize;
  }

  printf("Compression: %zu -> %d (%.1fx)\n",
         isize, csize, (1. * isize) / csize);

  ret = blosc_getitem(data_out, 5, 5, data_subset);
  if (ret < 0) {
    printf("Error in blosc_getitem().  Giving up.\n");
    return 1;
  }

  for (i = 0; i < 5; i++) {
    if (data_subset[i] != data_subset_ref[i]) {
      printf("blosc_getitem() fetched data differs from original!\n");
      return -1;
    }
  }
  printf("Correctly extracted 5 elements from compressed chunk!\n");

  /* Decompress  */
  dsize = blosc_decompress(data_out, data_dest, (size_t)dsize);
  if (dsize < 0) {
    printf("Decompression error.  Error code: %d\n", dsize);
    return dsize;
  }

  printf("Decompression succesful!\n");

  /* After using it, destroy the Blosc environment */
  blosc_destroy();

  for (i = 0; i < SIZE; i++) {
    if (data[i] != data_dest[i]) {
      printf("Decompressed data differs from original!\n");
      return -1;
    }
  }

  printf("Succesful roundtrip!\n");
  return 0;
}
