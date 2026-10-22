/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"

CUTEST_TEST_SETUP(b2nd_delta) {
  blosc2_init();
}

static int test_partial_edge_chunk(int32_t typesize, bool persistent) {
  char *urlpath = "test_b2nd_delta.b2frame";
  blosc2_remove_urlpath(urlpath);

  int8_t ndim = 3;
  int64_t shape[3] = {64, 160, 160};
  int32_t chunkshape[3] = {64, 160, 128};
  int32_t blockshape[3] = {16, 64, 32};

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  cparams.filters[0] = BLOSC_DELTA;
  cparams.filters_meta[0] = 0;

  blosc2_storage b2_storage = {.cparams = &cparams};
  if (persistent) {
    b2_storage.urlpath = urlpath;
  }

  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, ndim, shape, chunkshape, blockshape, NULL, 0, NULL, 0);
  CUTEST_ASSERT("ctx creation failed", ctx != NULL);

  int64_t nitems = shape[0] * shape[1] * shape[2];
  int64_t buffersize = nitems * typesize;
  uint8_t *src_buf = malloc((size_t)buffersize);
  CUTEST_ASSERT("malloc failed", src_buf != NULL);
  for (int64_t i = 0; i < buffersize; i++) {
    src_buf[i] = (uint8_t)(i % 251);
  }

  b2nd_array_t *array = NULL;
  int rc = b2nd_from_cbuffer(ctx, &array, src_buf, buffersize);
  CUTEST_ASSERT("b2nd_from_cbuffer failed", rc >= 0);
  b2nd_free_ctx(ctx);

  if (persistent) {
    b2nd_free(array);
    rc = b2nd_open(urlpath, &array);
    CUTEST_ASSERT("b2nd_open failed", rc >= 0);
  }

  uint8_t *dst_buf = malloc((size_t)buffersize);
  CUTEST_ASSERT("malloc failed", dst_buf != NULL);
  rc = b2nd_to_cbuffer(array, dst_buf, buffersize);
  CUTEST_ASSERT("b2nd_to_cbuffer failed", rc >= 0);

  CUTEST_ASSERT("roundtrip mismatch with DELTA filter", memcmp(src_buf, dst_buf, (size_t)buffersize) == 0);

  free(src_buf);
  free(dst_buf);
  b2nd_free(array);
  if (persistent) {
    blosc2_remove_urlpath(urlpath);
  }
  return 0;
}

static int test_subslice_non_zero_block(void) {
  int8_t ndim = 1;
  int64_t shape[1] = {1000};
  int32_t chunkshape[1] = {1000};
  int32_t blockshape[1] = {100};

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 4;
  cparams.filters[0] = BLOSC_DELTA;

  blosc2_storage b2_storage = {.cparams = &cparams};
  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, ndim, shape, chunkshape, blockshape, NULL, 0, NULL, 0);
  CUTEST_ASSERT("ctx creation failed", ctx != NULL);

  int32_t src[1000];
  for (int i = 0; i < 1000; i++) {
    src[i] = i * 10 + 3;
  }
  b2nd_array_t *arr = NULL;
  int rc = b2nd_from_cbuffer(ctx, &arr, src, sizeof(src));
  CUTEST_ASSERT("b2nd_from_cbuffer failed", rc >= 0);
  b2nd_free_ctx(ctx);

  int64_t start[1] = {200};
  int64_t stop[1] = {300};
  int32_t dst[100] = {0};
  int64_t dst_shape[1] = {100};
  rc = b2nd_get_slice_cbuffer(arr, start, stop, dst, dst_shape, sizeof(dst));
  CUTEST_ASSERT("b2nd_get_slice_cbuffer failed", rc >= 0);

  CUTEST_ASSERT("subslice mismatch with DELTA filter", memcmp(dst, &src[200], sizeof(dst)) == 0);

  b2nd_free(arr);
  return 0;
}

CUTEST_TEST_TEST(b2nd_delta) {
  CUTEST_ASSERT("test_partial_edge_chunk uint8 in-memory failed", test_partial_edge_chunk(1, false) == 0);
  CUTEST_ASSERT("test_partial_edge_chunk uint8 persistent failed", test_partial_edge_chunk(1, true) == 0);
  CUTEST_ASSERT("test_partial_edge_chunk int16 in-memory failed", test_partial_edge_chunk(2, false) == 0);
  CUTEST_ASSERT("test_partial_edge_chunk int16 persistent failed", test_partial_edge_chunk(2, true) == 0);
  CUTEST_ASSERT("test_partial_edge_chunk float32 in-memory failed", test_partial_edge_chunk(4, false) == 0);
  CUTEST_ASSERT("test_partial_edge_chunk float32 persistent failed", test_partial_edge_chunk(4, true) == 0);
  CUTEST_ASSERT("test_subslice_non_zero_block failed", test_subslice_non_zero_block() == 0);
  return BLOSC2_ERROR_SUCCESS;
}

CUTEST_TEST_TEARDOWN(b2nd_delta) {
  blosc2_destroy();
}

int main(void) {
  CUTEST_TEST_RUN(b2nd_delta);
}
