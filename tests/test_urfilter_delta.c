/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "blosc2.h"
#include "cutest.h"

#define USER_FILTER_ID 246
#define CHUNKSIZE (3 * 1024)

static int byte_forward(const uint8_t* src, uint8_t* dest, int32_t size,
                        uint8_t meta, blosc2_cparams* cparams, uint8_t id) {
  BLOSC_UNUSED_PARAM(meta);
  BLOSC_UNUSED_PARAM(cparams);
  BLOSC_UNUSED_PARAM(id);

  for (int32_t i = 0; i < size; ++i) {
    dest[i] = (uint8_t)(src[i] + 17);
  }
  return BLOSC2_ERROR_SUCCESS;
}

static int byte_backward(const uint8_t* src, uint8_t* dest, int32_t size,
                         uint8_t meta, blosc2_dparams* dparams, uint8_t id) {
  BLOSC_UNUSED_PARAM(meta);
  BLOSC_UNUSED_PARAM(dparams);
  BLOSC_UNUSED_PARAM(id);

  for (int32_t i = 0; i < size; ++i) {
    dest[i] = (uint8_t)(src[i] - 17);
  }
  return BLOSC2_ERROR_SUCCESS;
}

CUTEST_TEST_DATA(urfilter_delta) {
  uint8_t src[CHUNKSIZE];
  uint8_t dest[CHUNKSIZE];
};

CUTEST_TEST_SETUP(urfilter_delta) {
  BLOSC_UNUSED_PARAM(data);

  blosc2_init();

  blosc2_filter filter = {
      .id = USER_FILTER_ID,
      .name = "test_urfilter_delta",
      .version = 1,
      .forward = byte_forward,
      .backward = byte_backward,
  };
  blosc2_register_filter(&filter);
}

CUTEST_TEST_TEST(urfilter_delta) {
  for (int32_t i = 0; i < CHUNKSIZE; ++i) {
    data->src[i] = (uint8_t)((i * 31 + 17) & 0xff);
  }

  const char* names[] = {
      "user-filter-before-delta",
      "shuffle-before-delta",
      "bitshuffle-before-delta",
  };
  uint8_t pipelines[][BLOSC2_MAX_FILTERS] = {
      {USER_FILTER_ID, BLOSC_DELTA, 0, 0, 0, 0},
      {BLOSC_SHUFFLE, BLOSC_DELTA, 0, 0, 0, 0},
      {BLOSC_BITSHUFFLE, BLOSC_DELTA, 0, 0, 0, 0},
  };

  for (int i = 0; i < 3; ++i) {
    memset(data->dest, 0, CHUNKSIZE);

    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.compcode = BLOSC_ZSTD;
    cparams.clevel = 5;
    cparams.typesize = 4;
    cparams.nthreads = 1;
    cparams.blocksize = CHUNKSIZE;
    memset(cparams.filters, 0, BLOSC2_MAX_FILTERS);
    memset(cparams.filters_meta, 0, BLOSC2_MAX_FILTERS);
    memcpy(cparams.filters, pipelines[i], BLOSC2_MAX_FILTERS);

    blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
    dparams.nthreads = 1;

    blosc2_storage storage = {
        .contiguous = true,
        .cparams = &cparams,
        .dparams = &dparams,
    };
    blosc2_schunk* schunk = blosc2_schunk_new(&storage);
    CUTEST_ASSERT("schunk creation failed", schunk != NULL);

    int64_t nchunks = blosc2_schunk_append_buffer(schunk, data->src, CHUNKSIZE);
    CUTEST_ASSERT("append failed", nchunks == 1);

    uint8_t* cframe = NULL;
    bool cframe_needs_free = false;
    int64_t cframe_len = blosc2_schunk_to_buffer(schunk, &cframe, &cframe_needs_free);
    CUTEST_ASSERT("cframe export failed", cframe_len > 0);

    blosc2_schunk* schunk2 = blosc2_schunk_from_buffer(cframe, cframe_len, true);
    CUTEST_ASSERT("cframe import failed", schunk2 != NULL);

    int32_t nbytes = blosc2_schunk_decompress_chunk(schunk2, 0, data->dest, CHUNKSIZE);
    CUTEST_ASSERT("decompression failed", nbytes == CHUNKSIZE);
    CUTEST_ASSERT(names[i], memcmp(data->src, data->dest, CHUNKSIZE) == 0);

    blosc2_schunk_free(schunk2);
    if (cframe_needs_free) {
      free(cframe);
    }
    blosc2_schunk_free(schunk);
  }

  return BLOSC2_ERROR_SUCCESS;
}

CUTEST_TEST_TEARDOWN(urfilter_delta) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_destroy();
}

int main(void) {
  CUTEST_TEST_RUN(urfilter_delta);
}
