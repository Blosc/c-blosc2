/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include "test_common.h"
#include "cutest.h"

#define CHUNKSIZE (5 * 1000)  // > NCHUNKS for the bench purposes
#define NCHUNKS 10


typedef struct {
  int32_t open;
  int32_t close;
  int32_t tell;
  int32_t write;
  int32_t read;
  int32_t truncate;
} test_udio_params;


typedef struct {
  blosc2_stdio_file *bfile;
  test_udio_params *params;
} test_file;


void* test_open(const char *urlpath, const char *mode, void *params) {
  test_file *my = malloc(sizeof(test_file));
  my->params = params;
  my->params->open++;

  my->bfile = blosc2_stdio_open(urlpath, mode, NULL);
  return my;
}

int test_close(void *stream) {
  test_file *my = (test_file *) stream;
  my->params->close++;
  int err =  blosc2_stdio_close(my->bfile);
  free(my);
  return err;
}

int64_t test_size(void *stream) {
  test_file *my = (test_file *) stream;
  my->params->tell++;
  return blosc2_stdio_size(my->bfile);
}

int64_t test_write(const void *ptr, int64_t size, int64_t nitems, int64_t position, void *stream) {
  test_file *my = (test_file *) stream;
  my->params->write++;
  return blosc2_stdio_write(ptr, size, nitems, position, my->bfile);
}

int64_t test_read(void **ptr, int64_t size, int64_t nitems, int64_t position, void *stream) {
  test_file *my = (test_file *) stream;
  my->params->read++;
  return blosc2_stdio_read(ptr, size, nitems, position, my->bfile);
}

int test_truncate(void *stream, int64_t size) {
  test_file *my = (test_file *) stream;
  my->params->truncate++;
  return blosc2_stdio_truncate(my->bfile, size);
}

int test_destroy(void *params) {
  BLOSC_UNUSED_PARAM(params);
  return 0;
}


typedef struct {
  bool contiguous;
  char *urlpath;
} test_udio_backend;

CUTEST_TEST_DATA(udio) {
  blosc2_cparams cparams;
};

CUTEST_TEST_SETUP(udio) {
  blosc2_init();

  blosc2_io_cb io_cb;

  io_cb.id = 244;
  io_cb.is_allocation_necessary = true;
  io_cb.open = (blosc2_open_cb) test_open;
  io_cb.close = (blosc2_close_cb) test_close;
  io_cb.read = (blosc2_read_cb) test_read;
  io_cb.size = (blosc2_size_cb) test_size;
  io_cb.write = (blosc2_write_cb) test_write;
  io_cb.truncate = (blosc2_truncate_cb) test_truncate;
  io_cb.destroy = (blosc2_destroy_cb) test_destroy;

  blosc2_register_io_cb(&io_cb);

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
  BLOSC_UNUSED_PARAM(data);
  CUTEST_GET_PARAMETER(backend, test_udio_backend);

  /* Free resources */
  blosc2_remove_urlpath(backend.urlpath);

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
  blosc2_io io = {.id = 244, .params = &io_params};
  blosc2_storage storage = {.cparams=&cparams, .contiguous=backend.contiguous, .urlpath = backend.urlpath, .io=&io};

  blosc2_schunk *schunk = blosc2_schunk_new(&storage);

  for (int i = 0; i < NCHUNKS; ++i) {
    int64_t cbytes = blosc2_schunk_append_buffer(schunk, data_buffer, nbytes);
    CUTEST_ASSERT("Error during compression", cbytes >= 0);
  }

  blosc2_schunk *schunk2 = blosc2_schunk_open_udio(backend.urlpath, &io);

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
  CUTEST_ASSERT("Write must be positive", io_params.write > 0);
  CUTEST_ASSERT("Read must be positive", io_params.read > 0);
  CUTEST_ASSERT("Truncate must be positive", io_params.truncate > 0);

  blosc2_schunk_free(schunk);
  blosc2_schunk_free(schunk2);
  free(data_buffer);
  free(rec_buffer);

  return 0;
}

CUTEST_TEST_TEARDOWN(udio) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_destroy();
}


int main() {
  CUTEST_TEST_RUN(udio);
}
