/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Unit tests for BLOSC_NTHREADS environment variable in Blosc.

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"

int tests_run = 0;

/* Global vars */
void *src, *srccpy, *dest, *dest2;
int nbytes, cbytes;
int clevel = 1;
int doshuffle = 1;
size_t typesize = 4;
size_t size = 4 * 1000 * 1000;             /* must be divisible by 4 */


/* Check just compressing */
static char *test_compress(void) {
  int16_t nthreads;

  /* Before any blosc1_compress() or blosc1_decompress() the number of
     threads must be 1 */
  nthreads = blosc2_get_nthreads();
  mu_assert("ERROR: get_nthreads (compress, before) incorrect", nthreads == 1);

  /* Get a compressed buffer */
  cbytes = blosc1_compress(clevel, doshuffle, typesize, size, src,
                           dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cbytes is not correct", cbytes < (int)size);

  nthreads = blosc2_get_nthreads();
  mu_assert("ERROR: get_nthreads (compress, after) incorrect", nthreads == 3);

  return 0;
}


/* Check compressing + decompressing */
static char *test_compress_decompress(void) {
  int16_t nthreads;

  nthreads = blosc2_get_nthreads();
  mu_assert("ERROR: get_nthreads incorrect", nthreads == 3);

  /* Get a compressed buffer */
  cbytes = blosc1_compress(clevel, doshuffle, typesize, size, src,
                           dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cbytes is not correct", cbytes < (int)size);

  nthreads = blosc2_get_nthreads();
  mu_assert("ERROR: get_nthreads incorrect", nthreads == 3);

  /* Decompress the buffer */
  nbytes = blosc1_decompress(dest, dest2, size);
  mu_assert("ERROR: nbytes incorrect(1)", nbytes == (int)size);

  nthreads = blosc2_get_nthreads();
  mu_assert("ERROR: get_nthreads incorrect", nthreads == 3);

  return 0;
}

/* Check nthreads limits */
static char *test_nthreads_limits(void) {
  /* Get a compressed buffer */
  cbytes = blosc1_compress(clevel, doshuffle, typesize, size, src,
                           dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cbytes is not correct", cbytes < (int)size);

  int16_t nthreads = blosc2_set_nthreads((int16_t) (INT16_MAX + 1));
  mu_assert("ERROR: nthreads incorrect (1)", nthreads < 0);
  /* Decompress the buffer */
  nbytes = blosc1_decompress(dest, dest2, size);
  mu_assert("ERROR: nbytes incorrect(>=0)", nbytes < 0);

  nthreads = blosc2_set_nthreads(0);
  mu_assert("ERROR: nthreads incorrect (2)", nthreads < 0);
  /* Decompress the buffer */
  nbytes = blosc1_decompress(dest, dest2, size);
  mu_assert("ERROR: nbytes incorrect(>=0)", nbytes < 0);

  return 0;
}

/* Check nthreads limits */
static char *test_nthreads_limits_envvar(void) {
  /* Get a compressed buffer */
  cbytes = blosc1_compress(clevel, doshuffle, typesize, size, src,
                           dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cbytes is not correct", cbytes < (int)size);

  char strval[10];
  sprintf(strval, "%d", INT16_MAX + 1);
  setenv("BLOSC_NTHREADS", strval, 1);
  /* Decompress the buffer */
  nbytes = blosc1_decompress(dest, dest2, size);
  mu_assert("ERROR: nbytes incorrect (1)", nbytes < 0);

  sprintf(strval, "%d", -1);
  setenv("BLOSC_NTHREADS", strval, 1);
  /* Decompress the buffer */
  nbytes = blosc1_decompress(dest, dest2, size);
  mu_assert("ERROR: nbytes incorrect (2)", nbytes < 0);

  return 0;
}


static char *all_tests(void) {
  mu_run_test(test_compress);
  mu_run_test(test_compress_decompress);
  mu_run_test(test_nthreads_limits);
  mu_run_test(test_nthreads_limits_envvar);

  return 0;
}

#define BUFFER_ALIGN_SIZE   32

int main(void) {
  int32_t *_src;
  char *result;
  size_t i;

  /* Activate the BLOSC_NTHREADS variable */
  setenv("BLOSC_NTHREADS", "3", 1);

  install_blosc_callback_test(); /* optionally install callback test */
  blosc2_init();
  blosc2_set_nthreads(1);

  /* Initialize buffers */
  src = blosc_test_malloc(BUFFER_ALIGN_SIZE, size);
  srccpy = blosc_test_malloc(BUFFER_ALIGN_SIZE, size);
  dest = blosc_test_malloc(BUFFER_ALIGN_SIZE, size + BLOSC2_MAX_OVERHEAD);
  dest2 = blosc_test_malloc(BUFFER_ALIGN_SIZE, size);
  _src = (int32_t *)src;
  for (i=0; i < (size/4); i++) {
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
