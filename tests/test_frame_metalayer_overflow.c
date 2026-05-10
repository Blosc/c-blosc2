/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Regression test for an integer-overflow bounds-bypass in the
  metalayer / vlmetalayer parsers in frame.c. A frame whose
  metalayer-content-length field is close to INT32_MAX caused
  `header_len < offset + 1 + 4 + content_len` (all int32_t) to
  wrap into a negative value and pass, after which `memcpy` would
  read far past the end of the frame buffer.
*/

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "blosc-private.h"
#include "frame.h"
#include "test_common.h"

#define CHUNKSIZE (256)
#define NCHUNKS (4)

int tests_run = 0;

/* Locate a 4-byte big-endian content-length field that immediately follows
 * a 0xc6 marker inside the frame buffer, after a known meta name. Returns
 * the offset of the 4-byte length, or -1 if not found. */
static int64_t find_meta_content_len_offset(const uint8_t *buf, int64_t buflen,
                                            const char *meta_name,
                                            int32_t expected_content_len) {
  size_t name_len = strlen(meta_name);
  if (buflen < (int64_t)(1 + name_len + 1 + 4)) {
    return -1;
  }

  for (int64_t i = 0; i + (int64_t)name_len < buflen; ++i) {
    if (memcmp(buf + i, meta_name, name_len) != 0) {
      continue;
    }
    /* Walk forward looking for the 0xc6 marker that introduces the content. */
    for (int64_t j = i + (int64_t)name_len; j + 5 <= buflen; ++j) {
      if (buf[j] != 0xc6) {
        continue;
      }
      int32_t found_len;
      from_big(&found_len, buf + j + 1, sizeof(found_len));
      if (found_len == expected_content_len) {
        return j + 1;
      }
    }
  }
  return -1;
}

static char* test_reject_metalayer_content_len_overflow(void) {
  blosc2_init();

  blosc2_storage storage = {.contiguous = true};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("Cannot create schunk", schunk != NULL);

  uint8_t meta_payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
  int rc = blosc2_meta_add(schunk, "ovrfl", meta_payload, (int32_t)sizeof(meta_payload));
  mu_assert("Cannot add metalayer", rc >= 0);

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

  int64_t len_field = find_meta_content_len_offset(cframe, cframe_len, "ovrfl",
                                                   (int32_t)sizeof(meta_payload));
  mu_assert("Cannot locate metalayer content length field", len_field > 0);

  /* Sanity check: an out-of-range but non-overflowing content_len must
   * already be rejected by the existing bounds check. This guards
   * against regressions in the error-path cleanup independently of
   * the overflow fix. */
  int32_t mid_len = 1 << 28;
  to_big(cframe + len_field, &mid_len, sizeof(mid_len));
  blosc2_schunk *sanity = blosc2_schunk_from_buffer(cframe, cframe_len, true);
  mu_assert("Out-of-range content_len must be rejected", sanity == NULL);

  /* Overwrite the length field with INT32_MAX. Pre-fix,
   * `offset + 5 + INT32_MAX` overflowed int32_t and the bounds check
   * passed, triggering a 2GB OOB read in memcpy. With the fix, the
   * parser must reject this frame instead of crashing. */
  int32_t huge_len = INT32_MAX;
  to_big(cframe + len_field, &huge_len, sizeof(huge_len));

  blosc2_schunk *decoded = blosc2_schunk_from_buffer(cframe, cframe_len, true);
  mu_assert("Overflowing metalayer content_len must be rejected", decoded == NULL);

  if (cframe_needs_free) {
    free(cframe);
  }
  blosc2_schunk_free(schunk);
  blosc2_destroy();

  return EXIT_SUCCESS;
}

static char* test_reject_vlmetalayer_content_len_overflow(void) {
  blosc2_init();

  blosc2_storage storage = {.contiguous = true};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("Cannot create schunk", schunk != NULL);

  /* Use an incompressible marker so we can find the (compressed) content
   * by scanning for it without colliding with chunk data. */
  uint8_t vlmeta_payload[64];
  for (size_t i = 0; i < sizeof(vlmeta_payload); ++i) {
    vlmeta_payload[i] = (uint8_t)(0xA5 ^ i);
  }
  int rc = blosc2_vlmeta_add(schunk, "vlovr", vlmeta_payload,
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

  /* Pull the actual stored (compressed) content length out of the schunk so
   * we know exactly which 4-byte field to overwrite in the cframe. */
  int idx = blosc2_vlmeta_exists(schunk, "vlovr");
  mu_assert("vlmetalayer must exist", idx >= 0);
  int32_t stored_content_len = schunk->vlmetalayers[idx]->content_len;
  mu_assert("vlmetalayer content must be non-empty", stored_content_len > 0);

  uint8_t *cframe = NULL;
  bool cframe_needs_free = false;
  int64_t cframe_len = blosc2_schunk_to_buffer(schunk, &cframe, &cframe_needs_free);
  mu_assert("Cannot export cframe", cframe_len > 0 && cframe != NULL);

  int64_t len_field = find_meta_content_len_offset(cframe, cframe_len, "vlovr",
                                                   stored_content_len);
  mu_assert("Cannot locate vlmetalayer content length field", len_field > 0);

  int32_t huge_len = INT32_MAX;
  to_big(cframe + len_field, &huge_len, sizeof(huge_len));

  blosc2_schunk *decoded = blosc2_schunk_from_buffer(cframe, cframe_len, true);
  mu_assert("Overflowing vlmetalayer content_len must be rejected", decoded == NULL);

  if (cframe_needs_free) {
    free(cframe);
  }
  blosc2_schunk_free(schunk);
  blosc2_destroy();

  return EXIT_SUCCESS;
}


static char *all_tests(void) {
  mu_run_test(test_reject_metalayer_content_len_overflow);
  mu_run_test(test_reject_vlmetalayer_content_len_overflow);
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
