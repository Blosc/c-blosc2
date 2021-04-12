/*
  Copyright (C) 2020  The Blosc Developers
  http://blosc.org
  License: BSD (see LICENSE.txt)

  Benchmark showing Blosc zero detection capabilities via run-length.

*/

#include "test_common.h"
#include "cutest.h"

#define CHUNKSIZE (5 * 1000)  // > NCHUNKS for the bench purposes
#define NCHUNKS 10


typedef struct {
  int32_t open;
  int32_t close;
  int32_t seek;
  int32_t write;
  int32_t read;
} test_udio_params;


void* test_open(const char *urlpath, const char *mode, void *params) {
  test_udio_params *tparams = params;
  tparams->open++;
  return blosc2_stdio_open(urlpath, mode, params);
}

int test_close(void *stream, void* params) {
  test_udio_params *tparams = params;
  tparams->close++;
  return blosc2_stdio_close(stream, params);
}

int test_seek(void *stream, int64_t offset, int whence, void* params) {
  test_udio_params *tparams = params;
  tparams->seek++;
  return blosc2_stdio_seek(stream, offset, whence, params);
}

size_t test_write(const void *ptr, int64_t size, int64_t nitems, void *stream, void *params) {
  test_udio_params *tparams = params;
  tparams->write++;
  return blosc2_stdio_write(ptr, size, nitems, stream, params);
}

size_t test_read(void *ptr, int64_t size, int64_t nitems, void *stream, void *params) {
  test_udio_params *tparams = params;
  tparams->read++;
  return blosc2_stdio_read(ptr, size, nitems, stream, params);
}


typedef struct {
  bool contiguous;
  char *urlpath;
}test_udio_backend;

CUTEST_TEST_DATA(udio) {
  blosc2_io udio;
  blosc2_cparams cparams;
};

CUTEST_TEST_SETUP(udio) {
  blosc_init();

  data->udio.open = (blosc2_open_cb) test_open;
  data->udio.close = (blosc2_close_cb) test_close;
  data->udio.read = (blosc2_read_cb) test_read;
  data->udio.seek = (blosc2_seek_cb) test_seek;
  data->udio.write = (blosc2_write_cb) test_write;

  data->cparams = BLOSC2_CPARAMS_DEFAULTS;
  data->cparams.typesize = sizeof(int32_t);
  data->cparams.compcode = BLOSC_BLOSCLZ;
  data->cparams.clevel = 4;
  data->cparams.nthreads = 2;

  CUTEST_PARAMETRIZE(backend, test_udio_backend, CUTEST_DATA(
      {true, "test_udio.b2frame"}, // disk - cframe
      {false, "test_udio_s.b2frame"}, // disk - sframe
  ));
}


CUTEST_TEST_TEST(udio) {
  CUTEST_GET_PARAMETER(backend, test_udio_backend);

  /* Free resources */
  if (backend.urlpath != NULL && backend.contiguous == false) {
    blosc2_remove_dir(backend.urlpath);
  }

  int32_t nbytes = CHUNKSIZE * sizeof(int32_t);
  int32_t *data_buffer = malloc(nbytes);
  int32_t *rec_buffer = malloc(nbytes);

  /* Create a super-chunk container */
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 9;
  cparams.nthreads = 2;
  test_udio_params io_params = {0};
  data->udio.params = &io_params;

  blosc2_storage storage = {.cparams=&cparams, .contiguous=backend.contiguous, .urlpath = backend.urlpath, .udio=&data->udio};

  blosc2_schunk *schunk = blosc2_schunk_new(&storage);

  for (int i = 0; i < NCHUNKS; ++i) {
    int32_t cbytes = blosc2_schunk_append_buffer(schunk, data_buffer, nbytes);
    CUTEST_ASSERT("Error during compression", cbytes >= 0);
  }

  blosc2_schunk *schunk2 = blosc2_schunk_open_udio(backend.urlpath, &data->udio);

  for (int i = 0; i < NCHUNKS; ++i) {
    int32_t dbytes = blosc2_schunk_decompress_chunk(schunk2, i, rec_buffer, nbytes);
    CUTEST_ASSERT("Error during decompression", dbytes >= 0);
    for (int j = 0; j < CHUNKSIZE; ++j) {
      CUTEST_ASSERT("Data are not equal", data_buffer[j] == rec_buffer[j]);
    }
  }

  // Check io params
  CUTEST_ASSERT("Open must be positive", io_params.open > 0);
  CUTEST_ASSERT("Close must be positive", io_params.close > 0);
  CUTEST_ASSERT("Seek must be positive", io_params.seek > 0);
  CUTEST_ASSERT("Write must be positive", io_params.write > 0);
  CUTEST_ASSERT("Read must be positive", io_params.read > 0);

  free(schunk);
  free(schunk2);
  free(data_buffer);
  free(rec_buffer);

  return 0;
}

CUTEST_TEST_TEARDOWN(udio) {
  blosc_destroy();
}


int main() {
  CUTEST_TEST_RUN(udio)
}
