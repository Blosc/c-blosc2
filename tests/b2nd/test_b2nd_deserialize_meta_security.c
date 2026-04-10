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
