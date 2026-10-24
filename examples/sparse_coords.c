/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "blosc2.h"

#define CHUNK_ITEMS 10
#define BLOCK_ITEMS 5
#define NUM_CHUNKS 10
#define NUM_COORDS 3
#define NUM_ITEMS (CHUNK_ITEMS * NUM_CHUNKS)

static const size_t typesize = sizeof(int32_t);
static const int64_t coords[NUM_COORDS] = {
    4, 28, 99
};

int main(void) {
  int rc = 1;

  const char *urlpath = "sparse_coords.b2frame";
  uint8_t *dataset = NULL;
  blosc2_schunk *schunk = NULL;
  uint8_t *chunk = NULL;

  blosc2_init();
  blosc2_remove_urlpath(urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  cparams.blocksize = BLOCK_ITEMS * typesize;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_storage storage = {
    .contiguous = true,
    .urlpath = (char *)urlpath,
    .cparams = &cparams,
    .dparams = &dparams,
  };

  dataset = malloc((size_t)NUM_ITEMS * (size_t)typesize);
  if (dataset == NULL) {
    fprintf(stderr, "failed to malloc dataset");
    goto cleanup;
  }

  for (int32_t i = 0; i < NUM_ITEMS; ++i) {
    memcpy(dataset + i * typesize, &i, (size_t)typesize);
  }

  chunk = malloc((size_t)CHUNK_ITEMS * typesize);
  if (chunk == NULL) {
    fprintf(stderr, "failed to malloc chunk\n");
    goto cleanup;
  }

  schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    fprintf(stderr, "failed to create schunk\n");
    goto cleanup;
  }

  for (int start = 0; start < NUM_ITEMS; start += CHUNK_ITEMS) {
    int64_t stop = start + CHUNK_ITEMS;
    if (stop > NUM_ITEMS) {
      stop = NUM_ITEMS;
    }

    int64_t curr_chunk_items = stop - start;
    uint8_t *dataset_offset = dataset + (start * typesize);
    size_t chunk_size = (size_t)curr_chunk_items * typesize;
    memcpy(chunk, dataset_offset, chunk_size);
    
    int64_t nchunks = blosc2_schunk_append_buffer(schunk, chunk, (int32_t) chunk_size);
    if (nchunks < 0) {
      fprintf(stderr, "failed to append dataset at index: %d\n", start);
      goto cleanup;
    }
  }

  // blosc2_schunk_get_sparse_buffer() does not take a specific buffer size argument, so we must
  // allocate enough space for the buffer ourselves. We will request NUM_COORDS values, and each
  // value has size typesize (set to sizeof(int32_t)), so we allocate NUM_COORDS * typesize bytes.
  uint8_t *ouput_buffer = malloc((size_t)NUM_COORDS * typesize);
  
  rc = blosc2_schunk_get_sparse_buffer(schunk, NUM_COORDS, coords, ouput_buffer);
  if (rc < 0) {
    fprintf(stderr, "failed to retrieve sparse coordinates\n");
    goto cleanup;
  }

  rc = 0;

cleanup:
  free(dataset);
  free(chunk);

  if (schunk != NULL) {
    blosc2_schunk_free(schunk);
  }

  blosc2_remove_urlpath(urlpath);
  blosc2_destroy();

  return rc;
}
