/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* Regression test for #795: crafted shape/chunkshape/blockshape values whose
   cumulative product overflows.  update_shape_struct() accumulated them into
   int32 (chunknitems, blocknitems) and int64 (nitems, extnitems,
   extchunknitems) fields with no check, which is UB and, on the deserialization
   path, produced an array that was accepted with blocknitems == 0 and then
   divided by it.  A separate truncation turned an int64 byte count into an
   int32 allocation size while copy offsets kept using the full extent. */

#include "test_common.h"

/* Build a schunk carrying nothing but a crafted b2nd metalayer, the way
   b2nd_open() sees a file on disk, and try to turn it into an array. */
static int from_crafted_meta(int8_t ndim, const int64_t *shape, const int32_t *chunkshape,
                             const int32_t *blockshape, int32_t typesize,
                             b2nd_array_t **array, blosc2_schunk **schunk_out) {
  uint8_t *smeta = NULL;
  int32_t smeta_len = b2nd_serialize_meta(ndim, shape, chunkshape, blockshape,
                                          "|u1", DTYPE_NUMPY_FORMAT, &smeta);
  if (smeta_len < 0) {
    return smeta_len;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  blosc2_storage storage = {.cparams=&cparams, .contiguous=true};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    free(smeta);
    return BLOSC2_ERROR_FAILURE;
  }
  int rc = blosc2_meta_add(schunk, "b2nd", smeta, smeta_len);
  free(smeta);
  if (rc < 0) {
    blosc2_schunk_free(schunk);
    return rc;
  }

  *schunk_out = schunk;
  return b2nd_from_schunk(schunk, array);
}

CUTEST_TEST_SETUP(shape_overflow) {
  blosc2_init();
}

CUTEST_TEST_TEST(shape_overflow) {
  b2nd_array_t *array = NULL;
  blosc2_schunk *schunk = NULL;
  int rc;

  // blockshape product wraps int32: 65536 * 65536 == 0.  Used to be accepted
  // with blocknitems == 0, then divided by in get_set_slice().
  {
    int64_t shape[2] = {65536, 65536};
    int32_t chunkshape[2] = {65536, 1};
    int32_t blockshape[2] = {65536, 65536};
    array = NULL; schunk = NULL;
    rc = from_crafted_meta(2, shape, chunkshape, blockshape, 1, &array, &schunk);
    CUTEST_ASSERT("overflowing blockshape product must be rejected", rc < 0);
    CUTEST_ASSERT("no array must be produced on rejection", array == NULL);
    if (schunk != NULL) blosc2_schunk_free(schunk);
  }

  // Same for the chunkshape product.
  {
    int64_t shape[2] = {65536, 65536};
    int32_t chunkshape[2] = {65536, 65536};
    int32_t blockshape[2] = {1, 1};
    array = NULL; schunk = NULL;
    rc = from_crafted_meta(2, shape, chunkshape, blockshape, 1, &array, &schunk);
    CUTEST_ASSERT("overflowing chunkshape product must be rejected", rc < 0);
    CUTEST_ASSERT("no array must be produced on rejection", array == NULL);
    if (schunk != NULL) blosc2_schunk_free(schunk);
  }

  // The int64 products must be checked too: nitems here is 2^62 * 4 == 2^64.
  {
    int64_t shape[2] = {4611686018427387904LL, 4};
    int32_t chunkshape[2] = {1, 1};
    int32_t blockshape[2] = {1, 1};
    array = NULL; schunk = NULL;
    rc = from_crafted_meta(2, shape, chunkshape, blockshape, 1, &array, &schunk);
    CUTEST_ASSERT("overflowing shape product must be rejected", rc < 0);
    CUTEST_ASSERT("no array must be produced on rejection", array == NULL);
    if (schunk != NULL) blosc2_schunk_free(schunk);
  }

  // The creation path shares update_shape_struct(), and b2nd_create_ctx() has
  // its own blocknitems accumulation for cparams->blocksize.
  {
    int64_t shape[2] = {65536, 65536};
    int32_t chunkshape[2] = {65536, 65536};
    int32_t blockshape[2] = {65536, 65536};
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.typesize = 1;
    blosc2_storage storage = {.cparams=&cparams};
    b2nd_context_t *ctx = b2nd_create_ctx(&storage, 2, shape, chunkshape, blockshape,
                                          NULL, 0, NULL, 0);
    CUTEST_ASSERT("overflowing blockshape must not yield a context", ctx == NULL);
  }

  // Products fit int32, but extchunknitems * typesize exceeds INT32_MAX.  This
  // one is a legitimate array to build; only the oversized chunk buffer that
  // get_slice would allocate has to be refused, instead of truncated.
  {
    int64_t shape[1] = {1073741952};
    int32_t chunkshape[1] = {1073741952};
    int32_t blockshape[1] = {128};
    array = NULL; schunk = NULL;
    rc = from_crafted_meta(1, shape, chunkshape, blockshape, 4, &array, &schunk);
    CUTEST_ASSERT("array with an oversized chunk should still deserialize", rc == 0);
    CUTEST_ASSERT("array must exist", array != NULL);

    int64_t start[1] = {0}, stop[1] = {1}, buffershape[1] = {1};
    uint8_t out[8];
    rc = b2nd_get_slice_cbuffer(array, start, stop, out, buffershape, sizeof(out));
    CUTEST_ASSERT("oversized chunk buffer must be refused, not truncated", rc < 0);

    B2ND_TEST_ASSERT(b2nd_free(array));
  }

  // Control: an ordinary array must still round-trip, so none of the above
  // checks are rejecting valid shapes.
  {
    int64_t shape[2] = {20, 10};
    int32_t chunkshape[2] = {7, 5};
    int32_t blockshape[2] = {3, 5};
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.typesize = 1;
    blosc2_storage storage = {.cparams=&cparams};
    b2nd_context_t *ctx = b2nd_create_ctx(&storage, 2, shape, chunkshape, blockshape,
                                          NULL, 0, NULL, 0);
    CUTEST_ASSERT("valid context creation must still succeed", ctx != NULL);

    b2nd_array_t *arr = NULL;
    B2ND_TEST_ASSERT(b2nd_zeros(ctx, &arr));
    int64_t start[2] = {1, 1}, stop[2] = {3, 4}, buffershape[2] = {2, 3};
    uint8_t out[6];
    B2ND_TEST_ASSERT(b2nd_get_slice_cbuffer(arr, start, stop, out, buffershape, sizeof(out)));
    for (size_t i = 0; i < sizeof(out); i++) {
      CUTEST_ASSERT("valid slice must read back as zeros", out[i] == 0);
    }
    B2ND_TEST_ASSERT(b2nd_free(arr));
    B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));
  }

  // An empty shape keeps working: validate_shape_chunkshape_blockshape() is
  // intentionally lenient about chunkshape==0, and extchunknitems is 0 there,
  // which the new range checks must not mistake for an error.
  {
    int64_t shape[2] = {0, 0};
    int32_t chunkshape[2] = {0, 0};
    int32_t blockshape[2] = {0, 0};
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.typesize = 1;
    blosc2_storage storage = {.cparams=&cparams};
    b2nd_context_t *ctx = b2nd_create_ctx(&storage, 2, shape, chunkshape, blockshape,
                                          NULL, 0, NULL, 0);
    CUTEST_ASSERT("empty shape context must still be created", ctx != NULL);
    b2nd_array_t *arr = NULL;
    B2ND_TEST_ASSERT(b2nd_empty(ctx, &arr));
    B2ND_TEST_ASSERT(b2nd_free(arr));
    B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));
  }

  return 0;
}

CUTEST_TEST_TEARDOWN(shape_overflow) {
  blosc2_destroy();
}

int main(void) {
  CUTEST_TEST_RUN(shape_overflow);
}
