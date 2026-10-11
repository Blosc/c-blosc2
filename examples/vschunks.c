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

  examples/vlblocks.c showcases similar behavior at a different
  abstraction layer. That example focuses on variable-length 
  blocks within a chunk, and this example focuses on 
  variable-sized chunks within a schunk. They differ as well in
  that chunks do not expose append or replace operations for
  the blocks within, whereas schunks do support append and
  replace operations for the chunks within.
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blosc2.h"

static const char short_string[] = "short";
static const char longer_string[] = "a bit longer string";
static const char update_string[] = "an update to the bit longer string";
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

static int print_and_validate_chunk(blosc2_schunk* schunk, int current_chunk, const char* expected_string) {
  char buffer[64] = {0};
  int32_t dsize = blosc2_schunk_decompress_chunk(schunk, current_chunk, buffer, (int32_t)sizeof(buffer));

  if (dsize != (int32_t)strlen(expected_string) + 1) {
    fprintf(stderr, "unexpected decompression size %d for string: %s.\n", dsize, expected_string);
    return -1;
  }
  if (strcmp(buffer, expected_string) != 0) {
    fprintf(stderr, "unexpected decompressed result: %s. expected result: %s.\n", buffer, expected_string);
    return -2;
  }

  printf("Verified content of chunk %d (%d bytes): %s\n", current_chunk, dsize, buffer);

  return 0;
}

static int print_and_validate_chunks(blosc2_schunk* schunk, size_t nchunks, const int chunk_indexes[], const char* expected_results[]) {
  int rc = 0;
  for (size_t i = 0; i < nchunks; i++) {
    int current_chunk = chunk_indexes[i];
    const char* expected_string = expected_results[i];
    rc = print_and_validate_chunk(schunk, current_chunk, expected_string);
    if (rc < 0) {
      return rc;
    }
  }

  return 0;
}

int main(void) {
  int rc = 1;

  const char *urlpath = "vschunks_example.b2frame";
  blosc2_schunk *schunk = NULL;
  uint8_t *short_chunk = NULL;
  uint8_t *longer_chunk = NULL;
  uint8_t *update_chunk = NULL;
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

  printf("Verifying content of schunk after appends...\n");
  int chunk_indexes[] = {0, 1, 2};
  const char *expected_results[] = {short_string, longer_string, longest_string};
  int nchunks = sizeof(chunk_indexes) / sizeof(chunk_indexes[0]);
  int print_rc = print_and_validate_chunks(schunk, nchunks, chunk_indexes, expected_results);
  if (print_rc < 0) {
    goto cleanup;
  }

  int update_cbytes = compress_string_to_chunk(update_string, &update_chunk, schunk);
  if (update_cbytes <= 0) {
    goto cleanup;
  }

  num_chunks = blosc2_schunk_update_chunk(schunk, 1, update_chunk, true);
  if (num_chunks != 3) {
    fprintf(stderr, "failed to update chunk, got unexpected number of chunks: %d\n", num_chunks);
    goto cleanup;
  }

  printf("Verifying content of schunk after update...\n");
  expected_results[1] = update_string;
  print_rc = print_and_validate_chunks(schunk, nchunks, chunk_indexes, expected_results);
  if (print_rc < 0) {
    goto cleanup;
  }

  rc = 0;

cleanup:
  if (schunk != NULL) {
    blosc2_schunk_free(schunk);
  }

  free(short_chunk);
  free(longer_chunk);
  free(update_chunk);
  free(longest_chunk);

  blosc2_remove_urlpath(urlpath);
  blosc2_destroy();

  return rc;
}
