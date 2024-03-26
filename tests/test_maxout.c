/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Unit tests for basic features in Blosc.

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"

int tests_run = 0;

/* Global vars */
void* src, * srccpy, * dest, * dest2;
int nbytes, cbytes;
int clevel = 1;
int doshuffle = 0;
size_t typesize = 4;
size_t size = 1000;             /* must be divisible by 4 */


/* Check input size > BLOSC2_MAX_BUFFERSIZE */
static char* test_input_too_large(void) {

  /* Get a compressed buffer */
  cbytes = blosc1_compress(clevel, doshuffle, typesize, BLOSC2_MAX_BUFFERSIZE + 1, src,
                           dest, size + BLOSC2_MAX_OVERHEAD - 1);
  mu_assert("ERROR: cbytes is not == 0", cbytes < 0);

  return 0;
}


/* Check maxout < size */
static char* test_maxout_less(void) {

  /* Get a compressed buffer */
  cbytes = blosc1_compress(clevel, doshuffle, typesize, size, src, dest, size);
  mu_assert("ERROR: cbytes is not 0", cbytes == 0);

  return 0;
}


/* Check maxout < size (memcpy version) */
static char* test_maxout_less_memcpy(void) {

  /* Get a compressed buffer */
  cbytes = blosc1_compress(0, doshuffle, typesize, size, src, dest,
                           size + BLOSC2_MAX_OVERHEAD - 1);
  mu_assert("ERROR: cbytes is not 0", cbytes == 0);

  return 0;
}


/* Check maxout == size */
static char* test_maxout_equal(void) {

  /* Get a compressed buffer */
  cbytes = blosc1_compress(clevel, doshuffle, typesize, size, src, dest,
                           size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cbytes is not correct", cbytes <= (int)size + BLOSC2_MAX_OVERHEAD);

  /* Decompress the buffer */
  nbytes = blosc1_decompress(dest, dest2, size);
  mu_assert("ERROR: nbytes incorrect(1)", nbytes == (int)size);

  return 0;
}


/* Check maxout == size */
static char* test_maxout_equal_memcpy(void) {

  /* Get a compressed buffer */
  cbytes = blosc1_compress(0, doshuffle, typesize, size, src, dest,
                           size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cbytes is not correct", cbytes == (int)size + BLOSC2_MAX_OVERHEAD);

  /* Decompress the buffer */
  nbytes = blosc1_decompress(dest, dest2, size);
  mu_assert("ERROR: nbytes incorrect(1)", nbytes == (int)size);

  return 0;
}


/* Check maxout > size */
static char* test_maxout_great(void) {
  /* Get a compressed buffer */
  cbytes = blosc1_compress(clevel, doshuffle, typesize, size, src, dest,
                           size + BLOSC2_MAX_OVERHEAD + 1);
  mu_assert("ERROR: cbytes is not correct", cbytes <= (int)size + BLOSC2_MAX_OVERHEAD);

  /* Decompress the buffer */
  nbytes = blosc1_decompress(dest, dest2, size);
  mu_assert("ERROR: nbytes incorrect(1)", nbytes == (int)size);

  return 0;
}


/* Check maxout > size */
static char* test_maxout_great_memcpy(void) {
  /* Get a compressed buffer */
  cbytes = blosc1_compress(0, doshuffle, typesize, size, src, dest,
                           size + BLOSC2_MAX_OVERHEAD + 1);
  mu_assert("ERROR: cbytes is not correct", cbytes == (int)size + BLOSC2_MAX_OVERHEAD);

  /* Decompress the buffer */
  nbytes = blosc1_decompress(dest, dest2, size);
  mu_assert("ERROR: nbytes incorrect(1)", nbytes == (int)size);

  return 0;
}

/* Check maxout < BLOSC2_MAX_OVERHEAD */
static char* test_max_overhead(void) {
  blosc2_init();
  cbytes = blosc1_compress(0, doshuffle, typesize, size, src, dest,
                           BLOSC2_MAX_OVERHEAD - 1);
  mu_assert("ERROR: cbytes is not correct", cbytes < 0);
  blosc2_destroy();

  blosc2_init();
  cbytes = blosc1_compress(0, doshuffle, typesize, size, src, dest,
                           BLOSC2_MAX_OVERHEAD - 2);
  mu_assert("ERROR: cbytes is not correct", cbytes < 0);
  blosc2_destroy();

  blosc2_init();
  cbytes = blosc1_compress(0, doshuffle, typesize, size, src, dest, 0);
  mu_assert("ERROR: cbytes is not correct", cbytes < 0);
  blosc2_destroy();

  return 0;
}


static char* all_tests(void) {
  mu_run_test(test_input_too_large);
  mu_run_test(test_maxout_less);
  mu_run_test(test_maxout_less_memcpy);
  mu_run_test(test_maxout_equal);
  mu_run_test(test_maxout_equal_memcpy);
  mu_run_test(test_maxout_great);
  mu_run_test(test_maxout_great_memcpy);
  mu_run_test(test_max_overhead);

  return 0;
}

#define BUFFER_ALIGN_SIZE   32

int main(void) {
  int32_t* _src;
  char* result;
  size_t i;

  blosc2_init();
  blosc2_set_nthreads(1);

  /* Initialize buffers */
  src = blosc_test_malloc(BUFFER_ALIGN_SIZE, size);
  srccpy = blosc_test_malloc(BUFFER_ALIGN_SIZE, size);
  dest = blosc_test_malloc(BUFFER_ALIGN_SIZE, size + BLOSC2_MAX_OVERHEAD);
  dest2 = blosc_test_malloc(BUFFER_ALIGN_SIZE, size);
  _src = (int32_t*)src;
  for (i = 0; i < (size / 4); i++) {
    _src[i] = (int32_t)i;
  }
  memcpy(srccpy, src, size);

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
  blosc_test_free(dest2);

  blosc2_destroy();

  return result != 0;
}
