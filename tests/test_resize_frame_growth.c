/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  Building a b2nd array incrementally (b2nd_resize + set_slice in batches)
  on a contiguous on-disk frame must not leave the file measurably larger
  than creating the same array in one go.  Repeated resizes used to bloat
  the frame with dead space from rewriting it on every step.
*/

#include <stdio.h>
#include "test_common.h"
#include "b2nd.h"

#define NITEMS 100000
#define BSIZE 1000

/* Global vars */
int tests_run = 0;


static int64_t get_file_size(const char *path) {
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) {
    return -1;
  }
  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return -1;
  }
  int64_t size = ftell(fp);
  fclose(fp);
  return size;
}


static b2nd_context_t* create_ctx(const char *urlpath, blosc2_cparams *cparams,
                                  int64_t nitems) {
  blosc2_storage storage = {.contiguous=true, .urlpath=(char *)urlpath,
                            .cparams=cparams};
  blosc2_remove_urlpath(urlpath);

  int64_t shape[] = {nitems};
  int32_t chunkshape[] = {16384};
  int32_t blockshape[] = {256};
  return b2nd_create_ctx(&storage, 1, shape, chunkshape, blockshape,
                         NULL, 0, NULL, 0);
}


/* Build the array with one resize + set_slice per batch. */
static int create_extended_array(const char *urlpath) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int64_t);
  b2nd_context_t *ctx = create_ctx(urlpath, &cparams, 0);
  BLOSC_ERROR_NULL(ctx, BLOSC2_ERROR_FAILURE);

  b2nd_array_t *arr;
  BLOSC_ERROR(b2nd_empty(ctx, &arr));

  static int64_t buffer[BSIZE];
  for (int64_t start = 0; start < NITEMS; start += BSIZE) {
    int64_t stop = start + BSIZE > NITEMS ? NITEMS : start + BSIZE;
    int64_t newshape[] = {stop};
    BLOSC_ERROR(b2nd_resize(arr, newshape, NULL));

    for (int64_t i = 0; i < stop - start; ++i) {
      buffer[i] = start + i;
    }
    int64_t buffershape[] = {stop - start};
    int64_t slice_start[] = {start};
    int64_t slice_stop[] = {stop};
    BLOSC_ERROR(b2nd_set_slice_cbuffer(buffer, buffershape,
                                       (stop - start) * (int64_t)sizeof(int64_t),
                                       slice_start, slice_stop, arr));
  }

  BLOSC_ERROR(b2nd_free(arr));
  BLOSC_ERROR(b2nd_free_ctx(ctx));
  return BLOSC2_ERROR_SUCCESS;
}


/* Build the same array from a single buffer. */
static int create_one_go_array(const char *urlpath) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int64_t);
  b2nd_context_t *ctx = create_ctx(urlpath, &cparams, NITEMS);
  BLOSC_ERROR_NULL(ctx, BLOSC2_ERROR_FAILURE);

  static int64_t data[NITEMS];
  for (int64_t i = 0; i < NITEMS; ++i) {
    data[i] = i;
  }
  b2nd_array_t *arr;
  BLOSC_ERROR(b2nd_from_cbuffer(ctx, &arr, data, sizeof(data)));

  BLOSC_ERROR(b2nd_free(arr));
  BLOSC_ERROR(b2nd_free_ctx(ctx));
  return BLOSC2_ERROR_SUCCESS;
}


static char* test_no_frame_growth(void) {
  const char *extended_path = "test_resize_growth_extended.b2nd";
  const char *one_go_path = "test_resize_growth_one_go.b2nd";

  mu_assert("ERROR: cannot create the incrementally extended array",
            create_extended_array(extended_path) >= 0);
  mu_assert("ERROR: cannot create the one-go array",
            create_one_go_array(one_go_path) >= 0);

  int64_t extended_size = get_file_size(extended_path);
  int64_t one_go_size = get_file_size(one_go_path);
  mu_assert("ERROR: cannot stat the array files",
            extended_size > 0 && one_go_size > 0);

  // Allow a small slack for header/trailer padding differences
  mu_assert("ERROR: incremental resize produced unnecessary contiguous-frame growth",
            extended_size <= one_go_size + 4096);

  blosc2_remove_urlpath(extended_path);
  blosc2_remove_urlpath(one_go_path);
  return EXIT_SUCCESS;
}


static char *all_tests(void) {
  mu_run_test(test_no_frame_growth);

  return EXIT_SUCCESS;
}


int main(void) {
  char* result;

  install_blosc_callback_test(); /* optionally install callback test */
  blosc2_init();

  /* Run all the suite */
  result = all_tests();
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
