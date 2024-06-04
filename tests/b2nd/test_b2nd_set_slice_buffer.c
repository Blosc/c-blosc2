/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"

typedef struct {
  int8_t ndim;
  int64_t shape[B2ND_MAX_DIM];
  int32_t chunkshape[B2ND_MAX_DIM];
  int32_t blockshape[B2ND_MAX_DIM];
  int64_t start[B2ND_MAX_DIM];
  int64_t stop[B2ND_MAX_DIM];
} test_shapes_t;


CUTEST_TEST_SETUP(set_slice_buffer) {
  blosc2_init();

  // Add parametrizations
  CUTEST_PARAMETRIZE(typesize, uint8_t, CUTEST_DATA(
      1,
      2,
      4,
      8,
  ));

  CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
      {false, false},
      {true, false},
      {true, true},
      {false, true},
  ));


  CUTEST_PARAMETRIZE(shapes, test_shapes_t, CUTEST_DATA(
      {0, {0}, {0}, {0}, {0}, {0}}, // 0-dim
      {1, {5}, {3}, {2}, {2}, {5}}, // 1-idim
      {2, {20, 0}, {7, 0}, {3, 0}, {2, 0}, {8, 0}}, // 0-shape
      {2, {20, 10}, {7, 5}, {3, 5}, {2, 0}, {18, 0}}, // 0-shape
      {2, {14, 10}, {8, 5}, {2, 2}, {5, 3}, {9, 10}},
      {3, {12, 10, 14}, {3, 5, 9}, {3, 4, 4}, {3, 0, 3}, {6, 7, 10}},
      {4, {10, 21, 30, 5}, {8, 7, 15, 3}, {5, 5, 10, 1}, {5, 4, 3, 3}, {10, 8, 8, 4}},
      {2, {50, 50}, {25, 13}, {8, 8}, {0, 0}, {10, 10}},
      // The case below makes qemu-aarch64 (AARCH64 emulation) in CI (Ubuntu 22.04) to crash with a segfault.
      // Interestingly, this works perfectly well on both intel64 (native) and in aarch64 (emulated via docker).
      // Moreover, valgrind does not issue any warning at all when run in the later platforms.
      // In conclusion, this *may* be revealing a bug in the qemu-aarch64 binaries in Ubuntu 22.04.
      // {2, {143, 41}, {18, 13}, {7, 7}, {4, 2}, {6, 5}},
      // Replacing the above line by this one makes qemu-aarch64 happy.
      {2, {150, 45}, {15, 15}, {7, 7}, {4, 2}, {6, 5}},
      {2, {10, 10}, {5, 7}, {2, 2}, {0, 0}, {5, 5}},
      // Checks for fast path in setting a single chunk that is C contiguous
      {2, {20, 20}, {10, 10}, {5, 10}, {10, 10}, {20, 20}},
      {3, {3, 4, 5}, {1, 4, 5}, {1, 2, 5}, {1, 0, 0}, {2, 4, 5}},
      {3, {3, 8, 5}, {1, 4, 5}, {1, 2, 5}, {1, 4, 0}, {2, 8, 5}},

  ));
}

CUTEST_TEST_TEST(set_slice_buffer) {
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(shapes, test_shapes_t);
  CUTEST_GET_PARAMETER(typesize, uint8_t);

  char *urlpath = "test_set_slice_buffer.b2frame";
  blosc2_remove_urlpath(urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 2;
  cparams.typesize = typesize;
  blosc2_storage b2_storage = {.cparams=&cparams};
  if (backend.persistent) {
    b2_storage.urlpath = urlpath;
  }
  b2_storage.contiguous = backend.contiguous;

  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, shapes.ndim, shapes.shape,
                                        shapes.chunkshape, shapes.blockshape, NULL, 0, NULL, 0);

  /* Create dest buffer */
  int64_t shape[B2ND_MAX_DIM] = {0};
  int64_t buffersize = typesize;
  for (int i = 0; i < ctx->ndim; ++i) {
    shape[i] = shapes.stop[i] - shapes.start[i];
    buffersize *= shape[i];
  }

  uint8_t *buffer = malloc(buffersize);
  CUTEST_ASSERT("Buffer filled incorrectly", fill_buf(buffer, typesize, buffersize / typesize));

  /* Create b2nd_array_t with original data */
  b2nd_array_t *src;
  BLOSC_ERROR(b2nd_zeros(ctx, &src));


  BLOSC_ERROR(b2nd_set_slice_cbuffer(buffer, shape, buffersize,
                                     shapes.start, shapes.stop, src));


  uint8_t *destbuffer = malloc((size_t) buffersize);

  /* Fill dest buffer with a slice*/
  B2ND_TEST_ASSERT(b2nd_get_slice_cbuffer(src, shapes.start, shapes.stop,
                                          destbuffer,
                                          shape, buffersize));

  for (uint64_t i = 0; i < (uint64_t) buffersize / typesize; ++i) {
    uint64_t k = i + 1;
    switch (typesize) {
      case 8:
        CUTEST_ASSERT("Elements are not equals!",
                      (uint64_t) k == ((uint64_t *) destbuffer)[i]);
        break;
      case 4:
        CUTEST_ASSERT("Elements are not equals!",
                      (uint32_t) k == ((uint32_t *) destbuffer)[i]);
        break;
      case 2:
        CUTEST_ASSERT("Elements are not equals!",
                      (uint16_t) k == ((uint16_t *) destbuffer)[i]);
        break;
      case 1:
        CUTEST_ASSERT("Elements are not equals!",
                      (uint8_t) k == ((uint8_t *) destbuffer)[i]);
        break;
      default:
        B2ND_TEST_ASSERT(BLOSC2_ERROR_INVALID_PARAM);
    }
  }

  /* Free mallocs */
  free(buffer);
  free(destbuffer);
  B2ND_TEST_ASSERT(b2nd_free(src));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));
  blosc2_remove_urlpath(urlpath);

  return 0;
}

CUTEST_TEST_TEARDOWN(set_slice_buffer) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(set_slice_buffer);
}
