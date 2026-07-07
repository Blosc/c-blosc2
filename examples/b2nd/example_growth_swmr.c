/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating growth-SWMR: a reader handle on a disk-based
  b2nd array follows shape changes (resize/append) made through another
  handle — typically another process — without reopening the array.

  This is the classic HDF5-SWMR (single writer, multiple readers) use case:
  one writer grows an array while readers keep up with the new extents.  The
  reader's cached shape, strides and metalayers are refreshed automatically
  the next time it touches the array, so `array->shape` reads through to the
  current on-disk state.

  Things to know:

  - The contract is single writer: many readers are fine, two writers need
    external coordination.
  - Without locking, staleness is detected from the on-disk frame length, so
    growth (which appends chunks) is virtually always noticed.  A resize that
    leaves the frame length unchanged (e.g. a shrink within the last chunk)
    may go undetected.
  - With `locking` enabled on every handle (see examples/file-locking.c), a
    generation counter detects every mutation exactly, and the whole resize
    sequence becomes atomic for the readers.
  - Only shape changes are followed: if another handle rewrites ndim,
    chunkshape or blockshape, readers get BLOSC2_ERROR_DATA instead.
  - Consistency is per-operation (no whole-array snapshot isolation), the
    same weak ordering HDF5 SWMR offers.

  To compile this program:

  $ gcc example_growth_swmr.c -o growth_swmr -lblosc2

  To run:

  $ ./growth_swmr
  Reader sees shape (10, 10)
  Writer resized the array to (20, 10) and filled the new rows
  Reader now sees shape (20, 10) without reopening
  Reader read row 15: 150 151 152 153 154 155 156 157 158 159
  Success!

*/

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <blosc2.h>
#include <b2nd.h>

#define NROWS 10
#define NCOLS 10
#define URLPATH "growth_swmr.b2nd"


int main(void) {
  blosc2_init();
  int exit_code = EXIT_FAILURE;
  b2nd_array_t *writer = NULL;
  b2nd_array_t *reader = NULL;

  // Create a (NROWS, NCOLS) array of int32 zeros on disk
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  blosc2_storage storage = {.contiguous=true, .urlpath=URLPATH, .cparams=&cparams};
  blosc2_remove_urlpath(URLPATH);
  int64_t shape[] = {NROWS, NCOLS};
  int32_t chunkshape[] = {5, 5};
  int32_t blockshape[] = {5, 5};
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, 2, shape, chunkshape, blockshape,
                                        NULL, 0, NULL, 0);
  if (ctx == NULL) {
    fprintf(stderr, "Cannot create the b2nd context\n");
    goto cleanup;
  }
  b2nd_array_t *creator;
  if (b2nd_zeros(ctx, &creator) < 0) {
    fprintf(stderr, "Cannot create the array\n");
    b2nd_free_ctx(ctx);
    goto cleanup;
  }
  b2nd_free(creator);
  b2nd_free_ctx(ctx);

  // Open two independent handles on it, as a writer and a reader process would
  if (b2nd_open(URLPATH, &writer) < 0 || b2nd_open(URLPATH, &reader) < 0) {
    fprintf(stderr, "Cannot open the array twice\n");
    goto cleanup;
  }
  printf("Reader sees shape (%" PRId64 ", %" PRId64 ")\n",
         reader->shape[0], reader->shape[1]);

  // The writer grows the array and fills the new rows with row*NCOLS + col
  int64_t new_shape[] = {2 * NROWS, NCOLS};
  if (b2nd_resize(writer, new_shape, NULL) < 0) {
    fprintf(stderr, "Cannot resize the array\n");
    goto cleanup;
  }
  int32_t buffer[NROWS * NCOLS];
  for (int i = 0; i < NROWS * NCOLS; i++) {
    buffer[i] = NROWS * NCOLS + i;
  }
  int64_t start[] = {NROWS, 0};
  int64_t stop[] = {2 * NROWS, NCOLS};
  int64_t buffershape[] = {NROWS, NCOLS};
  if (b2nd_set_slice_cbuffer(buffer, buffershape, sizeof(buffer),
                             start, stop, writer) < 0) {
    fprintf(stderr, "Cannot write to the grown region\n");
    goto cleanup;
  }
  printf("Writer resized the array to (%" PRId64 ", %" PRId64 ") "
         "and filled the new rows\n", writer->shape[0], writer->shape[1]);

  // The reader follows: reading the grown region just works, no reopen needed
  int32_t row[NCOLS];
  int64_t rstart[] = {15, 0};
  int64_t rstop[] = {16, NCOLS};
  int64_t rshape[] = {1, NCOLS};
  if (b2nd_get_slice_cbuffer(reader, rstart, rstop, row, rshape, sizeof(row)) < 0) {
    fprintf(stderr, "The reader could not read the grown region\n");
    goto cleanup;
  }
  printf("Reader now sees shape (%" PRId64 ", %" PRId64 ") without reopening\n",
         reader->shape[0], reader->shape[1]);
  printf("Reader read row 15:");
  for (int i = 0; i < NCOLS; i++) {
    if (row[i] != 15 * NCOLS + i) {
      fprintf(stderr, "\nUnexpected value at column %d: %d\n", i, row[i]);
      goto cleanup;
    }
    printf(" %d", row[i]);
  }
  printf("\nSuccess!\n");
  exit_code = EXIT_SUCCESS;

  cleanup:
  if (writer != NULL) {
    b2nd_free(writer);
  }
  if (reader != NULL) {
    b2nd_free(reader);
  }
  blosc2_remove_urlpath(URLPATH);
  blosc2_destroy();
  return exit_code;
}
