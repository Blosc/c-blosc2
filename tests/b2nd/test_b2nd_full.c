/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/


#include "test_common.h"


CUTEST_TEST_SETUP(full) {
  blosc2_init();

  // Add parametrizations
  CUTEST_PARAMETRIZE(typesize, int32_t, CUTEST_DATA(
      1, 2, 4, 8, 16,
      255, 256, 257,  // useful to testing typesizes equal or larger than 255
      // 256 * 256, // this should work for all shapes as well, but is too slow
      //  256*256*256, 256*256*256*8,  // this should for work small shapes, but is way sloooower
  ));
  CUTEST_PARAMETRIZE(shapes, _test_shapes, CUTEST_DATA(
    {0, {0}, {0}, {0}}, // 0-dim
    {1, {5}, {3}, {2}}, // 1-idim
    {2, {20, 0}, {7, 0}, {3, 0}}, // 0-shape
    {2, {20, 10}, {20, 10}, {10, 10}}, // simple 2-dim
    {2, {20, 10}, {10, 5}, {10, 5}}, // non-contiguous
    {2, {4, 1}, {2, 1}, {2, 1}},
    {2, {1, 3}, {1, 2}, {1, 2}},
    {2, {20, 10}, {8, 6}, {7, 5}},
    {2, {20, 10}, {7, 5}, {3, 5}},
    {2, {14, 10}, {8, 5}, {2, 2}},
    {3, {12, 10, 14}, {3, 5, 9}, {3, 4, 4}}, // 3-dim
    {4, {10, 21, 20, 5}, {8, 7, 15, 3}, {5, 5, 10, 1}}, // 4-dim
  ));
  CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
      {false, false},
      {true, false},
      {true, true},
      {false, true},
  ));
  CUTEST_PARAMETRIZE(fill_value, int8_t, CUTEST_DATA(
      3, 113, 33, -5
  ));
}


CUTEST_TEST_TEST(full) {
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(shapes, _test_shapes);
  CUTEST_GET_PARAMETER(typesize, int32_t);
  CUTEST_GET_PARAMETER(fill_value, int8_t);

  char *urlpath = "test_full.b2frame";
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

  /* Create original data */
  int64_t buffersize = typesize;
  for (int i = 0; i < shapes.ndim; ++i) {
    buffersize *= shapes.shape[i];
  }

  /* Create b2nd_array_t with original data */
  b2nd_array_t *src;
  uint8_t *value = malloc(typesize);
  switch (typesize) {
    case 8:
      ((int64_t *) value)[0] = (int64_t) fill_value;
      break;
    case 4:
      ((int32_t *) value)[0] = (int32_t) fill_value;
      break;
    case 2:
      ((int16_t *) value)[0] = (int16_t) fill_value;
      break;
    case 1:
      ((int8_t *) value)[0] = fill_value;
      break;
    default:
      // Fill a buffer with fill_value
      for (int i = 0; i < typesize; ++i) {
        value[i] = fill_value;
      }
      break;
  }

  B2ND_TEST_ASSERT(b2nd_full(ctx, &src, value));

  /* Fill dest array with b2nd_array_t data */
  uint8_t *buffer_dest = malloc(buffersize);
  B2ND_TEST_ASSERT(b2nd_to_cbuffer(src, buffer_dest, buffersize));

  /* Testing */
  for (int i = 0; i < buffersize / typesize; ++i) {
    uint8_t *buffer_fill = malloc(typesize);
    bool is_true = false;
    switch (typesize) {
      case 8:
        is_true = ((int64_t *) buffer_dest)[i] == fill_value;
        break;
      case 4:
        is_true = ((int32_t *) buffer_dest)[i] == fill_value;
        break;
      case 2:
        is_true = ((int16_t *) buffer_dest)[i] == fill_value;
        break;
      case 1:
        is_true = ((int8_t *) buffer_dest)[i] == fill_value;
        break;
      default:
        // Fill a buffer with fill_value and compare with buffer_dest
        for (int32_t j = 0; j < typesize; ++j) {
          buffer_fill[j] = fill_value;
        }
        // Compare buffer_fill with buffer_dest
        is_true = memcmp(buffer_fill, buffer_dest, typesize) == 0;
        // print the N first bytes of buffer_fill and buffer_dest
        // printf("buffer_fill: ");
        // for (int j = 0; j < 3; ++j) {
        //   printf("%d vs %d ", buffer_fill[j], buffer_dest[j]);
        // }
        free(buffer_fill);
        break;
    }
    CUTEST_ASSERT("Elements are not equal", is_true);
  }

  /* Free mallocs */
  free(buffer_dest);
  free(value);
  B2ND_TEST_ASSERT(b2nd_free(src));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));

  blosc2_remove_urlpath(urlpath);

  return BLOSC2_ERROR_SUCCESS;
}


CUTEST_TEST_TEARDOWN(full) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(full);
}
