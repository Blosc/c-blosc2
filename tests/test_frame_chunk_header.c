/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  frame_get_chunk_header() hands out the same header as frame_get_lazychunk()
  (but for the lazy-chunk flag), for in-memory, file and directory frames, for
  stored chunks, chunks coded in their offset and a shorter last chunk.
*/

#include <stdio.h>
#include "test_common.h"
#include "frame.h"

#define CHUNKSIZE (10 * 1000)  /* int32 items */
#define URLPATH "test_frame_chunk_header.b2frame"

int tests_run = 0;
static int32_t data[CHUNKSIZE];


static char* check_headers(blosc2_storage* storage) {
  blosc2_remove_urlpath(storage->urlpath);
  blosc2_schunk* schunk = blosc2_schunk_new(storage);
  mu_assert("Cannot create the super-chunk", schunk != NULL);
  for (int i = 0; i < CHUNKSIZE; i++) {
    data[i] = i * 3;
  }
  mu_assert("Cannot append", blosc2_schunk_append_buffer(schunk, data, sizeof(data)) == 1);
  // Chunks of zeros are coded in their offset; the last one is shorter
  uint8_t zeros[BLOSC_EXTENDED_HEADER_LENGTH];
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  mu_assert("Cannot make a zero chunk",
            blosc2_chunk_zeros(cparams, sizeof(data), zeros, sizeof(zeros)) == sizeof(zeros));
  mu_assert("Cannot append", blosc2_schunk_append_chunk(schunk, zeros, true) == 2);
  mu_assert("Cannot append", blosc2_schunk_append_buffer(schunk, data, sizeof(data)) == 3);
  mu_assert("Cannot make a zero chunk",
            blosc2_chunk_zeros(cparams, sizeof(data) / 2, zeros, sizeof(zeros)) == sizeof(zeros));
  mu_assert("Cannot append", blosc2_schunk_append_chunk(schunk, zeros, true) == 4);

  blosc2_frame_s* frame = (blosc2_frame_s*) schunk->frame;
  for (int64_t nchunk = 0; nchunk < schunk->nchunks; nchunk++) {
    uint8_t header[BLOSC_EXTENDED_HEADER_LENGTH];
    int cbytes = frame_get_chunk_header(frame, nchunk, header);
    uint8_t* lazy;
    bool needs_free;
    int rc = frame_get_lazychunk(frame, nchunk, &lazy, &needs_free);
    mu_assert("Cannot get the header", cbytes >= BLOSC_EXTENDED_HEADER_LENGTH && rc >= 0);
    int32_t lazy_cbytes;
    blosc2_cbuffer_sizes(lazy, NULL, &lazy_cbytes, NULL);
    lazy[BLOSC2_CHUNK_BLOSC2_FLAGS] &= (uint8_t) ~0x08U;
    bool same = memcmp(header, lazy, sizeof(header)) == 0;
    if (needs_free) {
      free(lazy);
    }
    mu_assert("Not the header of the lazy chunk", same && cbytes == lazy_cbytes);
  }
  mu_assert("Out of range", frame_get_chunk_header(frame, schunk->nchunks, (uint8_t[32]){0}) < 0);
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(storage->urlpath);
  return EXIT_SUCCESS;
}


static char* test_headers(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  blosc2_storage storage = {.contiguous=true, .cparams=&cparams};
  char* msg = check_headers(&storage);
  if (msg != EXIT_SUCCESS) {
    return msg;
  }
  storage.urlpath = URLPATH;
  msg = check_headers(&storage);
  if (msg != EXIT_SUCCESS) {
    return msg;
  }
  storage.contiguous = false;
  return check_headers(&storage);
}


static char* all_tests(void) {
  mu_run_test(test_headers);
  return EXIT_SUCCESS;
}


int main(void) {
  blosc2_init();
  char* result = all_tests();
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
