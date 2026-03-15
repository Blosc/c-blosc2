/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdio.h>
#include <string.h>

#include "frame.h"
#include "test_common.h"

int tests_run = 0;

typedef struct {
  bool contiguous;
  const char *urlpath;
} test_data;

static test_data tdata;

static test_data backends[] = {
    {false, NULL},
    {true, "test_variable_chunks.b2frame"},
    {false, "test_variable_chunks_s.b2frame"},
};

static const char *values[] = {
    "alpha",
    "bravo bravo",
    "charlie-charlie-charlie",
};

static const char *updated_values[] = {
    "alpha",
    "bravo bravo bravo bravo",
    "tiny",
};

static const char *fixed_values[] = {
    "one",
    "two",
    "six",
};

static int append_string_chunk(blosc2_schunk *schunk, const char *value) {
  int32_t nbytes = (int32_t)strlen(value) + 1;
  uint8_t *chunk = malloc((size_t)nbytes + BLOSC2_MAX_OVERHEAD);
  int cbytes = blosc2_compress_ctx(schunk->cctx, value, nbytes, chunk, nbytes + BLOSC2_MAX_OVERHEAD);
  if (cbytes <= 0) {
    free(chunk);
    return cbytes;
  }
  return (int)blosc2_schunk_append_chunk(schunk, chunk, false);
}

static int update_string_chunk(blosc2_schunk *schunk, int64_t index, const char *value) {
  int32_t nbytes = (int32_t)strlen(value) + 1;
  uint8_t *chunk = malloc((size_t)nbytes + BLOSC2_MAX_OVERHEAD);
  int cbytes = blosc2_compress_ctx(schunk->cctx, value, nbytes, chunk, nbytes + BLOSC2_MAX_OVERHEAD);
  if (cbytes <= 0) {
    free(chunk);
    return cbytes;
  }
  return (int)blosc2_schunk_update_chunk(schunk, index, chunk, false);
}

static int assert_values(blosc2_schunk *schunk, const char *expected[], int nitems) {
  for (int i = 0; i < nitems; ++i) {
    char buffer[64] = {0};
    int32_t dsize = blosc2_schunk_decompress_chunk(schunk, i, buffer, (int32_t)sizeof(buffer));
    if (dsize != (int32_t)strlen(expected[i]) + 1) {
      return -1;
    }
    if (strcmp(buffer, expected[i]) != 0) {
      return -2;
    }
  }
  return 0;
}

static int assert_variable_chunk_flag(blosc2_schunk *schunk, bool expected) {
  uint8_t *cframe = NULL;
  bool needs_free = false;
  int64_t cframe_len = blosc2_schunk_to_buffer(schunk, &cframe, &needs_free);
  if (cframe_len < FRAME_HEADER_MINLEN || cframe == NULL) {
    return -1;
  }

  bool actual = (cframe[FRAME_FLAGS] & FRAME_VARIABLE_CHUNKS) != 0;
  if (needs_free) {
    free(cframe);
  }
  return actual == expected ? 0 : -2;
}

static int assert_frame_version(blosc2_schunk *schunk, uint8_t expected) {
  uint8_t *cframe = NULL;
  bool needs_free = false;
  int64_t cframe_len = blosc2_schunk_to_buffer(schunk, &cframe, &needs_free);
  if (cframe_len < FRAME_HEADER_MINLEN || cframe == NULL) {
    return -1;
  }

  uint8_t actual = cframe[FRAME_FLAGS] & 0x0fu;
  if (needs_free) {
    free(cframe);
  }
  return actual == expected ? 0 : -2;
}

static char *test_variable_chunks(void) {
  blosc2_remove_urlpath(tdata.urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_storage storage = {
      .contiguous = tdata.contiguous,
      .urlpath = (char *)tdata.urlpath,
      .cparams = &cparams,
      .dparams = &dparams,
  };

  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: cannot create schunk", schunk != NULL);

  for (int i = 0; i < 3; ++i) {
    int nchunks = append_string_chunk(schunk, values[i]);
    mu_assert("ERROR: chunk cannot be appended", nchunks == i + 1);
  }

  mu_assert("ERROR: schunk should switch to variable chunksize mode", schunk->chunksize == 0);
  mu_assert("ERROR: regular frame should keep cframe version 2",
            assert_frame_version(schunk, BLOSC2_VERSION_FRAME_FORMAT_RC1) == 0);
  mu_assert("ERROR: frame should signal variable chunk sizes",
            assert_variable_chunk_flag(schunk, true) == 0);
  mu_assert("ERROR: chunk cannot be decompressed", assert_values(schunk, values, 3) == 0);

  for (int i = 1; i < 3; ++i) {
    int nchunks = update_string_chunk(schunk, i, updated_values[i]);
    mu_assert("ERROR: chunk cannot be updated", nchunks == 3);
  }

  mu_assert("ERROR: schunk should keep variable chunksize mode", schunk->chunksize == 0);
  mu_assert("ERROR: updated frame should still signal variable chunk sizes",
            assert_variable_chunk_flag(schunk, true) == 0);
  mu_assert("ERROR: updated chunk cannot be decompressed", assert_values(schunk, updated_values, 3) == 0);

  blosc2_schunk_free(schunk);

  if (tdata.urlpath != NULL) {
    blosc2_schunk *reopened = blosc2_schunk_open(tdata.urlpath);
    mu_assert("ERROR: reopened schunk is NULL", reopened != NULL);
    mu_assert("ERROR: reopened schunk should keep variable chunksize mode", reopened->chunksize == 0);
    mu_assert("ERROR: reopened schunk nchunks mismatch", reopened->nchunks == 3);
    mu_assert("ERROR: reopened regular frame should keep cframe version 2",
              assert_frame_version(reopened, BLOSC2_VERSION_FRAME_FORMAT_RC1) == 0);
    mu_assert("ERROR: reopened frame should signal variable chunk sizes",
              assert_variable_chunk_flag(reopened, true) == 0);
    mu_assert("ERROR: reopened chunk cannot be decompressed", assert_values(reopened, updated_values, 3) == 0);
    blosc2_schunk_free(reopened);
  }

  blosc2_remove_urlpath(tdata.urlpath);
  return EXIT_SUCCESS;
}

static char *test_fixed_chunks_flag(void) {
  blosc2_remove_urlpath(tdata.urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_storage storage = {
      .contiguous = tdata.contiguous,
      .urlpath = (char *)tdata.urlpath,
      .cparams = &cparams,
      .dparams = &dparams,
  };

  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: cannot create schunk", schunk != NULL);

  for (int i = 0; i < 3; ++i) {
    int nchunks = append_string_chunk(schunk, fixed_values[i]);
    mu_assert("ERROR: fixed chunk cannot be appended", nchunks == i + 1);
  }

  mu_assert("ERROR: schunk should keep a fixed chunksize", schunk->chunksize == 4);
  mu_assert("ERROR: fixed-size regular frame should keep cframe version 2",
            assert_frame_version(schunk, BLOSC2_VERSION_FRAME_FORMAT_RC1) == 0);
  mu_assert("ERROR: frame should not signal variable chunk sizes",
            assert_variable_chunk_flag(schunk, false) == 0);
  mu_assert("ERROR: fixed chunk cannot be decompressed", assert_values(schunk, fixed_values, 3) == 0);

  blosc2_schunk_free(schunk);

  if (tdata.urlpath != NULL) {
    blosc2_schunk *reopened = blosc2_schunk_open(tdata.urlpath);
    mu_assert("ERROR: reopened schunk is NULL", reopened != NULL);
    mu_assert("ERROR: reopened schunk should keep a fixed chunksize", reopened->chunksize == 4);
    mu_assert("ERROR: reopened fixed-size regular frame should keep cframe version 2",
              assert_frame_version(reopened, BLOSC2_VERSION_FRAME_FORMAT_RC1) == 0);
    mu_assert("ERROR: reopened frame should not signal variable chunk sizes",
              assert_variable_chunk_flag(reopened, false) == 0);
    mu_assert("ERROR: reopened fixed chunk cannot be decompressed", assert_values(reopened, fixed_values, 3) == 0);
    blosc2_schunk_free(reopened);
  }

  blosc2_remove_urlpath(tdata.urlpath);
  return EXIT_SUCCESS;
}

static char *all_tests(void) {
  for (int i = 0; i < (int)ARRAY_SIZE(backends); ++i) {
    tdata = backends[i];
    mu_run_test(test_variable_chunks);
    mu_run_test(test_fixed_chunks_flag);
  }
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
