/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blosc2.h"

#define CHUNK_ITEMS 10
#define BLOCK_ITEMS 5
#define NUM_CHUNKS 10
#define NUM_COORDS 4
#define NUM_ITEMS (CHUNK_ITEMS * NUM_CHUNKS)

static const size_t typesize = sizeof(int32_t);
static const int64_t coords[NUM_COORDS] = {
    4, 27, 28, 59
};

int main(void) {
  int rc = 1;

  const char *urlpath = "sparse_coords.b2frame";
  uint8_t *dataset = NULL;
  blosc2_schunk *schunk = NULL;
  uint8_t *chunk = NULL;
  uint8_t *output = NULL;

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

  // blosc2_schunk_get_sparse_buffer() does not take an explicit buffer size argument, so we must
  // allocate enough space for the buffer ourselves. We will request NUM_COORDS values, and each
  // value has size typesize (set to sizeof(int32_t)), so we allocate NUM_COORDS * typesize bytes.
  output = malloc((size_t)NUM_COORDS * typesize);
  if (output == NULL) {
    fprintf(stderr, "failed to malloc output\n");
    goto cleanup;
  }

  // In our example, we have 100 integers spread across 10 chunks, with each chunk containing 10
  // integers each. We could retrieve each value in coords individually with a full-chunk
  // decompression per value. With the sparse-access API, we can instead have Blosc batch the
  // coordinates by chunk internally, minimizing decompression overhead. In our case, Blosc will
  // batch the retrievals for coordinates 27 and 28 together because they share a chunk: 
  // chunks                |---------|---------|---------|---------|---------|---------|
  // indices               0         10        20        30        40        50        60
  // requested                 ^                      ^^                              ^
  //                           4                     27,28                            59
  //
  // full chunk requests:  chunk 0 (4)     chunk 2 (27), chunk 2 (28)         chunk 5 (59)
  // sparse requests:      chunk 0 (4)           chunk 2 (27, 28)             chunk 5 (59)
  
  int retrieve_rc = blosc2_schunk_get_sparse_buffer(schunk, NUM_COORDS, coords, output);
  if (retrieve_rc < 0) {
    fprintf(stderr, "failed to retrieve sparse coordinates\n");
    goto cleanup;
  }

  printf("Comparing sparse retrieval results to expected values...\n");
  for (int i = 0; i < NUM_COORDS; ++i) {
    int32_t output_result, dataset_result;

    size_t output_offset = i * typesize;
    memcpy(&output_result, output + output_offset, typesize);

    size_t dataset_offset = coords[i] * typesize;
    memcpy(&dataset_result, dataset + dataset_offset, typesize);

    printf("  coords[%d] = %2" PRId64 ": sparse retrieval value: %2d, actual dataset value: %2d\n", i, coords[i], output_result, dataset_result);

    int compare_result = memcmp(output + output_offset, dataset + dataset_offset, typesize);
    if (compare_result != 0) {
      fprintf(stderr, "Bad sparse value at index %d: coord=%" PRId64 "\n", i, coords[i]);
      goto cleanup;
    }
  }

  rc = 0;

cleanup:
  free(dataset);
  free(chunk);
  free(output);

  if (schunk != NULL) {
    blosc2_schunk_free(schunk);
  }

  blosc2_remove_urlpath(urlpath);
  blosc2_destroy();

  return rc;
}
