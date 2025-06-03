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
    int64_t shape1[B2ND_MAX_DIM];
    int32_t chunkshape1[B2ND_MAX_DIM];
    int32_t blockshape1[B2ND_MAX_DIM];
    int64_t shape2[B2ND_MAX_DIM];
    int32_t chunkshape2[B2ND_MAX_DIM];
    int32_t blockshape2[B2ND_MAX_DIM];
} test_shapes_t;


CUTEST_TEST_SETUP(concatenate) {
  blosc2_init();

  // Add parametrization
  CUTEST_PARAMETRIZE(typesize, uint8_t, CUTEST_DATA(
      1,
//      2,
//      4,
//      8,
  ));

  CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
      {false, false},
//      {true, false},
//      {true, true},
//      {false, true},
  ));


  CUTEST_PARAMETRIZE(shapes, test_shapes_t, CUTEST_DATA(
      // {0, {0}, {0}, {0}, {0}, {0}}, // 0-dim
      // {1, {5}, {3}, {2}, {2}, {5}}, // 1-idim
      // {2, {20, 0}, {7, 0}, {3, 0}, {2, 0}, {8, 0}}, // 0-shape
      // {2, {20, 10}, {7, 5}, {3, 5}, {2, 0}, {18, 0}}, // 0-shape
      // {2, {14, 10}, {8, 5}, {2, 2}, {5, 3}, {9, 10}},
      // {3, {12, 10, 14}, {3, 5, 9}, {3, 4, 4}, {3, 0, 3}, {6, 7, 10}},
      // {4, {10, 21, 30, 5}, {8, 7, 15, 3}, {5, 5, 10, 1}, {5, 4, 3, 3}, {10, 8, 8, 4}},
      {2, {50, 50}, {25, 13}, {5, 8}, {50, 50}, {25, 13}, {5, 8}},
      // {2, {150, 45}, {15, 15}, {7, 7}, {4, 2}, {6, 5}},
      // {2, {10, 10}, {5, 7}, {2, 2}, {0, 0}, {5, 5}},
      // // Checks for fast path in setting a single chunk that is C contiguous
      // {2, {20, 20}, {10, 10}, {5, 10}, {10, 10}, {20, 20}},
      // {3, {3, 4, 5}, {1, 4, 5}, {1, 2, 5}, {1, 0, 0}, {2, 4, 5}},
      // {3, {3, 8, 5}, {1, 4, 5}, {1, 2, 5}, {1, 4, 0}, {2, 8, 5}},
  ));
  CUTEST_PARAMETRIZE(fill_value, int8_t, CUTEST_DATA(
      3,
//    113,
//    33,
//    -5
  ));
  CUTEST_PARAMETRIZE(axis, int8_t, CUTEST_DATA(
      0,
      1,
  ));

}

CUTEST_TEST_TEST(concatenate) {
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(shapes, test_shapes_t);
  CUTEST_GET_PARAMETER(typesize, uint8_t);
  CUTEST_GET_PARAMETER(fill_value, int8_t);
  CUTEST_GET_PARAMETER(axis, int8_t);

  char *urlpath = "test_concatenate.b2frame";
  char *urlpath1 = "test_concatenate1.b2frame";
  char *urlpath2 = "test_concatenate2.b2frame";
  blosc2_remove_urlpath(urlpath);
  blosc2_remove_urlpath(urlpath1);
  blosc2_remove_urlpath(urlpath2);

  // Create a helper buffer for storing the final array for the concatenation in C
  int64_t helpershape[B2ND_MAX_DIM] = {0};
  size_t buffersize = typesize;
  for (int i = 0; i < shapes.ndim; ++i) {
    if (i == axis) {
      buffersize *= (size_t) (shapes.shape1[i] + shapes.shape2[i]);
    }
    else {
      buffersize *= (size_t) shapes.shape1[i];
    }
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 2;
  cparams.typesize = typesize;
  blosc2_storage b2_storage1 = {.cparams=&cparams};
  if (backend.persistent) {
    b2_storage1.urlpath = urlpath1;
  }
  b2_storage1.contiguous = backend.contiguous;
  b2nd_context_t *ctx1 = b2nd_create_ctx(&b2_storage1, shapes.ndim, shapes.shape1,
                                        shapes.chunkshape1, shapes.blockshape1, NULL,
                                        0, NULL, 0);


  /* Create src1 with zeros */
  b2nd_array_t *src1;
  BLOSC_ERROR(b2nd_zeros(ctx1, &src1));

  /* Create src2 with a value */
  b2nd_array_t *src2;
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
  blosc2_storage b2_storage2 = {.cparams=&cparams};
  if (backend.persistent) {
    b2_storage2.urlpath = urlpath2;
  }
  b2_storage2.contiguous = backend.contiguous;
  b2nd_context_t *ctx2 = b2nd_create_ctx(&b2_storage2, shapes.ndim, shapes.shape2,
                                         shapes.chunkshape2, shapes.blockshape2,
                                         NULL, 0, NULL, 0);
  B2ND_TEST_ASSERT(b2nd_full(ctx2, &src2, value));

  /* Concatenate src1 and src2 */
  b2nd_array_t *array = NULL;
  blosc2_storage b2_storage = {.cparams=&cparams};
  if (backend.persistent) {
    b2_storage.urlpath = urlpath;
  }
  b2_storage2.contiguous = backend.contiguous;
  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, shapes.ndim, shapes.shape1,
                                        shapes.chunkshape1, shapes.blockshape1,
                                        NULL, 0, NULL, 0);
  B2ND_TEST_ASSERT(b2nd_concatenate(ctx, src1, src2, &array, axis));

  // Check the shape of the concatenated array
  for (int i = 0; i < ctx->ndim; ++i) {
    if (i == axis) {
      CUTEST_ASSERT("Shape is not equal!",
                    array->shape[i] == shapes.shape1[i] + shapes.shape2[i]);
    }
    else {
      CUTEST_ASSERT("Shape is not equal!",
                    array->shape[i] == shapes.shape1[i]);
    }
  }

  // Check the chunkshape of the concatenated array
  for (int i = 0; i < ctx->ndim; ++i) {
    CUTEST_ASSERT("Chunkshape is not equal!", array->chunkshape[i] == shapes.chunkshape1[i]);
  }

  // Check the data in the concatenated array
  int64_t start[B2ND_MAX_DIM] = {0};
  int64_t stop[B2ND_MAX_DIM] = {0};
  int64_t buffershape[B2ND_MAX_DIM] = {0};
  for (int i = 0; i < ctx->ndim; ++i) {
    start[i] = 0;
    stop[i] = array->shape[i];
    buffershape[i] = stop[i] - start[i];
    buffersize *= buffershape[i];
  }
  uint8_t *buffer = malloc(buffersize);
  B2ND_TEST_ASSERT(b2nd_get_slice_cbuffer(array, start, stop, buffer, buffershape, buffersize));
  for (int64_t i = 0; i < buffersize / typesize; ++i) {
    switch (typesize) {
    case 8:
      B2ND_TEST_ASSERT(((int64_t *) buffer)[i] == (i + 1));
      break;
    case 4:
      B2ND_TEST_ASSERT(((int32_t *) buffer)[i] == (i + 1));
      break;
    case 2:
      B2ND_TEST_ASSERT(((int16_t *) buffer)[i] == (i + 1));
      break;
    case 1:
      printf("Checking value at index %lld: %d\n", i, ((int8_t *) buffer)[i]);
        CUTEST_ASSERT("Value is not equal!", ((int8_t *) buffer)[i] == 0);
      break;
    default:
      // Check the value in the buffer
      for (int j = 0; j < typesize; ++j) {
        B2ND_TEST_ASSERT(buffer[i * typesize + j] == value[j]);
      }
      break;
    }
  }

  /* Free mallocs */
  // free(buffer);
  // free(destbuffer);
  free(value);
  B2ND_TEST_ASSERT(b2nd_free(src1));
  B2ND_TEST_ASSERT(b2nd_free(src2));
  B2ND_TEST_ASSERT(b2nd_free(array));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx1));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx2));
  blosc2_remove_urlpath(urlpath);
  blosc2_remove_urlpath(urlpath1);
  blosc2_remove_urlpath(urlpath2);

  return 0;
}

CUTEST_TEST_TEARDOWN(concatenate) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(concatenate);
}
