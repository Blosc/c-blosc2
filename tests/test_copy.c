/*
  Copyright (C) 2020  The Blosc Developers
  http://blosc.org
  License: BSD (see LICENSE.txt)

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
}test_copy_backend;


CUTEST_TEST_DATA(copy) {
  blosc2_cparams cparams;
  blosc2_cparams cparams2;
};


CUTEST_TEST_SETUP(copy) {
  blosc_init();
  data->cparams = BLOSC2_CPARAMS_DEFAULTS;
  data->cparams.typesize = sizeof(int32_t);
  data->cparams.clevel = 9;
  data->cparams.nthreads = NTHREADS;

  data->cparams2 = BLOSC2_CPARAMS_DEFAULTS;
  data->cparams2.typesize = sizeof(int32_t);
  data->cparams2.clevel = 2;
  data->cparams2.nthreads = NTHREADS;
  data->cparams2.blocksize = 10000;

  CUTEST_PARAMETRIZE(nchunks, int32_t, CUTEST_DATA(
      0, 1, 10, 20
  ));
  CUTEST_PARAMETRIZE(different_cparams, bool, CUTEST_DATA(
      false, true
  ));
  CUTEST_PARAMETRIZE(metalayers, bool, CUTEST_DATA(
      false, true
  ));
  CUTEST_PARAMETRIZE(usermeta, bool, CUTEST_DATA(
      false, true
  ));
  CUTEST_PARAMETRIZE(backend, test_copy_backend, CUTEST_DATA(
      {false, NULL},  // memory - schunk
      {true, NULL},  // memory - frame
      {true, "test_copy.b2frame"}, // disk - frame
      {false, "test_copy.b2sframe"}, // disk - sframe
  ));
  CUTEST_PARAMETRIZE(backend2, test_copy_backend, CUTEST_DATA(
      {false, NULL},  // memory - schunk
      {true, NULL},  // memory - frame
      {true, "test_copy2.b2frame"}, // disk - frame
      {false, "test_copy2.b2sframe"}, // disk - sframe
  ));
}


CUTEST_TEST_TEST(copy) {
  CUTEST_GET_PARAMETER(nchunks, int32_t);
  CUTEST_GET_PARAMETER(different_cparams, bool);
  CUTEST_GET_PARAMETER(metalayers, bool);
  CUTEST_GET_PARAMETER(usermeta, bool);
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
  blosc_init();

  /* Create a super-chunk container */
  blosc2_storage storage = {.cparams=&data->cparams, .contiguous=backend.contiguous, .urlpath = backend.urlpath};
  blosc2_schunk *schunk = blosc2_schunk_new(storage);
  CUTEST_ASSERT("Error creating a schunk", schunk != NULL);

  char* meta_name = "test_copy";
  uint32_t meta_content_len = 8;
  int64_t meta_content = -66;

  if (metalayers) {
    blosc2_add_metalayer(schunk, meta_name, (uint8_t *) &meta_content, meta_content_len);
  }
  if (usermeta) {
    blosc2_update_usermeta(schunk, (uint8_t *) &meta_content, meta_content_len, *storage.cparams);
  }

  /* Append the chunks */
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    int nc = blosc2_schunk_append_buffer(schunk, data_buffer, isize);
    CUTEST_ASSERT("Error appending chunk", nc >= 0);
  }

  /* Copy schunk */
  blosc2_storage storage2 = {.contiguous=backend2.contiguous, .urlpath = backend2.urlpath};
  storage2.cparams = different_cparams ? &data->cparams2 : &data->cparams;
  blosc2_schunk * schunk_copy = blosc2_schunk_copy(schunk, storage2);
  CUTEST_ASSERT("Error copying a schunk", schunk_copy != NULL);

  if (metalayers) {
    int64_t *content = malloc(meta_content_len);
    uint32_t content_len;
    blosc2_get_metalayer(schunk_copy, meta_name,  (uint8_t **) &content, &content_len);
    CUTEST_ASSERT("Metalayers are not equals.", *content == meta_content);
    free(content);
  }
  if (usermeta) {
    int64_t *content;
    blosc2_get_usermeta(schunk_copy,  (uint8_t **) &content);
    CUTEST_ASSERT("Usermeta are not equal.", *content == meta_content);
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
  if (backend.urlpath != NULL && backend.contiguous == false) {
    blosc2_remove_dir(backend.urlpath);
  }
  if (backend2.urlpath != NULL && backend2.contiguous == false) {
    blosc2_remove_dir(backend2.urlpath);
  }

  return 0;
}


CUTEST_TEST_TEARDOWN(copy) {
  blosc_destroy();
}


int main() {
  CUTEST_TEST_RUN(copy)
}
