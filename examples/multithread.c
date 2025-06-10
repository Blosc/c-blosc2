/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program using gcc or clang:

  $ gcc/clang multithread.c -o multithread -lblosc2 -lpthread

  or, if you don't have the blosc library installed:

  $ gcc -O3 -msse2 multithread.c ../blosc/!(*avx2*)*.c  -I../blosc -o multithread -lpthread

  or alternatively:

  $ gcc -O3 -msse2 multithread.c -I../blosc -o multithread -L../build/blosc -lblosc2
  $ export LD_LIBRARY_PATH=../build/blosc

  Using MSVC on Windows:

  $ cl /Ox /Femultithread.exe /Iblosc multithread.c blosc\*.c

  To run:

  $ ./multithread
  Blosc version info: 1.4.2.dev ($Date:: 2014-07-08 #$)
  Using 1 threads (previously using 1)
  Compression: 4000000 -> 158494 (25.2x)
  Successful roundtrip!
  Using 2 threads (previously using 1)
  Compression: 4000000 -> 158494 (25.2x)
  Successful roundtrip!
  Using 3 threads (previously using 2)
  Compression: 4000000 -> 158494 (25.2x)
  Successful roundtrip!
  Using 4 threads (previously using 3)
  Compression: 4000000 -> 158494 (25.2x)
  Successful roundtrip!

*/

#include <stdio.h>
#include <inttypes.h>
#include <blosc2.h>

#define SIZE (1000 * 1000)


int main(void) {
  static float data[SIZE];
  static float data_out[SIZE];
  static float data_dest[SIZE];
  size_t isize = SIZE * sizeof(float), osize = SIZE * sizeof(float);
  int dsize, csize;
  int16_t nthreads, pnthreads;
  int i;

  for (i = 0; i < SIZE; i++) {
    data[i] = (float)i;
  }

  /* Register the filter with the library */
  printf("Blosc version info: %s (%s)\n",
         BLOSC2_VERSION_STRING, BLOSC2_VERSION_DATE);

  /* Initialize the Blosc compressor */
  blosc2_init();

  /* Tell Blosc to use some number of threads */
  for (nthreads = 1; nthreads <= 4; nthreads++) {

    pnthreads = blosc2_set_nthreads(nthreads);
    printf("Using %d threads (previously using %d)\n", nthreads, pnthreads);

    /* Compress with clevel=5 and shuffle active  */
    csize = blosc1_compress(5, 1, sizeof(float), isize, data, data_out, osize);
    if (csize < 0) {
      printf("Compression error.  Error code: %d\n", csize);
      return csize;
    }

    printf("Compression: %" PRId64 " -> %d (%.1fx)\n", (int64_t)isize, csize, (1. * (double)isize) /
            csize);

    /* Decompress  */
    dsize = blosc1_decompress(data_out, data_dest, isize);
    if (dsize < 0) {
      printf("Decompression error.  Error code: %d\n", dsize);
      return dsize;
    }

    for (i = 0; i < SIZE; i++) {
      if (data[i] != data_dest[i]) {
        printf("Decompressed data differs from original!\n");
        return -1;
      }
    }

    printf("Successful roundtrip!\n");
  }

  /* After using it, destroy the Blosc environment */
  blosc2_destroy();

  return 0;
}
