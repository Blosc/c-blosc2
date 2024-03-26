/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2023  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/


#include "test_common.h"


const int64_t result_length = 2 * 2 * 2;
const uint8_t result[] = {0, 1,
                          2, 3,

                          4, 5,
                          6, 7};


CUTEST_TEST_SETUP(copy_buffer) {
  blosc2_init();
}

CUTEST_TEST_TEST(copy_buffer) {
  const int8_t ndim = 3;
  const uint8_t itemsize = sizeof(uint8_t);

  const int64_t chunk_shape[] = {3, 3, 1};

  const uint8_t chunk0x[] = {0, 0, 0,
                             0, 0, 2,
                             0, 4, 6};
  const int64_t chunk0s_start[] = {1, 1, 0};
  const int64_t chunk0s_stop[] = {3, 3, 1};
  const int64_t chunk0s_dest[] = {0, 0, 0};

  const uint8_t chunk1x[] = {1, 3, 0,
                             5, 7, 0,
                             0, 0, 0};
  const int64_t chunk1s_start[] = {0, 0, 0};
  const int64_t chunk1s_stop[] = {2, 2, 1};
  const int64_t chunk1s_dest[] = {0, 0, 1};

  uint8_t dest[] = {0, 0,
                    0, 0,

                    0, 0,
                    0, 0};
  const int64_t dest_shape[] = {2, 2, 2};

  B2ND_TEST_ASSERT(b2nd_copy_buffer(ndim, itemsize,
                                    chunk0x, chunk_shape, chunk0s_start, chunk0s_stop,
                                    dest, dest_shape, chunk0s_dest));
  B2ND_TEST_ASSERT(b2nd_copy_buffer(ndim, itemsize,
                                    chunk1x, chunk_shape, chunk1s_start, chunk1s_stop,
                                    dest, dest_shape, chunk1s_dest));

  for (int i = 0; i < result_length; ++i) {
    uint8_t a = dest[i];
    uint8_t b = result[i];
    CUTEST_ASSERT("Elements are not equal!", a == b);
  }

  return 0;
}

CUTEST_TEST_TEARDOWN(copy_buffer) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(copy_buffer);
}
