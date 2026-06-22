/*
  Regression test for GitHub issue #665 (python-blosc2, root cause in c-blosc2):
  compress2/decompress2 corrupt all-zeros buffer when length is not a multiple
  of typesize.  See https://github.com/Blosc/python-blosc2/issues/665.

  The fix: `read_chunk_header` no longer rejects BLOSC2_SPECIAL_ZERO chunks
  with non-multiple nbytes (memset handles any size).
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "blosc2.h"
#include "cutest.h"

static void build_special_zero_chunk(uint8_t *out, int32_t nbytes,
                                     int32_t typesize) {
  memset(out, 0, BLOSC_EXTENDED_HEADER_LENGTH);

  int32_t blocksize = nbytes > 1024 ? 1024 : nbytes;
  blocksize = blocksize / typesize * typesize;
  if (blocksize < typesize) blocksize = typesize;

  uint8_t flags = 0x1 | 0x4;  // DOSHUFFLE | DOBITSHUFFLE = extended header
  flags |= 1 << 5;             // LZ4 compformat

  out[0] = BLOSC2_VERSION_FORMAT_STABLE;
  out[1] = 1;                          // versionlz
  out[2] = flags;
  out[3] = (uint8_t)typesize;

  // nbytes (offset 4)
  out[4] = (uint8_t)(nbytes & 0xff);
  out[5] = (uint8_t)((nbytes >> 8) & 0xff);
  out[6] = (uint8_t)((nbytes >> 16) & 0xff);
  out[7] = (uint8_t)((nbytes >> 24) & 0xff);

  // blocksize (offset 8)
  out[8]  = (uint8_t)(blocksize & 0xff);
  out[9]  = (uint8_t)((blocksize >> 8) & 0xff);
  out[10] = (uint8_t)((blocksize >> 16) & 0xff);
  out[11] = (uint8_t)((blocksize >> 24) & 0xff);

  // cbytes = header only (offset 12)
  int32_t cbytes = BLOSC_EXTENDED_HEADER_LENGTH;
  out[12] = (uint8_t)(cbytes & 0xff);
  out[13] = (uint8_t)((cbytes >> 8) & 0xff);
  out[14] = (uint8_t)((cbytes >> 16) & 0xff);
  out[15] = (uint8_t)((cbytes >> 24) & 0xff);

  // blosc2_flags at offset 31: SPECIAL_ZERO
  out[31] = (uint8_t)(BLOSC2_SPECIAL_ZERO << 4);
}

typedef struct {
  int32_t nbytes;
  int32_t typesize;
} test_params;

CUTEST_TEST_DATA(special_zero_nonmultiple) {
  int dummy;
};

CUTEST_TEST_SETUP(special_zero_nonmultiple) {
  blosc2_init();

  CUTEST_PARAMETRIZE(p, test_params, CUTEST_DATA(
    /* non-multiple sizes — the bug scenario */
    {707658, 8},   // original bug report: 707658 % 8 == 2
    {80001, 8},    // 80001 % 8 == 1
    {80007, 8},    // 80007 % 8 == 7
    {101, 4},      // 101 % 4 == 1
    {103, 4},      // 103 % 4 == 3
    {51, 2},       // 51 % 2 == 1
    /* multiple sizes — control */
    {707656, 8},   // 707656 % 8 == 0
    {100, 4},      // 100 % 4 == 0
    {50, 2},       // 50 % 2 == 0
  ));
}

CUTEST_TEST_TEARDOWN(special_zero_nonmultiple) {
  (void)data;
  blosc2_destroy();
}

CUTEST_TEST_TEST(special_zero_nonmultiple) {
  CUTEST_GET_PARAMETER(p, test_params);

  int32_t nbytes = p.nbytes;
  int32_t typesize = p.typesize;

  uint8_t chunk[BLOSC_EXTENDED_HEADER_LENGTH];
  build_special_zero_chunk(chunk, nbytes, typesize);

  /* cbuffer_sizes must return correct nbytes */
  int32_t cb_nbytes, cb_cbytes, cb_blocksize;
  int rc = blosc2_cbuffer_sizes(chunk, &cb_nbytes, &cb_cbytes, &cb_blocksize);
  CUTEST_ASSERT("cbuffer_sizes should succeed", rc == 0);
  CUTEST_ASSERT("nbytes must match", cb_nbytes == nbytes);
  CUTEST_ASSERT("cbytes must be header-only", cb_cbytes == BLOSC_EXTENDED_HEADER_LENGTH);

  /* Decompress */
  uint8_t* decompressed = malloc((size_t)nbytes + 1);
  CUTEST_ASSERT("Allocation error", decompressed != NULL);
  memset(decompressed, 0xcc, (size_t)nbytes);
  decompressed[nbytes] = 0xbb;  // sentinel

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  blosc2_context* dctx = blosc2_create_dctx(dparams);
  CUTEST_ASSERT("Could not create decompression context", dctx != NULL);

  int dbytes = blosc2_decompress_ctx(dctx, chunk, BLOSC_EXTENDED_HEADER_LENGTH,
                                      decompressed, nbytes);
  blosc2_free_ctx(dctx);
  CUTEST_ASSERT("Decompression error", dbytes == nbytes);

  /* Verify all zeros and sentinel untouched */
  CUTEST_ASSERT("Sentinel byte overwritten", decompressed[nbytes] == 0xbb);
  int mismatch = 0;
  for (int32_t i = 0; i < nbytes; i++) {
    if (decompressed[i] != 0) {
      mismatch = 1;
      break;
    }
  }
  CUTEST_ASSERT("Decompressed data is not all zeros", !mismatch);

  free(decompressed);

  return 0;
}

int main(void) {
  CUTEST_TEST_RUN(special_zero_nonmultiple);
}
