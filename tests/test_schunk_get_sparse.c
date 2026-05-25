/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Unit tests for blosc2_schunk_get_sparse_buffer().

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
**********************************************************************/

#include "test_common.h"

#include <inttypes.h>

#define CHUNK_ITEMS 128
#define BLOCK_ITEMS 32
#define NCHUNKS 8
#define NITEMS (CHUNK_ITEMS * NCHUNKS)
#define NCOORDS 32
#define URLPATH "test_schunk_get_sparse.b2frame"

int tests_run = 0;
static int callback_calls = 0;

static void sparse_test_callback(void *callback_data, void (*dojob)(void *),
                                 int numjobs, size_t jobdata_elsize, void *jobdata) {
  UNUSED(callback_data);
  callback_calls++;
  for (int i = 0; i < numjobs; ++i) {
    dojob((uint8_t *)jobdata + (size_t)i * jobdata_elsize);
  }
}

static void fill_ref(uint8_t *ref, int64_t nitems, int32_t typesize) {
  for (int64_t i = 0; i < nitems; ++i) {
    uint64_t value = (uint64_t)i * UINT64_C(1315423911) + UINT64_C(0x12345678);
    memcpy(ref + i * typesize, &value, (size_t)typesize);
  }
}

static int create_regular_schunk(blosc2_schunk **schunk, int32_t typesize, int filter,
                                 bool frame, int16_t nthreads, int64_t nitems,
                                 uint8_t **ref_out) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  cparams.blocksize = BLOCK_ITEMS * typesize;
  cparams.clevel = 5;
  memset(cparams.filters, 0, sizeof(cparams.filters));
  if (filter == BLOSC_DELTA) {
    cparams.filters[BLOSC2_MAX_FILTERS - 2] = BLOSC_DELTA;
    cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_BITSHUFFLE;
  }
  else {
    cparams.filters[BLOSC2_MAX_FILTERS - 1] = (uint8_t)filter;
  }
  cparams.nthreads = nthreads;
  dparams.nthreads = nthreads;

  if (frame) {
    blosc2_remove_urlpath(URLPATH);
  }
  blosc2_storage storage = {.contiguous = frame, .urlpath = frame ? URLPATH : NULL,
                            .cparams = &cparams, .dparams = &dparams};
  *schunk = blosc2_schunk_new(&storage);
  if (*schunk == NULL) {
    return BLOSC2_ERROR_FAILURE;
  }

  uint8_t *ref = malloc((size_t)nitems * (size_t)typesize);
  uint8_t *chunk = malloc((size_t)CHUNK_ITEMS * (size_t)typesize);
  if (ref == NULL || chunk == NULL) {
    free(ref);
    free(chunk);
    return BLOSC2_ERROR_MEMORY_ALLOC;
  }
  fill_ref(ref, nitems, typesize);

  for (int64_t start = 0; start < nitems; start += CHUNK_ITEMS) {
    int64_t stop = start + CHUNK_ITEMS;
    if (stop > nitems) {
      stop = nitems;
    }
    int64_t chunk_items = stop - start;
    memcpy(chunk, ref + start * typesize, (size_t)chunk_items * (size_t)typesize);
    int64_t nchunks = blosc2_schunk_append_buffer(*schunk, chunk, (int32_t)(chunk_items * typesize));
    if (nchunks < 0) {
      free(ref);
      free(chunk);
      return BLOSC2_ERROR_FAILURE;
    }
  }
  free(chunk);

  if (frame) {
    blosc2_schunk_free(*schunk);
    *schunk = blosc2_schunk_open(URLPATH);
    if (*schunk == NULL) {
      free(ref);
      return BLOSC2_ERROR_FAILURE;
    }
    blosc2_free_ctx((*schunk)->dctx);
    blosc2_dparams dparams2 = BLOSC2_DPARAMS_DEFAULTS;
    dparams2.nthreads = nthreads;
    dparams2.schunk = *schunk;
    (*schunk)->dctx = blosc2_create_dctx(dparams2);
    if ((*schunk)->dctx == NULL) {
      free(ref);
      return BLOSC2_ERROR_FAILURE;
    }
  }

  *ref_out = ref;
  return BLOSC2_ERROR_SUCCESS;
}

static const int64_t coords[NCOORDS] = {
    0, 1, 2, 31, 32, 33, 63, 64,
    65, 127, 128, 129, 130, 191, 192, 255,
    256, 257, 300, 383, 384, 511, 512, 513,
    700, 701, 702, 895, 896, 897, 1022, 1023
};

static char *check_sparse_regular(bool frame, int32_t typesize, int filter, int16_t nthreads,
                                  int64_t nitems, const int64_t *selection, int64_t nselection) {
  blosc2_schunk *schunk = NULL;
  uint8_t *ref = NULL;
  uint8_t *out = malloc((size_t)nselection * (size_t)typesize);
  mu_assert("Could not allocate output", out != NULL);

  int rc = create_regular_schunk(&schunk, typesize, filter, frame, nthreads, nitems, &ref);
  mu_assert("Could not create schunk", rc == BLOSC2_ERROR_SUCCESS);

  rc = blosc2_schunk_get_sparse_buffer(schunk, nselection, selection, out);
  mu_assert("blosc2_schunk_get_sparse_buffer failed", rc == BLOSC2_ERROR_SUCCESS);

  for (int64_t i = 0; i < nselection; ++i) {
    if (memcmp(out + i * typesize, ref + selection[i] * typesize, (size_t)typesize) != 0) {
      fprintf(stderr, "Bad sparse value at %" PRId64 ": coord=%" PRId64 "\n", i, selection[i]);
      return "Sparse value mismatch";
    }
  }

  free(out);
  free(ref);
  blosc2_schunk_free(schunk);
  if (frame) {
    blosc2_remove_urlpath(URLPATH);
  }
  return NULL;
}

static char *test_sparse_memory(void) {
  return check_sparse_regular(false, sizeof(int32_t), BLOSC_SHUFFLE, 4, NITEMS, coords, NCOORDS);
}

static char *test_sparse_frame(void) {
  return check_sparse_regular(true, sizeof(int32_t), BLOSC_SHUFFLE, 4, NITEMS, coords, NCOORDS);
}

static char *test_sparse_nthreads1(void) {
  return check_sparse_regular(false, sizeof(int32_t), BLOSC_SHUFFLE, 1, NITEMS, coords, NCOORDS);
}

static char *test_sparse_typesizes_filters(void) {
  char *msg;
  msg = check_sparse_regular(false, sizeof(int16_t), BLOSC_NOFILTER, 4, NITEMS, coords, NCOORDS);
  if (msg != NULL) return msg;
  msg = check_sparse_regular(false, sizeof(double), BLOSC_BITSHUFFLE, 4, NITEMS, coords, NCOORDS);
  if (msg != NULL) return msg;
  return NULL;
}

static char *test_sparse_repeated_reverse_partial(void) {
  static const int64_t selection[] = {
      999, 998, 997, 997, 500, 129, 128, 127, 126, 33, 32, 31, 31, 0
  };
  return check_sparse_regular(false, sizeof(int32_t), BLOSC_SHUFFLE, 4, 1000,
                              selection, (int64_t)ARRAY_SIZE(selection));
}

static char *test_sparse_delta_fallback(void) {
  return check_sparse_regular(false, sizeof(int32_t), BLOSC_DELTA, 4, NITEMS, coords, NCOORDS);
}

static char *test_sparse_callback_backend(void) {
  callback_calls = 0;
  blosc2_set_threads_callback(sparse_test_callback, NULL);
  char *msg = check_sparse_regular(false, sizeof(int32_t), BLOSC_SHUFFLE, 4, NITEMS, coords, NCOORDS);
  blosc2_set_threads_callback(NULL, NULL);
  if (msg != NULL) return msg;
  mu_assert("thread callback backend was not used", callback_calls > 0);
  return NULL;
}

static char *test_sparse_special_zero(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  cparams.typesize = sizeof(float);
  cparams.blocksize = BLOCK_ITEMS * sizeof(float);
  dparams.nthreads = 4;
  blosc2_storage storage = {.cparams = &cparams, .dparams = &dparams};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("Could not create special schunk", schunk != NULL);

  int64_t nchunks = blosc2_schunk_fill_special(schunk, NITEMS, BLOSC2_SPECIAL_ZERO,
                                               CHUNK_ITEMS * (int32_t)sizeof(float));
  mu_assert("Could not fill special schunk", nchunks > 0);

  float out[NCOORDS];
  int rc = blosc2_schunk_get_sparse_buffer(schunk, NCOORDS, coords, out);
  mu_assert("blosc2_schunk_get_sparse_buffer failed for special zero", rc == BLOSC2_ERROR_SUCCESS);
  for (int i = 0; i < NCOORDS; ++i) {
    mu_assert("Special zero mismatch", out[i] == 0.0f);
  }
  blosc2_schunk_free(schunk);
  return NULL;
}

static char *test_sparse_bounds(void) {
  blosc2_schunk *schunk = NULL;
  uint8_t *ref = NULL;
  int32_t out[1];
  int64_t bad_coord[1] = {NITEMS};
  int rc = create_regular_schunk(&schunk, sizeof(int32_t), BLOSC_SHUFFLE, false, 4, NITEMS, &ref);
  mu_assert("Could not create schunk", rc == BLOSC2_ERROR_SUCCESS);

  rc = blosc2_schunk_get_sparse_buffer(schunk, 1, bad_coord, out);
  free(ref);
  blosc2_schunk_free(schunk);
  mu_assert("Out-of-bounds coordinate should fail", rc == BLOSC2_ERROR_INVALID_PARAM);
  return NULL;
}

static char *all_tests(void) {
  mu_run_test(test_sparse_memory);
  mu_run_test(test_sparse_frame);
  mu_run_test(test_sparse_nthreads1);
  mu_run_test(test_sparse_typesizes_filters);
  mu_run_test(test_sparse_repeated_reverse_partial);
  mu_run_test(test_sparse_delta_fallback);
  mu_run_test(test_sparse_callback_backend);
  mu_run_test(test_sparse_special_zero);
  mu_run_test(test_sparse_bounds);
  return NULL;
}

int main(void) {
  blosc2_init();

  char *result = all_tests();
  if (result != NULL) {
    printf("%s\n", result);
  }
  else {
    printf("\nALL TESTS PASSED\n");
  }
  printf("Tests run: %d\n", tests_run);

  blosc2_destroy();
  return result != NULL;
}
