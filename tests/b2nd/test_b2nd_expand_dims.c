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
  bool axis[B2ND_MAX_DIM];
  int64_t start[B2ND_MAX_DIM];
  int64_t stop[B2ND_MAX_DIM];
  int8_t final_dims;
} test_shapes_t;


CUTEST_TEST_SETUP(expand_dims) {
  blosc2_init();

  // Add parametrizations
  CUTEST_PARAMETRIZE(typesize, uint8_t, CUTEST_DATA(
      1,
      2,
      4,
      8
  ));
  CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
      {false, false},
      {true, false},
      {true, true},
      {false, true},
  ));
  CUTEST_PARAMETRIZE(shapes, test_shapes_t, CUTEST_DATA(
      {0, {0}, {0}, {0}, {1}, {0}, {1}, 1}, // 0-dim
      {1, {10}, {7}, {2}, {1, 0, 1}, {0, 2, 0}, {1, 9, 1}, 3},
      {1, {10}, {7}, {2}, {1, 0, 1, 1}, {0, 2, 0, 0}, {1, 9, 1, 1}, 4},
      {2, {14, 10}, {8, 5}, {2, 2}, {1, 0, 0}, {0, 5, 3}, {1, 9, 10}, 3},
      {2, {14, 10}, {8, 5}, {2, 2}, {0, 1, 0, 1}, {2, 0, 3, 0}, {8, 1, 5, 1}, 4},
      {2, {14, 10}, {8, 5}, {2, 2}, {0, 1, 1, 0}, {2, 0, 0, 2}, {8, 1, 1, 7}, 4},
      {3, {10, 10, 10}, {3, 5, 9}, {3, 4, 4}, {0, 1, 0, 1, 0},{3, 0, 3, 0, 3}, {6, 1, 7, 1, 5}, 5},
      {3, {10, 10, 10}, {3, 5, 9}, {3, 4, 4}, {1, 0, 0, 0},{0, 3, 0, 3}, {1, 6, 7, 10}, 4},
  ));
}


CUTEST_TEST_TEST(expand_dims) {
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(shapes, test_shapes_t);
  CUTEST_GET_PARAMETER(typesize, uint8_t);

  char *urlpath = "test_expand_dims.b2nd";
  char *urlpath2 = "test_expand_dims2.b2nd";

  blosc2_remove_urlpath(urlpath);
  blosc2_remove_urlpath(urlpath2);

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
  size_t buffersize = typesize;
  for (int i = 0; i < ctx->ndim; ++i) {
    buffersize *= (size_t) ctx->shape[i];
  }
  uint8_t *buffer = malloc(buffersize);
  CUTEST_ASSERT("Buffer filled incorrectly", fill_buf(buffer, typesize, buffersize / typesize));

  /* Create b2nd_array_t with original data */
  b2nd_array_t *src;
  B2ND_TEST_ASSERT(b2nd_from_cbuffer(ctx, &src, buffer, buffersize));

  b2nd_array_t *dest;
  B2ND_TEST_ASSERT(b2nd_expand_dims(src, &dest, shapes.axis, shapes.final_dims));
  CUTEST_ASSERT("dims are equal", (dest)->ndim == shapes.final_dims);
  int j = 0;
  // check dimensions added correctly
  for (int i = 0; i < dest->ndim; ++i) {
    if (shapes.axis[i]) {
      CUTEST_ASSERT("Shape of new axis is not 1", dest->shape[i] == 1);
    }
    else {
      CUTEST_ASSERT("Shape of original axis is not equal", dest->shape[i] == src->shape[j]);
      j++;
    }
  }

  if (backend.contiguous) {
    // Convert to a frame and restore from it
    uint8_t *destbuffer;
    int64_t destbuffersize;
    bool needs_free;
    b2nd_array_t *dest2;
    B2ND_TEST_ASSERT(b2nd_to_cframe(dest, &destbuffer, &destbuffersize, &needs_free));
    B2ND_TEST_ASSERT(b2nd_from_cframe(destbuffer, destbuffersize, true, &dest2));
    if (needs_free) {
      free(destbuffer);
    }
    CUTEST_ASSERT("dims are equal", dest2->ndim == shapes.final_dims);
    B2ND_TEST_ASSERT(b2nd_free(dest2));
  }

  // Check copy of view has correct shape and same data as original
  blosc2_storage b2_storage2 = {.cparams=&cparams};
  if (backend.persistent) {
    b2_storage2.urlpath = urlpath2;
  }
  b2_storage2.contiguous = backend.contiguous;

  b2nd_context_t *ctx2 = b2nd_create_ctx(&b2_storage2, (dest)->ndim, (dest)->shape,
                                         (dest)->chunkshape, (dest)->blockshape, NULL, 0, NULL, 0);
  b2nd_array_t *dest2;
  b2nd_copy(ctx2, dest, &dest2);
  CUTEST_ASSERT("dims are not equal", (dest2)->ndim == (dest)->ndim);

  // Check that the copy is correct
  if (backend.contiguous) { // frame is not NULL
    uint8_t *destbuffer;
    int64_t destbuffersize;
    bool needs_free;
    CUTEST_ASSERT("View not copied with contiguous=true", (dest2)->sc->storage->contiguous);
    B2ND_TEST_ASSERT(b2nd_to_cframe(dest2, &destbuffer, &destbuffersize, &needs_free));
    b2nd_array_t *dest3;
    B2ND_TEST_ASSERT(b2nd_from_cframe(destbuffer, destbuffersize, true, &dest3));
    if (needs_free) {
      free(destbuffer);
    }
    CUTEST_ASSERT("dims are equal", dest3->ndim == (dest)->ndim);
    B2ND_TEST_ASSERT(b2nd_free(dest3));
  }
  else {
    // Either frame or data is not NULL for an array
    if (!backend.persistent){
    CUTEST_ASSERT("data of view has not been copied", *(dest2)->sc->data != *(dest)->sc->data); //compare pointers to data
      }
    else {
      // Check data of copy for persistent storage
      uint8_t *buffer_dest = malloc(buffersize);
      B2ND_TEST_ASSERT(b2nd_to_cbuffer(dest, buffer_dest, buffersize));
      uint8_t *buffer_dest2 = malloc(buffersize);
      B2ND_TEST_ASSERT(b2nd_to_cbuffer(dest2, buffer_dest2, buffersize));
      B2ND_TEST_ASSERT_BUFFER(buffer_dest, buffer_dest2, (int) buffersize);
      free(buffer_dest);
      free(buffer_dest2);
    }
  }

  // Check data
  uint8_t *buffer_dest = malloc(buffersize);
  B2ND_TEST_ASSERT(b2nd_to_cbuffer(dest, buffer_dest, buffersize));
  B2ND_TEST_ASSERT_BUFFER(buffer, buffer_dest, (int) buffersize);

  // Check that get_slice works for a view
  b2nd_array_t *slice_dest;

  ctx2->b2_storage->urlpath = NULL; // do not use urlpath for slice
  B2ND_TEST_ASSERT(b2nd_get_slice(ctx2, &slice_dest, dest, shapes.start, shapes.stop));
  b2nd_array_t *slice_dest2;
  B2ND_TEST_ASSERT(b2nd_get_slice(ctx2, &slice_dest2, dest2, shapes.start, shapes.stop));
  int64_t destbuffersize = typesize;
  for (int i = 0; i < dest->ndim; ++i) {
    destbuffersize *= (shapes.stop[i] - shapes.start[i]);
  }

  uint8_t *newbuffer_dest = malloc(destbuffersize);
  uint8_t *buffer_dest2 = malloc(destbuffersize);
  B2ND_TEST_ASSERT(b2nd_to_cbuffer(slice_dest2, buffer_dest2, destbuffersize));
  B2ND_TEST_ASSERT(b2nd_to_cbuffer(slice_dest, newbuffer_dest, destbuffersize));

  for (int i = 0; i < destbuffersize / typesize; ++i) {
    uint8_t a = newbuffer_dest[i];
    uint8_t b = buffer_dest2[i];
    CUTEST_ASSERT("Elements are not equal!", a == b);
  }

  free(buffer_dest);
  free(buffer_dest2);
  free(newbuffer_dest);
  free(buffer);
  B2ND_TEST_ASSERT(b2nd_free(src));
  B2ND_TEST_ASSERT(b2nd_free(dest));
  B2ND_TEST_ASSERT(b2nd_free(dest2));
  B2ND_TEST_ASSERT(b2nd_free(slice_dest));
  B2ND_TEST_ASSERT(b2nd_free(slice_dest2));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx2));


  blosc2_remove_urlpath(urlpath);
  blosc2_remove_urlpath(urlpath2);

  return BLOSC2_ERROR_SUCCESS;
}

CUTEST_TEST_TEARDOWN(expand_dims) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(expand_dims);
}

