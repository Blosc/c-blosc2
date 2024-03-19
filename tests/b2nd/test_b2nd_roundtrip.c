/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"


CUTEST_TEST_SETUP(roundtrip) {
  blosc2_init();

  // Add parametrizations
  b2nd_default_parameters();
}


CUTEST_TEST_TEST(roundtrip) {
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(shapes, _test_shapes);
  CUTEST_GET_PARAMETER(typesize, uint8_t);

  char *urlpath = "test_roundtrip.b2frame";
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
  size_t buffersize = (size_t) typesize;
  for (int i = 0; i < shapes.ndim; ++i) {
    buffersize *= (size_t) shapes.shape[i];
  }
  uint8_t *buffer = malloc(buffersize);
  CUTEST_ASSERT("Buffer filled incorrectly", fill_buf(buffer, typesize, buffersize / typesize));

  /* Create b2nd_array_t with original data */
  b2nd_array_t *src;
  B2ND_TEST_ASSERT(b2nd_from_cbuffer(ctx, &src, buffer, buffersize));

  /* Fill dest array with b2nd_array_t data */
  uint8_t *buffer_dest = malloc(buffersize);
  B2ND_TEST_ASSERT(b2nd_to_cbuffer(src, buffer_dest, buffersize));

  /* Testing */
  B2ND_TEST_ASSERT_BUFFER(buffer, buffer_dest, (int) buffersize);

  /* Free mallocs */
  free(buffer);
  free(buffer_dest);
  B2ND_TEST_ASSERT(b2nd_free(src));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));
  blosc2_remove_urlpath(urlpath);

  return BLOSC2_ERROR_SUCCESS;
}


CUTEST_TEST_TEARDOWN(roundtrip) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(roundtrip);
}
