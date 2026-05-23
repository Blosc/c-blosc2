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
  b2nd_create_ctx, which is then overridden by b2nd_get_slice), so the
  fix lives in update_shape's stride loop: when chunkshape[i+1] or
  blockshape[i+1] is 0 the dimension is degenerate and the strides
  through it are set to 0 instead of triggering the 0/0 division.
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

  // The critical call:  pre-fix this divides by zero and kills the process.
  // Post-fix it returns cleanly; the resulting array is degenerate (strides
  // through the chunkshape==0 dimension are 0) but the parser does not crash.
  b2nd_array_t *arr_corrupt = NULL;
  int rc = b2nd_from_schunk(arr->sc, &arr_corrupt);
  CUTEST_ASSERT("b2nd_from_schunk must not crash on chunkshape=blockshape=0", rc >= 0);
  CUTEST_ASSERT("b2nd_from_schunk should produce an array on success", arr_corrupt != NULL);

  // Sanity-check that the degenerate strides we wrote did not leave the
  // process in a state that traps on the next basic read of the metadata.
  CUTEST_ASSERT("parsed ndim should round-trip", arr_corrupt->ndim == 2);
  CUTEST_ASSERT("parsed chunkshape[1] should reflect the mutation",
                arr_corrupt->chunkshape[1] == 0);
  CUTEST_ASSERT("parsed blockshape[1] should reflect the mutation",
                arr_corrupt->blockshape[1] == 0);

  // The block_chunk_strides slot that would have hit 0/0 in the old code
  // must now be the zeroed-out degenerate value.
  CUTEST_ASSERT("degenerate block_chunk_strides[0] should be zero",
                arr_corrupt->block_chunk_strides[0] == 0);

  b2nd_free(arr_corrupt);
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
