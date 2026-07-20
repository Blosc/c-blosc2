/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Regression test for an out-of-bounds read of the flags2 byte in
  schunk.c. The Blosc2 flags2 byte sits at offset 0x1e, inside the
  extended header, but a Blosc1-style chunk is only
  BLOSC_MIN_HEADER_LENGTH (16) bytes long. Appending, inserting or
  updating with such a chunk read 15 bytes past the end of the buffer.
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_common.h"

int tests_run = 0;

/* Build a minimal but valid Blosc1-style chunk of exactly
   BLOSC_MIN_HEADER_LENGTH bytes. read_chunk_header() accepts
   cbytes == BLOSC_MIN_HEADER_LENGTH, so this is a well-formed chunk. */
static uint8_t *make_min_header_chunk(void) {
  uint8_t *chunk = malloc(BLOSC_MIN_HEADER_LENGTH);
  if (chunk == NULL) {
    return NULL;
  }
  memset(chunk, 0, BLOSC_MIN_HEADER_LENGTH);
  chunk[0] = BLOSC2_VERSION_FORMAT;
  chunk[1] = 1;
  /* Not shuffle+bitshuffle, so no extended header is expected. */
  chunk[2] = BLOSC_MEMCPYED;
  chunk[3] = 1;
  int32_t nbytes = 0;
  int32_t blocksize = 1;
  int32_t cbytes = BLOSC_MIN_HEADER_LENGTH;
  memcpy(chunk + 4, &nbytes, sizeof(nbytes));
  memcpy(chunk + 8, &blocksize, sizeof(blocksize));
  memcpy(chunk + 12, &cbytes, sizeof(cbytes));
  return chunk;
}


static char* test_min_header_chunk(void) {
  blosc2_init();

  /* The chunk is well-formed as far as the header parser is concerned. */
  uint8_t *probe = make_min_header_chunk();
  mu_assert("cannot allocate chunk", probe != NULL);
  int32_t nbytes, cbytes;
  int rc = blosc2_cbuffer_sizes(probe, &nbytes, &cbytes, NULL);
  mu_assert("min-header chunk rejected by blosc2_cbuffer_sizes", rc >= 0);
  mu_assert("unexpected cbytes", cbytes == BLOSC_MIN_HEADER_LENGTH);
  free(probe);

  blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("cannot create schunk", schunk != NULL);

  /* Reads flags2 out of the caller-supplied buffer. */
  uint8_t *chunk = make_min_header_chunk();
  mu_assert("cannot allocate chunk", chunk != NULL);
  int64_t nchunks = blosc2_schunk_append_chunk(schunk, chunk, true);
  free(chunk);
  mu_assert("cannot append min-header chunk", nchunks == 1);

  /* Reads flags2 back out of the stored chunk 0 to compare against. */
  chunk = make_min_header_chunk();
  mu_assert("cannot allocate chunk", chunk != NULL);
  nchunks = blosc2_schunk_append_chunk(schunk, chunk, true);
  free(chunk);
  mu_assert("cannot append second min-header chunk", nchunks == 2);

  chunk = make_min_header_chunk();
  mu_assert("cannot allocate chunk", chunk != NULL);
  nchunks = blosc2_schunk_update_chunk(schunk, 0, chunk, true);
  free(chunk);
  mu_assert("cannot update with min-header chunk", nchunks >= 0);

  chunk = make_min_header_chunk();
  mu_assert("cannot allocate chunk", chunk != NULL);
  nchunks = blosc2_schunk_insert_chunk(schunk, 1, chunk, true);
  free(chunk);
  mu_assert("cannot insert min-header chunk", nchunks == 3);

  blosc2_schunk_free(schunk);
  blosc2_destroy();

  return EXIT_SUCCESS;
}


static char *all_tests(void) {
  mu_run_test(test_min_header_chunk);
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
