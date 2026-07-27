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

/* Write a 32-bit int to the chunk header in little-endian order.
 * Chunk headers are always little-endian, regardless of host endianness. */
static void put_i32_le(uint8_t* dst, int32_t value) {
  uint32_t uvalue = (uint32_t)value;
  dst[0] = (uint8_t)(uvalue & 0xffu);
  dst[1] = (uint8_t)((uvalue >> 8) & 0xffu);
  dst[2] = (uint8_t)((uvalue >> 16) & 0xffu);
  dst[3] = (uint8_t)((uvalue >> 24) & 0xffu);
}

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
  put_i32_le(chunk + BLOSC2_CHUNK_NBYTES, nbytes);
  put_i32_le(chunk + BLOSC2_CHUNK_BLOCKSIZE, blocksize);
  put_i32_le(chunk + BLOSC2_CHUNK_CBYTES, cbytes);
  return chunk;
}

/* Build a Blosc1-style chunk (again no extended header) carrying enough payload
   that cbytes reaches BLOSC_EXTENDED_HEADER_LENGTH or more, with the payload
   byte that lands on header offset 0x1e set to BLOSC2_VL_BLOCKS.  Nothing here
   is out of bounds: the point is that offset 0x1e holds data, not flags2, so
   sizing the decision on cbytes alone would read a data byte as flags. */
#define DECOY_PAYLOAD 24

static uint8_t *make_vlblocks_decoy_chunk(void) {
  int32_t cbytes = BLOSC_MIN_HEADER_LENGTH + DECOY_PAYLOAD;
  uint8_t *chunk = malloc((size_t)cbytes);
  if (chunk == NULL) {
    return NULL;
  }
  memset(chunk, 0, (size_t)cbytes);
  chunk[0] = BLOSC2_VERSION_FORMAT;
  chunk[1] = 1;
  chunk[2] = BLOSC_MEMCPYED;
  chunk[3] = 1;
  put_i32_le(chunk + BLOSC2_CHUNK_NBYTES, DECOY_PAYLOAD);
  put_i32_le(chunk + BLOSC2_CHUNK_BLOCKSIZE, DECOY_PAYLOAD);
  put_i32_le(chunk + BLOSC2_CHUNK_CBYTES, cbytes);
  chunk[BLOSC2_CHUNK_BLOSC2_FLAGS2] = BLOSC2_VL_BLOCKS;
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


/* A chunk without an extended header must never contribute flags2, however long
   it is.  Unlike the min-header case above this is fully defined behaviour, so
   it fails deterministically rather than only under a sanitizer. */
static char* test_no_extended_header_flags2(void) {
  blosc2_init();

  blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("cannot create schunk", schunk != NULL);

  uint8_t *chunk = make_vlblocks_decoy_chunk();
  mu_assert("cannot allocate chunk", chunk != NULL);
  int32_t cbytes;
  int rc = blosc2_cbuffer_sizes(chunk, NULL, &cbytes, NULL);
  mu_assert("decoy chunk rejected by blosc2_cbuffer_sizes", rc >= 0);
  mu_assert("decoy chunk must be long enough to reach offset 0x1e",
            cbytes >= BLOSC_EXTENDED_HEADER_LENGTH);

  int64_t nchunks = blosc2_schunk_append_chunk(schunk, chunk, true);
  free(chunk);
  mu_assert("cannot append decoy chunk", nchunks == 1);
  mu_assert("payload byte at offset 0x1e was taken for flags2",
            (schunk->flags2 & BLOSC2_VL_BLOCKS) == 0);

  /* And it must not be mistaken for a mismatch against the stored chunk 0 either */
  chunk = make_vlblocks_decoy_chunk();
  mu_assert("cannot allocate chunk", chunk != NULL);
  nchunks = blosc2_schunk_append_chunk(schunk, chunk, true);
  free(chunk);
  mu_assert("cannot append second decoy chunk", nchunks == 2);

  blosc2_schunk_free(schunk);
  blosc2_destroy();

  return EXIT_SUCCESS;
}


static char *all_tests(void) {
  mu_run_test(test_min_header_chunk);
  mu_run_test(test_no_extended_header_flags2);
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
