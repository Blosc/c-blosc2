/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Benchmark showing Blosc zero detection capabilities via run-length.
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

#define REPEATED_VALUE 1

enum {
  ZERO_DETECTION = 0,
  CHECK_ZEROS = 1,
  CHECK_NANS = 2,
  CHECK_VALUES = 3,
};

typedef struct {
  bool contiguous;
  char *urlpath;
} test_copy_backend;


CUTEST_TEST_DATA(copy) {
  blosc2_cparams cparams;
  blosc2_cparams cparams2;
};


CUTEST_TEST_SETUP(copy) {
  blosc2_init();
  data->cparams = BLOSC2_CPARAMS_DEFAULTS;
  data->cparams.typesize = sizeof(int32_t);
  data->cparams.clevel = 9;
  data->cparams.nthreads = NTHREADS;
  data->cparams.splitmode = BLOSC_NEVER_SPLIT;

  data->cparams2 = BLOSC2_CPARAMS_DEFAULTS;
  data->cparams2.typesize = sizeof(int32_t);
  data->cparams2.clevel = 2;
  data->cparams2.nthreads = NTHREADS;
  data->cparams2.blocksize = 10000;
  data->cparams2.splitmode = BLOSC_ALWAYS_SPLIT;

  CUTEST_PARAMETRIZE(nchunks, int32_t, CUTEST_DATA(
      0, 1, 10, 20
  ));
  CUTEST_PARAMETRIZE(different_cparams, bool, CUTEST_DATA(
      false, true
  ));
  CUTEST_PARAMETRIZE(metalayers, bool, CUTEST_DATA(
      false, true
  ));
  CUTEST_PARAMETRIZE(vlmetalayers, bool, CUTEST_DATA(
      false, true
  ));
  CUTEST_PARAMETRIZE(backend, test_copy_backend, CUTEST_DATA(
      {false, NULL},  // memory - schunk
      {true, NULL},  // memory - cframe
      {true, "test_copy.b2frame"}, // disk - cframe
      {false, "test_copy_s.b2frame"}, // disk - sframe
  ));
  CUTEST_PARAMETRIZE(backend2, test_copy_backend, CUTEST_DATA(
          {false, NULL},  // memory - schunk
          {true, NULL},  // memory - cframe
          {true, "test_copy2.b2frame"}, // disk - cframe
          {false, "test_copy2_s.b2frame"}, // disk - sframe
  ));
}


CUTEST_TEST_TEST(copy) {
  CUTEST_GET_PARAMETER(nchunks, int32_t);
  CUTEST_GET_PARAMETER(different_cparams, bool);
  CUTEST_GET_PARAMETER(metalayers, bool);
  CUTEST_GET_PARAMETER(vlmetalayers, bool);
  CUTEST_GET_PARAMETER(backend, test_copy_backend);
  CUTEST_GET_PARAMETER(backend2, test_copy_backend);

  /* Free resources */
  if (backend.urlpath != NULL && backend.contiguous == false) {
    blosc2_remove_dir(backend.urlpath);
  }
  if (backend2.urlpath != NULL && backend2.contiguous == false) {
    blosc2_remove_dir(backend2.urlpath);
  }

  int32_t itemsize = data->cparams.typesize;
  int32_t isize = CHUNKSIZE * itemsize;

  int32_t *data_buffer = malloc(isize);
  memset(data_buffer, 0, isize);

  int32_t *rec_buffer = malloc(isize);

  /* Initialize the Blosc compressor */
  blosc2_init();

  /* Create a super-chunk container */
  blosc2_storage storage = {.cparams=&data->cparams, .contiguous=backend.contiguous, .urlpath = backend.urlpath};
  if (backend.urlpath != NULL) {
    remove(backend.urlpath);
  }
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  CUTEST_ASSERT("Error creating a schunk", schunk != NULL);

  char* meta_name = "test_copy";
  int32_t meta_content_len = 8;
  int64_t meta_content = -66;

  if (metalayers) {
    blosc2_meta_add(schunk, meta_name, (uint8_t *) &meta_content, meta_content_len);
  }
  if (vlmetalayers) {
    blosc2_vlmeta_add(schunk, "vlmetalayer", (uint8_t *) &meta_content, meta_content_len, NULL);
  }

  /* Append the chunks */
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    int64_t nc = blosc2_schunk_append_buffer(schunk, data_buffer, isize);
    CUTEST_ASSERT("Error appending chunk", nc >= 0);
  }

  /* Copy schunk */
  blosc2_storage storage2 = {.contiguous=backend2.contiguous, .urlpath = backend2.urlpath};
  if (backend2.urlpath != NULL) {
    remove(backend2.urlpath);
  }
  storage2.cparams = different_cparams ? &data->cparams2 : &data->cparams;
  blosc2_schunk * schunk_copy = blosc2_schunk_copy(schunk, &storage2);
  CUTEST_ASSERT("Error copying a schunk", schunk_copy != NULL);

  if (metalayers) {
    int64_t *content = malloc(meta_content_len);
    int32_t content_len;
    blosc2_meta_get(schunk_copy, meta_name, (uint8_t **) &content, &content_len);
    CUTEST_ASSERT("Metalayers are not equals.", *content == meta_content);
    free(content);
  }
  if (vlmetalayers) {
    int32_t content_len;
    int64_t *content;
    blosc2_vlmeta_get(schunk_copy, "vlmetalayer", (uint8_t **) &content, &content_len);
    CUTEST_ASSERT("Variable-length metalayers are not equal.", *content == meta_content);
    free(content);
  }
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    int dsize;
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, data_buffer, isize);
    CUTEST_ASSERT("Decompression error", dsize >= 0);
    CUTEST_ASSERT("Decompression size is not equal to input size", dsize == (int) isize);

    dsize = blosc2_schunk_decompress_chunk(schunk_copy, nchunk, rec_buffer, isize);
    CUTEST_ASSERT("Decompression error", dsize >= 0);
    CUTEST_ASSERT("Decompression size is not equal to input size", dsize == (int) isize);
  }

  /* Free resources */
  free(data_buffer);
  free(rec_buffer);

  /* Destroy the super-chunk */
  blosc2_schunk_free(schunk);
  blosc2_schunk_free(schunk_copy);

  /* Free resources */
  blosc2_remove_urlpath(backend.urlpath);
  blosc2_remove_urlpath(backend2.urlpath);

  return 0;
}


CUTEST_TEST_TEARDOWN(copy) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_destroy();
}


int main() {
  CUTEST_TEST_RUN(copy);
}
