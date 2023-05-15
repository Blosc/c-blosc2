/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Generator data file for Blosc forward and backward tests.

  Author: Elvis Stansvik, Francesc Alted <francesc@blosc.org>

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/

#include "blosc2.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#if defined(_WIN32) && !defined(__MINGW32__)
#include <windows.h>
#endif  /* _WIN32 */

#define SIZE (1000 * 1000)


int main(int argc, char *argv[]) {
  BLOSC_UNUSED_PARAM(argc);
  static int32_t data[SIZE];
  static int32_t data_out[SIZE];
  static int32_t data_dest[SIZE];
  size_t isize = SIZE * sizeof(int32_t);
  size_t osize = SIZE * sizeof(int32_t);
  int dsize = SIZE * sizeof(int32_t);
  int csize;
  long fsize;
  int i;
  int exit_code = 0;

  FILE *f;

  /* Register the filter with the library */
  printf("Blosc version info: %s\n", blosc2_get_version_string());

  /* Initialize the Blosc compressor */
  blosc2_init();
  blosc2_set_nthreads(1);

  /* Use the argv[2] compressor. The supported ones are "blosclz",
  "lz4", "lz4hc", "zlib" and "zstd"*/
  blosc1_set_compressor(argv[2]);

  for (i = 0; i < SIZE; i++) {
    data[i] = i;
  }

  if (strcmp(argv[1], "compress") == 0) {

    /* Compress with clevel=9 and shuffle active  */
    csize = blosc1_compress(9, 1, sizeof(int32_t), isize, data, data_out, osize);
    if (csize == 0) {
      printf("Buffer is incompressible.  Giving up.\n");
      return 1;
    } else if (csize < 0) {
      printf("Compression error.  Error code: %d\n", csize);
      return csize;
    }

    printf("Compression: %d -> %d (%.1fx)\n", (int) isize, csize, (1. * (int)isize) / csize);

    /* Write data_out to argv[3] */
    f = fopen(argv[3], "wb+");
    if (fwrite(data_out, 1, (size_t) csize, f) == (uint32_t) csize) {
      printf("Wrote %s\n", argv[3]);
    } else {
      printf("Write failed");
    }
  } else {
    /* Read from argv[2] into data_out. */
    f = fopen(argv[2], "rb");
    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fread(data_out, 1, (size_t) fsize, f) == (uint32_t) fsize) {
      printf("Checking %s\n", argv[2]);
    } else {
      printf("Read failed");
    }

    /* Decompress */
    dsize = blosc1_decompress(data_out, data_dest, (size_t) dsize);
    if (dsize < 0) {
      printf("Decompression error.  Error code: %d\n", dsize);
      return dsize;
    }
    printf("Decompression successful!\n");

    char *isbitshuf =  strstr(argv[2], "-bitshuffle");
    if ((isbitshuf != NULL) && (dsize % 8) != 0) {
      dsize -= dsize % 8;   // do not check unaligned data (e.g. blosc-1.17.1-bitshuffle8-nomemcpy.cdata)
    }
    exit_code = memcmp(data, data_dest, dsize) ? EXIT_FAILURE : EXIT_SUCCESS;

    if (exit_code == EXIT_SUCCESS) {
      printf("Successful roundtrip!\n");
    }
    else {
      printf("Decompressed data differs from original!\n");
      for (i = 0; i < dsize; i++) {
        if (((uint8_t*)data)[i] != ((uint8_t*)data_dest)[i]) {
          printf("values start to differ in pos: %d\n", i);
          break;
        }
      }
    }
  }

  /* After using it, destroy the Blosc environment */
  blosc2_destroy();

  return exit_code;
}
