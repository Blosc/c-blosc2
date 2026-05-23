/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Regression test for a divide-by-zero in update_shape's stride loop
  reachable from a crafted b2nd metalayer carried inside a cframe.

  validate_shape_chunkshape_blockshape historically only rejected the
  inconsistency "blockshape[i]==0 while chunkshape[i]!=0", and a prior
  patch (656796e0) added a matching guard inside the per-axis loop of
  update_shape.  Both checks miss a third combination: chunkshape[i]==0
  *and* blockshape[i]==0 with shape[i]>0.  That tuple slips past both
  guards, but in the stride loop

      array->block_chunk_strides[i] =
          array->block_chunk_strides[i + 1] *
          (array->extchunkshape[i + 1] / array->blockshape[i + 1]);

  the divisor is now 0 because the previous loop set
  array->extchunkshape[i+1] = chunkshape[i+1] (==0) and
  array->blockshape[i+1] = blockshape[i+1] (==0).  The integer
  division 0/0 raises SIGFPE on x86, crashing any process that calls
  b2nd_from_schunk on the crafted cframe (e.g. through b2nd_open or
  b2nd_from_cframe).

  Tightening the validator turned out to break legitimate placeholder
  uses (examples/b2nd/example_empty_shape.c passes chunkshape=0 to
  b2nd_create_ctx, but the slice it ultimately writes has shape=0 in
  the matching axis so no divide-by-zero materializes).  The fix is
  therefore in two places:

    * update_shape's stride loop skips the inner-stride update when
      chunkshape[i+1] or blockshape[i+1] is 0, so the immediate
      0/0 division can never fire (defense in depth for any internal
      caller of update_shape).
    * b2nd_from_schunk rejects the combination outright on the
      deserialization path -- many other downstream code paths
      (b2nd_get_slice, b2nd_set_slice, set/get_orthogonal_selection,
      squeeze, resize) also divide/mod by chunkshape[i] / blockshape[i]
      without zero checks, so a parsed-but-degenerate array would still
      crash on the first real use.
*/

#include "test_common.h"


CUTEST_TEST_SETUP(zero_chunkshape_nonzero_shape) {
  blosc2_init();
}


CUTEST_TEST_TEST(zero_chunkshape_nonzero_shape) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  blosc2_storage storage = {.cparams = &cparams};

  // Build a real cframe, then mutate the on-wire b2nd metalayer to inject
  // chunkshape[1]=0 and blockshape[1]=0 while keeping shape[1]>0, and re-parse
  // via b2nd_from_schunk.  Pre-fix this path divides by zero in update_shape's
  // stride loop and aborts the process with SIGFPE -- if the patch is reverted
  // the test runner crashes here and CTest reports the test as failed via an
  // abnormal exit code rather than a clean assertion failure.
  int64_t arr_shape[2] = {20, 10};
  int32_t arr_chunkshape[2] = {7, 5};
  int32_t arr_blockshape[2] = {3, 5};
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, 2, arr_shape, arr_chunkshape, arr_blockshape,
                                        NULL, 0, NULL, 0);
  CUTEST_ASSERT("context creation should succeed", ctx != NULL);

  b2nd_array_t *arr = NULL;
  B2ND_TEST_ASSERT(b2nd_zeros(ctx, &arr));

  uint8_t *b2nd_meta = NULL;
  int32_t b2nd_meta_len = 0;
  B2ND_TEST_ASSERT(blosc2_meta_get(arr->sc, "b2nd", &b2nd_meta, &b2nd_meta_len));

  uint8_t *b2nd_meta_bad = malloc((size_t)b2nd_meta_len);
  CUTEST_ASSERT("cannot allocate b2nd metadata buffer", b2nd_meta_bad != NULL);
  memcpy(b2nd_meta_bad, b2nd_meta, (size_t)b2nd_meta_len);

  // Layout (b2nd_serialize_meta):
  //   [0]  0x97                       fixarray-of-7 marker
  //   [1]  version
  //   [2]  ndim
  //   [3]  0x90+ndim                  shape array marker
  //         then ndim * (1-byte 0xd3 int64 marker + 8 bytes)
  //   then 0x90+ndim                  chunkshape array marker
  //         then ndim * (1-byte 0xd2 int32 marker + 4 bytes)
  //   then 0x90+ndim                  blockshape array marker
  //         then ndim * (1-byte 0xd2 int32 marker + 4 bytes)
  size_t pos = 3;
  pos += 1 + (size_t)2 * (1 + sizeof(int64_t));   // skip shape
  size_t chunkshape_array_marker = pos;
  pos += 1 + (size_t)2 * (1 + sizeof(int32_t));   // skip chunkshape
  size_t blockshape_array_marker = pos;

  // chunkshape[1] value: skip array marker, skip chunkshape[0] (1 + 4), skip int32 marker
  size_t chunkshape1_value_offset = chunkshape_array_marker + 1 + (1 + sizeof(int32_t)) + 1;
  // blockshape[1] value: same layout for the blockshape array
  size_t blockshape1_value_offset = blockshape_array_marker + 1 + (1 + sizeof(int32_t)) + 1;

  CUTEST_ASSERT("chunkshape[1] field out of bounds",
                chunkshape1_value_offset + sizeof(int32_t) <= (size_t)b2nd_meta_len);
  CUTEST_ASSERT("blockshape[1] field out of bounds",
                blockshape1_value_offset + sizeof(int32_t) <= (size_t)b2nd_meta_len);

  int32_t zero = 0;
  swap_store(&b2nd_meta_bad[chunkshape1_value_offset], &zero, sizeof(int32_t));
  swap_store(&b2nd_meta_bad[blockshape1_value_offset], &zero, sizeof(int32_t));

  B2ND_TEST_ASSERT(blosc2_meta_update(arr->sc, "b2nd", b2nd_meta_bad, b2nd_meta_len));

  // Re-open the mutated container as a separate schunk before parsing it
  // into a second b2nd_array_t.  b2nd_from_schunk() transfers ownership of
  // the passed schunk to the returned array, so using arr->sc directly here
  // would make arr and arr_corrupt share ownership and double-free during
  // cleanup.
  uint8_t *cframe = NULL;
  bool cframe_needs_free = false;
  int64_t cframe_len = blosc2_schunk_to_buffer(arr->sc, &cframe, &cframe_needs_free);
  CUTEST_ASSERT("blosc2_schunk_to_buffer should serialize the mutated schunk",
                cframe_len > 0);

  blosc2_schunk *arr_corrupt_sc = blosc2_schunk_from_buffer(cframe, cframe_len, false);
  CUTEST_ASSERT("blosc2_schunk_from_buffer should reopen the mutated schunk",
                arr_corrupt_sc != NULL);

  // The critical call: pre-fix this divides by zero and kills the process.
  // Post-fix b2nd_from_schunk rejects the crafted metalayer cleanly, leaving
  // arr_corrupt as NULL and returning a negative error code -- if the patch
  // is reverted the test runner crashes here and CTest reports the test as
  // failed via an abnormal exit code rather than a clean assertion failure.
  b2nd_array_t *arr_corrupt = NULL;
  int rc = b2nd_from_schunk(arr_corrupt_sc, &arr_corrupt);
  CUTEST_ASSERT("b2nd_from_schunk must reject chunkshape=blockshape=0 with shape>0",
                rc < 0);
  CUTEST_ASSERT("b2nd_from_schunk must not produce an array on rejection",
                arr_corrupt == NULL);

  // b2nd_from_schunk only takes ownership of the schunk on success, so we
  // still own arr_corrupt_sc and have to free it ourselves.
  blosc2_schunk_free(arr_corrupt_sc);
  if (cframe_needs_free) {
    free(cframe);
  }
  free(b2nd_meta_bad);
  free(b2nd_meta);
  B2ND_TEST_ASSERT(b2nd_free(arr));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));

  return 0;
}


CUTEST_TEST_TEARDOWN(zero_chunkshape_nonzero_shape) {
  blosc2_destroy();
}


int main(void) {
  CUTEST_TEST_RUN(zero_chunkshape_nonzero_shape);
}
