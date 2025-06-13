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
} test_shapes_t;


CUTEST_TEST_SETUP(newaxis) {
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
  CUTEST_PARAMETRIZE(backend2, _test_backend, CUTEST_DATA(
      {false, false},
      // Persistent or contiguous is not implemented yet because B2ND metalayer increases in size,
      // and the new axis cannot be added to it without a data/trailer copy, which is expensive.
      // {true, false},
      // {true, true},
      // {false, true},
  ));


  CUTEST_PARAMETRIZE(shapes, test_shapes_t, CUTEST_DATA(
      {0, {0}, {0}, {0}, 0},
      {1, {10}, {7}, {2}, 0},
      {1, {10}, {7}, {2}, 1},
      {2, {14, 10}, {8, 5}, {2, 2}, 0},
      {2, {14, 10}, {8, 5}, {2, 2}, 1},
      {2, {14, 10}, {8, 5}, {2, 2}, 2},
      {3, {10, 10, 10}, {3, 5, 9}, {3, 4, 4}, 0},
      {3, {10, 10, 10}, {3, 5, 9}, {3, 4, 4}, 1},
      {3, {10, 10, 10}, {3, 5, 9}, {3, 4, 4}, 2},
      {3, {10, 10, 10}, {3, 5, 9}, {3, 4, 4}, 3},
  ));
}


CUTEST_TEST_TEST(newaxis) {
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(shapes, test_shapes_t);
  CUTEST_GET_PARAMETER(backend2, _test_backend);
  CUTEST_GET_PARAMETER(typesize, uint8_t);

  char *urlpath = "test_newaxis.b2nd";
  char *urlpath2 = "test_newaxis2.b2nd";

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

  /* Create storage for dest container */
  blosc2_storage b2_storage2 = {.cparams=&cparams};
  if (backend2.persistent) {
    b2_storage2.urlpath = urlpath2;
  }
  b2_storage2.contiguous = backend2.contiguous;
  b2nd_context_t *ctx2 = b2nd_create_ctx(&b2_storage2, shapes.ndim, shapes.shape,
                                         shapes.chunkshape, shapes.blockshape, NULL, 0, NULL, 0);

  b2nd_array_t *dest;
  B2ND_TEST_ASSERT(b2nd_copy(ctx2, src, &dest));
  B2ND_TEST_ASSERT(b2nd_newaxis(dest, shapes.axis));
  if (backend2.persistent) {
    b2nd_array_t *dest2;
    B2ND_TEST_ASSERT(b2nd_open(urlpath2, &dest2));
    CUTEST_ASSERT("dims are equal", dest2->ndim == src->ndim + 1);
    B2ND_TEST_ASSERT(b2nd_free(dest2));
  }
  if (backend2.contiguous) {
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
    CUTEST_ASSERT("dims are equal", dest2->ndim == src->ndim + 1);
    B2ND_TEST_ASSERT(b2nd_free(dest2));
  }

  CUTEST_ASSERT("dims are equal", dest->ndim == src->ndim + 1);

  // Check data
  uint8_t *buffer_dest = malloc(buffersize);
  B2ND_TEST_ASSERT(b2nd_to_cbuffer(dest, buffer_dest, buffersize));
  B2ND_TEST_ASSERT_BUFFER(buffer, buffer_dest, (int) buffersize);

  free(buffer_dest);
  free(buffer);
  B2ND_TEST_ASSERT(b2nd_free(src));
  B2ND_TEST_ASSERT(b2nd_free(dest));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx2));

  blosc2_remove_urlpath(urlpath);
  blosc2_remove_urlpath(urlpath2);

  return BLOSC2_ERROR_SUCCESS;
}

CUTEST_TEST_TEARDOWN(newaxis) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(newaxis);
}
