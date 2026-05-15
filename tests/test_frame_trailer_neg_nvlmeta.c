/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Regression test for a signedness bug in the trailer parser
  `get_vlmeta_from_trailer` in frame.c. The count was decoded
  into a local `int16_t nmetalayers`, so a wire value of 0xffff
  was interpreted as -1, slipped past the `> BLOSC2_MAX_VLMETALAYERS`
  ceiling, and was stored as a negative `schunk->nvlmetalayers`.
  A later `blosc2_vlmeta_add` then did
      schunk->vlmetalayers[schunk->nvlmetalayers] = ...;
      schunk->nvlmetalayers += 1;
  i.e. an out-of-bounds write through `vlmetalayers[-1]` into adjacent
  struct fields of `blosc2_schunk`. The fix decodes the count as
  `uint16_t` (matching the on-wire encoding and the matching reader in
  `get_meta_from_header`), so the ceiling check rejects 0xffff.
*/

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "blosc-private.h"
#include "frame.h"
#include "test_common.h"

#define CHUNKSIZE (256)
#define NCHUNKS (2)

int tests_run = 0;

/* The trailer layout (see frame_update_trailer) starts with:
 *   [0]   0x94             // fixarray with 4 elements
 *   [1]   FRAME_TRAILER_VERSION (==1)
 *   [2]   0x93             // fixarray with 3 elements
 *   [3]   0xcd             // uint16 marker (next 2 bytes = idx_size)
 *   [4-5] idx_size
 *   [6]   0xde             // map16 marker (next 2 bytes = nvlmetalayers)
 *   [7-8] nvlmetalayers    // uint16 big endian
 * Scan from the end of the cframe for this distinctive 4-byte
 * sequence and return the absolute offset of byte [7]. */
static int64_t find_trailer_nvlmeta_offset(const uint8_t *buf, int64_t buflen) {
  const uint8_t marker[4] = {0x94, FRAME_TRAILER_VERSION, 0x93, 0xcd};
  /* Trailer is short (FRAME_TRAILER_MINLEN==25, plus per-vlmeta overhead),
   * so it lives in the last few hundred bytes. Cap the scan window so we
   * don't accidentally match a collision deeper inside compressed data. */
  int64_t scan_from = buflen - 4096;
  if (scan_from < 0) {
    scan_from = 0;
  }
  for (int64_t i = buflen - 4; i >= scan_from; --i) {
    if (memcmp(buf + i, marker, sizeof(marker)) == 0 &&
        i + 7 + 2 <= buflen &&
        buf[i + 6] == 0xde) {
      return i + 7;
    }
  }
  return -1;
}

static char* test_reject_trailer_negative_nvlmetalayers(void) {
  blosc2_init();

  blosc2_storage storage = {.contiguous = true};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("Cannot create schunk", schunk != NULL);

  /* One vlmetalayer is enough to give the trailer a real
   * `0x94 0x01 0x93 0xcd ... 0xde NN NN` shape. */
  uint8_t vlmeta_payload[16];
  for (size_t i = 0; i < sizeof(vlmeta_payload); ++i) {
    vlmeta_payload[i] = (uint8_t)(0x5A ^ i);
  }
  int rc = blosc2_vlmeta_add(schunk, "vlsign", vlmeta_payload,
                             (int32_t)sizeof(vlmeta_payload), NULL);
  mu_assert("Cannot add vlmetalayer", rc >= 0);

  int32_t data[CHUNKSIZE];
  for (int i = 0; i < CHUNKSIZE; ++i) {
    data[i] = i;
  }
  for (int n = 0; n < NCHUNKS; ++n) {
    int64_t k = blosc2_schunk_append_buffer(schunk, data, (int32_t)sizeof(data));
    mu_assert("Cannot append chunk", k >= 0);
  }

  uint8_t *cframe = NULL;
  bool cframe_needs_free = false;
  int64_t cframe_len = blosc2_schunk_to_buffer(schunk, &cframe, &cframe_needs_free);
  mu_assert("Cannot export cframe", cframe_len > 0 && cframe != NULL);

  int64_t nvlmeta_off = find_trailer_nvlmeta_offset(cframe, cframe_len);
  mu_assert("Cannot locate trailer nvlmetalayers field", nvlmeta_off > 0);

  /* Sanity check: the unmodified cframe must round-trip cleanly. */
  blosc2_schunk *sanity = blosc2_schunk_from_buffer(cframe, cframe_len, true);
  mu_assert("Unmodified cframe must round-trip", sanity != NULL);
  mu_assert("Sanity schunk vlmetalayer count must be 1",
            sanity->nvlmetalayers == 1);
  blosc2_schunk_free(sanity);

  /* Corrupt the trailer's nvlmetalayers field to 0xffff. Pre-fix,
   * this was decoded into `int16_t` as -1 and slipped past the
   * `> BLOSC2_MAX_VLMETALAYERS` check, leaving `schunk->nvlmetalayers`
   * negative. With the fix it is decoded as uint16_t (65535) and
   * the ceiling check rejects the frame. */
  cframe[nvlmeta_off + 0] = 0xff;
  cframe[nvlmeta_off + 1] = 0xff;

  blosc2_schunk *decoded = blosc2_schunk_from_buffer(cframe, cframe_len, true);
  mu_assert("Trailer nvlmetalayers=0xffff must be rejected", decoded == NULL);

  /* Also reject an unambiguously-too-large value just above the
   * ceiling. This was already caught pre-fix because the ceiling
   * compare worked for positive int16 values; the assertion guards
   * against the fix accidentally widening what is accepted. */
  uint16_t too_big = (uint16_t)(BLOSC2_MAX_VLMETALAYERS + 1);
  cframe[nvlmeta_off + 0] = (uint8_t)(too_big >> 8);
  cframe[nvlmeta_off + 1] = (uint8_t)(too_big & 0xff);

  decoded = blosc2_schunk_from_buffer(cframe, cframe_len, true);
  mu_assert("Trailer nvlmetalayers > BLOSC2_MAX_VLMETALAYERS must be rejected",
            decoded == NULL);

  if (cframe_needs_free) {
    free(cframe);
  }
  blosc2_schunk_free(schunk);
  blosc2_destroy();

  return EXIT_SUCCESS;
}


static char *all_tests(void) {
  mu_run_test(test_reject_trailer_negative_nvlmetalayers);
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
