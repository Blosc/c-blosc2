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
  int32_t chunkshape2[B2ND_MAX_DIM];
  int32_t blockshape2[B2ND_MAX_DIM];
  int64_t start[B2ND_MAX_DIM];
  int64_t stop[B2ND_MAX_DIM];
  bool squeeze_indexes[B2ND_MAX_DIM];
} test_squeeze_index_shapes_t;


CUTEST_TEST_SETUP(squeeze_index) {
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
      {true, false},
      {true, true},
      {false, true},
  ));


  CUTEST_PARAMETRIZE(shapes, test_squeeze_index_shapes_t, CUTEST_DATA(
      {0, {0}, {0}, {0}, {0}, {0},
       {0}, {0}, {0}}, // 0-dim
      {1, {10}, {7}, {2}, {1}, {1},
       {2}, {3}, {0}}, // 1-idim
      {2, {14, 10}, {8, 5}, {2, 2}, {4, 1}, {2, 1},
       {5, 3}, {9, 4}, {0, 1}}, // general,
      {3, {10, 10, 10}, {3, 5, 9}, {3, 4, 4}, {1, 7, 1}, {1, 5, 1},
       {3, 0, 9}, {4, 7, 10}, {1, 0, 0}},
      {2, {20, 0}, {7, 0}, {3, 0}, {1, 0}, {1, 0},
       {1, 0}, {2, 0}, {1, 0}}, // 0-shape
      {2, {20, 10}, {7, 5}, {3, 5}, {1, 0}, {1, 0},
       {17, 0}, {18, 0}, {1, 0}}, // 0-shape
      {4, {10, 7, 6, 4}, {7, 5, 1, 4}, {2, 2, 1, 2}, {1, 1, 5, 1}, {1, 1, 2, 1},
       {4, 4, 0, 4}, {5, 5, 10, 5}, {1, 0, 0, 1}} // general
  ));
}


CUTEST_TEST_TEST(squeeze_index) {
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(shapes, test_squeeze_index_shapes_t);
  CUTEST_GET_PARAMETER(backend2, _test_backend);
  CUTEST_GET_PARAMETER(typesize, uint8_t);

  char *urlpath = "test_squeeze_index.b2frame";
  char *urlpath2 = "test_squezze_index2.b2frame";

  blosc2_remove_urlpath(urlpath);
  blosc2_remove_urlpath(urlpath2);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 2;
  cparams.typesize = typesize;
  blosc2_storage b2_storage = {.cparams=&cparams};
  if (backend.persistent) {
    b2_storage.urlpath = urlpath;
  }
  b2_storage.contiguous = backend.persistent;

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
                                         shapes.chunkshape2, shapes.blockshape2, NULL, 0, NULL, 0);

  b2nd_array_t *dest;
  B2ND_TEST_ASSERT(b2nd_get_slice(ctx2, &dest, src, shapes.start, shapes.stop));

  B2ND_TEST_ASSERT(b2nd_squeeze_index(dest, shapes.squeeze_indexes));

  int8_t nsq = 0;
  for (int i = 0; i < ctx->ndim; ++i) {
    if (shapes.squeeze_indexes[i] == true) {
      nsq++;
    }
  }
  CUTEST_ASSERT("dims are not correct", src->ndim == dest->ndim + nsq);

  free(buffer);
  B2ND_TEST_ASSERT(b2nd_free(src));
  B2ND_TEST_ASSERT(b2nd_free(dest));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx2));

  blosc2_remove_urlpath(urlpath);
  blosc2_remove_urlpath(urlpath2);

  return 0;
}


CUTEST_TEST_TEARDOWN(squeeze_index) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(squeeze_index);
}
