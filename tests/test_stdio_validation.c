/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Regression tests for argument validation and overflow hardening in the stdio
  IO backend (blosc/blosc2-stdio.c). Pre-hardening, negative sizes/positions
  and size*nitems overflow would silently propagate through (size_t) casts
  into fread/fwrite. The contract for an is_allocation_necessary=true backend
  also requires that *ptr is left untouched on read failure so the caller can
  still free its own buffer.
*/

#include "test_common.h"
#include "cutest.h"
#include "blosc2/blosc2-stdio.h"

#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>


CUTEST_TEST_DATA(stdio_validation) {
  bool placeholder;
};

CUTEST_TEST_SETUP(stdio_validation) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_init();
}

CUTEST_TEST_TEST(stdio_validation) {
  BLOSC_UNUSED_PARAM(data);

  /* Open/close hardening: invalid arguments must be rejected safely. */
  CUTEST_ASSERT("stdio open must reject NULL path",
                blosc2_stdio_open(NULL, "rb", NULL) == NULL);
  CUTEST_ASSERT("stdio open must reject NULL mode",
                blosc2_stdio_open("test_stdio_validation.tmp", NULL, NULL) == NULL);
  CUTEST_ASSERT("stdio close must reject NULL stream", blosc2_stdio_close(NULL) == -1);

  /* Size/truncate hardening: invalid stream/state must be rejected safely. */
  CUTEST_ASSERT("stdio size must reject NULL stream", blosc2_stdio_size(NULL) == -1);
  CUTEST_ASSERT("stdio truncate must reject NULL stream", blosc2_stdio_truncate(NULL, 0) == -1);
  blosc2_stdio_file invalid_fp_neg = { NULL };
  CUTEST_ASSERT("stdio truncate must reject negative size", blosc2_stdio_truncate(&invalid_fp_neg, -1) == -1);

  blosc2_stdio_file invalid_fp = { NULL };
  CUTEST_ASSERT("stdio size must reject NULL FILE*", blosc2_stdio_size(&invalid_fp) == -1);
  CUTEST_ASSERT("stdio truncate must reject NULL FILE*", blosc2_stdio_truncate(&invalid_fp, 0) == -1);

  blosc2_stdio_file fp;
  fp.file = tmpfile();
  CUTEST_ASSERT("tmpfile() must succeed", fp.file != NULL);

  /* Baseline: a valid write/read round-trip still works. */
  uint8_t src_ok[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  int64_t nw = blosc2_stdio_write(src_ok, 1, 8, 0, &fp);
  CUTEST_ASSERT("valid stdio write should succeed", nw == 8);

  uint8_t dst_buf[8] = {0};
  void* read_ptr = dst_buf;
  int64_t nr = blosc2_stdio_read(&read_ptr, 1, 8, 0, &fp);
  CUTEST_ASSERT("valid stdio read should succeed", nr == 8);
  CUTEST_ASSERT("valid stdio read should fill caller buffer",
                memcmp(dst_buf, src_ok, 8) == 0);
  CUTEST_ASSERT("valid stdio read must not change *ptr",
                read_ptr == (void*)dst_buf);

  /* size() should not alter stream position. */
  int seek_rc = fseek(fp.file, 3, SEEK_SET);
  CUTEST_ASSERT("fseek for size-position test should succeed", seek_rc == 0);
  int64_t size_before = blosc2_stdio_size(&fp);
  CUTEST_ASSERT("stdio size should return current file size", size_before == 8);
  int64_t pos_after = ftell(fp.file);
  CUTEST_ASSERT("stdio size must preserve current file position", pos_after == 3);

  /* Write: reject NULL stream and NULL underlying FILE*. */
  nw = blosc2_stdio_write(src_ok, 1, 1, 0, NULL);
  CUTEST_ASSERT("stdio write must reject NULL stream", nw == 0);
  blosc2_stdio_file bad_fp = { NULL };
  nw = blosc2_stdio_write(src_ok, 1, 1, 0, &bad_fp);
  CUTEST_ASSERT("stdio write must reject NULL my_fp->file", nw == 0);

  /* Write: reject NULL data pointer. */
  nw = blosc2_stdio_write(NULL, 1, 1, 0, &fp);
  CUTEST_ASSERT("stdio write must reject NULL ptr", nw == 0);

  /* Write: reject negative size / nitems / position. */
  nw = blosc2_stdio_write(src_ok, -1, 1, 0, &fp);
  CUTEST_ASSERT("stdio write must reject negative size", nw == 0);
  nw = blosc2_stdio_write(src_ok, 1, -1, 0, &fp);
  CUTEST_ASSERT("stdio write must reject negative nitems", nw == 0);
  nw = blosc2_stdio_write(src_ok, 1, 1, -1, &fp);
  CUTEST_ASSERT("stdio write must reject negative position", nw == 0);

  /* Write: reject size*nitems overflow. */
  nw = blosc2_stdio_write(src_ok, INT64_MAX, 2, 0, &fp);
  CUTEST_ASSERT("stdio write must reject size*nitems overflow", nw == 0);

  /* Read: reject NULL stream and NULL underlying FILE*. */
  read_ptr = dst_buf;
  nr = blosc2_stdio_read(&read_ptr, 1, 1, 0, NULL);
  CUTEST_ASSERT("stdio read must reject NULL stream", nr == 0);
  nr = blosc2_stdio_read(&read_ptr, 1, 1, 0, &bad_fp);
  CUTEST_ASSERT("stdio read must reject NULL my_fp->file", nr == 0);

  /* Read: reject NULL out-parameter. */
  nr = blosc2_stdio_read(NULL, 1, 1, 0, &fp);
  CUTEST_ASSERT("stdio read must reject NULL ptr argument", nr == 0);

  /* Read: reject NULL buffer (*ptr) when non-zero bytes requested. */
  void* null_buf = NULL;
  nr = blosc2_stdio_read(&null_buf, 1, 1, 0, &fp);
  CUTEST_ASSERT("stdio read must reject NULL *ptr with non-zero size", nr == 0);
  CUTEST_ASSERT("stdio read must NOT touch *ptr on NULL-buffer rejection",
                null_buf == NULL);

  /* Read: on every failure path, *ptr (caller-owned buffer) must be untouched.
     This is the is_allocation_necessary=true contract: nulling it would leak
     the caller's malloc'd buffer. */
  void* sentinel = (void*)(uintptr_t)0xDEADBEEF;

  read_ptr = sentinel;
  nr = blosc2_stdio_read(&read_ptr, -1, 1, 0, &fp);
  CUTEST_ASSERT("stdio read must reject negative size", nr == 0);
  CUTEST_ASSERT("stdio read must NOT touch *ptr on negative size",
                read_ptr == sentinel);

  read_ptr = sentinel;
  nr = blosc2_stdio_read(&read_ptr, 1, -1, 0, &fp);
  CUTEST_ASSERT("stdio read must reject negative nitems", nr == 0);
  CUTEST_ASSERT("stdio read must NOT touch *ptr on negative nitems",
                read_ptr == sentinel);

  read_ptr = sentinel;
  nr = blosc2_stdio_read(&read_ptr, 1, 1, -1, &fp);
  CUTEST_ASSERT("stdio read must reject negative position", nr == 0);
  CUTEST_ASSERT("stdio read must NOT touch *ptr on negative position",
                read_ptr == sentinel);

  read_ptr = sentinel;
  nr = blosc2_stdio_read(&read_ptr, INT64_MAX, 2, 0, &fp);
  CUTEST_ASSERT("stdio read must reject size*nitems overflow", nr == 0);
  CUTEST_ASSERT("stdio read must NOT touch *ptr on overflow",
                read_ptr == sentinel);

#if !defined(_WIN32) && (LONG_MAX < INT64_MAX)
  /* POSIX fseek takes long; on 32-bit POSIX, positions above LONG_MAX would
     truncate. The guard must reject such positions cleanly. */
  read_ptr = sentinel;
  nr = blosc2_stdio_read(&read_ptr, 1, 1, (int64_t)LONG_MAX + 1, &fp);
  CUTEST_ASSERT("stdio read must reject position > LONG_MAX on POSIX",
                nr == 0);
  CUTEST_ASSERT("stdio read must NOT touch *ptr on LONG_MAX overflow",
                read_ptr == sentinel);

  nw = blosc2_stdio_write(src_ok, 1, 1, (int64_t)LONG_MAX + 1, &fp);
  CUTEST_ASSERT("stdio write must reject position > LONG_MAX on POSIX",
                nw == 0);
#endif

  /* mmap API hardening: reject invalid argument/state transitions safely. */
  blosc2_stdio_mmap mmap_null_path = BLOSC2_STDIO_MMAP_DEFAULTS;
  CUTEST_ASSERT("mmap open must reject NULL path",
                blosc2_stdio_mmap_open(NULL, "rb", &mmap_null_path) == NULL);
  CUTEST_ASSERT("mmap open must reject NULL params",
                blosc2_stdio_mmap_open("test_stdio_validation.mmap", "rb", NULL) == NULL);

  blosc2_stdio_mmap mmap_params = BLOSC2_STDIO_MMAP_DEFAULTS;
  mmap_params.mode = NULL;
  CUTEST_ASSERT("mmap open must reject NULL mmap mode",
                blosc2_stdio_mmap_open("test_stdio_validation.mmap", "rb", &mmap_params) == NULL);
  CUTEST_ASSERT("mmap destroy must reject NULL params", blosc2_stdio_mmap_destroy(NULL) == -1);

  blosc2_stdio_mmap invalid_mmap = BLOSC2_STDIO_MMAP_DEFAULTS;
  CUTEST_ASSERT("mmap destroy must reject invalid uninitialized mapping state",
                blosc2_stdio_mmap_destroy(&invalid_mmap) == -1);

  fclose(fp.file);
  return 0;
}

CUTEST_TEST_TEARDOWN(stdio_validation) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_destroy();
}


int main(void) {
  CUTEST_TEST_RUN(stdio_validation);
}
