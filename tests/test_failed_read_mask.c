/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  A slice read that decompresses only some blocks of a chunk sets a block mask
  on the decompression context of the array.  When reading the chunk fails
  before it is decompressed (here: the file lost its end), the mask must not
  stay on the context, or the next decompression with it skips the blocks the
  failed read did not want.
*/

#include <stdio.h>
#include "test_common.h"
#include "b2nd.h"
#include "frame.h"

#if !defined(_WIN32)
#include <unistd.h>
#endif

#define URLPATH "test_failed_read_mask.b2nd"
#define NROWS 64
#define NCOLS 64

int tests_run = 0;
static int32_t data[NROWS * NCOLS];
static int32_t result[NROWS * NCOLS];


#if !defined(_WIN32)
static char* test_failed_partial_read(void) {
  for (int i = 0; i < NROWS * NCOLS; i++) {
    data[i] = i * 7 + (i % 13);
  }
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = 1;
  // DELTA keeps the partial read on the masked decompression into the chunk scratch
  cparams.filters[BLOSC2_MAX_FILTERS - 2] = BLOSC_DELTA;
  blosc2_storage storage = {.contiguous=true, .urlpath=URLPATH, .cparams=&cparams};
  int64_t shape[] = {NROWS, NCOLS};
  int32_t chunkshape[] = {NROWS / 2, NCOLS};
  int32_t blockshape[] = {NROWS / 8, NCOLS};
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, 2, shape, chunkshape, blockshape, NULL, 0, NULL, 0);
  mu_assert("Cannot create the context", ctx != NULL);
  blosc2_remove_urlpath(URLPATH);
  b2nd_array_t *array;
  mu_assert("Cannot create the array",
            b2nd_from_cbuffer(ctx, &array, data, sizeof(data)) == BLOSC2_ERROR_SUCCESS);
  b2nd_free_ctx(ctx);
  b2nd_free(array);

  mu_assert("Cannot open the array", b2nd_open(URLPATH, &array) == BLOSC2_ERROR_SUCCESS);
  blosc2_schunk *sc = array->sc;
  int32_t chunk_nbytes = (int32_t) sizeof(data) / 2;
  // Read the first chunk once, which also caches the chunk offsets
  mu_assert("Cannot read the first chunk",
            blosc2_schunk_decompress_chunk(sc, 0, result, chunk_nbytes) == chunk_nbytes);

  // Cut the file in the middle of the second (last) chunk
  uint8_t header[FRAME_HEADER_MINLEN];
  FILE *f = fopen(URLPATH, "rb");
  mu_assert("Cannot read the frame header",
            f != NULL && fread(header, 1, sizeof(header), f) == sizeof(header));
  fclose(f);
  int32_t header_len = (header[FRAME_HEADER_LEN] << 24) | (header[FRAME_HEADER_LEN + 1] << 16) |
                       (header[FRAME_HEADER_LEN + 2] << 8) | header[FRAME_HEADER_LEN + 3];
  int64_t *offsets = blosc2_frame_get_offsets(sc);
  mu_assert("Cannot get the chunk offsets", offsets != NULL);
  off_t cut = (off_t) (header_len + offsets[1] + BLOSC_EXTENDED_HEADER_LENGTH + 8);
  free(offsets);
  mu_assert("Cannot truncate the file", truncate(URLPATH, cut) == 0);

  // One row of the second chunk: one of its blocks
  int64_t start[] = {NROWS / 2 + 1, 0};
  int64_t stop[] = {NROWS / 2 + 2, NCOLS};
  int64_t buffershape[] = {1, NCOLS};
  int32_t row[NCOLS];
  mu_assert("Reading the cut chunk must fail",
            b2nd_get_slice_cbuffer(array, start, stop, row, buffershape, sizeof(row)) < 0);

  // The first chunk is all still there, and must decompress whole
  memset(result, 0, sizeof(result));
  mu_assert("Cannot read the first chunk again",
            blosc2_schunk_decompress_chunk(sc, 0, result, chunk_nbytes) == chunk_nbytes);
  mu_assert("The failed read left its block mask behind", memcmp(result, data, chunk_nbytes) == 0);

  b2nd_free(array);
  blosc2_remove_urlpath(URLPATH);
  return EXIT_SUCCESS;
}
#endif


static char* all_tests(void) {
#if !defined(_WIN32)
  mu_run_test(test_failed_partial_read);
#endif
  return EXIT_SUCCESS;
}


int main(void) {
  blosc2_init();
  char* result_ = all_tests();
  if (result_ != EXIT_SUCCESS) {
    printf(" (%s)\n", result_);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);
  blosc2_destroy();
  return result_ != EXIT_SUCCESS;
}
