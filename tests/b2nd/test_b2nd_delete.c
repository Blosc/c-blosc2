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
  int8_t axis;
  int64_t start;
  int64_t delete_len;
} test_shapes_t;


CUTEST_TEST_SETUP(delete) {
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
      {1, {10}, {3}, {2}, 0, 5, 5}, // delete the end
      {2, {18, 12}, {6, 6}, {3, 3}, 1, 0, 6}, // delete at the beginning
      {3, {12, 10, 27}, {3, 5, 9}, {3, 4, 4}, 2, 9, 9}, // delete in the middle
      {4, {10, 10, 5, 30}, {5, 7, 3, 3}, {2, 2, 1, 1}, 3, 12, 9}, // delete in the middle

  ));
}

CUTEST_TEST_TEST(delete) {
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(shapes, test_shapes_t);
  CUTEST_GET_PARAMETER(typesize, uint8_t);

  char *urlpath = "test_delete.b2frame";
  blosc2_remove_urlpath(urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 2;
  cparams.compcode = BLOSC_LZ4;
  cparams.typesize = typesize;
  blosc2_storage b2_storage = {.cparams=&cparams};
  if (backend.persistent) {
    b2_storage.urlpath = urlpath;
  }
  b2_storage.contiguous = backend.contiguous;

  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, shapes.ndim, shapes.shape,
                                        shapes.chunkshape, shapes.blockshape, NULL, 0, NULL, 0);

  /* Create b2nd_array_t with original data */
  b2nd_array_t *src;
  uint8_t *value = malloc(typesize);
  int8_t fill_value = 1;
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
      break;
  }
  BLOSC_ERROR(b2nd_full(ctx, &src, value));

  int64_t bufferlen = 1;
  int64_t stop[B2ND_MAX_DIM];
  int64_t buffer_shape[B2ND_MAX_DIM];
  for (int i = 0; i < ctx->ndim; ++i) {
    if (i != shapes.axis) {
      bufferlen *= shapes.shape[i];
      stop[i] = shapes.shape[i];
      buffer_shape[i] = shapes.shape[i];
    } else {
      bufferlen *= shapes.delete_len;
      stop[i] = shapes.start + shapes.delete_len;
      buffer_shape[i] = shapes.delete_len;
    }
  }
  // Set future deleted values to 0
  int64_t start[B2ND_MAX_DIM] = {0};
  start[shapes.axis] = shapes.start;
  uint8_t *buffer = calloc((size_t) bufferlen, (size_t) typesize);
  BLOSC_ERROR(b2nd_set_slice_cbuffer(buffer, buffer_shape, bufferlen * typesize, start, stop, src));

  BLOSC_ERROR(b2nd_delete(src, shapes.axis, shapes.start, shapes.delete_len));

  int64_t newshape[B2ND_MAX_DIM] = {0};
  for (int i = 0; i < shapes.ndim; ++i) {
    newshape[i] = shapes.shape[i];
  }
  newshape[shapes.axis] -= shapes.delete_len;

  // Create aux array to compare values
  b2nd_array_t *aux;
  b2_storage.urlpath = NULL;
  b2_storage.contiguous = backend.contiguous;
  b2nd_context_t *aux_ctx = b2nd_create_ctx(&b2_storage, shapes.ndim, newshape,
                                            shapes.chunkshape, shapes.blockshape, NULL, 0, NULL, 0);

  BLOSC_ERROR(b2nd_full(aux_ctx, &aux, value));

  /* Fill buffer with whole array data */
  uint8_t *src_buffer = malloc((size_t) (src->nitems * typesize));
  B2ND_TEST_ASSERT(b2nd_to_cbuffer(src, src_buffer, src->nitems * typesize));

  for (uint64_t i = 0; i < (uint64_t) src->nitems; ++i) {
    switch (typesize) {
      case 8:
        CUTEST_ASSERT("Elements are not equal!",
                      ((uint64_t *) src_buffer)[i] == (uint64_t) fill_value);
        break;
      case 4:
        CUTEST_ASSERT("Elements are not equal!",
                      ((uint32_t *) src_buffer)[i] == (uint32_t) fill_value);
        break;
      case 2:
        CUTEST_ASSERT("Elements are not equal!",
                      ((uint16_t *) src_buffer)[i] == (uint16_t) fill_value);
        break;
      case 1:
        CUTEST_ASSERT("Elements are not equal!",
                      ((uint8_t *) src_buffer)[i] == (uint8_t) fill_value);
        break;
      default:
        B2ND_TEST_ASSERT(BLOSC2_ERROR_INVALID_PARAM);
    }
  }
  /* Free mallocs */
  free(value);
  free(buffer);
  free(src_buffer);

  B2ND_TEST_ASSERT(b2nd_free(src));
  B2ND_TEST_ASSERT(b2nd_free(aux));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));
  B2ND_TEST_ASSERT(b2nd_free_ctx(aux_ctx));

  blosc2_remove_urlpath(urlpath);

  return 0;
}

CUTEST_TEST_TEARDOWN(delete) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(delete);
}
