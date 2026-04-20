/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Regression test for integer overflow in _blosc_getitem.

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
**********************************************************************/

#include "test_common.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

int main(void) {
  blosc2_init();

  uint8_t chunk[BLOSC_MIN_HEADER_LENGTH];
  memset(chunk, 0, sizeof(chunk));

  chunk[0] = 2;
  chunk[1] = 1;
  chunk[2] = (uint8_t)(BLOSC_MEMCPYED | BLOSC_DOSHUFFLE);
  chunk[3] = 2;

  int32_t nbytes = INT32_MAX;
  int32_t blocksize = 16;
  int32_t cbytes = BLOSC_MIN_HEADER_LENGTH;
  memcpy(chunk + 4, &nbytes, sizeof(nbytes));
  memcpy(chunk + 8, &blocksize, sizeof(blocksize));
  memcpy(chunk + 12, &cbytes, sizeof(cbytes));

  int start = 0;
  int nitems = 1073741824;
  uint8_t dest[16] = {0};

  int rc = blosc2_getitem(chunk, BLOSC_MIN_HEADER_LENGTH, start, nitems, dest, (int32_t)sizeof(dest));

  blosc2_destroy();

  if (rc >= 0) {
    printf("Expected blosc2_getitem to fail for overflow-triggering arguments, got rc=%d\n", rc);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
