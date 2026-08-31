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

  // Structural markers and the format version are part of the wire format,
  // not padding that a reader may silently skip.
  uint8_t saved_byte = smeta[0];
  smeta[0] = 0x94;
  rc = b2nd_deserialize_meta(smeta, smeta_len, &parsed_ndim, parsed_shape,
                             parsed_chunkshape, parsed_blockshape, &dtype, &dtype_format);
  CUTEST_ASSERT("invalid outer array marker should fail", rc < 0);
  smeta[0] = saved_byte;

  saved_byte = smeta[1];
  smeta[1] = B2ND_METALAYER_VERSION + 1;
  rc = b2nd_deserialize_meta(smeta, smeta_len, &parsed_ndim, parsed_shape,
                             parsed_chunkshape, parsed_blockshape, &dtype, &dtype_format);
  CUTEST_ASSERT("unknown metadata version should fail", rc < 0);
  smeta[1] = saved_byte;

  saved_byte = smeta[3];
  smeta[3] = 0x91;
  rc = b2nd_deserialize_meta(smeta, smeta_len, &parsed_ndim, parsed_shape,
                             parsed_chunkshape, parsed_blockshape, &dtype, &dtype_format);
  CUTEST_ASSERT("incorrect shape array length should fail", rc < 0);
  smeta[3] = saved_byte;

  saved_byte = smeta[4];
  smeta[4] = 0xd2;
  rc = b2nd_deserialize_meta(smeta, smeta_len, &parsed_ndim, parsed_shape,
                             parsed_chunkshape, parsed_blockshape, &dtype, &dtype_format);
  CUTEST_ASSERT("incorrect shape element marker should fail", rc < 0);
  smeta[4] = saved_byte;

  uint8_t *smeta_trailing = malloc((size_t)smeta_len + 1);
  CUTEST_ASSERT("cannot allocate metadata with trailing byte", smeta_trailing != NULL);
  memcpy(smeta_trailing, smeta, (size_t)smeta_len);
  smeta_trailing[smeta_len] = 0;
  rc = b2nd_deserialize_meta(smeta_trailing, smeta_len + 1, &parsed_ndim, parsed_shape,
                             parsed_chunkshape, parsed_blockshape, NULL, NULL);
  CUTEST_ASSERT("trailing metadata should fail even when dtype output is ignored", rc < 0);
  free(smeta_trailing);

  // The 2023 writer declared six entries while actually writing seven.
  // Readers retain this narrow compatibility exception.
  smeta[0] = 0x96;
  rc = b2nd_deserialize_meta(smeta, smeta_len, &parsed_ndim, parsed_shape,
                             parsed_chunkshape, parsed_blockshape, &dtype, &dtype_format);
  CUTEST_ASSERT("legacy six-entry declaration should remain readable", rc == smeta_len);
  CUTEST_ASSERT("legacy dtype should be returned", dtype != NULL && strcmp(dtype, "|u1") == 0);
  free(dtype);
  dtype = NULL;
  smeta[0] = 0x97;

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

  // Older Caterva metadata ended after blockshape and declared five entries.
  uint8_t *caterva_meta = malloc(dtype_offset);
  CUTEST_ASSERT("cannot allocate legacy Caterva metadata", caterva_meta != NULL);
  memcpy(caterva_meta, smeta, dtype_offset);
  caterva_meta[0] = 0x95;
  rc = b2nd_deserialize_meta(caterva_meta, (int32_t)dtype_offset, &parsed_ndim, parsed_shape,
                             parsed_chunkshape, parsed_blockshape, &dtype, &dtype_format);
  CUTEST_ASSERT("legacy metadata without dtype should remain readable", rc == (int32_t)dtype_offset);
  CUTEST_ASSERT("legacy metadata without dtype should return NULL", dtype == NULL);
  free(caterva_meta);

  int32_t negative_dtype_len = -1;
  swap_store(&smeta_bad[dtype_len_offset], &negative_dtype_len, sizeof(int32_t));

  dtype = NULL;
  rc = b2nd_deserialize_meta(smeta_bad, smeta_len, &parsed_ndim, parsed_shape,
                             parsed_chunkshape, parsed_blockshape, &dtype, &dtype_format);
  CUTEST_ASSERT("negative dtype length should fail", rc < 0);
  CUTEST_ASSERT("dtype must remain NULL on malformed metadata", dtype == NULL);

  // Version 0 assigns B2ND-specific meaning to 0xa0 for 16-element vectors,
  // even though MessagePack defines that byte as fixstr(0).
  int64_t shape16[B2ND_MAX_DIM];
  int32_t chunkshape16[B2ND_MAX_DIM];
  int32_t blockshape16[B2ND_MAX_DIM];
  for (int i = 0; i < B2ND_MAX_DIM; ++i) {
    shape16[i] = i + 1;
    chunkshape16[i] = 1;
    blockshape16[i] = 1;
  }
  uint8_t *smeta16 = NULL;
  int32_t smeta16_len = b2nd_serialize_meta(B2ND_MAX_DIM, shape16, chunkshape16, blockshape16,
                                             "|u1", DTYPE_NUMPY_FORMAT, &smeta16);
  CUTEST_ASSERT("version 0 should serialize 16-D metadata", smeta16_len > 0);
  CUTEST_ASSERT("16-D shape marker should be 0xa0", smeta16[3] == 0xa0);
  size_t chunkshape16_offset = 4 + B2ND_MAX_DIM * (1 + sizeof(int64_t));
  size_t blockshape16_offset = chunkshape16_offset + 1 + B2ND_MAX_DIM * (1 + sizeof(int32_t));
  CUTEST_ASSERT("16-D chunkshape marker should be 0xa0", smeta16[chunkshape16_offset] == 0xa0);
  CUTEST_ASSERT("16-D blockshape marker should be 0xa0", smeta16[blockshape16_offset] == 0xa0);
  rc = b2nd_deserialize_meta(smeta16, smeta16_len, &parsed_ndim, parsed_shape,
                             parsed_chunkshape, parsed_blockshape, NULL, NULL);
  CUTEST_ASSERT("version 0 16-D metadata should be readable", rc == smeta16_len);
  CUTEST_ASSERT("version 0 16-D metadata values should round-trip",
                parsed_ndim == B2ND_MAX_DIM && parsed_shape[B2ND_MAX_DIM - 1] == B2ND_MAX_DIM);
  free(smeta16);

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

  // Corrupt fixed-length metalayer content length to negative; get must fail safely.
  int nmetalayer = blosc2_meta_exists(arr->sc, "b2nd");
  CUTEST_ASSERT("b2nd metalayer should exist", nmetalayer >= 0);
  blosc2_metalayer *b2nd_meta_layer = arr->sc->metalayers[nmetalayer];
  int32_t saved_b2nd_meta_len = b2nd_meta_layer->content_len;
  b2nd_meta_layer->content_len = -1;
  uint8_t *bad_meta_content = NULL;
  int32_t bad_meta_content_len = 0;
  rc = blosc2_meta_get(arr->sc, "b2nd", &bad_meta_content, &bad_meta_content_len);
  CUTEST_ASSERT("negative fixed-length metalayer length should fail", rc < 0);
  CUTEST_ASSERT("fixed-length metalayer get must not allocate on corrupted size", bad_meta_content == NULL);
  CUTEST_ASSERT("fixed-length metalayer length output must remain zero on failure", bad_meta_content_len == 0);
  b2nd_meta_layer->content_len = saved_b2nd_meta_len;

  rc = blosc2_meta_add(arr->sc, "bad_metalayer", NULL, -1);
  CUTEST_ASSERT("negative fixed-length metalayer content length must be rejected", rc < 0);
  rc = blosc2_vlmeta_add(arr->sc, "bad_vlmetalayer", NULL, -1, NULL);
  CUTEST_ASSERT("negative variable-length metalayer content length must be rejected", rc < 0);

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
