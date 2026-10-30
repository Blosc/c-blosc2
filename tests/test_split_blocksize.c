/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Unit tests checking that a split block whose blocksize is not a
  multiple of typesize is rejected on decompression.

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"

int tests_run = 0;

/* Global vars */
#define TYPESIZE  4
#define NELEM     128               /* bytes per split */
#define BLOCKSIZE (TYPESIZE * NELEM + 3)
#define NBLOCKS   2
#define NBYTES    (BLOCKSIZE * NBLOCKS)
#define CBYTES    (BLOSC_MIN_HEADER_LENGTH + 4 * NBLOCKS + \
                   NBLOCKS * TYPESIZE * (4 + NELEM))

static uint8_t src[CBYTES];
static uint8_t dest[NBYTES];


static void write_le32(uint8_t *p, int32_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
}


/* Build a shuffled buffer whose blocks are split in TYPESIZE streams that
   are stored verbatim, but whose blocksize leaves 3 bytes no split covers. */
static void build_buffer(void) {
  uint8_t *p;
  int i, j;

  memset(src, 0, sizeof(src));
  src[0] = BLOSC1_VERSION_FORMAT;
  src[1] = BLOSC_BLOSCLZ_VERSION_FORMAT;
  src[2] = BLOSC_DOSHUFFLE;
  src[3] = TYPESIZE;
  write_le32(src + 4, NBYTES);
  write_le32(src + 8, BLOCKSIZE);
  write_le32(src + 12, CBYTES);
  p = src + BLOSC_MIN_HEADER_LENGTH + 4 * NBLOCKS;
  for (j = 0; j < NBLOCKS; j++) {
    write_le32(src + BLOSC_MIN_HEADER_LENGTH + 4 * j, (int32_t)(p - src));
    for (i = 0; i < TYPESIZE; i++) {
      write_le32(p, NELEM);
      p += 4;
      memset(p, 0x11, NELEM);
      p += NELEM;
    }
  }
}


static char *test_decompress(void) {
  int r = blosc1_decompress(src, dest, sizeof(dest));
  mu_assert("ERROR: blosc1_decompress accepted a partially covered block",
            r < 0);

  r = blosc2_decompress(src, sizeof(src), dest, sizeof(dest));
  mu_assert("ERROR: blosc2_decompress accepted a partially covered block",
            r < 0);
  return NULL;
}


static char *test_getitem(void) {
  int r = blosc1_getitem(src, 0, NBYTES / TYPESIZE, dest);
  mu_assert("ERROR: blosc1_getitem accepted a partially covered block", r < 0);

  r = blosc2_getitem(src, sizeof(src), 0, NBYTES / TYPESIZE, dest, sizeof(dest));
  mu_assert("ERROR: blosc2_getitem accepted a partially covered block", r < 0);
  return NULL;
}


static char *all_tests(void) {
  mu_run_test(test_decompress);
  mu_run_test(test_getitem);

  return NULL;
}


int main(void) {
  install_blosc_callback_test();
  blosc2_init();
  blosc2_set_nthreads(1);

  build_buffer();

  char *result = all_tests();
  if (result != NULL) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  blosc2_destroy();

  return result != NULL;
}
