/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* Test for using blosc without blosc2_init() and blosc2_destroy() */

#include <unistd.h>
#include <assert.h>
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

  /* Get a compressed buffer */
  cbytes = blosc1_compress(clevel, doshuffle, typesize, size, src,
                           dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cbytes is not correct", cbytes < (int)size);

  return 0;
}


/* Check compressing + decompressing */
static char *test_compress_decompress(void) {

  /* Get a compressed buffer */
  cbytes = blosc1_compress(clevel, doshuffle, typesize, size, src,
                           dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cbytes is not correct", cbytes < (int)size);

  /* Decompress the buffer */
  nbytes = blosc1_decompress(dest, dest2, size);
  mu_assert("ERROR: nbytes incorrect(1)", nbytes == (int)size);

  return 0;
}


static char *all_tests(void) {
  mu_run_test(test_compress);
  mu_run_test(test_compress_decompress);

  return 0;
}

#define BUFFER_ALIGN_SIZE   32

int main(void) {
  int32_t *_src;
  char *result;
  int i;
  int pid, nchildren = 4;

  /* Launch several subprocesses */
  for (i = 1; i <= nchildren; i++) {
    pid = fork();
    if (pid < 0) {
      printf("Error at fork()!");
      exit(1);
    }
  }

  blosc2_set_nthreads(4);

  /* Initialize buffers */
  src = blosc_test_malloc(BUFFER_ALIGN_SIZE, size);
  srccpy = blosc_test_malloc(BUFFER_ALIGN_SIZE, size);
  dest = blosc_test_malloc(BUFFER_ALIGN_SIZE, size + BLOSC2_MAX_OVERHEAD);
  dest2 = blosc_test_malloc(BUFFER_ALIGN_SIZE, size);
  _src = (int32_t *)src;
  for (i=0; i < (int)(size/4); i++) {
    _src[i] = (int32_t)i;
  }
  memcpy(srccpy, src, size);

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


  return result != 0;
}
