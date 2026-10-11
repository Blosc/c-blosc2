/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating how to store chunks of different
  lengths inside a single variable-sized-chunk schunk.

  Appends, updates, and indexed retrieval of variable-sized
  chunks within a schunk are possible because each chunk header
  contains the chunk's uncompressed byte count, and the schunk 
  maintains chunk order internally across mutation. Unlike a
  flat fixed-width layout, such as typical C arrays, the schunk
  does not need every chunk to expand to the same byte size.
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "blosc2.h"

static const char short_string[] = "short";
static const char longer_string[] = "a bit longer string";
static const char longest_string[] = "this is the longest string stored in the chunk";

static int compress_string_to_chunk(const char* string, uint8_t** chunk, blosc2_schunk* schunk) {
  int32_t nbytes = (int32_t)strlen(string) + 1;
  *chunk = malloc((size_t)nbytes + BLOSC2_MAX_OVERHEAD);
  if (*chunk == NULL) {
    fprintf(stderr, "failed to allocate chunk for string: %s\n", string);
    return -1;
  }

  int cbytes = blosc2_compress_ctx(schunk->cctx, string, nbytes, *chunk, nbytes + BLOSC2_MAX_OVERHEAD);
  if (cbytes <= 0) {
    fprintf(stderr, "failed chunk compression for string: %s\n", string);
    return -1;
  }

  return cbytes;
}

static int decompress_and_validate_chunk(blosc2_schunk* schunk, int current_chunk, const char* expected_string) {
  char buffer[64] = {0};
  int32_t dsize = blosc2_schunk_decompress_chunk(schunk, current_chunk, buffer, (int32_t)sizeof(buffer));

  if (dsize != (int32_t)strlen(expected_string) + 1) {
    fprintf(stderr, "unexpected decompression size %d for string %s.\n", dsize, expected_string);
    return -1;
  }
  if (strcmp(buffer, expected_string) != 0) {
    fprintf(stderr, "unexpected decompressed string %s for string %s.\n", buffer, expected_string);
    return -2;
  }

  return 0;
}

int main(void) {
  int rc = 1;

  const char *urlpath = "vschunks_example.b2frame";
  blosc2_schunk *schunk = NULL;
  uint8_t *short_chunk = NULL;
  uint8_t *longer_chunk = NULL;
  uint8_t *longest_chunk = NULL;

  blosc2_init();
  blosc2_remove_urlpath(urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_storage storage = {
    .contiguous = true,
    .urlpath = (char *)urlpath,
    .cparams = &cparams,
    .dparams = &dparams,
  };
  
  schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    fprintf(stderr, "failed to create schunk\n");
    goto cleanup;
  }

  int short_cbytes = compress_string_to_chunk(short_string, &short_chunk, schunk);
  if (short_cbytes <= 0) {
    goto cleanup;
  }
  int longer_cbytes = compress_string_to_chunk(longer_string, &longer_chunk, schunk);
  if (longer_cbytes <= 0) {
    goto cleanup;
  }
  int longest_cbytes = compress_string_to_chunk(longest_string, &longest_chunk, schunk);
  if (longest_cbytes <= 0) {
    goto cleanup;
  }

  int num_chunks = 0;
  num_chunks = blosc2_schunk_append_chunk(schunk, short_chunk, true);
  if (num_chunks != 1) {
    fprintf(stderr, "failed to append chunk for short string\n");
    goto cleanup;
  } 
  num_chunks = blosc2_schunk_append_chunk(schunk, longer_chunk, true);
  if (num_chunks != 2) {
    fprintf(stderr, "failed to append chunk for longer string\n");
    goto cleanup;
  }
  num_chunks = blosc2_schunk_append_chunk(schunk, longest_chunk, true);
  if (num_chunks != 3) {
    fprintf(stderr, "failed to append chunk for longest string\n");
    goto cleanup;
  }

  printf("checkpoint. TODO make this better\n");

  int d_retval = 0;
  d_retval = decompress_and_validate_chunk(schunk, 0, short_string);
  if (d_retval < 0) {
    goto cleanup;
  }
  d_retval = decompress_and_validate_chunk(schunk, 1, longer_string);
  if (d_retval < 0) {
    goto cleanup;
  }
  d_retval = decompress_and_validate_chunk(schunk, 2, longest_string);
  if (d_retval < 0) {
    goto cleanup;
  }

  rc = 0;

cleanup:
  if (schunk != NULL) {
    blosc2_schunk_free(schunk);
  }

  free(short_chunk);
  free(longer_chunk);
  free(longest_chunk);

  blosc2_remove_urlpath(urlpath);
  blosc2_destroy();

  return rc;
}
