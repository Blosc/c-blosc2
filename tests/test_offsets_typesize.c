/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  The chunk offsets of a frame are compressed with their own typesize, whatever
  the BLOSC_TYPESIZE environment variable says, by every operation: a frame
  whose last chunk was inserted, or whose chunk was updated in place, is the
  frame that appending the same chunks gives.
*/

#include <stdio.h>
#include "test_common.h"

#define CHUNKSIZE (10 * 1000)  /* int32 items */
#define NCHUNKS 40

int tests_run = 0;
static int32_t data[CHUNKSIZE];


/* A frame with NCHUNKS chunks, the last one inserted at the end if `insert` */
static blosc2_schunk* new_frame(bool insert) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = 1;
  blosc2_storage storage = {.contiguous=true, .cparams=&cparams};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    return NULL;
  }
  for (int n = 0; n < NCHUNKS; n++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = n * i;
    }
    int64_t rc;
    if (insert && n == NCHUNKS - 1) {
      uint8_t chunk[sizeof(data) + BLOSC2_MAX_OVERHEAD];
      int cbytes = blosc2_compress_ctx(schunk->cctx, data, sizeof(data), chunk, sizeof(chunk));
      rc = cbytes < 0 ? cbytes : blosc2_schunk_insert_chunk(schunk, n, chunk, true);
    }
    else {
      rc = blosc2_schunk_append_buffer(schunk, data, sizeof(data));
    }
    if (rc != n + 1) {
      blosc2_schunk_free(schunk);
      return NULL;
    }
  }
  return schunk;
}


static bool same_frames(blosc2_schunk* a, blosc2_schunk* b) {
  uint8_t* fa;
  uint8_t* fb;
  bool free_a;
  bool free_b;
  int64_t la = blosc2_schunk_to_buffer(a, &fa, &free_a);
  int64_t lb = blosc2_schunk_to_buffer(b, &fb, &free_b);
  bool same = la > 0 && la == lb && memcmp(fa, fb, (size_t) la) == 0;
  if (free_a) {
    free(fa);
  }
  if (free_b) {
    free(fb);
  }
  return same;
}


static char* test_offsets_typesize(void) {
  blosc2_schunk* appended = new_frame(false);
  blosc2_schunk* inserted = new_frame(true);
  mu_assert("Cannot create the frames", appended != NULL && inserted != NULL);
  mu_assert("Inserting the last chunk gave another frame", same_frames(appended, inserted));

  // Update a chunk with itself
  uint8_t* chunk;
  bool needs_free;
  int cbytes = blosc2_schunk_get_chunk(appended, 1, &chunk, &needs_free);
  mu_assert("Cannot get a chunk", cbytes > 0);
  uint8_t* copy = malloc((size_t) cbytes);
  memcpy(copy, chunk, (size_t) cbytes);
  if (needs_free) {
    free(chunk);
  }
  mu_assert("Cannot update", blosc2_schunk_update_chunk(inserted, 1, copy, true) == NCHUNKS);
  free(copy);
  mu_assert("Updating a chunk with itself gave another frame", same_frames(appended, inserted));

  blosc2_schunk_free(appended);
  blosc2_schunk_free(inserted);
  return EXIT_SUCCESS;
}


static char* all_tests(void) {
  mu_run_test(test_offsets_typesize);
  return EXIT_SUCCESS;
}


int main(void) {
#if defined(_WIN32)
  _putenv_s("BLOSC_TYPESIZE", "4");
#else
  setenv("BLOSC_TYPESIZE", "4", 1);
#endif
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
