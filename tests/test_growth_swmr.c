/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  Growth-SWMR: a reader handle on a disk-based b2nd array must follow shape
  changes (resize/append) made through another handle, without reopening.
  Also checks that vlmetalayer readers see updates made by another handle.
  See plans/growth-SWMR.md.
*/

#include <stdio.h>
#include "test_common.h"
#include "b2nd.h"

#define NROWS 10
#define NCOLS 10

/* Global vars */
int tests_run = 0;
char* urlpath;
bool contiguous;


static b2nd_array_t* create_array(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  blosc2_storage storage = {.contiguous=contiguous, .urlpath=urlpath, .cparams=&cparams};
  blosc2_remove_urlpath(urlpath);

  int64_t shape[] = {NROWS, NCOLS};
  int32_t chunkshape[] = {5, 5};
  int32_t blockshape[] = {5, 5};
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, 2, shape, chunkshape, blockshape,
                                        NULL, 0, NULL, 0);
  if (ctx == NULL) {
    return NULL;
  }
  b2nd_array_t *array = NULL;
  if (b2nd_zeros(ctx, &array) < 0) {
    array = NULL;
  }
  b2nd_free_ctx(ctx);
  return array;
}


/* An existing reader must see the grown region written by another handle. */
static char* test_grow(void) {
  b2nd_array_t *creator = create_array();
  mu_assert("ERROR: cannot create the array", creator != NULL);
  b2nd_free(creator);

  b2nd_array_t *w = NULL;
  b2nd_array_t *r = NULL;
  mu_assert("ERROR: cannot open the writer handle", b2nd_open(urlpath, &w) >= 0);
  mu_assert("ERROR: cannot open the reader handle", b2nd_open(urlpath, &r) >= 0);

  // Warm the reader's cached view
  int32_t buffer[NROWS * NCOLS] = {0};
  int64_t start[] = {0, 0};
  int64_t stop[] = {1, NCOLS};
  int64_t buffershape[] = {1, NCOLS};
  mu_assert("ERROR: cannot read from the reader before the resize",
            b2nd_get_slice_cbuffer(r, start, stop, buffer, buffershape,
                                   sizeof(buffer)) >= 0);
  mu_assert("ERROR: unexpected initial shape", r->shape[0] == NROWS);

  // Writer grows the array and fills the new region
  int64_t new_shape[] = {2 * NROWS, NCOLS};
  mu_assert("ERROR: cannot resize the array", b2nd_resize(w, new_shape, NULL) >= 0);
  int32_t wbuffer[NROWS * NCOLS];
  for (int i = 0; i < NROWS * NCOLS; i++) {
    wbuffer[i] = i + 1;
  }
  int64_t wstart[] = {NROWS, 0};
  int64_t wstop[] = {2 * NROWS, NCOLS};
  int64_t wshape[] = {NROWS, NCOLS};
  mu_assert("ERROR: cannot write to the grown region",
            b2nd_set_slice_cbuffer(wbuffer, wshape, sizeof(wbuffer),
                                   wstart, wstop, w) >= 0);

  // The stale reader must now see the new shape and the new data
  int32_t rbuffer[NROWS * NCOLS] = {0};
  mu_assert("ERROR: cannot read the grown region from the stale reader",
            b2nd_get_slice_cbuffer(r, wstart, wstop, rbuffer, wshape,
                                   sizeof(rbuffer)) >= 0);
  mu_assert("ERROR: the reader shape did not follow the resize",
            r->shape[0] == 2 * NROWS && r->shape[1] == NCOLS);
  for (int i = 0; i < NROWS * NCOLS; i++) {
    mu_assert("ERROR: wrong data in the grown region", rbuffer[i] == wbuffer[i]);
  }

  b2nd_free(w);
  b2nd_free(r);
  blosc2_remove_urlpath(urlpath);
  return EXIT_SUCCESS;
}


/* An existing reader must see the shrunk shape (chunks were deleted). */
static char* test_shrink(void) {
  b2nd_array_t *creator = create_array();
  mu_assert("ERROR: cannot create the array", creator != NULL);
  b2nd_free(creator);

  b2nd_array_t *w = NULL;
  b2nd_array_t *r = NULL;
  mu_assert("ERROR: cannot open the writer handle", b2nd_open(urlpath, &w) >= 0);
  mu_assert("ERROR: cannot open the reader handle", b2nd_open(urlpath, &r) >= 0);

  // Warm the reader's cached view
  int32_t buffer[NROWS * NCOLS] = {0};
  int64_t start[] = {0, 0};
  int64_t stop[] = {NROWS, NCOLS};
  int64_t buffershape[] = {NROWS, NCOLS};
  mu_assert("ERROR: cannot read from the reader before the resize",
            b2nd_get_slice_cbuffer(r, start, stop, buffer, buffershape,
                                   sizeof(buffer)) >= 0);

  // Writer shrinks the array so that a row of chunks disappears
  int64_t new_shape[] = {NROWS / 2, NCOLS};
  mu_assert("ERROR: cannot shrink the array", b2nd_resize(w, new_shape, NULL) >= 0);

  // A read within the new bounds re-syncs the reader
  int64_t rstop[] = {NROWS / 2, NCOLS};
  int64_t rshape[] = {NROWS / 2, NCOLS};
  mu_assert("ERROR: cannot read within the shrunk bounds",
            b2nd_get_slice_cbuffer(r, start, rstop, buffer, rshape,
                                   NROWS / 2 * NCOLS * (int64_t)sizeof(int32_t)) >= 0);
  mu_assert("ERROR: the reader shape did not follow the shrink",
            r->shape[0] == NROWS / 2 && r->shape[1] == NCOLS);

  b2nd_free(w);
  b2nd_free(r);
  blosc2_remove_urlpath(urlpath);
  return EXIT_SUCCESS;
}


/* vlmetalayer readers must see updates made by another handle. */
static char* test_vlmeta(void) {
  b2nd_array_t *creator = create_array();
  mu_assert("ERROR: cannot create the array", creator != NULL);
  uint8_t content1[] = "old";
  mu_assert("ERROR: cannot add the vlmetalayer",
            blosc2_vlmeta_add(creator->sc, "test_vlmeta", content1,
                              sizeof(content1), NULL) >= 0);
  b2nd_free(creator);

  b2nd_array_t *w = NULL;
  b2nd_array_t *r = NULL;
  mu_assert("ERROR: cannot open the writer handle", b2nd_open(urlpath, &w) >= 0);
  mu_assert("ERROR: cannot open the reader handle", b2nd_open(urlpath, &r) >= 0);

  // Warm the reader's cached vlmetalayers
  uint8_t* content = NULL;
  int32_t content_len;
  mu_assert("ERROR: cannot read the vlmetalayer before the update",
            blosc2_vlmeta_get(r->sc, "test_vlmeta", &content, &content_len) >= 0);
  free(content);

  // Writer updates the vlmetalayer with content of a different length
  uint8_t content2[] = "brand-new";
  mu_assert("ERROR: cannot update the vlmetalayer",
            blosc2_vlmeta_update(w->sc, "test_vlmeta", content2,
                                 sizeof(content2), NULL) >= 0);

  // The stale reader must see the new content
  content = NULL;
  mu_assert("ERROR: cannot read the vlmetalayer after the update",
            blosc2_vlmeta_get(r->sc, "test_vlmeta", &content, &content_len) >= 0);
  mu_assert("ERROR: the reader did not follow the vlmetalayer update",
            content_len == sizeof(content2) &&
            memcmp(content, content2, sizeof(content2)) == 0);
  free(content);

  b2nd_free(w);
  b2nd_free(r);
  blosc2_remove_urlpath(urlpath);
  return EXIT_SUCCESS;
}


/* b2nd_refresh() must make a shape change visible without reading data
   (the deterministic sync point pollers use, e.g. python-blosc2). */
static char* test_explicit_refresh(void) {
  b2nd_array_t *creator = create_array();
  mu_assert("ERROR: cannot create the array", creator != NULL);
  b2nd_free(creator);

  b2nd_array_t *w = NULL;
  b2nd_array_t *r = NULL;
  mu_assert("ERROR: cannot open the writer handle", b2nd_open(urlpath, &w) >= 0);
  mu_assert("ERROR: cannot open the reader handle", b2nd_open(urlpath, &r) >= 0);

  // A fresh handle is already current
  mu_assert("ERROR: refresh on a current handle should return 0",
            b2nd_refresh(r) == 0);

  // Writer grows the array behind the reader's back
  int64_t new_shape[] = {2 * NROWS, NCOLS};
  mu_assert("ERROR: cannot resize the array", b2nd_resize(w, new_shape, NULL) >= 0);
  mu_assert("ERROR: the reader shape should still be stale before the refresh",
            r->shape[0] == NROWS);

  // An explicit refresh must re-sync the shape, with no data access involved
  mu_assert("ERROR: refresh on a stale handle should return 1",
            b2nd_refresh(r) == 1);
  mu_assert("ERROR: the reader shape did not follow the resize",
            r->shape[0] == 2 * NROWS && r->shape[1] == NCOLS);
  mu_assert("ERROR: a second refresh should return 0",
            b2nd_refresh(r) == 0);

  b2nd_free(w);
  b2nd_free(r);
  blosc2_remove_urlpath(urlpath);
  return EXIT_SUCCESS;
}


static char *all_tests(void) {
  urlpath = "test_growth_swmr.b2nd";
  contiguous = true;
  mu_run_test(test_grow);
  mu_run_test(test_shrink);
  mu_run_test(test_vlmeta);
  mu_run_test(test_explicit_refresh);

  urlpath = "test_growth_swmr_s.b2nd";
  contiguous = false;
  mu_run_test(test_grow);
  mu_run_test(test_shrink);
  mu_run_test(test_vlmeta);
  mu_run_test(test_explicit_refresh);

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
