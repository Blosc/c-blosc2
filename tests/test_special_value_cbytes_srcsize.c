/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Regression test: a BLOSC2_SPECIAL_VALUE chunk whose header cbytes is larger
  than the buffer handed to the decoder must be rejected, even when the lazy
  bit is set in its blosc2_flags.

  The width of the repeated value is derived from cbytes, and set_values()
  reads that many bytes after the extended header.  The only cbytes vs srcsize
  check, in blosc2_initialize_context_from_header(), was skipped for any chunk
  with the lazy bit, but blosc_d() and _blosc_getitem() never treat a special
  chunk as lazy, so they went on to read cbytes - 32 bytes out of a smaller
  buffer (CWE-125) and replicate them into the caller's output.  The same
  happens for a file frame, where frame_get_lazychunk() returns a special
  chunk of BLOSC_EXTENDED_HEADER_LENGTH + frame typesize bytes.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "blosc2.h"
#include "cutest.h"

#define TYPESIZE 4
#define NITEMS 200
#define FAKE_WIDTH 200  // divides NITEMS * TYPESIZE, so the header still validates
#define LAZY_FLAG 0x08  // lazy chunk bit in BLOSC2_CHUNK_BLOSC2_FLAGS

static void store_le32(uint8_t *dest, int32_t value) {
  dest[0] = (uint8_t)(value & 0xFF);
  dest[1] = (uint8_t)((value >> 8) & 0xFF);
  dest[2] = (uint8_t)((value >> 16) & 0xFF);
  dest[3] = (uint8_t)((value >> 24) & 0xFF);
}

CUTEST_TEST_DATA(special_value_cbytes_srcsize) {
  int dummy;
};

CUTEST_TEST_SETUP(special_value_cbytes_srcsize) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_init();
}

CUTEST_TEST_TEARDOWN(special_value_cbytes_srcsize) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_destroy();
}

CUTEST_TEST_TEST(special_value_cbytes_srcsize) {
  BLOSC_UNUSED_PARAM(data);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = TYPESIZE;
  uint8_t chunk[BLOSC_EXTENDED_HEADER_LENGTH + TYPESIZE];
  int32_t value = 0x11223344;
  int csize = blosc2_chunk_repeatval(cparams, NITEMS * TYPESIZE, chunk, sizeof(chunk), &value);
  CUTEST_ASSERT("repeatval failed", csize == (int)sizeof(chunk));

  // In-memory chunk, exact-size allocation, cbytes larger than srcsize
  uint8_t *bad = malloc(sizeof(chunk));
  CUTEST_ASSERT("alloc failed", bad != NULL);
  memcpy(bad, chunk, sizeof(chunk));
  store_le32(bad + BLOSC2_CHUNK_CBYTES, BLOSC_EXTENDED_HEADER_LENGTH + FAKE_WIDTH);
  bad[BLOSC2_CHUNK_BLOSC2_FLAGS] |= LAZY_FLAG;
  uint8_t *out = malloc(NITEMS * TYPESIZE);
  CUTEST_ASSERT("alloc failed", out != NULL);
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  blosc2_context *dctx = blosc2_create_dctx(dparams);
  CUTEST_ASSERT("dctx create failed", dctx != NULL);
  int rc = blosc2_decompress_ctx(dctx, bad, (int32_t)sizeof(chunk), out, NITEMS * TYPESIZE);
  CUTEST_ASSERT("decompress accepted cbytes > srcsize", rc < 0);
  rc = blosc2_getitem_ctx(dctx, bad, (int32_t)sizeof(chunk), 0, NITEMS, out, NITEMS * TYPESIZE);
  CUTEST_ASSERT("getitem accepted cbytes > srcsize", rc < 0);
  blosc2_free_ctx(dctx);
  free(bad);

  // The same chunk stored in a file frame and read back as a lazy chunk
  const char *urlpath = "test_special_value_cbytes_srcsize.b2frame";
  blosc2_remove_urlpath(urlpath);
  blosc2_storage storage = {.contiguous = true, .urlpath = (char *)urlpath, .cparams = &cparams};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  CUTEST_ASSERT("schunk create failed", schunk != NULL);
  int64_t nchunks = blosc2_schunk_append_chunk(schunk, chunk, true);
  CUTEST_ASSERT("append failed", nchunks == 1);
  blosc2_schunk_free(schunk);

  FILE *fp = fopen(urlpath, "rb+");
  CUTEST_ASSERT("cannot open frame", fp != NULL);
  fseek(fp, 0, SEEK_END);
  long flen = ftell(fp);
  uint8_t *frame = malloc((size_t)flen);
  CUTEST_ASSERT("alloc failed", frame != NULL);
  fseek(fp, 0, SEEK_SET);
  CUTEST_ASSERT("cannot read frame", fread(frame, 1, (size_t)flen, fp) == (size_t)flen);
  long pos = -1;
  for (long i = 0; i + (long)sizeof(chunk) <= flen; i++) {
    if (memcmp(frame + i, chunk, sizeof(chunk)) == 0) {
      pos = i;
      break;
    }
  }
  CUTEST_ASSERT("stored chunk not found", pos >= 0);
  uint8_t cbytes_le[4];
  store_le32(cbytes_le, BLOSC_EXTENDED_HEADER_LENGTH + FAKE_WIDTH);
  fseek(fp, pos + BLOSC2_CHUNK_CBYTES, SEEK_SET);
  CUTEST_ASSERT("cannot patch frame", fwrite(cbytes_le, 1, 4, fp) == 4);
  uint8_t flags = chunk[BLOSC2_CHUNK_BLOSC2_FLAGS] | LAZY_FLAG;
  fseek(fp, pos + BLOSC2_CHUNK_BLOSC2_FLAGS, SEEK_SET);
  CUTEST_ASSERT("cannot patch frame", fwrite(&flags, 1, 1, fp) == 1);
  fclose(fp);
  free(frame);

  schunk = blosc2_schunk_open(urlpath);
  CUTEST_ASSERT("cannot reopen frame", schunk != NULL);
  rc = blosc2_schunk_get_slice_buffer(schunk, 0, NITEMS, out);
  CUTEST_ASSERT("slice read accepted cbytes > lazy chunk size", rc < 0);
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(urlpath);
  free(out);

  return 0;
}

int main(void) {
  CUTEST_TEST_RUN(special_value_cbytes_srcsize);
}
