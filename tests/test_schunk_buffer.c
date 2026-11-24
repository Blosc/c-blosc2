/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  Tests for blosc2_schunk_append_buffer(), blosc2_schunk_insert_buffer() and
  blosc2_schunk_update_buffer() on every storage.  A prefilter stamps each
  item with the number of the chunk being compressed, which checks that the
  prefilter sees the right chunk.
*/

#include <stdio.h>
#include "test_common.h"

#define CHUNKITEMS (50 * 1000)
#define NCHUNKS 5
#define NTHREADS 2
#define STAMP (1 << 20)

int tests_run = 0;

typedef struct {
  bool contiguous;
  char *urlpath;
} test_storage;

test_storage tstorage[] = {
    {false, NULL},  // memory - schunk
    {true, NULL},  // memory - cframe
    {true, "test_schunk_buffer.b2frame"},  // disk - cframe
    {false, "test_schunk_buffer_s.b2frame"},  // disk - sframe
};

test_storage tdata;

static int32_t *data;
static int32_t *data_dest;

static int stamp_prefilter(blosc2_prefilter_params *params) {
  int32_t nitems = params->output_size / params->output_typesize;
  const int32_t *in = (const int32_t *) params->input;
  int32_t *out = (int32_t *) params->output;
  for (int32_t i = 0; i < nitems; i++) {
    out[i] = in[i] + (int32_t) params->nchunk * STAMP;
  }
  return 0;
}

static void fill(int32_t value) {
  for (int32_t i = 0; i < CHUNKITEMS; i++) {
    data[i] = value + i % 1000;
  }
}

/* Whether chunk @p nchunk holds the items of fill(value) as stamped for chunk @p stamp */
static bool chunk_holds(blosc2_schunk *schunk, int64_t nchunk, int32_t value, int64_t stamp) {
  int dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, data_dest, CHUNKITEMS * sizeof(int32_t));
  if (dsize != CHUNKITEMS * (int) sizeof(int32_t)) {
    return false;
  }
  for (int32_t i = 0; i < CHUNKITEMS; i++) {
    if (data_dest[i] != value + i % 1000 + (int32_t) stamp * STAMP) {
      return false;
    }
  }
  return true;
}

static char *test_schunk_buffer(void) {
  blosc2_remove_urlpath(tdata.urlpath);
  int32_t isize = CHUNKITEMS * sizeof(int32_t);
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_LZ4;
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS;
  cparams.prefilter = stamp_prefilter;
  blosc2_prefilter_params preparams = {0};
  cparams.preparams = &preparams;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams = &cparams, .dparams = &dparams,
                            .urlpath = tdata.urlpath, .contiguous = tdata.contiguous};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: cannot create the super-chunk", schunk != NULL);

  // Chunk i holds fill(i * 1000), stamped i
  for (int32_t i = 0; i < NCHUNKS; i++) {
    fill(i * 1000);
    mu_assert("ERROR: bad append", blosc2_schunk_append_buffer(schunk, data, isize) == i + 1);
  }

  // Insert in the middle: the chunks after it move up and keep their stamps
  fill(-7000);
  mu_assert("ERROR: bad insert", blosc2_schunk_insert_buffer(schunk, 2, data, isize) == NCHUNKS + 1);
  mu_assert("ERROR: bad inserted chunk", chunk_holds(schunk, 2, -7000, 2));
  mu_assert("ERROR: chunk before the insert changed", chunk_holds(schunk, 1, 1000, 1));
  mu_assert("ERROR: chunk after the insert changed", chunk_holds(schunk, 3, 2000, 2));
  // ...and at the end
  fill(-8000);
  mu_assert("ERROR: bad insert at the end",
            blosc2_schunk_insert_buffer(schunk, NCHUNKS + 1, data, isize) == NCHUNKS + 2);
  mu_assert("ERROR: bad chunk inserted at the end", chunk_holds(schunk, NCHUNKS + 1, -8000, NCHUNKS + 1));

  // Update every chunk, several times, each time with other data (the compression buffer is reused)
  int64_t nchunks = schunk->nchunks;
  for (int round = 0; round < 3; round++) {
    for (int64_t n = nchunks - 1; n >= 0; n--) {
      fill((int32_t) (round * 100000 + n * 10));
      mu_assert("ERROR: bad update", blosc2_schunk_update_buffer(schunk, n, data, isize) == nchunks);
    }
  }
  for (int64_t n = 0; n < nchunks; n++) {
    mu_assert("ERROR: bad updated chunk", chunk_holds(schunk, n, (int32_t) (200000 + n * 10), n));
  }
  mu_assert("ERROR: bad nbytes", schunk->nbytes == nchunks * isize);

  // Invalid positions and sizes fail and change nothing
  fill(0);
  mu_assert("ERROR: update past the end must fail", blosc2_schunk_update_buffer(schunk, nchunks, data, isize) < 0);
  mu_assert("ERROR: update at a negative position must fail", blosc2_schunk_update_buffer(schunk, -1, data, isize) < 0);
  mu_assert("ERROR: insert past the end must fail",
            blosc2_schunk_insert_buffer(schunk, nchunks + 1, data, isize) < 0);
  mu_assert("ERROR: negative nbytes must fail", blosc2_schunk_append_buffer(schunk, data, -1) < 0);
  mu_assert("ERROR: a failed write changed nchunks", schunk->nchunks == nchunks);
  mu_assert("ERROR: a failed write changed nbytes", schunk->nbytes == nchunks * isize);

  // A reopened frame holds the same chunks
  if (tdata.urlpath != NULL) {
    blosc2_schunk_free(schunk);
    schunk = blosc2_schunk_open(tdata.urlpath);
    mu_assert("ERROR: cannot reopen the super-chunk", schunk != NULL);
    mu_assert("ERROR: bad nchunks after reopening", schunk->nchunks == nchunks);
  }
  for (int64_t n = 0; n < nchunks; n++) {
    mu_assert("ERROR: bad chunk after reopening", chunk_holds(schunk, n, (int32_t) (200000 + n * 10), n));
  }

  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(tdata.urlpath);
  return EXIT_SUCCESS;
}

static char *all_tests(void) {
  for (int i = 0; i < (int) ARRAY_SIZE(tstorage); ++i) {
    tdata = tstorage[i];
    mu_run_test(test_schunk_buffer);
  }
  return EXIT_SUCCESS;
}

#define BUFFER_ALIGN_SIZE 32

int main(void) {
  blosc2_init();
  install_blosc_callback_test();
  data = blosc_test_malloc(BUFFER_ALIGN_SIZE, CHUNKITEMS * sizeof(int32_t));
  data_dest = blosc_test_malloc(BUFFER_ALIGN_SIZE, CHUNKITEMS * sizeof(int32_t));

  char *result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  blosc_test_free(data);
  blosc_test_free(data_dest);
  blosc2_destroy();
  return result != EXIT_SUCCESS;
}
