/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"
#include "b2nd.h"
#include "frame.h"
#include "blosc2/blosc2-stdio.h"

#if !defined(_WIN32)
#include <unistd.h>
#endif

#define NITEMS 2000
#define FAILCLOSE_IO_ID 245

int tests_run = 0;

typedef struct {
  bool fail_close;
  int close_calls;
} failclose_params;

typedef struct {
  blosc2_stdio_file *bfile;
  failclose_params *params;
} failclose_file;

static void *fc_open(const char *urlpath, const char *mode, void *params) {
  failclose_file *f = malloc(sizeof(failclose_file));
  if (f == NULL) {
    return NULL;
  }
  f->params = (failclose_params *)params;
  f->bfile = blosc2_stdio_open(urlpath, mode, NULL);
  if (f->bfile == NULL) {
    free(f);
    return NULL;
  }
  return f;
}

static int fc_close(void *stream) {
  failclose_file *f = (failclose_file *)stream;
  int rc = blosc2_stdio_close(f->bfile);
  bool should_fail = f->params != NULL && f->params->fail_close;
  if (f->params != NULL) {
    f->params->close_calls++;
  }
  free(f);
  if (should_fail) {
    return -1;
  }
  return rc;
}

static int64_t fc_size(void *stream) {
  failclose_file *f = (failclose_file *)stream;
  return blosc2_stdio_size(f->bfile);
}

static int64_t fc_write(const void *ptr, int64_t size, int64_t nitems, int64_t position, void *stream) {
  failclose_file *f = (failclose_file *)stream;
  return blosc2_stdio_write(ptr, size, nitems, position, f->bfile);
}

static int64_t fc_read(void **ptr, int64_t size, int64_t nitems, int64_t position, void *stream) {
  failclose_file *f = (failclose_file *)stream;
  return blosc2_stdio_read(ptr, size, nitems, position, f->bfile);
}

static int fc_truncate(void *stream, int64_t size) {
  failclose_file *f = (failclose_file *)stream;
  return blosc2_stdio_truncate(f->bfile, size);
}

static int fc_destroy(void *params) {
  BLOSC_UNUSED_PARAM(params);
  return 0;
}


static char *test_custom_io_close_errors(void) {
  const char *cframe_path = "test_close_err_cframe.b2frame";
  const char *sframe_path = "test_close_err_sframe.b2frame";
  const char *tofile_path = "test_close_err_tofile.b2frame";
  const char *append_path = "test_close_err_append.b2frame";

  blosc2_remove_urlpath(cframe_path);
  blosc2_remove_urlpath(sframe_path);
  blosc2_remove_urlpath(tofile_path);
  blosc2_remove_urlpath(append_path);

  int32_t data[NITEMS];
  for (int32_t i = 0; i < NITEMS; i++) {
    data[i] = i;
  }

  failclose_params fc_params = {false, 0};
  blosc2_io io = {FAILCLOSE_IO_ID, "failclose", &fc_params};

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = 1;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;

  /* 1. Contiguous disk frame via custom IO: failing close() fails append */
  blosc2_storage storage = {.contiguous = true, .urlpath = (char *)cframe_path,
                            .cparams = &cparams, .dparams = &dparams, .io = &io};
  blosc2_schunk *sc = blosc2_schunk_new(&storage);
  mu_assert("schunk_new failed", sc != NULL);
  mu_assert("initial append failed", blosc2_schunk_append_buffer(sc, data, sizeof(data)) == 1);

  fc_params.fail_close = true;
  int64_t nchunks = blosc2_schunk_append_buffer(sc, data, sizeof(data));
  mu_assert("append on contiguous frame must fail when close() fails", nchunks < 0);
  fc_params.fail_close = false;
  mu_assert("schunk_free failed", blosc2_schunk_free(sc) == 0);

  /* 2. Sparse disk frame via custom IO: failing close() fails sframe_create_chunk */
  blosc2_storage sstorage = {.contiguous = false, .urlpath = (char *)sframe_path,
                             .cparams = &cparams, .dparams = &dparams, .io = &io};
  blosc2_schunk *ssc = blosc2_schunk_new(&sstorage);
  mu_assert("sframe schunk_new failed", ssc != NULL);
  fc_params.fail_close = true;
  nchunks = blosc2_schunk_append_buffer(ssc, data, sizeof(data));
  mu_assert("append on sframe must fail when close() fails", nchunks < 0);
  fc_params.fail_close = false;
  mu_assert("sframe schunk_free failed", blosc2_schunk_free(ssc) == 0);

  /* 3. blosc2_schunk_to_file & blosc2_schunk_append_file with failing close() */
  blosc2_storage mem_storage = {.contiguous = true, .cparams = &cparams, .dparams = &dparams, .io = &io};
  blosc2_schunk *mem_sc = blosc2_schunk_new(&mem_storage);
  mu_assert("mem schunk_new failed", mem_sc != NULL);
  mu_assert("mem append failed", blosc2_schunk_append_buffer(mem_sc, data, sizeof(data)) == 1);

  fc_params.fail_close = true;
  mu_assert("schunk_to_file must fail when close() fails",
            blosc2_schunk_to_file(mem_sc, tofile_path) < 0);
  mu_assert("schunk_append_file must fail when close() fails",
            blosc2_schunk_append_file(mem_sc, append_path) < 0);
  fc_params.fail_close = false;
  mu_assert("mem schunk_free failed", blosc2_schunk_free(mem_sc) == 0);

  blosc2_remove_urlpath(cframe_path);
  blosc2_remove_urlpath(sframe_path);
  blosc2_remove_urlpath(tofile_path);
  blosc2_remove_urlpath(append_path);
  return 0;
}


static char *test_flush_api(void) {
  const char *cpath = "test_flush_c.b2nd";
  const char *spath = "test_flush_s.b2nd";
  const char *mpath = "test_flush_mmap.b2frame";

  blosc2_remove_urlpath(cpath);
  blosc2_remove_urlpath(spath);
  blosc2_remove_urlpath(mpath);

  /* NULL handling */
  mu_assert("schunk_flush(NULL) must return BLOSC2_ERROR_NULL_POINTER",
            blosc2_schunk_flush(NULL) == BLOSC2_ERROR_NULL_POINTER);
  mu_assert("b2nd_flush(NULL) must return BLOSC2_ERROR_NULL_POINTER",
            b2nd_flush(NULL) == BLOSC2_ERROR_NULL_POINTER);

  int64_t shape[2] = {10, 20};
  int32_t chunkshape[2] = {5, 10};
  int32_t blockshape[2] = {5, 5};
  int32_t buf[200];
  int32_t out[200];
  for (int i = 0; i < 200; i++) {
    buf[i] = i * 3;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = 1;

  /* In-memory array and view */
  blosc2_storage mem_st = {.contiguous = true, .cparams = &cparams};
  b2nd_context_t *ctx = b2nd_create_ctx(&mem_st, 2, shape, chunkshape, blockshape, "<i4",
                                        DTYPE_NUMPY_FORMAT, NULL, 0);
  mu_assert("ctx failed", ctx != NULL);
  b2nd_array_t *arr = NULL;
  mu_assert("b2nd_from_cbuffer failed", b2nd_from_cbuffer(ctx, &arr, buf, sizeof(buf)) == 0);
  mu_assert("b2nd_flush on memory array failed", b2nd_flush(arr) == 0);

  bool axis[3] = {true, false, false};
  b2nd_array_t *view = NULL;
  mu_assert("b2nd_expand_dims failed", b2nd_expand_dims(arr, &view, axis, 3) == 0);
  mu_assert("b2nd_flush on view failed", b2nd_flush(view) == 0);
  mu_assert("b2nd_free view failed", b2nd_free(view) == 0);
  mu_assert("b2nd_free mem arr failed", b2nd_free(arr) == 0);
  b2nd_free_ctx(ctx);

  /* On-disk contiguous and sparse arrays */
  for (int c = 0; c < 2; c++) {
    const char *path = c ? cpath : spath;
    blosc2_storage disk_st = {.contiguous = (bool)c, .urlpath = (char *)path, .cparams = &cparams};
    b2nd_context_t *dctx = b2nd_create_ctx(&disk_st, 2, shape, chunkshape, blockshape, "<i4",
                                           DTYPE_NUMPY_FORMAT, NULL, 0);
    mu_assert("disk ctx failed", dctx != NULL);
    b2nd_array_t *darr = NULL;
    mu_assert("disk b2nd_from_cbuffer failed", b2nd_from_cbuffer(dctx, &darr, buf, sizeof(buf)) == 0);
    mu_assert("disk b2nd_flush failed", b2nd_flush(darr) == 0);
    /* Flushing a second time when already synced is a clean no-op */
    mu_assert("disk b2nd_flush second call failed", b2nd_flush(darr) == 0);
    mu_assert("disk b2nd_free failed", b2nd_free(darr) == 0);
    b2nd_free_ctx(dctx);

    /* Reopen read-only and flush (no-op) + verify data */
    b2nd_array_t *rarr = NULL;
    mu_assert("b2nd_open failed", b2nd_open(path, &rarr) == 0);
    mu_assert("read-only b2nd_flush failed", b2nd_flush(rarr) == 0);
    memset(out, 0, sizeof(out));
    mu_assert("b2nd_to_cbuffer failed", b2nd_to_cbuffer(rarr, out, sizeof(out)) == 0);
    mu_assert("roundtrip data mismatch", memcmp(buf, out, sizeof(buf)) == 0);
    mu_assert("reopened b2nd_free failed", b2nd_free(rarr) == 0);
  }

  /* On-disk mmap frame */
  blosc2_stdio_mmap mmap_params = BLOSC2_STDIO_MMAP_DEFAULTS;
  mmap_params.mode = "w+";
  mmap_params.initial_mapping_size = 1024 * 1024;
  blosc2_io mmap_io = {BLOSC2_IO_FILESYSTEM_MMAP, "mmap", &mmap_params};
  blosc2_storage mmap_st = {.contiguous = true, .urlpath = (char *)mpath,
                            .cparams = &cparams, .io = &mmap_io};
  blosc2_schunk *msc = blosc2_schunk_new(&mmap_st);
  mu_assert("mmap schunk_new failed", msc != NULL);
  mu_assert("mmap append failed", blosc2_schunk_append_buffer(msc, buf, sizeof(buf)) == 1);
  mu_assert("mmap schunk_flush failed", blosc2_schunk_flush(msc) == 0);
  mu_assert("mmap schunk_free failed", blosc2_schunk_free(msc) == 0);

  blosc2_stdio_mmap mmap_read_params = BLOSC2_STDIO_MMAP_DEFAULTS;
  mmap_read_params.mode = "r";
  blosc2_io mmap_read_io = {BLOSC2_IO_FILESYSTEM_MMAP, "mmap", &mmap_read_params};
  blosc2_schunk *mrsc = blosc2_schunk_open_udio(mpath, &mmap_read_io);
  mu_assert("mmap open_udio failed", mrsc != NULL);
  mu_assert("mmap read-only schunk_flush failed", blosc2_schunk_flush(mrsc) == 0);
  mu_assert("mmap read-only schunk_free failed", blosc2_schunk_free(mrsc) == 0);

  blosc2_remove_urlpath(cpath);
  blosc2_remove_urlpath(spath);
  blosc2_remove_urlpath(mpath);
  return 0;
}


#if !defined(_WIN32)
static char *test_cached_writer_close_propagation(void) {
  const char *path1 = "test_cached_close_flush.b2nd";
  const char *path2 = "test_cached_close_free.b2nd";
  blosc2_remove_urlpath(path1);
  blosc2_remove_urlpath(path2);

  int64_t shape[1] = {NITEMS};
  int32_t chunkshape[1] = {500};
  int32_t blockshape[1] = {250};
  int32_t buf[NITEMS];
  for (int i = 0; i < NITEMS; i++) {
    buf[i] = i;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = 1;

  /* Case A: error surfaced by b2nd_flush() / blosc2_schunk_flush() */
  blosc2_storage st1 = {.contiguous = true, .urlpath = (char *)path1, .cparams = &cparams};
  b2nd_context_t *ctx1 = b2nd_create_ctx(&st1, 1, shape, chunkshape, blockshape, "<i4",
                                         DTYPE_NUMPY_FORMAT, NULL, 0);
  mu_assert("ctx1 failed", ctx1 != NULL);
  b2nd_array_t *arr1 = NULL;
  mu_assert("arr1 create failed", b2nd_from_cbuffer(ctx1, &arr1, buf, sizeof(buf)) == 0);
  blosc2_frame_s *f1 = (blosc2_frame_s *)arr1->sc->frame;
  mu_assert("expected cached write_fp on POSIX", f1 != NULL && f1->write_fp != NULL);
  blosc2_stdio_file *wfp1 = (blosc2_stdio_file *)f1->write_fp;
  close(fileno(wfp1->file));
  mu_assert("b2nd_flush must report fsync/close error", b2nd_flush(arr1) == BLOSC2_ERROR_FILE_WRITE);
  mu_assert("b2nd_free after failed flush must succeed", b2nd_free(arr1) == 0);
  b2nd_free_ctx(ctx1);

  /* Case B: error surfaced directly by b2nd_free() -> blosc2_schunk_free() -> frame_free() */
  blosc2_storage st2 = {.contiguous = true, .urlpath = (char *)path2, .cparams = &cparams};
  b2nd_context_t *ctx2 = b2nd_create_ctx(&st2, 1, shape, chunkshape, blockshape, "<i4",
                                         DTYPE_NUMPY_FORMAT, NULL, 0);
  mu_assert("ctx2 failed", ctx2 != NULL);
  b2nd_array_t *arr2 = NULL;
  mu_assert("arr2 create failed", b2nd_from_cbuffer(ctx2, &arr2, buf, sizeof(buf)) == 0);
  blosc2_frame_s *f2 = (blosc2_frame_s *)arr2->sc->frame;
  mu_assert("expected cached write_fp on POSIX", f2 != NULL && f2->write_fp != NULL);
  blosc2_stdio_file *wfp2 = (blosc2_stdio_file *)f2->write_fp;
  close(fileno(wfp2->file));
  mu_assert("b2nd_free must report cached writer close error", b2nd_free(arr2) == BLOSC2_ERROR_FILE_WRITE);
  b2nd_free_ctx(ctx2);

  blosc2_remove_urlpath(path1);
  blosc2_remove_urlpath(path2);
  return 0;
}
#endif


static char *all_tests(void) {
  mu_run_test(test_custom_io_close_errors);
  mu_run_test(test_flush_api);
#if !defined(_WIN32)
  mu_run_test(test_cached_writer_close_propagation);
#endif
  return 0;
}


int main(void) {
  blosc2_init();

  blosc2_io_cb io_cb;
  io_cb.id = FAILCLOSE_IO_ID;
  io_cb.name = "failclose";
  io_cb.is_allocation_necessary = true;
  io_cb.open = fc_open;
  io_cb.close = fc_close;
  io_cb.read = fc_read;
  io_cb.size = fc_size;
  io_cb.write = fc_write;
  io_cb.truncate = fc_truncate;
  io_cb.destroy = fc_destroy;
  if (blosc2_register_io_cb(&io_cb) < 0) {
    printf("Failed to register failclose IO callback\n");
    blosc2_destroy();
    return 1;
  }

  char *result = all_tests();
  if (result != 0) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED\n");
  }
  printf("Tests run: %d\n", tests_run);

  blosc2_destroy();
  return result != 0;
}
