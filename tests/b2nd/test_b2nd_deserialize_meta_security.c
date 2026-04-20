/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
*********************************************************************/

#include "test_common.h"


CUTEST_TEST_SETUP(deserialize_meta_security) {
  blosc2_init();
}


CUTEST_TEST_TEST(deserialize_meta_security) {
  int8_t ndim = 2;
  int64_t shape[B2ND_MAX_DIM] = {4, 5};
  int32_t chunkshape[B2ND_MAX_DIM] = {2, 5};
  int32_t blockshape[B2ND_MAX_DIM] = {1, 5};

  uint8_t *smeta = NULL;
  int32_t smeta_len = b2nd_serialize_meta(ndim, shape, chunkshape, blockshape,
                                          "|u1", DTYPE_NUMPY_FORMAT, &smeta);
  B2ND_TEST_ASSERT(smeta_len);

  // Truncated metadata must be rejected instead of being over-read.
  int8_t parsed_ndim = 0;
  int64_t parsed_shape[B2ND_MAX_DIM];
  int32_t parsed_chunkshape[B2ND_MAX_DIM];
  int32_t parsed_blockshape[B2ND_MAX_DIM];
  char *dtype = NULL;
  int8_t dtype_format = 0;
  int rc = b2nd_deserialize_meta(smeta, smeta_len - 1, &parsed_ndim, parsed_shape,
                                 parsed_chunkshape, parsed_blockshape, &dtype, &dtype_format);
  CUTEST_ASSERT("truncated metadata should fail", rc < 0);
  CUTEST_ASSERT("dtype must remain NULL on failure", dtype == NULL);

  // Corrupt dtype length to negative; parser must fail before allocating/copying.
  uint8_t *smeta_bad = malloc((size_t)smeta_len);
  CUTEST_ASSERT("cannot allocate test buffer", smeta_bad != NULL);
  memcpy(smeta_bad, smeta, (size_t)smeta_len);

  size_t dtype_offset = 3;
  dtype_offset += 1 + (size_t)ndim * (1 + sizeof(int64_t));
  dtype_offset += 1 + (size_t)ndim * (1 + sizeof(int32_t));
  dtype_offset += 1 + (size_t)ndim * (1 + sizeof(int32_t));
  size_t dtype_len_offset = dtype_offset + 2;
  CUTEST_ASSERT("dtype length field out of bounds", dtype_len_offset + sizeof(int32_t) <= (size_t)smeta_len);

  int32_t negative_dtype_len = -1;
  swap_store(&smeta_bad[dtype_len_offset], &negative_dtype_len, sizeof(int32_t));

  dtype = NULL;
  rc = b2nd_deserialize_meta(smeta_bad, smeta_len, &parsed_ndim, parsed_shape,
                             parsed_chunkshape, parsed_blockshape, &dtype, &dtype_format);
  CUTEST_ASSERT("negative dtype length should fail", rc < 0);
  CUTEST_ASSERT("dtype must remain NULL on malformed metadata", dtype == NULL);

  // Corrupt blockshape[0] to 0 while chunkshape[0] stays non-zero; opening must fail cleanly.
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  blosc2_storage storage = {.cparams=&cparams};
  int64_t arr_shape[2] = {20, 10};
  int32_t arr_chunkshape[2] = {7, 5};
  int32_t arr_blockshape[2] = {3, 5};
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, 2, arr_shape, arr_chunkshape, arr_blockshape,
                                        NULL, 0, NULL, 0);
  CUTEST_ASSERT("context creation should succeed", ctx != NULL);

  b2nd_array_t *arr;
  B2ND_TEST_ASSERT(b2nd_zeros(ctx, &arr));

  uint8_t *b2nd_meta;
  int32_t b2nd_meta_len;
  B2ND_TEST_ASSERT(blosc2_meta_get(arr->sc, "b2nd", &b2nd_meta, &b2nd_meta_len));
  uint8_t *b2nd_meta_bad = malloc((size_t)b2nd_meta_len);
  CUTEST_ASSERT("cannot allocate b2nd metadata buffer", b2nd_meta_bad != NULL);
  memcpy(b2nd_meta_bad, b2nd_meta, (size_t)b2nd_meta_len);

  size_t blockshape_offset = 3;
  blockshape_offset += 1 + (size_t)2 * (1 + sizeof(int64_t));
  blockshape_offset += 1 + (size_t)2 * (1 + sizeof(int32_t));
  size_t blockshape0_value_offset = blockshape_offset + 2;
  CUTEST_ASSERT("blockshape field out of bounds",
                blockshape0_value_offset + sizeof(int32_t) <= (size_t)b2nd_meta_len);

  int32_t zero = 0;
  swap_store(&b2nd_meta_bad[blockshape0_value_offset], &zero, sizeof(int32_t));
  B2ND_TEST_ASSERT(blosc2_meta_update(arr->sc, "b2nd", b2nd_meta_bad, b2nd_meta_len));

  b2nd_array_t *arr_corrupt = NULL;
  rc = b2nd_from_schunk(arr->sc, &arr_corrupt);
  CUTEST_ASSERT("corrupted blockshape/chunkshape metadata should fail", rc < 0);
  if (arr_corrupt != NULL) {
    B2ND_TEST_ASSERT(b2nd_free(arr_corrupt));
  }

  free(b2nd_meta_bad);
  free(b2nd_meta);
  B2ND_TEST_ASSERT(b2nd_free(arr));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));

  free(smeta_bad);
  free(smeta);

  return 0;
}


CUTEST_TEST_TEARDOWN(deserialize_meta_security) {
  blosc2_destroy();
}


int main() {
  CUTEST_TEST_RUN(deserialize_meta_security);
}
