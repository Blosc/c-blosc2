/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program showing how to store variable-length chunks
  (chunks of different lengths) inside one schunk (super-chunk).

  There are two levels of variable-length storage in C-Blosc2:

  1. Variable-Length Chunks (this example: vlchunks.c)
    Operates inside ONE schunk. After the update in this example:
      schunk
        ├── Chunk 0  (uncompressed:  6 bytes, compressed: 38 bytes)
        ├── Chunk 1  (uncompressed: 35 bytes, compressed: 67 bytes)
        └── Chunk 2  (uncompressed: 47 bytes, compressed: 79 bytes)

      * Each chunk's header contains its independent uncompressed
      and compressed byte counts, and the schunk maintains chunk
      order internally across mutations.
      * Chunks can be appended, updated (shrunk/grown), and
      decompressed individually.

  2. Variable-Length Blocks (examples/vlblocks.c)
    Operates inside ONE Chunk. Example:
      chunk
        ├── Block 0 (payload: 10 bytes)
        ├── Block 1 (payload: 50 bytes)
        └── Block 2 (payload: 20 bytes)

      * Blocks are stored together in one chunk.
      * Blocks can be decompressed individually, but appending or
      updating requires rebuilding and replacing the entire chunk.
*/

#include <inttypes.h>
#include <stddef.h>
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

/* 
  Grabs the uncompressed and compressed sizes for a chunk by
  reading from the chunk's header. The uncompressed size is
  used here to allocate memory before decompressing the chunk,
  whereas compressed size is used for printing.
*/
static int schunk_get_chunk_buffer_size(blosc2_schunk* schunk, int nchunk, int32_t* chunk_nbytes, int32_t* chunk_cbytes) {
  int rc;

  bool needs_free;
  uint8_t* cchunk;
  rc = blosc2_schunk_get_lazychunk(schunk, nchunk, &cchunk, &needs_free);
  if (rc <= 0) {
    return -1;
  }

  rc = blosc2_cbuffer_sizes(cchunk, chunk_nbytes, chunk_cbytes, NULL);
  if (needs_free) {
    free(cchunk);
  }

  return rc;
}

static int print_and_validate_chunk(blosc2_schunk* schunk, int nchunk, const char* expected_string) {
  int32_t chunk_nbytes, chunk_cbytes;
  int rc = schunk_get_chunk_buffer_size(schunk, nchunk, &chunk_nbytes, &chunk_cbytes);
  if (rc < 0) {
    return rc;
  }

  char *buffer = malloc((size_t)chunk_nbytes);
  if (buffer == NULL) {
    fprintf(stderr, "failed to allocate decompression buffer\n");
    return -1;
  }

  int32_t dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, buffer, chunk_nbytes);

  if (dsize != (int32_t)strlen(expected_string) + 1) {
    fprintf(stderr, "unexpected decompression size %d for string: %s.\n", dsize, expected_string);
    free(buffer);
    return -1;
  }
  if (strcmp(buffer, expected_string) != 0) {
    fprintf(stderr, "unexpected decompressed result: %s. expected result: %s.\n", buffer, expected_string);
    free(buffer);
    return -2;
  }

  printf("  chunk %d  (uncompressed: %2d bytes, compressed: %2d bytes, content: \"%s\")\n",
    nchunk, chunk_nbytes, chunk_cbytes, buffer);

  free(buffer);
  return 0;
}

/*
  Alongside chunk-level info, this function prints two 
  similar but different fields of an schunk.

  chunksize: The size of each chunk, or if variable-length
  chunks are used, 0. In this program, we only ever print 0.

  nbytes: The total, uncompressed data size of the chunks.
*/
static int print_and_validate_schunk(blosc2_schunk* schunk, size_t nchunks, const int chunk_indexes[], const char* expected_results[]) {
  printf("  chunksize: %2d (<- indicates variable-length mode)\n", schunk->chunksize);
  printf("  nbytes:    %2" PRIi64 " (<- sum of each chunk's uncompressed bytes)\n", schunk->nbytes);

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

  const char *urlpath = "vlchunks_example.b2frame";
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

  /*
    There is also a blosc2_schunk_append_buffer() function that would allow us to
    directly append the strings as raw buffers. We use blosc2_schunk_append_chunk()
    for this example instead to demonstrate management with variable-length chunks.

    In addition, note that blosc2_schunk_append_chunk() has a copy argument that
    specifies whether the schunk copies the compressed chunk buffer internally or
    takes ownership of the chunk on success. The blosc2_schunk_update_chunk(),
    which is used later, also has a copy argument with analogous behavior.

    Finally, note that schunk maintains a chunksize field which is set to 0 to
    indicate variable-length chunks are used. After the first append, chunksize
    will be set to the first chunk's size. After the second append and onward,
    because the second chunk has a different size, the chunksize will be 0.
  */
  int64_t num_chunks = 0;
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

  printf("After appending 3 chunks to the schunk...\n");
  int chunk_indexes[] = {0, 1, 2};
  const char *expected_results[] = {short_string, longer_string, longest_string};
  size_t nchunks = sizeof(chunk_indexes) / sizeof(chunk_indexes[0]);
  int print_rc = print_and_validate_schunk(schunk, nchunks, chunk_indexes, expected_results);
  if (print_rc < 0) {
    goto cleanup;
  }

  int update_cbytes = compress_string_to_chunk(update_string, &update_chunk, schunk);
  if (update_cbytes <= 0) {
    goto cleanup;
  }

  int nchunk_to_update = 1;
  num_chunks = blosc2_schunk_update_chunk(schunk, nchunk_to_update, update_chunk, true);
  if (num_chunks != 3) {
    fprintf(stderr, "failed to update chunk, got unexpected number of chunks: %d\n", num_chunks);
    goto cleanup;
  }

  printf("\nAfter updating chunk 1 of the schunk, nbytes should update...\n");
  expected_results[nchunk_to_update] = update_string;
  print_rc = print_and_validate_schunk(schunk, nchunks, chunk_indexes, expected_results);
  if (print_rc < 0) {
    goto cleanup;
  }

  blosc2_schunk_free(schunk);

  schunk = blosc2_schunk_open(urlpath);
  if (schunk == NULL) {
    fprintf(stderr, "failed to reopen schunk\n");
    goto cleanup;
  }

  printf("\nAfter closing and reopening the schunk, our values should persist...\n");
  print_rc = print_and_validate_schunk(schunk, nchunks, chunk_indexes, expected_results);
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
