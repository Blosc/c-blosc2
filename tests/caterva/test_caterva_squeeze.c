/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"

typedef struct {
    int8_t ndim;
    int64_t shape[CATERVA_MAX_DIM];
    int32_t chunkshape[CATERVA_MAX_DIM];
    int32_t blockshape[CATERVA_MAX_DIM];
    int32_t chunkshape2[CATERVA_MAX_DIM];
    int32_t blockshape2[CATERVA_MAX_DIM];
    int64_t start[CATERVA_MAX_DIM];
    int64_t stop[CATERVA_MAX_DIM];
} test_shapes_t;


CUTEST_TEST_SETUP(squeeze) {
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


  CUTEST_PARAMETRIZE(shapes, test_shapes_t, CUTEST_DATA(
          {0, {0}, {0}, {0}, {0}, {0}, {0}, {0}}, // 0-dim
          {1, {10}, {7}, {2}, {1}, {1}, {2}, {3}}, // 1-idim
          {2, {14, 10}, {8, 5}, {2, 2}, {4, 1}, {2, 1}, {5, 3}, {9, 4}}, // general,
          {3, {10, 10, 10}, {3, 5, 9}, {3, 4, 4}, {1, 7, 1}, {1, 5, 1}, {3, 0, 9}, {4, 7, 10}},
          {2, {20, 0}, {7, 0}, {3, 0}, {1, 0}, {1, 0}, {1, 0}, {2, 0}}, // 0-shape
          {2, {20, 10}, {7, 5}, {3, 5}, {1, 0}, {1, 0}, {17, 0}, {18, 0}}, // 0-shape
  ));
}


CUTEST_TEST_TEST(squeeze) {
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(shapes, test_shapes_t);
  CUTEST_GET_PARAMETER(backend2, _test_backend);
  CUTEST_GET_PARAMETER(typesize, uint8_t);

  char *urlpath = "test_squeeze.b2frame";
  char *urlpath2 = "test_squeeze2.b2frame";

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

  caterva_context_t *ctx = caterva_create_ctx(&b2_storage, shapes.ndim, shapes.shape,
                                              shapes.chunkshape, shapes.blockshape, NULL, 0);

  /* Create original data */
  size_t buffersize = typesize;
  for (int i = 0; i < ctx->ndim; ++i) {
    buffersize *= (size_t) ctx->shape[i];
  }
  uint8_t *buffer = malloc(buffersize);
  CUTEST_ASSERT("Buffer filled incorrectly", fill_buf(buffer, typesize, buffersize / typesize));

  /* Create caterva_array_t with original data */
  caterva_array_t *src;
  CATERVA_TEST_ASSERT(caterva_from_buffer(ctx, &src, buffer, buffersize));


  /* Create storage for dest container */

  blosc2_storage b2_storage2 = {.cparams=&cparams};
  if (backend2.persistent) {
    b2_storage.urlpath = urlpath2;
  }
  b2_storage.contiguous = backend2.contiguous;

  // shape will then be overwritten
  caterva_context_t *params2 = caterva_create_ctx(&b2_storage2, shapes.ndim, shapes.shape,
                                                  shapes.chunkshape2, shapes.blockshape2, NULL, 0);

  caterva_array_t *dest;
  CATERVA_TEST_ASSERT(caterva_get_slice(params2, &dest, src, shapes.start, shapes.stop));

  CATERVA_TEST_ASSERT(caterva_squeeze(dest));

  if (ctx->ndim != 0) {
    CUTEST_ASSERT("dims are equal", src->ndim != dest->ndim);
  }

  free(buffer);
  CATERVA_TEST_ASSERT(caterva_free(&src));
  CATERVA_TEST_ASSERT(caterva_free(&dest));
  CATERVA_TEST_ASSERT(caterva_free_ctx(ctx));
  CATERVA_TEST_ASSERT(caterva_free_ctx(params2));
  blosc2_remove_urlpath(urlpath);
  blosc2_remove_urlpath(urlpath2);

  return CATERVA_SUCCEED;
}

CUTEST_TEST_TEARDOWN(squeeze) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(squeeze);
}
