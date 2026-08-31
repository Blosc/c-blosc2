/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Regression test for blosclz bounds checking and integer overflow.

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
**********************************************************************/

#include "test_common.h"
#include "blosclz.h"

static int test_blosclz_oversized_match(void) {
  int maxout = 65536;
  int input_len = 100000;
  uint8_t* input = malloc((size_t)input_len);
  uint8_t* output = malloc((size_t)maxout);
  if (!input || !output) {
    free(input);
    free(output);
    return EXIT_FAILURE;
  }

  /* Craft a blosclz stream:
     1 literal byte first, then match token 0xE0 (len_code=7), followed by many 0xFF bytes */
  input[0] = 0;      /* literal token (1 byte) */
  input[1] = 'A';    /* 1 literal byte */
  input[2] = 0xE0;   /* match token with len = 7 - 1 */
  memset(input + 3, 0xFF, (size_t)(input_len - 5));
  input[input_len - 2] = 0x01; /* offset low byte */
  input[input_len - 1] = 0x00; /* offset high byte */

  int dsize = blosclz_decompress(input, input_len, output, maxout);
  if (dsize != 0) {
    printf("Expected blosclz_decompress to return 0 for oversized match, got %d\n", dsize);
    free(input);
    free(output);
    return EXIT_FAILURE;
  }

  free(input);
  free(output);
  return EXIT_SUCCESS;
}

static int test_blosclz_oversized_literal(void) {
  int maxout = 10;
  uint8_t input[33];
  uint8_t* output = malloc((size_t)maxout);
  if (!output) {
    return EXIT_FAILURE;
  }

  /* Literal ctrl = 31 -> 32 bytes literal, which exceeds maxout = 10 */
  input[0] = 31;
  memset(input + 1, 'X', 32);

  int dsize = blosclz_decompress(input, 33, output, maxout);
  if (dsize != 0) {
    printf("Expected blosclz_decompress to return 0 for literal > maxout, got %d\n", dsize);
    free(output);
    return EXIT_FAILURE;
  }

  free(output);
  return EXIT_SUCCESS;
}

static int test_blosclz_chunk_poc(void) {
  int32_t blocksize = 65536;
  int32_t cbytes = 100000;
  uint8_t* chunk = calloc(1, (size_t)cbytes);
  uint8_t* output = malloc((size_t)blocksize);
  if (!chunk || !output) {
    free(chunk);
    free(output);
    return EXIT_FAILURE;
  }

  /* Blosc2 chunk header */
  chunk[0] = 2; /* Blosc format version */
  chunk[1] = 4; /* Blosc2 format version */
  chunk[2] = 0; /* Flags (no filter) */
  chunk[3] = 1; /* Typesize */

  /* nbytes */
  chunk[4] = (uint8_t)(blocksize & 0xff);
  chunk[5] = (uint8_t)((blocksize >> 8) & 0xff);
  chunk[6] = (uint8_t)((blocksize >> 16) & 0xff);
  chunk[7] = (uint8_t)((blocksize >> 24) & 0xff);

  /* blocksize */
  chunk[8] = (uint8_t)(blocksize & 0xff);
  chunk[9] = (uint8_t)((blocksize >> 8) & 0xff);
  chunk[10] = (uint8_t)((blocksize >> 16) & 0xff);
  chunk[11] = (uint8_t)((blocksize >> 24) & 0xff);

  /* cbytes */
  chunk[12] = (uint8_t)(cbytes & 0xff);
  chunk[13] = (uint8_t)((cbytes >> 8) & 0xff);
  chunk[14] = (uint8_t)((cbytes >> 16) & 0xff);
  chunk[15] = (uint8_t)((cbytes >> 24) & 0xff);

  /* Codec is BLOSC_BLOSCLZ (0) at byte 20 */
  chunk[20] = BLOSC_BLOSCLZ;

  int header_len = BLOSC_EXTENDED_HEADER_LENGTH;
  chunk[header_len] = 0;        /* 1-byte literal ctrl */
  chunk[header_len + 1] = 'A';  /* literal data */
  chunk[header_len + 2] = 0xE0; /* match token */
  memset(chunk + header_len + 3, 0xFF, (size_t)(cbytes - header_len - 5));
  chunk[cbytes - 2] = 0x01;
  chunk[cbytes - 1] = 0x00;

  int dsize = blosc2_decompress(chunk, cbytes, output, blocksize);
  if (dsize >= 0) {
    printf("Expected blosc2_decompress to reject crafted chunk, got %d\n", dsize);
    free(chunk);
    free(output);
    return EXIT_FAILURE;
  }

  free(chunk);
  free(output);
  return EXIT_SUCCESS;
}

static int test_blosclz_roundtrip_valid(void) {
  const int size = 100000;
  uint8_t* src = malloc((size_t)size);
  uint8_t* compressed = malloc((size_t)size + 1000);
  uint8_t* decompressed = malloc((size_t)size);

  if (!src || !compressed || !decompressed) {
    free(src);
    free(compressed);
    free(decompressed);
    return EXIT_FAILURE;
  }

  for (int i = 0; i < size; i++) {
    src[i] = (uint8_t)(i % 127);
  }

  int csize = blosclz_compress(5, src, size, compressed, size + 1000, NULL);
  if (csize <= 0) {
    printf("blosclz_compress failed: %d\n", csize);
    free(src);
    free(compressed);
    free(decompressed);
    return EXIT_FAILURE;
  }

  int dsize = blosclz_decompress(compressed, csize, decompressed, size);
  if (dsize != size) {
    printf("blosclz_decompress returned %d, expected %d\n", dsize, size);
    free(src);
    free(compressed);
    free(decompressed);
    return EXIT_FAILURE;
  }

  if (memcmp(src, decompressed, (size_t)size) != 0) {
    printf("Decompressed data mismatch!\n");
    free(src);
    free(compressed);
    free(decompressed);
    return EXIT_FAILURE;
  }

  free(src);
  free(compressed);
  free(decompressed);
  return EXIT_SUCCESS;
}

int main(void) {
  blosc2_init();

  if (test_blosclz_oversized_match() != EXIT_SUCCESS) {
    blosc2_destroy();
    return EXIT_FAILURE;
  }

  if (test_blosclz_oversized_literal() != EXIT_SUCCESS) {
    blosc2_destroy();
    return EXIT_FAILURE;
  }

  if (test_blosclz_chunk_poc() != EXIT_SUCCESS) {
    blosc2_destroy();
    return EXIT_FAILURE;
  }

  if (test_blosclz_roundtrip_valid() != EXIT_SUCCESS) {
    blosc2_destroy();
    return EXIT_FAILURE;
  }

  blosc2_destroy();
  printf("All blosclz bounds tests passed successfully.\n");
  return EXIT_SUCCESS;
}
