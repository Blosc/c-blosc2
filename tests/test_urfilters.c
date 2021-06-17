/*
  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdio.h>
#include <stdint.h>

#include "blosc2.h"
#include "cutest.h"


#define KB  1024
#define MB  (1024*KB)
#define GB  (1024*MB)

#define NCHUNKS (10)
#define CHUNKSIZE (5 * 1000)  // > NCHUNKS for the bench purposes
#define NTHREADS 4


int filter_forward(const uint8_t* src, uint8_t* dest, int32_t size, uint8_t meta, blosc2_cparams *cparams) {
  blosc2_schunk *schunk = cparams->schunk;
  if (meta != 101) {
    return -1;
  }

  for (int i = 0; i < size / schunk->typesize; ++i) {
    switch (schunk->typesize) {
      case 8:
        ((int64_t *) dest)[i] = ((int64_t *) src)[i] + 1;
        break;
      case 4:
        ((int32_t *) dest)[i] = ((int32_t *) src)[i] + 1;
        break;
      case 2:
        ((int16_t *) dest)[i] = ((int16_t *) src)[i] + 1;
        break;
      default:
        BLOSC_TRACE_ERROR("Item size %d not supported", schunk->typesize);
        return BLOSC2_ERROR_FAILURE;
    }
  }
  return BLOSC2_ERROR_SUCCESS;
}
int filter_backward(const uint8_t* src, uint8_t* dest, int32_t size, uint8_t meta, blosc2_dparams *dparams) {
  if (meta != 101) {
    return -1;
  }

  blosc2_schunk *schunk = dparams->schunk;

  for (int i = 0; i < size / schunk->typesize; ++i) {
    switch (schunk->typesize) {
      case 8:
        ((int64_t *) dest)[i] = ((int64_t *) src)[i] - 1;
        break;
      case 4:
        ((int32_t *) dest)[i] = ((int32_t *) src)[i] - 1;
        break;
      case 2:
        ((int16_t *) dest)[i] = ((int16_t *) src)[i] - 1;
        break;
      default:
        BLOSC_TRACE_ERROR("Item size %d not supported", schunk->typesize);
        return BLOSC2_ERROR_FAILURE;
    }
  }
  return BLOSC2_ERROR_SUCCESS;
}

int filter_backward_error(const uint8_t* src, uint8_t* dest, int32_t size, uint8_t meta, blosc2_dparams *dparams) {
  if (meta != 101) {
    return -1;
  }

  blosc2_schunk *schunk = dparams->schunk;

  for (int i = 0; i < size / schunk->typesize; ++i) {
    switch (schunk->typesize) {
      case 8:
        ((int64_t *) dest)[i] = ((int64_t *) src)[i];
        break;
      case 4:
        ((int32_t *) dest)[i] = ((int32_t *) src)[i] + 31;
        break;
      case 2:
        ((int16_t *) dest)[i] = ((int16_t *) src)[i] - 13;
        break;
      default:
        BLOSC_TRACE_ERROR("Item size %d not supported", schunk->typesize);
        return BLOSC2_ERROR_FAILURE;
    }
  }
  return BLOSC2_ERROR_SUCCESS;
}


CUTEST_TEST_DATA(urfilters) {
  blosc2_cparams cparams;
};

CUTEST_TEST_SETUP(urfilters) {
  blosc_init();
  data->cparams = BLOSC2_CPARAMS_DEFAULTS;
  data->cparams.typesize = sizeof(int32_t);
  data->cparams.clevel = 9;
  data->cparams.nthreads = NTHREADS;

  CUTEST_PARAMETRIZE(nchunks, int32_t, CUTEST_DATA(
      0,
      1,
      10,
      20,
  ));

  CUTEST_PARAMETRIZE(itemsize, int8_t, CUTEST_DATA(
      2,
      4,
      8,
  ));

  CUTEST_PARAMETRIZE(correct_backward, bool, CUTEST_DATA(
      true,
      false,
  ));
}


CUTEST_TEST_TEST(urfilters) {
  CUTEST_GET_PARAMETER(nchunks, int32_t);
  CUTEST_GET_PARAMETER(itemsize, int8_t);
  CUTEST_GET_PARAMETER(correct_backward, bool);

  int32_t isize = CHUNKSIZE * itemsize;
  uint8_t *bdata = malloc(isize);
  uint8_t *bdata_dest = malloc(isize);

  int dsize;

  blosc2_filter urfilter;
    urfilter.forward = filter_forward;
  if (correct_backward) {
      urfilter.backward = filter_backward;
      urfilter.id = 244;
  } else {
      urfilter.backward = filter_backward_error;
      urfilter.id = 245;
  }

  blosc2_register_filter(&urfilter);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = (int32_t) itemsize;
  cparams.filters[4] = urfilter.id;
  cparams.filters_meta[4] = 101;
  cparams.clevel = 9;

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;

  blosc2_schunk* schunk;
  int i, nchunk;

  /* Create a super-chunk container */
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams};
  schunk = blosc2_schunk_new(&storage);

  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      switch (itemsize) {
        case 8:
          ((int64_t *) bdata)[i] = i * nchunk;
          break;
        case 4:
          ((int32_t *) bdata)[i] = i * nchunk;
          break;
        case 2:
          ((int16_t *) bdata)[i] = i * nchunk;
          break;
        default:
          BLOSC_TRACE_ERROR("Itemsize %d not supported\n", itemsize);
          return -1;
      }
    }
    int nchunks_ = blosc2_schunk_append_buffer(schunk, bdata, isize);
    if (nchunks_ != nchunk + 1) {
      BLOSC_TRACE_ERROR("Unexpected nchunks!");
      return -1;
    }
  }

  /* Retrieve and decompress the chunks (0-based count) */
  for (nchunk = NCHUNKS-1; nchunk >= 0; nchunk--) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, bdata_dest, isize);
    if (dsize < 0) {
      BLOSC_TRACE_ERROR("Decompression error.  Error code: %d\n", dsize);
      return dsize;
    }
  }

  /* Check integrity of the second chunk (made of non-zeros) */
  blosc2_schunk_decompress_chunk(schunk, 1, bdata_dest, isize);
  for (i = 0; i < CHUNKSIZE; i++) {
    bool equals = false;
    switch (itemsize) {
      case 8:
        if (((int64_t *) bdata_dest)[i] == i) {
          equals = true;
        }
        break;
      case 4:
        if (((int32_t *) bdata_dest)[i] == i) {
          equals = true;
        }
        break;
      case 2:
        if (((int16_t *) bdata_dest)[i] == i) {
          equals = true;
        }
        break;
      default:
        BLOSC_TRACE_ERROR("Itemsize %d not supported\n", itemsize);
        return -1;
    }
    if (!equals && correct_backward) {
      BLOSC_TRACE_ERROR("Decompressed bdata differs from original!\n");
      return -1;
    }
    if (equals && !correct_backward) {
      BLOSC_TRACE_ERROR("Decompressed bdata is equal than original!\n");
      return -1;
    }
  }

  /* Free resources */
  /* Destroy the super-chunk */
  blosc2_schunk_free(schunk);
  free(bdata);
  free(bdata_dest);

  return BLOSC2_ERROR_SUCCESS;
}


CUTEST_TEST_TEARDOWN(urfilters) {
  blosc_destroy();
}


int main() {
  CUTEST_TEST_RUN(urfilters)
}
