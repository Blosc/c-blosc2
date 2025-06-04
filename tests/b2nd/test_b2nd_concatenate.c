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
 * Fill a region of a multidimensional buffer with a constant value.
 */
int fill_buffer_region(uint8_t *buffer,
                       int64_t *buffer_shape,
                       int8_t ndim,
                       int64_t *start,
                       int64_t *stop,
                       const void *value,
                       uint8_t typesize) {
  // Calculate strides for the buffer
  int64_t strides[B2ND_MAX_DIM];
  strides[ndim - 1] = 1;
  for (int i = ndim - 2; i >= 0; i--) {
    strides[i] = strides[i + 1] * buffer_shape[i + 1];
  }

  // Start the recursive filling
  return fill_recursive_region(buffer, strides, ndim, start, stop,
                              value, typesize, 0, 0);
}


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
  CUTEST_PARAMETRIZE(copy, bool, CUTEST_DATA(
      true,
      false,
  ));

}

CUTEST_TEST_TEST(concatenate) {
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(shapes, test_shapes_t);
  CUTEST_GET_PARAMETER(typesize, uint8_t);
  CUTEST_GET_PARAMETER(fill_value, int8_t);
  CUTEST_GET_PARAMETER(axis, int8_t);
  CUTEST_GET_PARAMETER(copy, bool);

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
                     start_src2, stop_src2, value, typesize);

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
  printf("Array shapes: %d x %d\n", (int)array->shape[0], (int)array->shape[1]);
  printf("Helperbuffer shapes: %d x %d\n", (int)helpershape[0], (int)helpershape[1]);
  printf("Axis: %d\n", axis);

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

  B2ND_TEST_ASSERT(b2nd_get_slice_cbuffer(array, start, stop, buffer, buffershape, buffersize));
  // Check if the data in the concatenated array matches the helperbuffer
  uint8_t *buffer_fill = malloc(typesize);
  for (int64_t i = 0; i < buffersize / typesize; ++i) {
    bool is_true = false;
    switch (typesize) {
      case 8:
        is_true = ((int64_t *) buffer)[i] == ((int64_t *) helperbuffer)[i];
        break;
      case 4:
        is_true = ((int32_t *) buffer)[i] == ((int32_t *) helperbuffer)[i];
        break;
      case 2:
        is_true = ((int16_t *) buffer)[i] == ((int16_t *) helperbuffer)[i];
        break;
      case 1:
        is_true = ((int8_t *) buffer)[i] == ((int8_t *) helperbuffer)[i];
        break;
      default:
        // For default case, don't copy helperbuffer over buffer data
        memcpy(buffer_fill, &buffer[i * typesize], typesize);
        is_true = memcmp(buffer_fill, helperbuffer + i * typesize, typesize) == 0;
        break;
    }
    if (!is_true) {
      // Print the raw byte values for better debugging
      fprintf(stderr, "Data mismatch at index %d: buffer bytes = ", (int)i);
      for (int b = 0; b < typesize; b++) {
        fprintf(stderr, "%02x ", buffer[i * typesize + b]);
      }
      fprintf(stderr, ", helperbuffer bytes = ");
      for (int b = 0; b < typesize; b++) {
        fprintf(stderr, "%02x ", helperbuffer[i * typesize + b]);
      }
      fprintf(stderr, "\n");
    }
    CUTEST_ASSERT("Data in the concatenated array does not match the helperbuffer",
                  is_true);
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
