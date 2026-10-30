/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  Who owns what when a super-chunk operation fails.  A chunk passed to
  blosc2_schunk_update_chunk() without a copy stays with the caller when the
  update fails (as for append and insert), so the caller frees it; the
  library's own callers (blosc2_schunk_set_slice_buffer(), the b2nd slice
  writes) must then free theirs too; and a blosc2_schunk_new() that fails
  frees what it allocated.  The failures are provoked by removing the frame
  file under an open handle (its cached writer cannot be reopened) and by
  creating a frame where one cannot be created.  Run under AddressSanitizer
  (double frees) and LeakSanitizer (leaks) to check the ownership.
*/

#include <stdio.h>
#include "test_common.h"
#include "b2nd.h"

#if !defined(_WIN32)
#include <unistd.h>
#endif

#define CHUNKSIZE (10 * 1000)  /* int32 items */
#define NCHUNKS 3
#define URLPATH "test_chunk_ownership.b2frame"
#define URLPATH_B2ND "test_chunk_ownership.b2nd"
#define DIRPATH "test_chunk_ownership_dir.b2frame"

int tests_run = 0;
static int32_t data[CHUNKSIZE];


static blosc2_schunk* new_schunk_with_chunks(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  blosc2_storage storage = {.contiguous=true, .urlpath=URLPATH, .cparams=&cparams};
  blosc2_remove_urlpath(URLPATH);
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    return NULL;
  }
  for (int n = 0; n < NCHUNKS; n++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = n * CHUNKSIZE + i;
    }
    if (blosc2_schunk_append_buffer(schunk, data, sizeof(data)) != n + 1) {
      blosc2_schunk_free(schunk);
      return NULL;
    }
  }
  return schunk;
}


/* A failed update without a copy leaves the chunk to the caller (it used to be
   freed by the frame on some failure paths, so freeing it here was a double
   free), and leaves the counters alone. */
static char* test_failed_update_keeps_chunk(void) {
  blosc2_schunk* schunk = new_schunk_with_chunks();
  mu_assert("Cannot create the schunk", schunk != NULL);
  int64_t nbytes = schunk->nbytes;
  int64_t cbytes = schunk->cbytes;

  uint8_t* chunk = malloc(sizeof(data) + BLOSC2_MAX_OVERHEAD);
  mu_assert("Cannot allocate", chunk != NULL);
  for (int i = 0; i < CHUNKSIZE; i++) {
    data[i] = 7 * i;  // compresses to another size than the chunk it replaces
  }
  mu_assert("Cannot compress",
            blosc2_compress_ctx(schunk->cctx, data, sizeof(data), chunk, sizeof(data) + BLOSC2_MAX_OVERHEAD) > 0);
  blosc2_remove_urlpath(URLPATH);
  mu_assert("An update of a removed frame file must fail",
            blosc2_schunk_update_chunk(schunk, 1, chunk, false) < 0);
  free(chunk);
  mu_assert("The failed update must not change the counters",
            schunk->nbytes == nbytes && schunk->cbytes == cbytes && schunk->nchunks == NCHUNKS);
  blosc2_schunk_free(schunk);
  return EXIT_SUCCESS;
}


/* blosc2_schunk_set_slice_buffer() compresses its own chunks: a failed update
   must not leak them (nor its scratch buffer). */
static char* test_failed_set_slice_buffer(void) {
  blosc2_schunk* schunk = new_schunk_with_chunks();
  mu_assert("Cannot create the schunk", schunk != NULL);
  for (int i = 0; i < CHUNKSIZE; i++) {
    data[i] = -i;
  }
  blosc2_remove_urlpath(URLPATH);
  /* A whole chunk and a part of one */
  mu_assert("A slice write to a removed frame file must fail",
            blosc2_schunk_set_slice_buffer(schunk, CHUNKSIZE, 2 * CHUNKSIZE, data) < 0);
  mu_assert("A slice write to a removed frame file must fail",
            blosc2_schunk_set_slice_buffer(schunk, 10, 20, data) < 0);
  blosc2_schunk_free(schunk);
  return EXIT_SUCCESS;
}


/* The b2nd slice writes compress their own chunks too */
static char* test_failed_b2nd_set_slice(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  blosc2_storage storage = {.contiguous=true, .urlpath=URLPATH_B2ND, .cparams=&cparams};
  blosc2_remove_urlpath(URLPATH_B2ND);
  int64_t shape[] = {40, 40};
  int32_t chunkshape[] = {20, 20};
  int32_t blockshape[] = {10, 10};
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, 2, shape, chunkshape, blockshape, NULL, 0, NULL, 0);
  mu_assert("Cannot create the context", ctx != NULL);
  b2nd_array_t *array;
  mu_assert("Cannot create the array", b2nd_zeros(ctx, &array) >= 0);
  b2nd_free_ctx(ctx);

  int32_t buffer[40 * 40];
  for (int i = 0; i < 40 * 40; i++) {
    buffer[i] = i;
  }
  int64_t start[] = {0, 0};
  int64_t stop[] = {40, 40};
  int64_t bshape[] = {40, 40};
  mu_assert("Cannot write", b2nd_set_slice_cbuffer(buffer, bshape, sizeof(buffer), start, stop, array) >= 0);

  blosc2_remove_urlpath(URLPATH_B2ND);
  /* Whole chunks (compressed straight from the buffer) and a partial one */
  mu_assert("A slice write to a removed file must fail",
            b2nd_set_slice_cbuffer(buffer, bshape, sizeof(buffer), start, stop, array) < 0);
  int64_t pstop[] = {5, 5};
  int64_t pshape[] = {5, 5};
  mu_assert("A slice write to a removed file must fail",
            b2nd_set_slice_cbuffer(buffer, pshape, 25 * sizeof(int32_t), start, pstop, array) < 0);
  b2nd_free(array);
  return EXIT_SUCCESS;
}


/* A blosc2_schunk_new() that fails must free the schunk it started */
static char* test_failed_schunk_new(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  blosc2_storage storage = {.contiguous=true, .urlpath=URLPATH, .cparams=&cparams};

  /* The file is there already */
  blosc2_remove_urlpath(URLPATH);
  FILE* fp = fopen(URLPATH, "wb");
  mu_assert("Cannot create the file", fp != NULL);
  fclose(fp);
  mu_assert("Overwriting an existing frame must fail", blosc2_schunk_new(&storage) == NULL);
  blosc2_remove_urlpath(URLPATH);

  /* The file cannot be created */
  storage.urlpath = "test_chunk_ownership_no_such_dir/frame.b2frame";
  mu_assert("Creating a frame in a missing directory must fail", blosc2_schunk_new(&storage) == NULL);

#if !defined(_WIN32)
  /* The directory of a sparse frame is there already */
  storage.contiguous = false;
  storage.urlpath = DIRPATH;
  blosc2_remove_urlpath(DIRPATH);
  mu_assert("Cannot create the directory", mkdir(DIRPATH, 0777) == 0);
  mu_assert("Overwriting an existing sparse frame must fail", blosc2_schunk_new(&storage) == NULL);
  blosc2_remove_urlpath(DIRPATH);
#endif
  return EXIT_SUCCESS;
}


static char* all_tests(void) {
#if !defined(_WIN32)
  /* Removing an open file only works this way on POSIX */
  mu_run_test(test_failed_update_keeps_chunk);
  mu_run_test(test_failed_set_slice_buffer);
  mu_run_test(test_failed_b2nd_set_slice);
#endif
  mu_run_test(test_failed_schunk_new);
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
