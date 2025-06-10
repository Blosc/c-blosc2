/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

*/

#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "blosc2.h"
#include "cutest.h"


// Exceed > 2 GB in size for more thorough tests
#define NCHUNKS (600)
#define CHUNKSHAPE (1000 * 1000)
#define NTHREADS 4

enum {
  CHECK_ZEROS = 1,
  CHECK_NANS = 2,
  CHECK_UNINIT = 3,
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

  CUTEST_PARAMETRIZE(svalue, int, CUTEST_DATA(
          CHECK_ZEROS,
          CHECK_NANS,
          CHECK_UNINIT,
  ));
  CUTEST_PARAMETRIZE(leftover_items, int, CUTEST_DATA(
          0,
          1,
          10,
  ));
  CUTEST_PARAMETRIZE(backend, test_fill_special_backend, CUTEST_DATA(
      {false, NULL},  // memory - schunk
      {true, NULL},  // memory - cframe
      {true, "test_fill_special.b2frame"}, // disk - cframe
      {false, "test_fill_special_s.b2frame"}, // disk - sframe
  ));
}


CUTEST_TEST_TEST(fill_special) {
  blosc2_cparams* cparams = &data->cparams;
  blosc2_dparams* dparams = &data->dparams;
  int32_t isize = CHUNKSHAPE * cparams->typesize;
  int32_t* data_dest = malloc(isize);
  int nchunk;

  CUTEST_GET_PARAMETER(svalue, int);
  CUTEST_GET_PARAMETER(leftover_items, int);
  CUTEST_GET_PARAMETER(backend, test_fill_special_backend);

  // Remove a possible stale sparse frame
  blosc2_remove_urlpath(backend.urlpath);

  /* Create a super-chunk container */
  blosc2_storage storage = {
          .cparams=cparams, .dparams=dparams,
          .urlpath=backend.urlpath, .contiguous=backend.contiguous};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  CUTEST_ASSERT("Error creating schunk", schunk != NULL);

  int ret;
  int special_value;
  switch (svalue) {
    case CHECK_ZEROS:
      special_value = BLOSC2_SPECIAL_ZERO;
      ret = blosc2_chunk_zeros(*cparams, isize, data_dest, isize);
      break;
    case CHECK_NANS:
      special_value = BLOSC2_SPECIAL_NAN;
      ret = blosc2_chunk_nans(*cparams, isize, data_dest, isize);
      break;
    case CHECK_UNINIT:
      special_value = BLOSC2_SPECIAL_UNINIT;
      ret = blosc2_chunk_uninit(*cparams, isize, data_dest, isize);
      break;
    default:
      CUTEST_ASSERT("Unrecognized case", false);
  }
  CUTEST_ASSERT("Creation error in special chunk", ret == BLOSC_EXTENDED_HEADER_LENGTH);

  // Add some items into the super-chunk
  int64_t nitems;
  // Make nitems a non-divisible number of CHUNKSHAPE
  nitems = (int64_t)NCHUNKS * CHUNKSHAPE + leftover_items;
  int32_t leftover_bytes = (int32_t)(nitems % CHUNKSHAPE) * cparams->typesize;
  int64_t nchunks = blosc2_schunk_fill_special(schunk, nitems, special_value, isize);
  if (leftover_items != 0) {
    CUTEST_ASSERT("Error in fill special", nchunks == NCHUNKS + 1);
  }
  else {
    CUTEST_ASSERT("Error in fill special", nchunks == NCHUNKS);
  }

  /* Retrieve and decompress the chunks from the super-chunks and compare values */
  for (nchunk = 0; nchunk < nchunks; nchunk++) {
    int32_t dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, data_dest, isize);
    if ((nchunk == nchunks - 1) && (leftover_items > 0)) {
      CUTEST_ASSERT("Wrong size for last chunk.", dsize == leftover_bytes);
    }
    else {
      CUTEST_ASSERT("Wrong size for last chunk.", dsize == isize);
    }

    // Check values
    int32_t cbytes;
    uint8_t* chunk;
    bool needs_free;
    float fvalue;
    if (svalue % 2) {
      cbytes = blosc2_schunk_get_chunk(schunk, nchunk, &chunk, &needs_free);
    }
    else {
      cbytes = blosc2_schunk_get_lazychunk(schunk, nchunk, &chunk, &needs_free);
    }
    CUTEST_ASSERT("Wrong chunk size!", cbytes == BLOSC_EXTENDED_HEADER_LENGTH);
    dsize = blosc2_getitem_ctx(schunk->dctx, chunk, cbytes, 0, 1, &fvalue, sizeof(float));
    CUTEST_ASSERT("Wrong decompressed item size!", dsize == sizeof(float));
    switch (special_value) {
      case CHECK_ZEROS:
        CUTEST_ASSERT("Wrong value!", fvalue == 0.);
        break;
      case CHECK_NANS:
        CUTEST_ASSERT("Wrong value!", isnan(fvalue));
        break;
      default:
        // We cannot check non initialized values
        break;
    }

    if (needs_free) {
      free(chunk);
    }
  }

  /* Free resources */
  blosc2_schunk_free(schunk);
  free(data_dest);

  return 0;
}

CUTEST_TEST_TEARDOWN(fill_special) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_destroy();
}


int main() {
  CUTEST_TEST_RUN(fill_special);
}
