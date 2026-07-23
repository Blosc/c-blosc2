/*
  Regression test: getitem on a BLOSC2_SPECIAL_VALUE chunk must derive the
  repeated value's width from cbytes, not from the header typesize byte.

  The typesize byte is not constrained for special-value chunks (only the
  derived width cbytes - BLOSC_EXTENDED_HEADER_LENGTH is validated), so a chunk
  whose typesize byte is larger than that width made _blosc_getitem() hand the
  oversized typesize to set_values(), which then read past the end of the chunk
  buffer (heap out-of-bounds read, CWE-125).
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "blosc2.h"
#include "cutest.h"

CUTEST_TEST_DATA(getitem_special_value_typesize) {
  int dummy;
};

CUTEST_TEST_SETUP(getitem_special_value_typesize) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_init();
}

CUTEST_TEST_TEARDOWN(getitem_special_value_typesize) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_destroy();
}

CUTEST_TEST_TEST(getitem_special_value_typesize) {
  BLOSC_UNUSED_PARAM(data);

  // A valid special-value chunk with a 1-byte value: cbytes == header + 1.
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  uint8_t value = 0xAB;
  uint8_t scratch[128];
  int csize = blosc2_chunk_repeatval(cparams, 64, scratch, sizeof(scratch), &value);
  CUTEST_ASSERT("repeatval failed", csize == BLOSC_EXTENDED_HEADER_LENGTH + 1);

  // Copy the chunk into an exact-size allocation so any read past its end is a
  // genuine heap overflow (and is caught by ASAN when enabled).
  uint8_t *chunk = malloc((size_t)csize);
  CUTEST_ASSERT("alloc failed", chunk != NULL);
  memcpy(chunk, scratch, (size_t)csize);

  // Corrupt the header typesize byte to be wider than the real value width.
  chunk[BLOSC2_CHUNK_TYPESIZE] = 8;

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  blosc2_context *dctx = blosc2_create_dctx(dparams);
  CUTEST_ASSERT("dctx create failed", dctx != NULL);

  uint8_t out[8];
  memset(out, 0, sizeof(out));
  int rc = blosc2_getitem_ctx(dctx, chunk, csize, 0, 1, out, sizeof(out));
  blosc2_free_ctx(dctx);
  free(chunk);

  CUTEST_ASSERT("getitem returned wrong size", rc == 8);
  // With the real 1-byte width the value is replicated across every output
  // byte; before the fix the extra bytes came from an out-of-bounds read.
  int ok = 1;
  for (int i = 0; i < 8; i++) {
    if (out[i] != 0xAB) ok = 0;
  }
  CUTEST_ASSERT("getitem read outside the chunk buffer", ok);

  return 0;
}

int main(void) {
  CUTEST_TEST_RUN(getitem_special_value_typesize);
}
