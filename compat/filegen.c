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

typedef enum {
  OP_COMPRESS,
  OP_SHUFFLE,
  OP_BITSHUFFLE,
  OP_DECOMPRESS,
  OP_UNSHUFFLE,
  OP_BITUNSHUFFLE,
} op_t;

static op_t parse_op(const char *op) {
  if (strcmp(op, "compress") == 0) {
    return OP_COMPRESS;
  } else if (strcmp(op, "shuffle") == 0) {
    return OP_SHUFFLE;
  } else if (strcmp(op, "bitshuffle") == 0) {
    return OP_BITSHUFFLE;
  } else if (strcmp(op, "decompress") == 0) {
    return OP_DECOMPRESS;
  } else if (strcmp(op, "unshuffle") == 0) {
    return OP_UNSHUFFLE;
  } else if (strcmp(op, "bitunshuffle") == 0) {
    return OP_BITUNSHUFFLE;
  } else {
    printf("Unknown operation: %s\n", op);
    exit(-1);
  }
}

static int32_t run(op_t op, const void * src, void * dest, const size_t size) {
  int32_t result = -1;
  if (op == OP_COMPRESS) {
    /* Compress with clevel=9 and shuffle active  */
    result = blosc1_compress(9, 1, sizeof(int32_t), size, src, dest, size);
    if (result == 0) {
      printf("Buffer is incompressible.  Giving up.\n");
      exit(1);
    }
    if (result < 0) {
      printf("Compression error. Error code: %d\n", result);
      return result;
    }
    printf("Compression successful: %d bytes compressed.\n", result);
  } else if (op == OP_SHUFFLE) {
    result = blosc2_shuffle(sizeof(int32_t), size, src, dest);
    if (result < 0) {
      printf("Shuffle error. Error code: %d\n", result);
      return result;
    }
    printf("Shuffle successful: %d bytes shuffled.\n", result);
  } else if (op == OP_BITSHUFFLE) {
    result = blosc2_bitshuffle(sizeof(int32_t), size, src, dest);
    if (result < 0) {
      printf("Bitshuffle error. Error code: %d\n", result);
      return result;
    }
    printf("Bitshuffle successful: %d bytes shuffled.\n", result);
  } else if (op == OP_DECOMPRESS) {
    /* Compress with clevel=9 and shuffle active  */
    result = blosc1_decompress(src, dest, size);
    if (result < 0) {
      printf("Decompression error.  Error code: %d\n", result);
      return result;
    }
    printf("Decompression successful!\n");
  } else if (op == OP_UNSHUFFLE) {
    result = blosc2_unshuffle(sizeof(int32_t), size, src, dest);
    if (result < 0) {
      printf("Unshuffle error. Error code: %d\n", result);
      return result;
    }
    printf("Unshuffle successful: %d bytes unshuffled.\n", result);
  } else if (op == OP_BITUNSHUFFLE) {
    result = blosc2_bitunshuffle(sizeof(int32_t), size, src, dest);
    if (result < 0) {
      printf("Bitunshuffle error. Error code: %d\n", result);
      return result;
    }
    printf("Bitunshuffle successful: %d bytes bitunshuffled.\n", result);
  }
  return result;
}

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

  if (argc != 3 && argc != 4) {
    printf("Usage:\n");
    printf("%s <compress|shuffle|bitshuffle> <compressor> <output_file>\n", argv[0]);
    printf("%s <unshuffle|bitunshuffle> <output_file>\n", argv[0]);
    return 1;
  }

  op_t operation = parse_op(argv[1]);
  int is_encoding = operation == OP_COMPRESS || operation == OP_SHUFFLE || operation == OP_BITSHUFFLE;

  /* Register the filter with the library */
  printf("Blosc version info: %s\n", blosc2_get_version_string());

  /* Initialize the Blosc compressor */
  blosc2_init();
  blosc2_set_nthreads(1);

  if (operation == OP_COMPRESS) {
    /* Use the argv[2] compressor. The supported ones are "blosclz",
    "lz4", "lz4hc", "zlib" and "zstd"*/
    blosc1_set_compressor(argv[2]);
  }

  for (i = 0; i < SIZE; i++) {
    data[i] = i;
  }

  if (is_encoding) {
    csize = run(operation, data, data_out, isize);
    if (csize < 0) {
      return csize;
    }

    if (operation == OP_COMPRESS) {
      printf("Compression: %d -> %d (%.1fx)\n", (int)isize, csize, (1. * (int)isize) / csize);
    }

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

    dsize = run(operation, data_out, data_dest, (size_t) dsize);
    if (dsize < 0) {
      return dsize;
    }

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
