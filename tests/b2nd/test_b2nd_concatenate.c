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
  int8_t axis;
  int64_t shape1[B2ND_MAX_DIM];
  int32_t chunkshape1[B2ND_MAX_DIM];
  int32_t blockshape1[B2ND_MAX_DIM];
  int64_t shape2[B2ND_MAX_DIM];
  int32_t chunkshape2[B2ND_MAX_DIM];
  int32_t blockshape2[B2ND_MAX_DIM];
} test_shapes_t;


/**
 * Helper function for recursive region filling
 */
static int fill_recursive_region(uint8_t *buffer,
                                int64_t *strides,
                                int8_t ndim,
                                int64_t *start,
                                int64_t *stop,
                                const void *value,
                                uint8_t typesize,
                                int dim,
                                int64_t current_offset) {
  if (dim == ndim) {
    // We've reached the innermost dimension, copy the value
    memcpy(buffer + (current_offset * typesize), value, typesize);
    return 0;
  }

  // Iterate through the current dimension within the region
  for (int64_t i = start[dim]; i < stop[dim]; i++) {
    int64_t offset = current_offset + i * strides[dim];
    int err = fill_recursive_region(buffer, strides, ndim, start, stop,
                                   value, typesize, dim + 1, offset);
    if (err < 0) return err;
  }
  return 0;
}

/**
 * Helper function for recursive region filling with arange
 */
void increment_large_value(uint8_t *value, uint8_t typesize) {
  for (int i = typesize - 1; i >= 0; --i) {
    if (++value[i] != 0) {
      break;  // Si no hay desbordamiento, salimos
    }
  }
}
static int fill_recursive_arange(uint8_t *buffer,
                                int64_t *strides,
                                int8_t ndim,
                                int64_t *start,
                                int64_t *stop,
                                uint8_t *value,
                                uint8_t typesize,
                                int dim,
                                int64_t current_offset) {
  if (dim == ndim) {
    // We've reached the innermost dimension, copy the value
    memcpy(buffer + (current_offset * typesize), value, typesize);
    // Increment value for arange
    increment_large_value(value, typesize);
    return 0;
  }

  // Iterate through the current dimension within the region
  for (int64_t i = start[dim]; i < stop[dim]; i++) {
    int64_t offset = current_offset + i * strides[dim];
    int err = fill_recursive_arange(buffer, strides, ndim, start, stop,
                                    value, typesize, dim + 1, offset);
    if (err < 0) return err;
  }
  return 0;
}

/**
 * Fill a region of a multidimensional buffer with a constant value or linspace.
 */
int fill_buffer_region(uint8_t *buffer,
                       int64_t *buffer_shape,
                       int8_t ndim,
                       int64_t *start,
                       int64_t *stop,
                       uint8_t *value,
                       uint8_t typesize,
                       bool arange) {
  // Calculate strides for the buffer
  int64_t strides[B2ND_MAX_DIM];
  strides[ndim - 1] = 1;
  for (int i = ndim - 2; i >= 0; i--) {
    strides[i] = strides[i + 1] * buffer_shape[i + 1];
  }

  // Start the recursive filling
  if (arange) {
    for (int i = 0; i < typesize; ++i) {
      value[i] = 0;
    }
    // If arange is true, fill with increasing values
    return fill_recursive_arange(buffer, strides, ndim, start, stop,
                                 value, typesize, 0, 0);
  }
  else{
    return fill_recursive_region(buffer, strides, ndim, start, stop,
                                value, typesize, 0, 0);
  }
}


CUTEST_TEST_SETUP(concatenate) {
  blosc2_init();

  // Add parametrization
  CUTEST_PARAMETRIZE(typesize, uint8_t, CUTEST_DATA(
      1,
      2,
      // The following cases are skipped to reduce the execution time of the test suite
      // 4,
      // 8,
      13,
  ));
  CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
      {false, false},
      {true, false}, // contiguous = True, persistent = False
      {true, true},
      {false, true},
  ));
  CUTEST_PARAMETRIZE(shapes, test_shapes_t, CUTEST_DATA(
      // 0-dim is not supported in concatenate
      // {0, 0, {0}, {0}, {0}, {0}, {0}, {0}},
      // 1-dim
     {1, 0, {10}, {5}, {1}, {5}, {5}, {1}},
     {1, 0, {2}, {25}, {5}, {49}, {25}, {5}},
      // 2-dim
      {2, 0, {10, 10}, {2, 2}, {1, 1}, {4, 10}, {2, 2}, {1, 1}},
      {2, 1, {10, 8}, {2, 2}, {1, 1}, {10, 8}, {2, 2}, {1, 1}},
      {2, 0, {4, 4}, {4, 4}, {2, 2}, {4, 4}, {4, 4}, {2, 2}},
      {2, 1, {25, 50}, {25, 25}, {5, 5}, {25, 5}, {25, 25}, {5, 5}},
      // 3-dim
      {3, 0, {50, 5, 50}, {25, 13, 10}, {5, 8, 5}, {50, 5, 50}, {25, 13, 10}, {5, 8, 5}},
      {3, 1, {50, 5, 50}, {25, 13, 10}, {5, 8, 5}, {50, 5, 50}, {25, 13, 10}, {5, 8, 5}},
      {3, 2, {50, 5, 50}, {25, 13, 10}, {5, 8, 5}, {50, 5, 50}, {25, 13, 10}, {5, 8, 5}},
      {3, 0, {5, 5, 50}, {25, 13, 10}, {5, 8, 5}, {51, 5, 50}, {25, 13, 10}, {5, 8, 5}},
      // Inner 0-dims are supported
      {3, 1, {50, 1, 50}, {25, 13, 10}, {5, 8, 5}, {50, 0, 50}, {25, 13, 10}, {5, 8, 5}},
      {3, 2, {50, 50, 0}, {25, 13, 10}, {5, 8, 5}, {50, 50, 49}, {25, 13, 10}, {5, 8, 5}},
      {3, 2, {10, 10, 0}, {10, 10, 10}, {10, 10, 10}, {10, 10, 10},{10, 10, 10}, {10, 10, 10}},
      // 4-dim
      {4, 0, {5, 5, 5, 5}, {2, 5, 10, 5}, {5, 2, 5, 2}, {5, 5, 5, 5}, {5, 5, 10, 5}, {5, 2, 5, 2}},
      {4, 1, {5, 5, 5, 5}, {2, 5, 10, 5}, {5, 2, 5, 2}, {5, 5, 5, 5}, {5, 5, 10, 5}, {5, 2, 5, 2}},
      {4, 2, {5, 5, 5, 5}, {2, 13, 10, 5}, {5, 8, 5, 2}, {5, 5, 5, 5}, {5, 13, 10, 5}, {5, 8, 5, 2}},
      {4, 3, {5, 5, 5, 5}, {2, 13, 10, 5}, {5, 8, 5, 2}, {5, 5, 5, 5}, {5, 13, 10, 5}, {5, 8, 5, 2}},
      // The following cases are skipped to reduce the execution time of the test suite
      // {4, 0, {5, 5, 5, 5}, {2, 13, 10, 5}, {5, 8, 5, 2}, {6, 5, 5, 5}, {15, 13, 10, 5}, {5, 8, 5, 2}},
      // {4, 1, {5, 5, 5, 5}, {2, 13, 10, 5}, {5, 8, 5, 2}, {5, 6, 5, 5}, {15, 13, 10, 5}, {5, 8, 5, 2}},
      // {4, 2, {5, 5, 5, 5}, {2, 13, 10, 5}, {5, 8, 5, 2}, {5, 5, 6, 5}, {15, 13, 10, 5}, {5, 8, 5, 2}},
      // {4, 3, {5, 5, 5, 5}, {2, 13, 10, 5}, {5, 8, 5, 2}, {5, 5, 5, 6}, {15, 13, 10, 5}, {5, 8, 5, 2}},
  ));
  CUTEST_PARAMETRIZE(fill_value, int8_t, CUTEST_DATA(
      3,
      // The following cases are skipped to reduce the execution time of the test suite
      // -5,
      // 113,
      // 33,
  ));
  CUTEST_PARAMETRIZE(copy, bool, CUTEST_DATA(
      true,
      false,
  ));
  CUTEST_PARAMETRIZE(arange, bool, CUTEST_DATA(
      true,
      false,
  ));

}

CUTEST_TEST_TEST(concatenate) {
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(shapes, test_shapes_t);
  CUTEST_GET_PARAMETER(typesize, uint8_t);
  CUTEST_GET_PARAMETER(fill_value, int8_t);
  CUTEST_GET_PARAMETER(copy, bool);
  CUTEST_GET_PARAMETER(arange, bool);

  int axis = shapes.axis;
  char *urlpath = "test_concatenate.b2frame";
  char *urlpath1 = "test_concatenate1.b2frame";
  char *urlpath2 = "test_concatenate2.b2frame";
  blosc2_remove_urlpath(urlpath);
  blosc2_remove_urlpath(urlpath1);
  blosc2_remove_urlpath(urlpath2);

  // Create a helper buffer for storing the final array for the concatenation in C
  int64_t helpershape[B2ND_MAX_DIM] = {0};
  int64_t buffersize = typesize;
  for (int i = 0; i < shapes.ndim; ++i) {
    if (i == axis) {
      helpershape[i] = shapes.shape1[i] + shapes.shape2[i];
    }
    else {
      helpershape[i] = shapes.shape1[i];
    }
    buffersize *= helpershape[i];  // Multiply by each dimension
  }
  // Allocate a buffer for the concatenated array
  uint8_t *helperbuffer = malloc(buffersize);

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

  // Fill helperbuffer with zeros
  memset(helperbuffer, 0, buffersize);

  /* Create src2 with a value */
  b2nd_array_t *src2;

  blosc2_storage b2_storage2 = {.cparams=&cparams};
  if (backend.persistent) {
    b2_storage2.urlpath = urlpath2;
  }
  b2_storage2.contiguous = backend.contiguous;
  b2nd_context_t *ctx2 = b2nd_create_ctx(&b2_storage2, shapes.ndim, shapes.shape2,
                                         shapes.chunkshape2, shapes.blockshape2,
                                         NULL, 0, NULL, 0);

  uint8_t *value = malloc(typesize);
  // Fill a buffer with fill_value
  for (int i = 0; i < typesize; ++i) {
    value[i] = fill_value;
  }

  if (arange) {
    int64_t buffsize = typesize;
    for (int i = 0; i < shapes.ndim; ++i) {
      buffsize *= shapes.shape2[i];  // Multiply by each dimension
    }
    // Allocate a buffer for the src2 array
    uint8_t *buff = malloc(buffsize);
    int64_t start_[B2ND_MAX_DIM] = {0};

    fill_buffer_region(buff, shapes.shape2, shapes.ndim,
                     start_, shapes.shape2, value, typesize, arange); // value is not used in this case
    B2ND_TEST_ASSERT(b2nd_from_cbuffer(ctx2, &src2, buff, buffsize));

  }

  else {
    B2ND_TEST_ASSERT(b2nd_full(ctx2, &src2, value));
  }
  /* Concatenate src1 and src2 */
  b2nd_array_t *array = NULL;
  blosc2_storage b2_storage = {.cparams=&cparams};
  if (backend.persistent) {
    if (copy) {
      b2_storage.urlpath = urlpath;
    }
    else { // If copy is false, we use the urlpath of src1
      b2_storage.urlpath = urlpath1;
    }
  }
  b2_storage.contiguous = backend.contiguous;
  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, shapes.ndim, shapes.shape1,
                                        shapes.chunkshape1, shapes.blockshape1,
                                        NULL, 0, NULL, 0);
  B2ND_TEST_ASSERT(b2nd_concatenate(ctx, src1, src2, axis, copy, &array));

  // Fill the proper section of the helperbuffer with the value from src2
  int64_t start_src2[B2ND_MAX_DIM] = {0};
  int64_t stop_src2[B2ND_MAX_DIM];

  // Set up the region to fill (corresponding to src2's position)
  for (int i = 0; i < shapes.ndim; i++) {
    if (i == axis) {
      start_src2[i] = shapes.shape1[i];  // src2 starts after src1
      stop_src2[i] = shapes.shape1[i] + shapes.shape2[i];
    } else {
      start_src2[i] = 0;
      // Use the minimum of shape1 and shape2 for non-axis dimensions
      stop_src2[i] = (shapes.shape1[i] < shapes.shape2[i]) ?
                      shapes.shape1[i] : shapes.shape2[i];
    }
  }

  // Fill the region with the value
  fill_buffer_region(helperbuffer, helpershape, shapes.ndim,
                     start_src2, stop_src2, value, typesize, arange); // value is not used if arange is true

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

  int64_t start[B2ND_MAX_DIM] = {0};
  int64_t stop[B2ND_MAX_DIM] = {0};
  int64_t buffershape[B2ND_MAX_DIM] = {0};
  size_t elementcount = 1;  // Count of elements
  for (int i = 0; i < ctx->ndim; ++i) {
    start[i] = 0;
    stop[i] = array->shape[i];
    buffershape[i] = stop[i] - start[i];
    elementcount *= buffershape[i];
  }
  size_t buffersize2 = elementcount * typesize;
  uint8_t *buffer = malloc(buffersize2);

  B2ND_TEST_ASSERT(b2nd_get_slice_cbuffer(array, start, stop, buffer, buffershape, buffersize2));

  // Data in the concatenated array matches the helperbuffer?
  uint8_t *buffer_fill = malloc(typesize);
  for (int64_t i = 0; i < buffersize / typesize; ++i) {
    bool is_true = false;
    memcpy(buffer_fill, &buffer[i * typesize], typesize);
    is_true = memcmp(buffer_fill, helperbuffer + i * typesize, typesize) == 0;
    CUTEST_ASSERT("Data in the concatenated array does not match the helperbuffer", is_true);
  }

  /* Free mallocs */
  free(buffer_fill);
  free(buffer);
  free(helperbuffer);
  free(value);
  B2ND_TEST_ASSERT(b2nd_free(src1));
  B2ND_TEST_ASSERT(b2nd_free(src2));
  if (copy) {
    // If copy is true, we need to free the concatenated array
    B2ND_TEST_ASSERT(b2nd_free(array));
  }
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
