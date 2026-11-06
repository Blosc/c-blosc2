/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  blosc2_schunk_from_buffer(..., copy=true) of a frame with a damaged chunk must
  fail cleanly.  frame_to_schunk() stops copying at the bad chunk, and its error
  path frees schunk->data[i] for every chunk, also the ones it never set.
*/

#include <stdio.h>
#include "test_common.h"

#define NITEMS 1000

int main(void) {
  blosc2_init();
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  blosc2_storage storage = {.contiguous = true, .cparams = &cparams};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  int32_t data[NITEMS];
  for (int n = 0; n < 3; ++n) {
    for (int i = 0; i < NITEMS; ++i) {
      data[i] = n * NITEMS + i * 7;
    }
    if (blosc2_schunk_append_buffer(schunk, data, sizeof(data)) < 0) {
      printf("append failed\n");
      return 1;
    }
  }

  // Find chunk 2 in the frame and make its cbytes field run past the frame
  uint8_t *chunk;
  bool chunk_needs_free;
  int cbytes = blosc2_schunk_get_chunk(schunk, 2, &chunk, &chunk_needs_free);
  uint8_t *cframe;
  bool cframe_needs_free;
  int64_t len = blosc2_schunk_to_buffer(schunk, &cframe, &cframe_needs_free);
  if (cbytes < 0 || len < 0) {
    printf("could not get the chunk or the frame\n");
    return 1;
  }
  uint8_t *bad = malloc(len);
  memcpy(bad, cframe, len);
  uint8_t *pos = NULL;
  for (int64_t i = 0; i + cbytes <= len; ++i) {
    if (memcmp(bad + i, chunk, cbytes) == 0) {
      pos = bad + i;
      break;
    }
  }
  if (pos == NULL) {
    printf("chunk 2 not found in the frame\n");
    return 1;
  }
  int32_t huge = cbytes + 1000000;
  memcpy(pos + BLOSC2_CHUNK_CBYTES, &huge, sizeof(huge));

  blosc2_schunk *copy = blosc2_schunk_from_buffer(bad, len, true);
  int rc = 0;
  if (copy != NULL) {
    printf("a frame with a damaged chunk was accepted\n");
    blosc2_schunk_free(copy);
    rc = 1;
  }

  free(bad);
  if (chunk_needs_free) {
    free(chunk);
  }
  if (cframe_needs_free) {
    free(cframe);
  }
  blosc2_schunk_free(schunk);
  blosc2_destroy();
  if (rc == 0) {
    printf("test_frame_copy_bad_chunk: ok\n");
  }
  return rc;
}
