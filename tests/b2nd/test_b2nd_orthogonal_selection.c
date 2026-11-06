/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"


static int check_get_and_set_orthogonal_selection(bool contiguous, char *urlpath) {
  const int8_t ndim = 3;
  const int64_t shape[] = {10, 10, 10};
  const int32_t chunkshape[] = {10, 10, 10};
  const int32_t blockshape[] = {10, 10, 10};
  const int64_t nitems = 1000;

  int32_t src[1000];
  for (int64_t i = 0; i < shape[0]; ++i) {
    for (int64_t j = 0; j < shape[1]; ++j) {
      for (int64_t k = 0; k < shape[2]; ++k) {
        src[(i * shape[1] + j) * shape[2] + k] = (int32_t)(i * 100 + j * 10 + k);
      }
    }
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = 1;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
  storage.cparams = &cparams;
  storage.dparams = &dparams;
  storage.contiguous = contiguous;
  storage.urlpath = urlpath;
  blosc2_remove_urlpath(urlpath);

  b2nd_context_t *ctx = b2nd_create_ctx(&storage, ndim, shape, chunkshape, blockshape,
                                        NULL, 0, NULL, 0);
  CUTEST_ASSERT("Could not create context", ctx != NULL);
  b2nd_array_t *array = NULL;
  B2ND_TEST_ASSERT(b2nd_from_cbuffer(ctx, &array, src, nitems * (int64_t)sizeof(int32_t)));

  int64_t sel0[] = {3, 1, 2};
  int64_t sel1[] = {2, 5};
  int64_t sel2[] = {3, 3, 3, 9, 3, 1, 0};
  int64_t *selection[] = {sel0, sel1, sel2};
  int64_t selection_size[] = {3, 2, 7};
  int64_t buffershape[] = {3, 2, 7};
  const int64_t out_nitems = 3 * 2 * 7;

  int32_t out[42] = {0};
  B2ND_TEST_ASSERT(b2nd_get_orthogonal_selection(array, selection, selection_size,
                                                 out, buffershape,
                                                 out_nitems * (int64_t)sizeof(int32_t)));

  for (int64_t i = 0; i < selection_size[0]; ++i) {
    for (int64_t j = 0; j < selection_size[1]; ++j) {
      for (int64_t k = 0; k < selection_size[2]; ++k) {
        int64_t flat = (i * selection_size[1] + j) * selection_size[2] + k;
        int32_t expected = (int32_t)(sel0[i] * 100 + sel1[j] * 10 + sel2[k]);
        CUTEST_ASSERT("Orthogonal get selection mismatch", out[flat] == expected);
      }
    }
  }

  int64_t set_sel2[] = {3, 9, 1, 0};
  int64_t *set_selection[] = {sel0, sel1, set_sel2};
  int64_t set_selection_size[] = {3, 2, 4};
  int64_t set_buffershape[] = {3, 2, 4};
  const int64_t set_out_nitems = 3 * 2 * 4;

  int32_t replacement[24];
  for (int64_t i = 0; i < set_out_nitems; ++i) {
    replacement[i] = (int32_t)(1000 + i);
  }
  B2ND_TEST_ASSERT(b2nd_set_orthogonal_selection(array, set_selection, set_selection_size,
                                                 replacement, set_buffershape,
                                                 set_out_nitems * (int64_t)sizeof(int32_t)));

  int32_t roundtrip[24] = {0};
  B2ND_TEST_ASSERT(b2nd_get_orthogonal_selection(array, set_selection, set_selection_size,
                                                 roundtrip, set_buffershape,
                                                 set_out_nitems * (int64_t)sizeof(int32_t)));
  for (int64_t i = 0; i < set_out_nitems; ++i) {
    CUTEST_ASSERT("Orthogonal set selection mismatch", roundtrip[i] == replacement[i]);
  }

  // Items outside the selection keep their values
  int32_t expected_all[1000];
  memcpy(expected_all, src, sizeof(src));
  for (int64_t i = 0; i < set_selection_size[0]; ++i) {
    for (int64_t j = 0; j < set_selection_size[1]; ++j) {
      for (int64_t k = 0; k < set_selection_size[2]; ++k) {
        int64_t flat = (set_selection[0][i] * shape[1] + set_selection[1][j]) * shape[2] + set_selection[2][k];
        expected_all[flat] = replacement[(i * set_selection_size[1] + j) * set_selection_size[2] + k];
      }
    }
  }
  int32_t all[1000];
  B2ND_TEST_ASSERT(b2nd_to_cbuffer(array, all, nitems * (int64_t)sizeof(int32_t)));
  for (int64_t i = 0; i < nitems; ++i) {
    CUTEST_ASSERT("Orthogonal set changed other items", all[i] == expected_all[i]);
  }

  b2nd_free(array);
  b2nd_free_ctx(ctx);
  blosc2_remove_urlpath(urlpath);
  return 0;
}


/* Indexes outside [0, shape) and negative selection sizes are rejected without touching the buffer or the array */
static int check_out_of_bounds_selection(void) {
  const int64_t shape[] = {10, 10};
  const int32_t chunkshape[] = {4, 4};
  const int32_t blockshape[] = {2, 2};
  int32_t src[100];
  for (int i = 0; i < 100; ++i) {
    src[i] = i;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = 1;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
  storage.cparams = &cparams;
  storage.dparams = &dparams;

  b2nd_context_t *ctx = b2nd_create_ctx(&storage, 2, shape, chunkshape, blockshape, NULL, 0, NULL, 0);
  CUTEST_ASSERT("Could not create context", ctx != NULL);
  b2nd_array_t *array = NULL;
  B2ND_TEST_ASSERT(b2nd_from_cbuffer(ctx, &array, src, sizeof(src)));

  const int64_t bad[] = {-1, -3, 10, INT64_MIN, INT64_MAX};
  int64_t selection_size[] = {2, 2};
  int64_t buffershape[] = {2, 2};
  for (int axis = 0; axis < 2; ++axis) {
    for (size_t b = 0; b < sizeof(bad) / sizeof(bad[0]); ++b) {
      // Bad index second: every entry is checked, not just the first
      int64_t sel_bad[] = {0, bad[b]};
      int64_t sel_ok[] = {0, 9};
      int64_t *selection[2];
      selection[axis] = sel_bad;
      selection[1 - axis] = sel_ok;
      int32_t buffer[4] = {-7, -7, -7, -7};
      int rc = b2nd_get_orthogonal_selection(array, selection, selection_size, buffer, buffershape,
                                             sizeof(buffer));
      CUTEST_ASSERT("Out-of-bounds index not rejected by get", rc == BLOSC2_ERROR_INVALID_INDEX);
      for (int i = 0; i < 4; ++i) {
        CUTEST_ASSERT("Rejected get wrote to the buffer", buffer[i] == -7);
      }
      rc = b2nd_set_orthogonal_selection(array, selection, selection_size, buffer, buffershape, sizeof(buffer));
      CUTEST_ASSERT("Out-of-bounds index not rejected by set", rc == BLOSC2_ERROR_INVALID_INDEX);
    }
  }
  int32_t all[100];
  B2ND_TEST_ASSERT(b2nd_to_cbuffer(array, all, sizeof(all)));
  CUTEST_ASSERT("Rejected set changed the array", memcmp(all, src, sizeof(src)) == 0);

  // Negative sizes whose product looks like a valid one-item buffer
  int64_t sel_one[] = {0};
  int64_t *selection_neg_size[] = {sel_one, sel_one};
  int64_t neg_size[] = {-1, -1};
  int32_t one;
  CUTEST_ASSERT("Negative selection size not rejected",
                b2nd_get_orthogonal_selection(array, selection_neg_size, neg_size, &one, buffershape, sizeof(one)) ==
                BLOSC2_ERROR_INVALID_PARAM);

  b2nd_free(array);
  b2nd_free_ctx(ctx);
  return 0;
}


CUTEST_TEST_SETUP(orthogonal_selection) {
  blosc2_init();
}

CUTEST_TEST_TEST(orthogonal_selection) {
  // In memory (plain super-chunk and frame) and on disk (contiguous and sparse frame)
  CUTEST_ASSERT("Orthogonal selection regression check failed (memory)",
                check_get_and_set_orthogonal_selection(false, NULL) == 0);
  CUTEST_ASSERT("Orthogonal selection regression check failed (cframe)",
                check_get_and_set_orthogonal_selection(true, NULL) == 0);
  CUTEST_ASSERT("Orthogonal selection regression check failed (disk cframe)",
                check_get_and_set_orthogonal_selection(true, "test_orthogonal_selection.b2nd") == 0);
  CUTEST_ASSERT("Orthogonal selection regression check failed (disk sframe)",
                check_get_and_set_orthogonal_selection(false, "test_orthogonal_selection_s.b2nd") == 0);
  CUTEST_ASSERT("Out-of-bounds check failed", check_out_of_bounds_selection() == 0);
  return 0;
}

CUTEST_TEST_TEARDOWN(orthogonal_selection) {
  blosc2_destroy();
}

int main(void) {
  CUTEST_TEST_RUN(orthogonal_selection);
}
