/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Unit test: a Blosc1 (format version 2) chunk that was bit-shuffled and
  has leftover bytes (a block size that is not a multiple of the typesize)
  must be decoded completely.

  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"

int tests_run = 0;

/* Produced by C-Blosc 1.21 (blosc_compress) with typesize 2, lz4, BITSHUFFLE,
 * from the 129 bytes 0, 1, ..., 128: 64 elements and 1 leftover byte. */
static const uint8_t chunk[] = {
  0x02, 0x01, 0x34, 0x02, 0x81, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x6c, 0x00, 0x00, 0x00,
  0x18, 0x00, 0x00, 0x00, 0x67, 0x00, 0x00, 0x00, 0x4b, 0x00, 0x00, 0x00, 0x13, 0x00, 0x01, 0x00,
  0x13, 0xaa, 0x01, 0x00, 0x13, 0xcc, 0x01, 0x00, 0x13, 0xf0, 0x01, 0x00, 0x23, 0x00, 0xff, 0x02,
  0x00, 0x23, 0x00, 0xff, 0x04, 0x00, 0x40, 0x00, 0x00, 0xff, 0xff, 0x0c, 0x00, 0x02, 0x02, 0x00,
  0x00, 0x0c, 0x00, 0x00, 0x02, 0x00, 0x03, 0x3f, 0x00, 0x13, 0xaa, 0x3f, 0x00, 0x13, 0xcc, 0x3f,
  0x00, 0x13, 0xf0, 0x3e, 0x00, 0x03, 0x3c, 0x00, 0x02, 0x38, 0x00, 0x00, 0x30, 0x00, 0x80, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x80
};

static char *test_blosc1_bitshuffle_leftover(void) {
  uint8_t expected[129];
  uint8_t dest[129];
  for (int i = 0; i < 129; i++) {
    expected[i] = (uint8_t)i;
  }
  mu_assert("ERROR: not a Blosc1 chunk", chunk[0] == BLOSC1_VERSION_FORMAT);

  int nbytes = blosc1_decompress(chunk, dest, sizeof(dest));
  mu_assert("ERROR: wrong number of decompressed bytes", nbytes == (int)sizeof(dest));
  mu_assert("ERROR: leftover byte was not restored", dest[128] == expected[128]);
  mu_assert("ERROR: bad roundtrip", memcmp(dest, expected, sizeof(dest)) == 0);
  return 0;
}

static char *all_tests(void) {
  mu_run_test(test_blosc1_bitshuffle_leftover);
  return 0;
}

int main(void) {
  blosc2_init();
  char *result = all_tests();
  if (result != 0) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED\n");
  }
  printf("\tTests run: %d\n", tests_run);
  blosc2_destroy();
  return result != 0;
}
