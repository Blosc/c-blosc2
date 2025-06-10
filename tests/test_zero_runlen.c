/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Benchmark showing Blosc zero detection capabilities via run-length.
*/

#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "blosc2.h"
#include "cutest.h"


#define KB  1024
#define MB  (1024*KB)
#define GB  (1024*MB)

#define NCHUNKS (10)
#define CHUNKSIZE (5 * 1000)  // > NCHUNKS for the bench purposes
#define NTHREADS 4

#define REPEATED_VALUE 1

enum {
  ZERO_DETECTION = 0,
  CHECK_ZEROS = 1,
  CHECK_NANS = 2,
  CHECK_VALUES = 3,
  CHECK_UNINIT = 4,
};

typedef struct {
  bool contiguous;
  char *urlpath;
} test_zero_runlen_backend;

CUTEST_TEST_DATA(zero_runlen) {
  blosc2_cparams cparams;
};

CUTEST_TEST_SETUP(zero_runlen) {
  blosc2_init();

  data->cparams = BLOSC2_CPARAMS_DEFAULTS;
  data->cparams.typesize = sizeof(int32_t);
  data->cparams.compcode = BLOSC_BLOSCLZ;
  data->cparams.clevel = 9;
  data->cparams.nthreads = NTHREADS;

  CUTEST_PARAMETRIZE(svalue, int, CUTEST_DATA(
      ZERO_DETECTION,
      CHECK_ZEROS,
      CHECK_UNINIT,
      CHECK_NANS,
      CHECK_VALUES
  ));
  CUTEST_PARAMETRIZE(backend, test_zero_runlen_backend, CUTEST_DATA(
      {false, NULL},  // memory - schunk
      {true, NULL},  // memory - cframe
      {true, "test_zero_runlen.b2frame"}, // disk - cframe
      {false, "test_zero_runlen_s.b2frame"}, // disk - sframe
  ));
}


CUTEST_TEST_TEST(zero_runlen) {
  BLOSC_UNUSED_PARAM(data);

  CUTEST_GET_PARAMETER(svalue, int);
  CUTEST_GET_PARAMETER(backend, test_zero_runlen_backend);

  blosc2_schunk *schunk;
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int32_t osize = CHUNKSIZE * sizeof(int32_t) + BLOSC2_MAX_OVERHEAD;
  int dsize, csize;
  int nchunk;
  int64_t nchunks;
  int rc;
  int32_t value = REPEATED_VALUE;
  float fvalue;

  int32_t *data_buffer = malloc(CHUNKSIZE * sizeof(int32_t));
  int32_t *rec_buffer = malloc(CHUNKSIZE * sizeof(int32_t));

  /* Create a super-chunk container */
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .contiguous=backend.contiguous, .urlpath = backend.urlpath};
  blosc2_remove_urlpath(backend.urlpath);

  schunk = blosc2_schunk_new(&storage);

  /* Append the chunks */
  void* chunk = malloc(BLOSC_EXTENDED_HEADER_LENGTH + isize);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    switch (svalue) {
      case ZERO_DETECTION:
        memset(data_buffer, 0, isize);
        csize = blosc2_compress(5, 1, sizeof(int32_t), data_buffer, isize, chunk, osize);
        break;
      case CHECK_ZEROS:
        csize = blosc2_chunk_zeros(cparams, isize, chunk, BLOSC_EXTENDED_HEADER_LENGTH);
        break;
      case CHECK_UNINIT:
        csize = blosc2_chunk_uninit(cparams, isize, chunk, BLOSC_EXTENDED_HEADER_LENGTH);
        break;
      case CHECK_NANS:
        csize = blosc2_chunk_nans(cparams, isize, chunk, BLOSC_EXTENDED_HEADER_LENGTH);
        break;
      case CHECK_VALUES:
        csize = blosc2_chunk_repeatval(cparams, isize, chunk,
                                       BLOSC_EXTENDED_HEADER_LENGTH + sizeof(int32_t), &value);
        break;
      default:
        CUTEST_ASSERT("Unrecognized case", false);
    }

    CUTEST_ASSERT("Error creating chunk", csize >= 0);

    nchunks = blosc2_schunk_append_chunk(schunk, chunk, true);
    CUTEST_ASSERT("Error appending chunk", nchunks >= 0);
  }
  free(chunk);


  /* Retrieve and decompress the chunks */
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, rec_buffer, isize);
    CUTEST_ASSERT("Decompression error", dsize >= 0);

    CUTEST_ASSERT("", dsize == (int)isize);
  }

  /* Exercise the getitem */
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    bool needs_free;
    uint8_t* chunk_;
    csize = blosc2_schunk_get_chunk(schunk, nchunk, &chunk_, &needs_free);
    CUTEST_ASSERT("blosc2_schunk_get_chunk error.  Error code: %d\n", csize >= 0);

    switch (svalue) {
      case CHECK_VALUES:
        rc = blosc1_getitem(chunk_, nchunk, 1, &value);
        CUTEST_ASSERT("Error in getitem of a special value", rc >= 0);
        CUTEST_ASSERT("Wrong value!", value == REPEATED_VALUE);
        break;
      case CHECK_NANS:
        rc = blosc1_getitem(chunk_, nchunk, 1, &fvalue);
        CUTEST_ASSERT("Error in getitem of a special value", rc >= 0);
        CUTEST_ASSERT("Wrong value!", isnan(fvalue));
        break;
      case CHECK_ZEROS:
        rc = blosc1_getitem(chunk_, nchunk, 1, &value);
        CUTEST_ASSERT("Error in getitem of a special value", rc >= 0);
        CUTEST_ASSERT("Wrong value!", value == 0);
        break;
      default:
        // It can only be non initialized values
        rc = blosc1_getitem(chunk_, nchunk, 1, &value);
        CUTEST_ASSERT("Error in getitem of a special value", rc >= 0);
    }
    if (needs_free) {
      free(chunk_);
    }
  }

  /* Check that all the values have a good roundtrip */
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) rec_buffer, isize);
    CUTEST_ASSERT("Decompression error", dsize >= 0);
    CUTEST_ASSERT("Dest size is not equal to src size", dsize == (int)isize);

    if (svalue == CHECK_VALUES) {
      int32_t* buffer = (int32_t*)rec_buffer;
      for (int i = 0; i < CHUNKSIZE; i++) {
        CUTEST_ASSERT("Value is not correct in chunk", buffer[i] == REPEATED_VALUE);
      }
    }
    else if (svalue == CHECK_NANS) {
      float* buffer = (float*)rec_buffer;
      for (int i = 0; i < CHUNKSIZE; i++) {
        CUTEST_ASSERT("Value is not correct in chunk", isnan(buffer[i]));
      }
    }
    else if (svalue == CHECK_ZEROS) {
      float* buffer = (float*)rec_buffer;
      for (int i = 0; i < CHUNKSIZE; i++) {
        CUTEST_ASSERT("Value is not correct in chunk", buffer[i] == 0);
      }
    }
    else {
      // We can't do any check for non initialized values
    }
  }

  /* Free resources */
  free(data_buffer);
  free(rec_buffer);
  blosc2_schunk_free(schunk);

  /* Free resources */
  blosc2_remove_urlpath(storage.urlpath);

  return 0;
}

CUTEST_TEST_TEARDOWN(zero_runlen) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_destroy();
}


int main() {
  CUTEST_TEST_RUN(zero_runlen);
}
