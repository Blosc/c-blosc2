/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"
#include <math.h>

CUTEST_TEST_SETUP(nans) {
  blosc2_init();

  // Add parametrizations
  CUTEST_PARAMETRIZE(typesize, uint8_t, CUTEST_DATA(
      4, 8
  ));
  CUTEST_PARAMETRIZE(shapes, _test_shapes, CUTEST_DATA(
      {0, {0}, {0}, {0}}, // 0-dim
      {1, {5}, {3}, {2}}, // 1-idim
      {2, {20, 0}, {7, 0}, {3, 0}}, // 0-shape
      {2, {20, 10}, {7, 5}, {3, 5}}, // 0-shape
      {2, {14, 10}, {8, 5}, {2, 2}}, // general,
      {3, {12, 10, 14}, {3, 5, 9}, {3, 4, 4}}, // general
      {3, {10, 21, 30, 55}, {8, 7, 15, 3}, {5, 5, 10, 1}}, // general,
  ));
  CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
      {false, false},
      {true, false},
      {true, true},
      {false, true},
  ));
}


CUTEST_TEST_TEST(nans) {
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(shapes, _test_shapes);
  CUTEST_GET_PARAMETER(typesize, uint8_t);

  char *urlpath = "test_nans.b2frame";
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
  B2ND_TEST_ASSERT(b2nd_nans(ctx, &src));

  /* Fill dest array with b2nd_array_t data */
  uint8_t *buffer_dest = malloc(buffersize);
  B2ND_TEST_ASSERT(b2nd_to_cbuffer(src, buffer_dest, buffersize));

  /* Testing */
  for (int i = 0; i < buffersize / typesize; ++i) {
    bool is_true = false;
    switch (typesize) {
      case 8:
        is_true = isnan(((double *) buffer_dest)[i]);
        break;
      case 4:
        is_true = isnan(((float *) buffer_dest)[i]);
        break;
      default:
        break;
    }
    CUTEST_ASSERT("Elements are not equals", is_true);
  }

  /* Free mallocs */
  free(buffer_dest);
  B2ND_TEST_ASSERT(b2nd_free(src));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));
  blosc2_remove_urlpath(urlpath);

  return BLOSC2_ERROR_SUCCESS;
}


CUTEST_TEST_TEARDOWN(nans) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(nans);
}
