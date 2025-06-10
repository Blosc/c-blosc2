/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Unit tests for the blosc1_decompress() function.

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdio.h>
#include "test_common.h"

/** Test the blosc1_decompress function. */
static char* test_empty_buffer(int clevel, int do_shuffle, int32_t typesize) {
  void* buf = NULL;
  int buf_size = 0;
  int csize, dsize;
  void* dest = malloc(0 + BLOSC2_MAX_OVERHEAD);

  csize = blosc2_compress(clevel, do_shuffle, typesize, buf, buf_size, dest, 0 + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: Compression error.", csize > 0);

  void* decomp = NULL;
  dsize = blosc1_decompress(dest, decomp, 0);
  free(dest);
  mu_assert("ERROR: in blosc1_decompress.", dsize >= 0);

  return EXIT_SUCCESS;
}


int main(void) {
  /* Initialize blosc before running tests. */
  blosc2_init();
  char* result = test_empty_buffer(3, BLOSC_NOSHUFFLE, 1);
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }

  /* Cleanup blosc resources. */
  blosc2_destroy();
  return result != EXIT_SUCCESS;

}
