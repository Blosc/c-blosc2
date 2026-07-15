/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* Regression test for get_slice with typesize > BLOSC_MAX_TYPESIZE (255).
   Such chunks are compressed with an internal typesize of 1, and the
   compact get path (which reads single blocks via blosc2_getitem_ctx)
   used to pass item counts in array-typesize units, returning truncated
   blocks.  The 1-item slice below exercises that path; the full-array
   slice covers the regular maskout path. */

#include "test_common.h"

typedef struct {
  int64_t start;
  int64_t stop;
} test_slice_t;


CUTEST_TEST_SETUP(large_typesize) {
  blosc2_init();

  CUTEST_PARAMETRIZE(typesize, int32_t, CUTEST_DATA(256, 261, 256 * 256));
  CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
      {false, false},
      {true, true},
  ));
  CUTEST_PARAMETRIZE(slice, test_slice_t, CUTEST_DATA(
      {0, 1},      // single block: compact get path
      {5, 133},    // crosses chunk boundary
      {0, 512},    // whole array: maskout path
  ));
}

CUTEST_TEST_TEST(large_typesize) {
  CUTEST_GET_PARAMETER(typesize, int32_t);
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(slice, test_slice_t);

  char *urlpath = "test_b2nd_large_typesize.b2frame";
  blosc2_remove_urlpath(urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 2;
  cparams.typesize = typesize;
  blosc2_storage b2_storage = {.cparams=&cparams};
  if (backend.persistent) {
    b2_storage.urlpath = urlpath;
  }
  b2_storage.contiguous = backend.contiguous;

  int64_t shape[] = {512};
  int32_t chunkshape[] = {128};
  int32_t blockshape[] = {2};  // many blocks per chunk, so 1 item is a "small fraction"
  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, 1, shape,
                                        chunkshape, blockshape, NULL, 0, NULL, 0);

  size_t buffersize = (size_t) shape[0] * typesize;
  uint8_t *buffer = malloc(buffersize);
  for (size_t i = 0; i < buffersize; ++i) {
    buffer[i] = (uint8_t) (i % 251);  // position-dependent, catches offset/length bugs
  }

  b2nd_array_t *src;
  B2ND_TEST_ASSERT(b2nd_from_cbuffer(ctx, &src, buffer, (int64_t) buffersize));

  int64_t destshape[] = {slice.stop - slice.start};
  int64_t destbuffersize = destshape[0] * typesize;
  uint8_t *destbuffer = malloc((size_t) destbuffersize);
  memset(destbuffer, 0xAA, (size_t) destbuffersize);

  B2ND_TEST_ASSERT(b2nd_get_slice_cbuffer(src, &slice.start, &slice.stop, destbuffer,
                                          destshape, destbuffersize));

  size_t offset = (size_t) slice.start * typesize;
  for (int64_t i = 0; i < destbuffersize; ++i) {
    CUTEST_ASSERT("Elements are not equals!",
                  destbuffer[i] == (uint8_t) ((offset + i) % 251));
  }

  free(buffer);
  free(destbuffer);
  B2ND_TEST_ASSERT(b2nd_free(src));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));

  blosc2_remove_urlpath(urlpath);

  return 0;
}

CUTEST_TEST_TEARDOWN(large_typesize) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(large_typesize);
}
