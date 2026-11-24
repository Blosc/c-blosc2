/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdio.h>
#include <string.h>

#include "blosc-private.h"
#include "frame.h"
#include "schunk-private.h"
#include "test_common.h"

int tests_run = 0;

static const char *test_urlpath = "test_schunk_slice_chunksize.b2frame";

static int append_chunk(blosc2_schunk *schunk, const char *value) {
  int32_t nbytes = (int32_t)strlen(value) + 1;
  uint8_t *chunk = malloc((size_t)nbytes + BLOSC2_MAX_OVERHEAD);
  int cbytes = blosc2_compress_ctx(schunk->cctx, value, nbytes, chunk, nbytes + BLOSC2_MAX_OVERHEAD);
  if (cbytes <= 0) {
    free(chunk);
    return cbytes;
  }
  return (int)blosc2_schunk_append_chunk(schunk, chunk, false);
}

static char *test_slice_null_and_bounds_params(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  blosc2_storage storage = {.cparams = &cparams};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: cannot create schunk", schunk != NULL);

  int32_t buf[16] = {0};
  int64_t *chunks_idx = NULL;
  int64_t start = 0;
  int64_t stop = 5;

  // Null pointers
  mu_assert("ERROR: NULL schunk not rejected in get_slice",
            blosc2_schunk_get_slice_buffer(NULL, 0, 5, buf) == BLOSC2_ERROR_NULL_POINTER);
  mu_assert("ERROR: NULL buffer not rejected in get_slice",
            blosc2_schunk_get_slice_buffer(schunk, 0, 5, NULL) == BLOSC2_ERROR_NULL_POINTER);
  mu_assert("ERROR: NULL schunk not rejected in set_slice",
            blosc2_schunk_set_slice_buffer(NULL, 0, 5, buf) == BLOSC2_ERROR_NULL_POINTER);
  mu_assert("ERROR: NULL buffer not rejected in set_slice",
            blosc2_schunk_set_slice_buffer(schunk, 0, 5, NULL) == BLOSC2_ERROR_NULL_POINTER);
  mu_assert("ERROR: NULL schunk not rejected in schunk_get_slice_nchunks",
            schunk_get_slice_nchunks(NULL, 0, 5, &chunks_idx) == BLOSC2_ERROR_NULL_POINTER);
  mu_assert("ERROR: NULL chunks_idx not rejected in schunk_get_slice_nchunks",
            schunk_get_slice_nchunks(schunk, 0, 5, NULL) == BLOSC2_ERROR_NULL_POINTER);
  mu_assert("ERROR: NULL schunk not rejected in blosc2_get_slice_nchunks",
            blosc2_get_slice_nchunks(NULL, &start, &stop, &chunks_idx) == BLOSC2_ERROR_NULL_POINTER);
  mu_assert("ERROR: NULL start not rejected in blosc2_get_slice_nchunks",
            blosc2_get_slice_nchunks(schunk, NULL, &stop, &chunks_idx) == BLOSC2_ERROR_NULL_POINTER);
  mu_assert("ERROR: NULL stop not rejected in blosc2_get_slice_nchunks",
            blosc2_get_slice_nchunks(schunk, &start, NULL, &chunks_idx) == BLOSC2_ERROR_NULL_POINTER);
  mu_assert("ERROR: NULL chunks_idx not rejected in blosc2_get_slice_nchunks",
            blosc2_get_slice_nchunks(schunk, &start, &stop, NULL) == BLOSC2_ERROR_NULL_POINTER);

  // Invalid slice bounds
  mu_assert("ERROR: negative start not rejected in get_slice",
            blosc2_schunk_get_slice_buffer(schunk, -1, 5, buf) == BLOSC2_ERROR_INVALID_PARAM);
  mu_assert("ERROR: stop < start not rejected in get_slice",
            blosc2_schunk_get_slice_buffer(schunk, 5, 2, buf) == BLOSC2_ERROR_INVALID_PARAM);
  mu_assert("ERROR: negative start not rejected in set_slice",
            blosc2_schunk_set_slice_buffer(schunk, -1, 5, buf) == BLOSC2_ERROR_INVALID_PARAM);
  mu_assert("ERROR: stop < start not rejected in set_slice",
            blosc2_schunk_set_slice_buffer(schunk, 5, 2, buf) == BLOSC2_ERROR_INVALID_PARAM);
  mu_assert("ERROR: negative start not rejected in schunk_get_slice_nchunks",
            schunk_get_slice_nchunks(schunk, -1, 5, &chunks_idx) == BLOSC2_ERROR_INVALID_PARAM);
  mu_assert("ERROR: stop < start not rejected in schunk_get_slice_nchunks",
            schunk_get_slice_nchunks(schunk, 5, 2, &chunks_idx) == BLOSC2_ERROR_INVALID_PARAM);

  start = -1; stop = 5;
  mu_assert("ERROR: negative start not rejected in blosc2_get_slice_nchunks",
            blosc2_get_slice_nchunks(schunk, &start, &stop, &chunks_idx) == BLOSC2_ERROR_INVALID_PARAM);
  start = 5; stop = 2;
  mu_assert("ERROR: stop < start not rejected in blosc2_get_slice_nchunks",
            blosc2_get_slice_nchunks(schunk, &start, &stop, &chunks_idx) == BLOSC2_ERROR_INVALID_PARAM);

  // Empty slice (start == stop) returns 0 / success
  mu_assert("ERROR: empty slice failed in get_slice",
            blosc2_schunk_get_slice_buffer(schunk, 3, 3, buf) == BLOSC2_ERROR_SUCCESS);
  mu_assert("ERROR: empty slice failed in set_slice",
            blosc2_schunk_set_slice_buffer(schunk, 3, 3, buf) == BLOSC2_ERROR_SUCCESS);
  mu_assert("ERROR: empty slice failed in schunk_get_slice_nchunks",
            schunk_get_slice_nchunks(schunk, 3, 3, &chunks_idx) == 0 && chunks_idx == NULL);
  start = 3; stop = 3;
  mu_assert("ERROR: empty slice failed in blosc2_get_slice_nchunks",
            blosc2_get_slice_nchunks(schunk, &start, &stop, &chunks_idx) == 0 && chunks_idx == NULL);

  blosc2_schunk_free(schunk);
  return EXIT_SUCCESS;
}

static char *test_slice_variable_chunksize_rejected(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  blosc2_storage storage = {.contiguous = true, .cparams = &cparams};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: cannot create schunk", schunk != NULL);

  // Append chunks of different lengths to make schunk variable-chunksize (chunksize == 0)
  append_chunk(schunk, "hello");
  append_chunk(schunk, "world, hello again!");
  mu_assert("ERROR: chunksize must be 0 for variable chunks", schunk->chunksize == 0);

  char buf[32];
  int64_t *chunks_idx = NULL;
  int64_t start = 0;
  int64_t stop = 5;

  // Slices must cleanly return BLOSC2_ERROR_INVALID_PARAM instead of divide-by-zero
  int rc_get = blosc2_schunk_get_slice_buffer(schunk, 0, 5, buf);
  mu_assert("ERROR: get_slice_buffer should return INVALID_PARAM when chunksize <= 0",
            rc_get == BLOSC2_ERROR_INVALID_PARAM);

  int rc_set = blosc2_schunk_set_slice_buffer(schunk, 0, 5, buf);
  mu_assert("ERROR: set_slice_buffer should return INVALID_PARAM when chunksize <= 0",
            rc_set == BLOSC2_ERROR_INVALID_PARAM);

  int64_t rc_nchunks = schunk_get_slice_nchunks(schunk, 0, 5, &chunks_idx);
  mu_assert("ERROR: schunk_get_slice_nchunks should return INVALID_PARAM when chunksize <= 0",
            rc_nchunks == BLOSC2_ERROR_INVALID_PARAM);

  int64_t rc_pub = blosc2_get_slice_nchunks(schunk, &start, &stop, &chunks_idx);
  mu_assert("ERROR: blosc2_get_slice_nchunks should return INVALID_PARAM when chunksize <= 0",
            rc_pub == BLOSC2_ERROR_INVALID_PARAM);

  // Serialize to in-memory frame buffer and test blosc2_schunk_from_buffer
  uint8_t *cframe = NULL;
  bool needs_free = false;
  int64_t cframe_len = blosc2_schunk_to_buffer(schunk, &cframe, &needs_free);
  mu_assert("ERROR: cannot serialize schunk to buffer", cframe_len > 0 && cframe != NULL);

  blosc2_schunk *reopened = blosc2_schunk_from_buffer(cframe, cframe_len, false);
  mu_assert("ERROR: cannot reopen schunk from buffer", reopened != NULL);
  mu_assert("ERROR: reopened chunksize must be 0", reopened->chunksize == 0);

  mu_assert("ERROR: get_slice_buffer should fail on reopened variable-chunk schunk",
            blosc2_schunk_get_slice_buffer(reopened, 0, 5, buf) == BLOSC2_ERROR_INVALID_PARAM);
  mu_assert("ERROR: set_slice_buffer should fail on reopened variable-chunk schunk",
            blosc2_schunk_set_slice_buffer(reopened, 0, 5, buf) == BLOSC2_ERROR_INVALID_PARAM);
  mu_assert("ERROR: schunk_get_slice_nchunks should fail on reopened variable-chunk schunk",
            schunk_get_slice_nchunks(reopened, 0, 5, &chunks_idx) == BLOSC2_ERROR_INVALID_PARAM);
  start = 0; stop = 5;
  mu_assert("ERROR: blosc2_get_slice_nchunks should fail on reopened variable-chunk schunk",
            blosc2_get_slice_nchunks(reopened, &start, &stop, &chunks_idx) == BLOSC2_ERROR_INVALID_PARAM);

  blosc2_schunk_free(reopened);
  if (needs_free) {
    free(cframe);
  }
  blosc2_schunk_free(schunk);

  return EXIT_SUCCESS;
}

static char *test_slice_crafted_frame_zero_chunksize(void) {
  blosc2_remove_urlpath(test_urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  blosc2_storage storage = {
      .contiguous = true,
      .urlpath = (char *)test_urlpath,
      .cparams = &cparams,
  };
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: cannot create schunk", schunk != NULL);

  int32_t data[64];
  for (int i = 0; i < 64; ++i) {
    data[i] = i;
  }
  blosc2_schunk_append_buffer(schunk, data, sizeof(data));

  // Serialize to buffer
  uint8_t *cframe = NULL;
  bool needs_free = false;
  int64_t cframe_len = blosc2_schunk_to_buffer(schunk, &cframe, &needs_free);
  mu_assert("ERROR: cannot serialize schunk", cframe_len > FRAME_HEADER_MINLEN && cframe != NULL);

  // Craft frame by zeroing the chunksize field at FRAME_CHUNKSIZE (offset 58)
  uint8_t *crafted = malloc((size_t)cframe_len);
  memcpy(crafted, cframe, (size_t)cframe_len);
  int32_t zero_chunksize = 0;
  to_big(crafted + FRAME_CHUNKSIZE, &zero_chunksize, sizeof(zero_chunksize));

  blosc2_schunk *schunk_crafted = blosc2_schunk_from_buffer(crafted, cframe_len, true);
  if (schunk_crafted != NULL) {
    int32_t buf[16];
    int64_t *chunks_idx = NULL;
    int rc = blosc2_schunk_get_slice_buffer(schunk_crafted, 0, 10, buf);
    mu_assert("ERROR: crafted zero chunksize must be rejected in get_slice_buffer",
              rc == BLOSC2_ERROR_INVALID_PARAM);

    rc = blosc2_schunk_set_slice_buffer(schunk_crafted, 0, 10, buf);
    mu_assert("ERROR: crafted zero chunksize must be rejected in set_slice_buffer",
              rc == BLOSC2_ERROR_INVALID_PARAM);

    int64_t rc64 = schunk_get_slice_nchunks(schunk_crafted, 0, 10, &chunks_idx);
    mu_assert("ERROR: crafted zero chunksize must be rejected in schunk_get_slice_nchunks",
              rc64 == BLOSC2_ERROR_INVALID_PARAM);

    int64_t start = 0, stop = 10;
    rc64 = blosc2_get_slice_nchunks(schunk_crafted, &start, &stop, &chunks_idx);
    mu_assert("ERROR: crafted zero chunksize must be rejected in blosc2_get_slice_nchunks",
              rc64 == BLOSC2_ERROR_INVALID_PARAM);

    blosc2_schunk_free(schunk_crafted);
  }

  free(crafted);
  if (needs_free) {
    free(cframe);
  }
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(test_urlpath);

  return EXIT_SUCCESS;
}

static char *all_tests(void) {
  mu_run_test(test_slice_null_and_bounds_params);
  mu_run_test(test_slice_variable_chunksize_rejected);
  mu_run_test(test_slice_crafted_frame_zero_chunksize);
  return EXIT_SUCCESS;
}

int main(void) {
  char *result;
  blosc2_init();
  result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("	Tests run: %d\n", tests_run);
  blosc2_destroy();
  return result != EXIT_SUCCESS;
}
