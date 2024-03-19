/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  $ gcc -O many_compressors.c -o many_compressors -lblosc2

  To run:

  $ ./many_compressors
  Blosc version info: 2.0.0a6.dev ($Date:: 2018-05-18 #$)
  Using 4 threads (previously using 1)
  Using blosclz compressor
  Compression: 4000000 -> 57577 (69.5x)
  Successful roundtrip!
  Using lz4 compressor
  Compression: 4000000 -> 97276 (41.1x)
  Successful roundtrip!
  Using lz4hc compressor
  Compression: 4000000 -> 38314 (104.4x)
  Successful roundtrip!
  Using zlib compressor
  Compression: 4000000 -> 21486 (186.2x)
  Successful roundtrip!
  Using zstd compressor
  Compression: 4000000 -> 10692 (374.1x)
  Successful roundtrip!

 */

#include <stdio.h>
#include <blosc2.h>

#define SIZE 100*100*100
#define SHAPE {100,100,100}
#define CHUNKSHAPE {1,100,100}

int main(void) {
  static float data[SIZE];
  static float data_out[SIZE];
  static float data_dest[SIZE];
  int isize = SIZE * sizeof(float), osize = SIZE * sizeof(float);
  int dsize = SIZE * sizeof(float), csize;
  int16_t nthreads, pnthreads;
  int i;
  char* compressors[] = {"blosclz", "lz4", "lz4hc", "zlib", "zstd"};
  int ccode, rcode;

  for (i = 0; i < SIZE; i++) {
    data[i] = i;
  }

  /* Register the filter with the library */
  printf("Blosc version info: %s (%s)\n",
         BLOSC2_VERSION_STRING, BLOSC2_VERSION_DATE);

  /* Initialize the Blosc compressor */
  blosc_init();

  nthreads = 4;
  pnthreads = blosc_set_nthreads(nthreads);
  printf("Using %d threads (previously using %d)\n", nthreads, pnthreads);

  /* Tell Blosc to use some number of threads */
  for (ccode = 0; ccode < 5; ccode++) {

    rcode = blosc_set_compressor(compressors[ccode]);
    if (rcode < 0) {
      printf("Error setting %s compressor.  It really exists?",
             compressors[ccode]);
      return rcode;
    }
    printf("Using %s compressor\n", compressors[ccode]);

    /* Compress with clevel=5 and shuffle active  */
    csize = blosc_compress(5, 1, sizeof(float), isize, data, data_out, osize);
    if (csize < 0) {
      printf("Compression error.  Error code: %d\n", csize);
      return csize;
    }

    printf("Compression: %d -> %d (%.1fx)\n", isize, csize, (1. * isize) / csize);

    /* Decompress  */
    dsize = blosc_decompress(data_out, data_dest, dsize);
    if (dsize < 0) {
      printf("Decompression error.  Error code: %d\n", dsize);
      return dsize;
    }

    /* After using it, destroy the Blosc environment */
    blosc_destroy();

    for (i = 0; i < SIZE; i++) {
      if (data[i] != data_dest[i]) {
        printf("Decompressed data differs from original!\n");
        return -1;
      }
    }

    printf("Successful roundtrip!\n");
  }

  return 0;
}
