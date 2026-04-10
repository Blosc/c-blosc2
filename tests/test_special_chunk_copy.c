/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Regression test for issue #611: copying a cframe with a SPECIAL chunk
  followed by a NORMAL chunk must not corrupt chunk ownership.
*/

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "blosc2.h"
#include "cutest.h"

#define CHUNK_NITEMS 1024

typedef struct {
  bool copy;
} special_chunk_copy_params;

CUTEST_TEST_DATA(special_chunk_copy) {
  blosc2_cparams cparams;
};

static int assert_chunk_kind(blosc2_schunk *schunk, int nchunk, bool expect_special) {
  uint8_t *chunk = NULL;
  bool needs_free = false;
  int cbytes = blosc2_schunk_get_chunk(schunk, nchunk, &chunk, &needs_free);
  CUTEST_ASSERT("Error getting chunk", cbytes >= 0);
  CUTEST_ASSERT("Unexpected chunk kind",
                (cbytes == BLOSC_EXTENDED_HEADER_LENGTH) == expect_special);
  if (needs_free) {
    free(chunk);
  }

  return 0;
}

static int assert_chunk_values(blosc2_schunk *schunk, int nchunk, const float *expected) {
  float *dest = malloc(CHUNK_NITEMS * sizeof(float));
  CUTEST_ASSERT("Allocation error", dest != NULL);

  int dsize = blosc2_schunk_decompress_chunk(
      schunk, nchunk, dest, (int32_t)(CHUNK_NITEMS * (int)sizeof(float)));
  CUTEST_ASSERT("Unexpected decompressed size",
                dsize == (int32_t)(CHUNK_NITEMS * (int)sizeof(float)));

  for (int i = 0; i < CHUNK_NITEMS; ++i) {
    CUTEST_ASSERT("Roundtrip mismatch", dest[i] == expected[i]);
  }

  free(dest);

  return 0;
}

CUTEST_TEST_SETUP(special_chunk_copy) {
  blosc2_init();

  data->cparams = BLOSC2_CPARAMS_DEFAULTS;
  data->cparams.typesize = sizeof(float);
  data->cparams.clevel = 5;

  CUTEST_PARAMETRIZE(params, special_chunk_copy_params, CUTEST_DATA(
      {false},
      {true}
  ));
}

CUTEST_TEST_TEST(special_chunk_copy) {
  CUTEST_GET_PARAMETER(params, special_chunk_copy_params);

  float *zeros = calloc(CHUNK_NITEMS, sizeof(float));
  float *ones = malloc(CHUNK_NITEMS * sizeof(float));
  CUTEST_ASSERT("Allocation error", zeros != NULL);
  CUTEST_ASSERT("Allocation error", ones != NULL);

  for (int i = 0; i < CHUNK_NITEMS; ++i) {
    ones[i] = 1.0f;
  }

  blosc2_storage storage = {
      .contiguous = false,
      .cparams = &data->cparams,
  };
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  CUTEST_ASSERT("Error creating schunk", schunk != NULL);

  int64_t rc = blosc2_schunk_append_buffer(
      schunk, zeros, (int32_t)(CHUNK_NITEMS * (int)sizeof(float)));
  CUTEST_ASSERT("Error appending special chunk", rc == 1);

  rc = blosc2_schunk_append_buffer(
      schunk, ones, (int32_t)(CHUNK_NITEMS * (int)sizeof(float)));
  CUTEST_ASSERT("Error appending normal chunk", rc == 2);

  CUTEST_ASSERT("Error checking special chunk kind", assert_chunk_kind(schunk, 0, true) == 0);
  CUTEST_ASSERT("Error checking normal chunk kind", assert_chunk_kind(schunk, 1, false) == 0);
  CUTEST_ASSERT("Error checking special chunk values", assert_chunk_values(schunk, 0, zeros) == 0);
  CUTEST_ASSERT("Error checking normal chunk values", assert_chunk_values(schunk, 1, ones) == 0);

  uint8_t *cframe = NULL;
  bool cframe_needs_free = false;
  int64_t cframe_len = blosc2_schunk_to_buffer(schunk, &cframe, &cframe_needs_free);
  CUTEST_ASSERT("Error serializing schunk", cframe_len > 0);
  CUTEST_ASSERT("Expected owned cframe copy", cframe_needs_free == true);

  blosc2_schunk *copied = blosc2_schunk_from_buffer(cframe, cframe_len, params.copy);
  CUTEST_ASSERT("Error recreating schunk from buffer", copied != NULL);

  CUTEST_ASSERT("Error checking copied special chunk kind", assert_chunk_kind(copied, 0, true) == 0);
  CUTEST_ASSERT("Error checking copied normal chunk kind", assert_chunk_kind(copied, 1, false) == 0);
  CUTEST_ASSERT("Error checking copied special chunk values", assert_chunk_values(copied, 0, zeros) == 0);
  CUTEST_ASSERT("Error checking copied normal chunk values", assert_chunk_values(copied, 1, ones) == 0);

  blosc2_schunk_free(copied);
  blosc2_schunk_free(schunk);
  free(cframe);
  free(ones);
  free(zeros);

  return 0;
}

CUTEST_TEST_TEARDOWN(special_chunk_copy) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_destroy();
}

int main(void) {
  CUTEST_TEST_RUN(special_chunk_copy);
}
