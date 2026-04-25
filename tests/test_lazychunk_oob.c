/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Regression test for lazychunk allocation-required IO callbacks.
*/

#include <stdio.h>
#include "test_common.h"
#include "blosc2/blosc2-stdio.h"

#define NITEMS 5000
#define BLOCKSIZE NITEMS

int tests_run = 0;

typedef struct {
  blosc2_stdio_file *bfile;
} test_file;

static void* test_open(const char *urlpath, const char *mode, void *params) {
  BLOSC_UNUSED_PARAM(params);
  test_file *my = malloc(sizeof(test_file));
  if (my == NULL) {
    return NULL;
  }
  my->bfile = blosc2_stdio_open(urlpath, mode, NULL);
  if (my->bfile == NULL) {
    free(my);
    return NULL;
  }
  return my;
}

static int test_close(void *stream) {
  test_file *my = (test_file *)stream;
  int rc = blosc2_stdio_close(my->bfile);
  free(my);
  return rc;
}

static int64_t test_size(void *stream) {
  test_file *my = (test_file *)stream;
  return blosc2_stdio_size(my->bfile);
}

static int64_t test_write(const void *ptr, int64_t size, int64_t nitems, int64_t position, void *stream) {
  test_file *my = (test_file *)stream;
  return blosc2_stdio_write(ptr, size, nitems, position, my->bfile);
}

static int64_t test_read(void **ptr, int64_t size, int64_t nitems, int64_t position, void *stream) {
  test_file *my = (test_file *)stream;
  if (size <= 0 || nitems <= 0 || size > INT64_MAX / nitems) {
    return 0;
  }

  int64_t bytes = size * nitems;
  void *tmp = malloc((size_t)bytes);
  if (tmp == NULL) {
    return 0;
  }

  *ptr = tmp;
  return blosc2_stdio_read(ptr, size, nitems, position, my->bfile);
}

static int test_truncate(void *stream, int64_t size) {
  test_file *my = (test_file *)stream;
  return blosc2_stdio_truncate(my->bfile, size);
}

static int test_destroy(void *params) {
  BLOSC_UNUSED_PARAM(params);
  return 0;
}

static char* test_lazychunk_oob(void) {
  const char *urlpath = "test_lazychunk_oob.b2frame";
  int32_t *src = malloc(NITEMS * sizeof(int32_t));
  int32_t *dst = malloc(NITEMS * sizeof(int32_t));
  blosc2_schunk *schunk = NULL;
  blosc2_schunk *opened = NULL;
  uint8_t *lazy_chunk = NULL;
  bool needs_free = false;
  int cbytes;
  int dbytes;

  if (src == NULL || dst == NULL) {
    free(src);
    free(dst);
    return "ERROR: memory allocation failed";
  }

  for (int32_t i = 0; i < NITEMS; ++i) {
    src[i] = (i * 2654435761U) ^ (i >> 3);
    dst[i] = 0;
  }

  blosc2_init();
  blosc2_remove_urlpath(urlpath);

  blosc2_io_cb io_cb = {0};
  io_cb.id = 246;
  io_cb.is_allocation_necessary = true;
  io_cb.open = test_open;
  io_cb.close = test_close;
  io_cb.size = test_size;
  io_cb.write = test_write;
  io_cb.read = test_read;
  io_cb.truncate = test_truncate;
  io_cb.destroy = test_destroy;

  mu_assert("ERROR: cannot register io callback", blosc2_register_io_cb(&io_cb) >= 0);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.clevel = 9;
  cparams.nthreads = 1;
  cparams.blocksize = NITEMS * (int32_t)sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;

  blosc2_io io = {0};
  io.id = 246;
  io.params = NULL;

  blosc2_storage storage = {0};
  storage.contiguous = true;
  storage.urlpath = (char *)urlpath;
  storage.cparams = &cparams;
  storage.dparams = &dparams;
  storage.io = &io;

  schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: cannot create schunk", schunk != NULL);

  mu_assert("ERROR: append failed", blosc2_schunk_append_buffer(schunk, src, NITEMS * (int32_t)sizeof(int32_t)) >= 0);
  blosc2_schunk_free(schunk);
  schunk = NULL;

  opened = blosc2_schunk_open_udio(urlpath, &io);
  mu_assert("ERROR: cannot reopen frame through allocation-required IO", opened != NULL);

  cbytes = blosc2_schunk_get_lazychunk(opened, 0, &lazy_chunk, &needs_free);
  mu_assert("ERROR: lazychunk retrieval failed", cbytes > 0);
  mu_assert("ERROR: lazychunk pointer is NULL", lazy_chunk != NULL);

  dbytes = blosc2_decompress_ctx(opened->dctx, lazy_chunk, cbytes, dst, NITEMS * (int32_t)sizeof(int32_t));
  mu_assert("ERROR: lazychunk decompress failed", dbytes >= 0);
  for (int32_t i = 0; i < NITEMS; ++i) {
    mu_assert("ERROR: roundtrip mismatch", dst[i] == src[i]);
  }

  if (needs_free) {
    free(lazy_chunk);
  }
  blosc2_schunk_free(opened);
  blosc2_remove_urlpath(urlpath);
  blosc2_destroy();
  free(src);
  free(dst);
  return EXIT_SUCCESS;
}

static char *all_tests(void) {
  mu_run_test(test_lazychunk_oob);
  return EXIT_SUCCESS;
}

int main(void) {
  char *result;

  install_blosc_callback_test();
  blosc2_init();

  result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  blosc2_destroy();

  return result != EXIT_SUCCESS;
}
