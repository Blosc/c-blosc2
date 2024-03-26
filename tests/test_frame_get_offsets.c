/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdio.h>
#include <stdint.h>

#include "blosc2.h"
#include "cutest.h"


#define NCHUNKS (10)
#define CHUNKSHAPE (5 * 1000)
#define NTHREADS 4

enum {
  CHECK_ZEROS = 1,
};

typedef struct {
  bool contiguous;
  char *urlpath;
} test_fill_special_backend;

CUTEST_TEST_DATA(fill_special) {
  blosc2_cparams cparams;
  blosc2_dparams dparams;
};

CUTEST_TEST_SETUP(fill_special) {
  blosc2_init();
  data->cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_cparams* cparams = &data->cparams;
  cparams->typesize = sizeof(float);
  cparams->clevel = 9;
  cparams->nthreads = NTHREADS;
  data->dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_dparams* dparams = &data->dparams;
  dparams->nthreads = NTHREADS;

  CUTEST_PARAMETRIZE(nchunks, int, CUTEST_DATA(
          5,
          10,
  ));
  CUTEST_PARAMETRIZE(backend, test_fill_special_backend, CUTEST_DATA(
      {true, NULL},  // memory - cframe
      {true, "test_fill_special.b2frame"}, // disk - cframe
      {false, "test_fill_special_s.b2frame"}, // disk - sframe
  ));
}


CUTEST_TEST_TEST(fill_special) {
  blosc2_cparams* cparams = &data->cparams;
  blosc2_dparams* dparams = &data->dparams;
  int32_t isize = CHUNKSHAPE * cparams->typesize;

  CUTEST_GET_PARAMETER(nchunks, int);
  CUTEST_GET_PARAMETER(backend, test_fill_special_backend);

  // Remove a possible stale sparse frame
  blosc2_remove_urlpath(backend.urlpath);

  /* Create a super-chunk container */
  blosc2_storage storage = {
          .cparams=cparams, .dparams=dparams,
          .urlpath=backend.urlpath, .contiguous=backend.contiguous};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  CUTEST_ASSERT("Error creating schunk", schunk != NULL);


  int32_t *data_ = malloc(isize);
  int chunksize = isize / schunk->typesize;
  int64_t _nchunks;
  for (int j = 0; j < chunksize; j++) {
    data_[j] = j + chunksize;
  }
  for (int i = 0; i < nchunks; ++i) {
    _nchunks = blosc2_schunk_append_buffer(schunk, data_, isize);
    CUTEST_ASSERT("ERROR: bad append in frame", _nchunks >= 0);
  }

  int64_t *offsets = blosc2_frame_get_offsets(schunk);

  if (schunk->storage->urlpath != NULL && !schunk->storage->contiguous) {
    for (int i = 0; i < schunk->nchunks; ++i) {
      CUTEST_ASSERT("Error getting the offsets", offsets[i] == i);
    }
  } else {
    int64_t chunk_size = offsets[1] - offsets[0];
    bool needs_free;
    uint8_t* chunk;
    int32_t nbytes_, cbytes_, blocksize;
    int dsize = blosc2_schunk_get_chunk(schunk, 0, &chunk, &needs_free);
    CUTEST_ASSERT("ERROR: chunk cannot be retrieved correctly.", dsize >= 0);

    blosc2_cbuffer_sizes(chunk, &nbytes_, &cbytes_, &blocksize);
    CUTEST_ASSERT("ERROR: chunk size is not the expected.", chunk_size == cbytes_);
    if (needs_free) {
      free(chunk);
    }
    for (int i = 1; i < schunk->nchunks; ++i) {
      dsize = blosc2_schunk_get_chunk(schunk, i-1, &chunk, &needs_free);
      CUTEST_ASSERT("ERROR: chunk cannot be retrieved correctly.", dsize >= 0);
      blosc2_cbuffer_sizes(chunk, &nbytes_, &cbytes_, &blocksize);
      if (needs_free) {
        free(chunk);
      }
      CUTEST_ASSERT("Error getting the offsets", (offsets[i] - offsets[i - 1]) == cbytes_);
    }
  }


  /* Free resources */
  blosc2_schunk_free(schunk);
  free(offsets);

  return 0;
}

CUTEST_TEST_TEARDOWN(fill_special) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_destroy();
}


int main() {
  CUTEST_TEST_RUN(fill_special);
}
