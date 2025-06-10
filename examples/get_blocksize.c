/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating the use of a Blosc from C code.

  To compile this program:

  $ gcc -O get_blocksize.c -o get_blocksize -lblosc2

  To run:

  $ ./get_blocksize
  Blosc version info: 2.10.3.dev ($Date:: 2023-08-19 #$)
  Compression: 10000000 -> 32 (312500.0x)
  osize, csize, blocksize: 10000000, 32, 16384
  Compression: 10000000 -> 32 (312500.0x)
  osize, csize, blocksize: 10000000, 32, 131072
  Compression: 10000000 -> 32 (312500.0x)
  osize, csize, blocksize: 10000000, 32, 65536
  Compression: 10000000 -> 32 (312500.0x)
  osize, csize, blocksize: 10000000, 32, 131072
  Compression: 10000000 -> 32 (312500.0x)
  osize, csize, blocksize: 10000000, 32, 262144
  Compression: 10000000 -> 32 (312500.0x)
  osize, csize, blocksize: 10000000, 32, 262144
  Compression: 10000000 -> 32 (312500.0x)
  osize, csize, blocksize: 10000000, 32, 524288
  Compression: 10000000 -> 32 (312500.0x)
  osize, csize, blocksize: 10000000, 32, 1048576
  Compression: 10000000 -> 32 (312500.0x)
  osize, csize, blocksize: 10000000, 32, 524288
  Compression: 10000000 -> 32 (312500.0x)
  osize, csize, blocksize: 10000000, 32, 2097152

  Process finished with exit code 0

*/

#include <stdio.h>
#include "blosc2.h"


int main(void) {
  blosc2_init();

  static uint8_t data_dest[BLOSC2_MAX_OVERHEAD];
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(float);
  cparams.compcode = BLOSC_ZSTD;

  printf("Blosc version info: %s (%s)\n",
         BLOSC2_VERSION_STRING, BLOSC2_VERSION_DATE);

  /* Do the actual compression */
  for (int clevel=0; clevel < 10; clevel++) {
    cparams.clevel = clevel;
    cparams.splitmode = clevel % 2;
    int isize = 10 * 1000 * 1000;
    int osize, csize, blocksize;
    csize = blosc2_chunk_zeros(cparams, isize, data_dest, BLOSC2_MAX_OVERHEAD);
    printf("Compression: %d -> %d (%.1fx)\n", isize, csize, (1. * isize) / csize);

    BLOSC_ERROR(blosc2_cbuffer_sizes(data_dest, &osize, &csize, &blocksize));
    printf("osize, csize, blocksize: %d, %d, %d\n", osize, csize, blocksize);
  }

  blosc2_destroy();

  return 0;
}
