/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Regression test for an OOB-read in frame_get_lazychunk when an attacker
  crafts a contiguous frame whose chunk header advertises a `cbytes` value
  that extends past the end of the in-memory cframe buffer.

  Before the fix, the in-memory branch of frame_get_lazychunk parsed the
  chunk header but the post-parse validation was guarded by
  `if (rc && ...)`. Since blosc2_cbuffer_sizes returns 0 on success, the
  validation was effectively dead code on the success path, and the
  attacker-controlled `cbytes` was returned to callers as the chunk size.
  Callers (e.g. blosc2_decompress_ctx, blosc2_getitem_ctx) then trusted
  this size as srcsize and read past frame->cframe.
*/

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "blosc-private.h"
#include "frame.h"
#include "test_common.h"

#define CHUNKSIZE (256)

/* Global vars */
int tests_run = 0;


static void store_le32(uint8_t *dest, int32_t value) {
  dest[0] = (uint8_t)(value & 0xFF);
  dest[1] = (uint8_t)((value >> 8) & 0xFF);
  dest[2] = (uint8_t)((value >> 16) & 0xFF);
  dest[3] = (uint8_t)((value >> 24) & 0xFF);
}


static int32_t read_le32(const uint8_t *src) {
  return (int32_t)((uint32_t)src[0] |
                   ((uint32_t)src[1] << 8) |
                   ((uint32_t)src[2] << 16) |
                   ((uint32_t)src[3] << 24));
}


static char* test_lazychunk_rejects_inflated_cbytes(void) {
  int32_t data[CHUNKSIZE];

  blosc2_init();

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.clevel = 5;

  blosc2_storage storage = {.contiguous = true, .cparams = &cparams};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("Cannot create schunk", schunk != NULL);

  for (int i = 0; i < CHUNKSIZE; ++i) {
    data[i] = i;
  }

  /* Use two chunks: frame_to_schunk inspects chunk[0] at open time, but
   * with copy=false it does not iterate the per-chunk validation loop,
   * so chunk[1]'s header reaches frame_get_lazychunk untouched. */
  for (int c = 0; c < 2; ++c) {
    int64_t n = blosc2_schunk_append_buffer(schunk, data, (int32_t)sizeof(data));
    mu_assert("Cannot append chunk", n >= 0);
  }

  uint8_t *cframe = NULL;
  bool cframe_needs_free = false;
  int64_t cframe_len = blosc2_schunk_to_buffer(schunk, &cframe, &cframe_needs_free);
  mu_assert("Cannot export cframe", cframe_len > 0 && cframe != NULL);

  int32_t header_len;
  int64_t cbytes;
  from_big(&header_len, cframe + FRAME_HEADER_LEN, sizeof(header_len));
  from_big(&cbytes, cframe + FRAME_CBYTES, sizeof(cbytes));
  mu_assert("Invalid header_len", header_len >= FRAME_HEADER_MINLEN);
  mu_assert("Invalid cbytes", cbytes > 0);

  /* In a freshly built contiguous schunk, chunks are laid out
   * sequentially in append order, so chunk[1] starts right after
   * chunk[0]'s cbytes. */
  int32_t chunk0_cbytes = read_le32(cframe + header_len + BLOSC2_CHUNK_CBYTES);
  mu_assert("Bogus chunk[0] cbytes",
            chunk0_cbytes >= BLOSC_EXTENDED_HEADER_LENGTH &&
            (int64_t)header_len + chunk0_cbytes + BLOSC_EXTENDED_HEADER_LENGTH <= cframe_len);

  int64_t chunk1_offset = (int64_t)header_len + chunk0_cbytes;
  mu_assert("chunk[1] header out of bounds",
            chunk1_offset + BLOSC_EXTENDED_HEADER_LENGTH <= cframe_len);

  int32_t chunk1_cbytes_orig = -1;
  int rc = blosc2_cbuffer_sizes(cframe + chunk1_offset, NULL, &chunk1_cbytes_orig, NULL);
  mu_assert("Cannot read chunk[1] header", rc == 0);
  mu_assert("Unexpected original chunk[1] cbytes",
            chunk1_cbytes_orig >= BLOSC_EXTENDED_HEADER_LENGTH &&
            chunk1_offset + chunk1_cbytes_orig <= cframe_len);

  /* Tamper: inflate chunk[1]'s `cbytes` to a value that extends way past
   * the end of the cframe buffer.  This is exactly the malformed input
   * the in-memory branch of frame_get_lazychunk has to reject. */
  int32_t inflated_cbytes = INT32_MAX;
  store_le32(cframe + chunk1_offset + BLOSC2_CHUNK_CBYTES, inflated_cbytes);

  /* Use copy=false so frame_to_schunk's per-chunk validation loop is
   * skipped (the loop only runs on copy=true).  This is the same flag
   * accepted by the public API. */
  blosc2_schunk *decoded = blosc2_schunk_from_buffer(cframe, cframe_len, false);
  mu_assert("Cannot open in-memory cframe", decoded != NULL);

  uint8_t *lazy_chunk = NULL;
  bool needs_free = false;
  int got_cbytes = blosc2_schunk_get_lazychunk(decoded, 1, &lazy_chunk, &needs_free);

  /* Before the fix this would return inflated_cbytes (positive, larger
   * than cframe_len), which any caller would then pass as srcsize to
   * blosc2_decompress_ctx -> OOB read.  After the fix it must be a
   * negative error code. */
  mu_assert("frame_get_lazychunk must reject inflated cbytes (got positive size larger than frame)",
            got_cbytes < 0 || (int64_t)got_cbytes <= cframe_len);
  mu_assert("frame_get_lazychunk should signal a read-buffer error",
            got_cbytes < 0);

  if (needs_free && lazy_chunk != NULL) {
    free(lazy_chunk);
  }

  blosc2_schunk_free(decoded);
  blosc2_schunk_free(schunk);
  if (cframe_needs_free) {
    free(cframe);
  }
  blosc2_destroy();

  return EXIT_SUCCESS;
}


static char* test_lazychunk_accepts_well_formed_cbytes(void) {
  /* Companion test: make sure the new validation does not regress the
   * happy path — a freshly built frame must still hand out a usable
   * lazy chunk that round-trips through blosc2_decompress_ctx. */
  int32_t data[CHUNKSIZE];
  int32_t data_dest[CHUNKSIZE];

  blosc2_init();

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.clevel = 5;

  blosc2_storage storage = {.contiguous = true, .cparams = &cparams};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("Cannot create schunk", schunk != NULL);

  for (int i = 0; i < CHUNKSIZE; ++i) {
    data[i] = i;
  }
  int64_t n = blosc2_schunk_append_buffer(schunk, data, (int32_t)sizeof(data));
  mu_assert("Cannot append chunk", n >= 0);

  uint8_t *cframe = NULL;
  bool cframe_needs_free = false;
  int64_t cframe_len = blosc2_schunk_to_buffer(schunk, &cframe, &cframe_needs_free);
  mu_assert("Cannot export cframe", cframe_len > 0 && cframe != NULL);

  blosc2_schunk *decoded = blosc2_schunk_from_buffer(cframe, cframe_len, false);
  mu_assert("Cannot open in-memory cframe", decoded != NULL);

  uint8_t *lazy_chunk = NULL;
  bool needs_free = false;
  int got_cbytes = blosc2_schunk_get_lazychunk(decoded, 0, &lazy_chunk, &needs_free);
  mu_assert("Lazy chunk for well-formed frame should succeed", got_cbytes > 0);

  int dsize = blosc2_decompress_ctx(decoded->dctx, lazy_chunk, got_cbytes,
                                    data_dest, (int32_t)sizeof(data_dest));
  mu_assert("Decompression of lazy chunk failed", dsize >= 0);
  mu_assert("Round-trip data mismatch", memcmp(data, data_dest, sizeof(data)) == 0);

  if (needs_free && lazy_chunk != NULL) {
    free(lazy_chunk);
  }

  blosc2_schunk_free(decoded);
  blosc2_schunk_free(schunk);
  if (cframe_needs_free) {
    free(cframe);
  }
  blosc2_destroy();

  return EXIT_SUCCESS;
}


static char *all_tests(void) {
  mu_run_test(test_lazychunk_rejects_inflated_cbytes);
  mu_run_test(test_lazychunk_accepts_well_formed_cbytes);
  return EXIT_SUCCESS;
}


int main(void) {
  char *result;

  result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  return result != EXIT_SUCCESS;
}
