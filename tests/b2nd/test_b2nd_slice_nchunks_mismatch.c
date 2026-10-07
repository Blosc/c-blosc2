/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* Regression test for a heap out-of-bounds write in b2nd_get_slice_nchunks().
   The chunks_idx buffer was sized from schunk->nchunks, while the loop filling
   it runs once per chunk of the bounding box derived from the b2nd shape and
   chunkshape.  A container whose metalayer announces more chunks than the
   frame stores makes that loop write past the end of the allocation, both
   through the public blosc2_get_slice_nchunks() and through the single-chunk
   fast path b2nd_to_cbuffer() takes. */

#include "test_common.h"

/* A schunk carrying a b2nd metalayer that announces `nitems` chunks of one
   item each, with only `nchunks_stored` of them actually appended. */
static blosc2_schunk *crafted_schunk(int64_t nitems, int64_t nchunks_stored) {
  int64_t shape[1] = {nitems};
  int32_t chunkshape[1] = {1};
  int32_t blockshape[1] = {1};
  uint8_t *smeta = NULL;
  int32_t smeta_len = b2nd_serialize_meta(1, shape, chunkshape, blockshape,
                                          "|u1", DTYPE_NUMPY_FORMAT, &smeta);
  if (smeta_len < 0) {
    return NULL;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  blosc2_storage storage = {.cparams=&cparams, .contiguous=true};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    free(smeta);
    return NULL;
  }
  int rc = blosc2_meta_add(schunk, "b2nd", smeta, smeta_len);
  free(smeta);
  if (rc < 0) {
    blosc2_schunk_free(schunk);
    return NULL;
  }

  uint8_t item = 0xa5;
  for (int64_t i = 0; i < nchunks_stored; i++) {
    if (blosc2_schunk_append_buffer(schunk, &item, 1) < 0) {
      blosc2_schunk_free(schunk);
      return NULL;
    }
  }
  return schunk;
}

CUTEST_TEST_SETUP(slice_nchunks_mismatch) {
  blosc2_init();
}

CUTEST_TEST_TEST(slice_nchunks_mismatch) {
  /* A mebichunk announced, one stored: the loop used to push 8 MiB of chunk
     indexes into an 8-byte allocation, which aborts the test runner. */
  const int64_t nitems = 1024 * 1024;
  blosc2_schunk *schunk = crafted_schunk(nitems, 1);
  CUTEST_ASSERT("cannot build the crafted schunk", schunk != NULL);
  CUTEST_ASSERT("the frame should hold a single chunk", schunk->nchunks == 1);

  int64_t start = 0;
  int64_t stop = nitems;
  int64_t *chunks_idx = NULL;
  int64_t nchunks = blosc2_get_slice_nchunks(schunk, &start, &stop, &chunks_idx);
  CUTEST_ASSERT("the slice should span every announced chunk", nchunks == nitems);
  CUTEST_ASSERT("no chunk index buffer was returned", chunks_idx != NULL);
  CUTEST_ASSERT("unexpected first chunk index", chunks_idx[0] == 0);
  CUTEST_ASSERT("unexpected last chunk index", chunks_idx[nchunks - 1] == nitems - 1);
  free(chunks_idx);
  blosc2_schunk_free(schunk);

  /* Same mismatch seen from b2nd_to_cbuffer(): one item announced and no chunk
     stored, so the single-chunk fast path indexed a zero-sized allocation.  It
     must now report the missing chunk instead. */
  blosc2_schunk *empty = crafted_schunk(1, 0);
  CUTEST_ASSERT("cannot build the empty crafted schunk", empty != NULL);
  b2nd_array_t *array = NULL;
  B2ND_TEST_ASSERT(b2nd_from_schunk(empty, &array));
  uint8_t out = 0;
  CUTEST_ASSERT("reading an announced but absent chunk must fail",
                b2nd_to_cbuffer(array, &out, 1) < 0);
  B2ND_TEST_ASSERT(b2nd_free(array));

  return 0;
}

CUTEST_TEST_TEARDOWN(slice_nchunks_mismatch) {
  blosc2_destroy();
}

int main(void) {
  CUTEST_TEST_RUN(slice_nchunks_mismatch);
}
