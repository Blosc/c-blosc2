/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Unit tests for BLOSC_BLOSC1_COMPAT environment variable in Blosc.

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"

#define BUFFER_ALIGN_SIZE 32
#define NTHREADS 1

int tests_run = 0;

/* Global vars */
void *src, *srccpy, *dest, *dest2;
int nbytes, cbytes;
int clevel = 1;
int doshuffle = 1;
size_t typesize = sizeof(int32_t);
size_t size = sizeof(int32_t) * 1000 * 1000;


/* Check compressing + decompressing */
static char *test_compress_decompress(void) {
  int32_t *_src = (int32_t *)src;
  for (int i = 0; i < (int)(size / sizeof(int32_t)); i++) {
    _src[i] = (int32_t)i;
  }
  memcpy(srccpy, src, size);

  /* Get a compressed buffer */
  cbytes = blosc1_compress(clevel, doshuffle, typesize, size, src,
                           dest, size + BLOSC_MIN_HEADER_LENGTH);
  mu_assert("ERROR: cbytes is not correct", cbytes < (int)size);

  /* Decompress the buffer */
  nbytes = blosc1_decompress(dest, dest2, size);
  mu_assert("ERROR: nbytes incorrect(1)", nbytes == (int)size);

  // Check roundtrip
  int exit_code = memcmp(srccpy, dest2, size) ? EXIT_FAILURE : EXIT_SUCCESS;
  mu_assert("ERROR: Bad roundtrip!", exit_code == EXIT_SUCCESS);

  return 0;
}


/* Check compressing + decompressing */
static char *test_compress_decompress_zeros(void) {
  int32_t *_src = (int32_t *)src;
  for (int i = 0; i < (int)(size / sizeof(int32_t)); i++) {
    _src[i] = 0;
  }
  memcpy(srccpy, src, size);

  /* Get a compressed buffer */
  cbytes = blosc1_compress(clevel, doshuffle, typesize, size, src,
                           dest, size + BLOSC_MIN_HEADER_LENGTH);
  mu_assert("ERROR: cbytes is not correct", cbytes < (int)size);

  /* Decompress the buffer */
  nbytes = blosc1_decompress(dest, dest2, size);
  mu_assert("ERROR: nbytes incorrect(1)", nbytes == (int)size);

  // Check roundtrip
  int exit_code = memcmp(srccpy, dest2, size) ? EXIT_FAILURE : EXIT_SUCCESS;
  mu_assert("ERROR: Bad roundtrip!", exit_code == EXIT_SUCCESS);

  return 0;
}


/* Check compressing + getitem */
static char *test_compress_getitem(void) {
  int32_t *_src = (int32_t *)src;
  for (int i = 0; i < (int)(size / sizeof(int32_t)); i++) {
    _src[i] = (int32_t)i;
  }
  memcpy(srccpy, src, size);

  /* Get a compressed buffer */
  cbytes = blosc1_compress(clevel, doshuffle, typesize, size, src,
                           dest, size + BLOSC_MIN_HEADER_LENGTH);
  mu_assert("ERROR: cbytes is not correct", cbytes < (int)size);

  /* Decompress the buffer */
  nbytes = blosc1_getitem(dest, 1, 10, dest2);
  mu_assert("ERROR: nbytes incorrect(1)", nbytes == (int32_t) (10 * typesize));

  // Check roundtrip
  int exit_code = memcmp(srccpy + typesize, dest2, 10 * typesize) ? EXIT_FAILURE : EXIT_SUCCESS;
  mu_assert("ERROR: Bad roundtrip!", exit_code == EXIT_SUCCESS);

  return 0;
}

static char *all_tests(void) {
  mu_run_test(test_compress_decompress);
  mu_run_test(test_compress_getitem);
  mu_run_test(test_compress_decompress_zeros);

  return 0;
}


int main(void) {
  char *result;

  /* Activate the BLOSC_BLOSC1_COMPAT variable */
  setenv("BLOSC_BLOSC1_COMPAT", "TRUE", 0);

  blosc2_init();
  blosc2_set_nthreads(NTHREADS);

  /* Initialize buffers */
  src = blosc_test_malloc(BUFFER_ALIGN_SIZE, size);
  srccpy = blosc_test_malloc(BUFFER_ALIGN_SIZE, size);
  dest = blosc_test_malloc(BUFFER_ALIGN_SIZE, size + BLOSC_MIN_HEADER_LENGTH);
  dest2 = blosc_test_malloc(BUFFER_ALIGN_SIZE, size);

  /* Run all the suite */
  result = all_tests();
  if (result != 0) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED\n");
  }
  printf("\tTests run: %d\n", tests_run);

  blosc_test_free(src);
  blosc_test_free(srccpy);
  blosc_test_free(dest);
  blosc_test_free(dest2);

  blosc2_destroy();

  /* Reset envvar */
  unsetenv("BLOSC_BLOSC1_COMPAT");

  return result != 0;
}
