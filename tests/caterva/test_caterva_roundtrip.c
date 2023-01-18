/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"


CUTEST_TEST_DATA(roundtrip) {
    void *unused;
};


CUTEST_TEST_SETUP(roundtrip) {
  blosc2_init();

  // Add parametrizations
  caterva_default_parameters();
}


CUTEST_TEST_TEST(roundtrip) {
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(shapes, _test_shapes);
  CUTEST_GET_PARAMETER(typesize, uint8_t);

  char *urlpath = "test_roundtrip.b2frame";
  blosc2_remove_urlpath(urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 2;
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.typesize = typesize;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_storage b2_storage = {.cparams=&cparams, .dparams=&dparams};
  if (backend.persistent) {
    b2_storage.urlpath = urlpath;
  }
  b2_storage.contiguous = backend.contiguous;

  caterva_context_t *params = caterva_create_ctx(&b2_storage, shapes.ndim, shapes.shape,
                                                 shapes.chunkshape, shapes.blockshape, NULL, 0);

  /* Create original data */
  size_t buffersize = (size_t) typesize;
  for (int i = 0; i < shapes.ndim; ++i) {
    buffersize *= (size_t) shapes.shape[i];
  }
  uint8_t *buffer = malloc(buffersize);
  CUTEST_ASSERT("Buffer filled incorrectly", fill_buf(buffer, typesize, buffersize / typesize));

  /* Create caterva_array_t with original data */
  caterva_array_t *src;
  CATERVA_TEST_ASSERT(caterva_from_buffer(buffer, buffersize, params, &src));

  /* Fill dest array with caterva_array_t data */
  uint8_t *buffer_dest = malloc(buffersize);
  CATERVA_TEST_ASSERT(caterva_to_buffer(src, buffer_dest, buffersize));

  /* Testing */
  CATERVA_TEST_ASSERT_BUFFER(buffer, buffer_dest, (int) buffersize);

  /* Free mallocs */
  free(buffer);
  free(buffer_dest);
  CATERVA_TEST_ASSERT(caterva_free(&src));
  CATERVA_TEST_ASSERT(caterva_free_ctx(params));
  blosc2_remove_urlpath(urlpath);

  return CATERVA_SUCCEED;
}


CUTEST_TEST_TEARDOWN(roundtrip) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(roundtrip);
}
