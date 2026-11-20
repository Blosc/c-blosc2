/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  A frame whose header cbytes is too large to add to header_len must be
  rejected.  The offsets lookup used to quietly fall back to header_len in
  that case, which put the offsets index on top of data chunk 0 and left every
  "chunk ends before cbytes" check comparing against ~INT64_MAX.
*/

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "blosc-private.h"
#include "frame.h"
#include "test_common.h"

#define TYPESIZE (int32_t)sizeof(int64_t)
#define CHUNK_NBYTES (2 * TYPESIZE)
#define CHUNK_CBYTES (BLOSC_EXTENDED_HEADER_LENGTH + CHUNK_NBYTES)
#define BIG_CBYTES (4096)

/* Global vars */
int tests_run = 0;

/* Two memcpyed chunks.  Chunk 0 doubles as a valid offsets chunk {0, 48}
   for a 2-chunk frame, so it is accepted as the index once the frame
   cbytes no longer points past the data. */
static int64_t build_cframe(uint8_t **cframe, bool *needs_free) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = TYPESIZE;
  cparams.clevel = 0;
  blosc2_storage storage = {.contiguous = true, .cparams = &cparams};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    return -1;
  }
  int64_t chunk0[2] = {0, CHUNK_CBYTES};
  int64_t chunk1[2] = {7, 7};
  if (blosc2_schunk_append_buffer(schunk, chunk0, CHUNK_NBYTES) != 1 ||
      blosc2_schunk_append_buffer(schunk, chunk1, CHUNK_NBYTES) != 2) {
    blosc2_schunk_free(schunk);
    return -1;
  }
  uint8_t *buffer = NULL;
  bool buffer_needs_free = false;
  int64_t len = blosc2_schunk_to_buffer(schunk, &buffer, &buffer_needs_free);
  if (len > 0) {
    /* Own a copy, so the frame can be patched and outlive the schunk */
    *cframe = malloc((size_t)len);
    if (*cframe == NULL) {
      len = -1;
    }
    else {
      memcpy(*cframe, buffer, (size_t)len);
      *needs_free = true;
    }
  }
  if (buffer_needs_free) {
    free(buffer);
  }
  blosc2_schunk_free(schunk);
  return len;
}

static char* check_cbytes(int64_t (*make_cbytes)(int32_t header_len)) {
  uint8_t *cframe = NULL;
  bool needs_free = false;
  int64_t len = build_cframe(&cframe, &needs_free);
  mu_assert("Cannot build cframe", len > 0 && cframe != NULL);

  int32_t header_len;
  from_big(&header_len, cframe + FRAME_HEADER_LEN, sizeof(header_len));
  mu_assert("Invalid header_len", header_len >= FRAME_HEADER_MINLEN);

  uint8_t *chunk0 = cframe + header_len;
  uint8_t *chunk1 = chunk0 + CHUNK_CBYTES;
  int32_t cbytes0, cbytes1;
  blosc2_cbuffer_sizes(chunk0, NULL, &cbytes0, NULL);
  blosc2_cbuffer_sizes(chunk1, NULL, &cbytes1, NULL);
  mu_assert("Chunks are not memcpyed", cbytes0 == CHUNK_CBYTES && cbytes1 == CHUNK_CBYTES);

  /* Chunk 1 now claims to run far past the end of the frame */
  int64_t cbytes = make_cbytes(header_len);
  to_big(cframe + FRAME_CBYTES, &cbytes, sizeof(cbytes));
  int32_t big = BIG_CBYTES;
  memcpy(chunk1 + BLOSC2_CHUNK_CBYTES, &big, sizeof(big));

  blosc2_schunk *copy = blosc2_schunk_from_buffer(cframe, len, true);
  if (copy != NULL) {
    blosc2_schunk_free(copy);
  }
  mu_assert("Overflowing frame cbytes must be rejected (copy)", copy == NULL);

  blosc2_schunk *view = blosc2_schunk_from_buffer(cframe, len, false);
  if (view != NULL) {
    blosc2_schunk_free(view);
  }
  mu_assert("Overflowing frame cbytes must be rejected (no copy)", view == NULL);

  if (needs_free) {
    free(cframe);
  }
  return EXIT_SUCCESS;
}

static int64_t cbytes_at_limit(int32_t header_len) {
  return INT64_MAX - header_len;
}

static int64_t cbytes_past_limit(int32_t header_len) {
  (void)header_len;
  return INT64_MAX;
}

static char* test_cbytes_at_limit(void) {
  return check_cbytes(cbytes_at_limit);
}

static char* test_cbytes_past_limit(void) {
  return check_cbytes(cbytes_past_limit);
}


static char *all_tests(void) {
  mu_run_test(test_cbytes_at_limit);
  mu_run_test(test_cbytes_past_limit);
  return EXIT_SUCCESS;
}


int main(void) {
  char *result;

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
