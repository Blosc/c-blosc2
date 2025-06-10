/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Unit tests for BLOSC_COMPRESSOR environment variable in Blosc.

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"

int tests_run = 0;

/* Global vars */
uint8_t *src, *srccpy, *dest;
int nbytes, cbytes;
int clevel = 1;
int doshuffle = 1;
int typesize;
int size = 7 * 12 * 13 * 16 * 24 * 10;  /* must be divisible by typesize */


/* Check compressor */
static char *test_delta(void) {
  int cbytes2;
  int buf_equal;

  switch (typesize) {
    case 1:
      for (int i = 0; i < size / typesize; i++) {
        src[i] = (uint8_t)i;
      }
      break;
    case 2:
      for (int i = 0; i < size / typesize; i++) {
        ((uint16_t*)src)[i] = (uint16_t)i;
      }
      break;
    case 4:
      for (int i = 0; i < size / typesize; i++) {
        ((uint32_t*)src)[i] = (uint32_t)i;
      }
      break;
    case 7:
      for (int i = 0; i < size / typesize; i++) {
        *(uint32_t*)(src + i * 4) = (uint32_t)i;
        *(uint16_t*)(src + i * 4 + 2) = (uint16_t)i;
        *(src + i * 4 + 2 + 1) = (uint8_t)i;
      }
      break;
    case 8:
      for (int i = 0; i < size / typesize; i++) {
        ((uint64_t*)src)[i] = (uint64_t)i;
      }
      break;
    case 12:
      for (int i = 0; i < size / typesize; i++) {
        *(uint64_t*)(src + i * 8) = (uint64_t)i;
        *(uint32_t*)(src + i * 8 + 4) = 1;
      }
      break;
    case 13:
      for (int i = 0; i < size / typesize; i++) {
        *(uint64_t*)(src + i * 8) = (uint64_t)i;
        *(uint32_t*)(src + i * 8 + 4) = 1;
        *(src + i * 8 + 4 + 1) = 1;
      }
      break;
    case 16:
      for (int i = 0; i < size / typesize; i += 2) {
        ((uint64_t*)src)[i] = (uint64_t)i;
        ((uint64_t*)src)[i+1] = (uint64_t)i + 1;
      }
      break;
    case 24:
      for (int i = 0; i < size / typesize; i++) {
        *(uint64_t*)(src + i * 8) = (uint64_t)i;
        *(uint32_t*)(src + i * 8 + 4) = 1;
        *(uint64_t*)(src + i * 8 + 4 + 8) = (uint64_t)i;
        *(uint32_t*)(src + i * 8 + 4 + 8 + 4) = 2;
      }
      break;
    default:
      for (int i = 0; i < size / typesize; i++) {
        src[i] = (uint8_t)i;
      }
  }
  memcpy(srccpy, src, size);

  /* Get a compressed buffer without delta */
  blosc2_set_delta(0);
  cbytes = blosc1_compress(clevel, doshuffle, (size_t)typesize, (size_t)size, src,
                           dest, (size_t)size + BLOSC2_MAX_OVERHEAD);

  /* Activate the delta filter and compress again */
  blosc2_set_delta(1);
  cbytes2 = blosc1_compress(clevel, doshuffle, (size_t)typesize, (size_t)size, src,
                            dest, (size_t)size + BLOSC2_MAX_OVERHEAD);
  if ((typesize == 12) || (typesize == 15) || (typesize == 24)) {
    // For typesizes 12, 15 and 24 we make an exception and allow less compression
    if ((2 * cbytes2) > (4 * cbytes)) {
      fprintf(stderr, "Failed test for DELTA and typesize: %d\n", typesize);
      fprintf(stderr, "Size with no DELTA: %d.  Size with DELTA: %d\n",
              cbytes, cbytes2);
      mu_assert("ERROR: DELTA does not work correctly",
                (2 * cbytes2) < (4 * cbytes));
    }
  }
  else if (cbytes2 > cbytes) {
    fprintf(stderr, "Failed test for DELTA and typesize: %d\n", typesize);
    fprintf(stderr, "Size with no DELTA: %d.  Size with DELTA: %d\n",
            cbytes, cbytes2);
    mu_assert("ERROR: DELTA does not work correctly", cbytes2 < cbytes);
  }

  /* Decompress the buffer with delta */
  nbytes = blosc1_decompress(dest, src, (size_t)size);
  mu_assert("ERROR: nbytes incorrect", nbytes == size);

  buf_equal = memcmp(src, srccpy, (size_t)size);
  if (buf_equal != 0) {
    fprintf(stderr, "Failed test for DELTA and typesize: %d\n", typesize);
  }
  mu_assert("ERROR: roundtrip not successful", buf_equal == 0);

  return 0;
}

static char *all_tests(void) {
  typesize = 1;
  mu_run_test(test_delta);
  typesize = 2;
  mu_run_test(test_delta);
  typesize = 4;
  mu_run_test(test_delta);
  typesize = 7;
  mu_run_test(test_delta);
  typesize = 8;
  mu_run_test(test_delta);
  typesize = 12;
  mu_run_test(test_delta);
  typesize = 13;
  mu_run_test(test_delta);
  typesize = 15;
  mu_run_test(test_delta);
  typesize = 16;
  mu_run_test(test_delta);

  return 0;
}

#define BUFFER_ALIGN_SIZE   32

int main(void) {
  char *result;

  blosc2_init();
  blosc1_set_compressor("blosclz");

  /* Initialize buffers */
  src = blosc_test_malloc(BUFFER_ALIGN_SIZE, (size_t)size);
  srccpy = blosc_test_malloc(BUFFER_ALIGN_SIZE, (size_t)size);
  dest = blosc_test_malloc(BUFFER_ALIGN_SIZE, (size_t)size + BLOSC2_MAX_OVERHEAD);

  /* Run all the suite */
  result = all_tests();
  if (result != 0) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  blosc_test_free(src);
  blosc_test_free(srccpy);
  blosc_test_free(dest);

  blosc2_destroy();

  return result != 0;
}
