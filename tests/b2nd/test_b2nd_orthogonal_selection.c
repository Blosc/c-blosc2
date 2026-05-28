/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"


static int check_get_and_set_orthogonal_selection(void) {
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

  b2nd_free(array);
  b2nd_free_ctx(ctx);
  return 0;
}


CUTEST_TEST_SETUP(orthogonal_selection) {
  blosc2_init();
}

CUTEST_TEST_TEST(orthogonal_selection) {
  CUTEST_ASSERT("Orthogonal selection regression check failed",
                check_get_and_set_orthogonal_selection() == 0);
  return 0;
}

CUTEST_TEST_TEARDOWN(orthogonal_selection) {
  blosc2_destroy();
}

int main(void) {
  CUTEST_TEST_RUN(orthogonal_selection);
}
