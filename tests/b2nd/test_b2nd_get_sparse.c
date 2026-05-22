/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"


typedef struct {
  int32_t a;
  int32_t b;
} pair_t;

static int check_pair_selection(b2nd_array_t *array, const pair_t *src,
                                const int64_t *coords, int64_t ncoords) {
  pair_t *out = calloc((size_t)ncoords, sizeof(pair_t));
  CUTEST_ASSERT("Allocation failed", out != NULL);
  B2ND_TEST_ASSERT(b2nd_get_sparse_cbuffer(array, ncoords, coords, out,
                                           ncoords * (int64_t)sizeof(pair_t)));
  for (int64_t i = 0; i < ncoords; ++i) {
    CUTEST_ASSERT("Field a mismatch", out[i].a == src[coords[i]].a);
    CUTEST_ASSERT("Field b mismatch", out[i].b == src[coords[i]].b);
  }
  free(out);
  return 0;
}

static int check_1d_non_behaved(void) {
  const int8_t ndim = 1;
  const int64_t shape[] = {100};
  const int32_t chunkshape[] = {44};
  const int32_t blockshape[] = {33};

  pair_t src[100];
  for (int64_t i = 0; i < 100; ++i) {
    src[i].a = (int32_t)(i + 1);
    src[i].b = (int32_t)(200 - i);
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(pair_t);
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
  B2ND_TEST_ASSERT(b2nd_from_cbuffer(ctx, &array, src, sizeof(src)));

  int64_t sorted[98];
  for (int64_t i = 0; i < 98; ++i) {
    sorted[i] = i + 2;
  }
  check_pair_selection(array, src, sorted, 98);

  int64_t reversed[98];
  for (int64_t i = 0; i < 98; ++i) {
    reversed[i] = 99 - i;
  }
  check_pair_selection(array, src, reversed, 98);

  const int64_t repeated[] = {5, 1, 5, 99, 0, 44, 43};
  check_pair_selection(array, src, repeated, 7);

  b2nd_free(array);
  b2nd_free_ctx(ctx);
  return 0;
}

static int check_2d_flat_logical_coords(void) {
  const int8_t ndim = 2;
  const int64_t shape[] = {6, 7};
  const int32_t chunkshape[] = {4, 5};
  const int32_t blockshape[] = {3, 4};
  const int64_t nitems = shape[0] * shape[1];

  int32_t src[42];
  for (int64_t i = 0; i < nitems; ++i) {
    src[i] = (int32_t)(i + 10);
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

  const int64_t coords[] = {0, 6, 7, 13, 20, 41, 20, 5};
  const int64_t ncoords = (int64_t)(sizeof(coords) / sizeof(coords[0]));
  int32_t out[8] = {0};
  B2ND_TEST_ASSERT(b2nd_get_sparse_cbuffer(array, ncoords, coords, out, sizeof(out)));
  for (int64_t i = 0; i < ncoords; ++i) {
    CUTEST_ASSERT("2-D sparse value mismatch", out[i] == src[coords[i]]);
  }

  b2nd_free(array);
  b2nd_free_ctx(ctx);
  return 0;
}

CUTEST_TEST_SETUP(get_sparse) {
  blosc2_init();
}

CUTEST_TEST_TEST(get_sparse) {
  CUTEST_ASSERT("1-D sparse check failed", check_1d_non_behaved() == 0);
  CUTEST_ASSERT("2-D sparse check failed", check_2d_flat_logical_coords() == 0);
  return 0;
}

CUTEST_TEST_TEARDOWN(get_sparse) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(get_sparse);
}
