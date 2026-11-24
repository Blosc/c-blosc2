/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Variable-length metalayers are not chunk data, so the prefilter and the
  postfilter of a super-chunk must not run on their content.  The filters here
  count their calls and overwrite whatever they get.  The content has to come
  back intact from blosc2_vlmeta_add()/blosc2_vlmeta_update() with the cparams
  of the super-chunk (prefilter included), blosc2_vlmeta_get(),
  blosc2_schunk_copy(), blosc2_schunk_to_buffer(), and from b2nd_copy() into an
  array whose cparams have a prefilter and a lossy filter, for every kind of
  storage.
*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "b2nd.h"
#include "blosc2.h"
#include "cutest.h"

#define NITEMS (10 * 1000)
#define LARGE_LEN (100 * 1000)

typedef struct {
  bool contiguous;
  char *urlpath;
  char *urlpath2;
} storage_params;

CUTEST_TEST_DATA(vlmeta_prepostfilter) {
  uint8_t tiny[9];  // a short content is stored memcpyed (not `small`: a macro on Windows)
  uint8_t *large;
};

static int nprefilter = 0;
static int npostfilter = 0;

static int clobbering_prefilter(blosc2_prefilter_params *params) {
  nprefilter++;
  memset(params->output, 0xAA, (size_t)params->output_size);
  return 0;
}

static int clobbering_postfilter(blosc2_postfilter_params *params) {
  npostfilter++;
  memset(params->output, 0x55, (size_t)params->size);
  return 0;
}

/* `schunk` has the vlmetalayer `name` with the `len` bytes of `content` */
static int check_vlmeta(blosc2_schunk *schunk, const char *name, const uint8_t *content, int32_t len) {
  uint8_t *got = NULL;
  int32_t got_len = 0;
  CUTEST_ASSERT("Missing vlmetalayer", blosc2_vlmeta_get(schunk, name, &got, &got_len) >= 0);
  bool same = got_len == len && memcmp(got, content, (size_t)len) == 0;
  free(got);
  CUTEST_ASSERT("Wrong vlmetalayer content", same);
  return 0;
}

static int check_both(blosc2_schunk *schunk, struct vlmeta_prepostfilter_data *data) {
  CUTEST_ASSERT("Wrong small vlmetalayer",
                check_vlmeta(schunk, "small", data->tiny, sizeof(data->tiny)) == 0);
  CUTEST_ASSERT("Wrong large vlmetalayer", check_vlmeta(schunk, "large", data->large, LARGE_LEN) == 0);
  return 0;
}

CUTEST_TEST_SETUP(vlmeta_prepostfilter) {
  blosc2_init();
  memcpy(data->tiny, "\x81\xa6" "answer*", sizeof(data->tiny));
  data->large = malloc(LARGE_LEN);
  for (int i = 0; i < LARGE_LEN; i++) {
    data->large[i] = (uint8_t)(i % 251);
  }

  CUTEST_PARAMETRIZE(params, storage_params, CUTEST_DATA(
      {true, NULL, NULL},
      {false, NULL, NULL},
      {true, "test_vlmeta_prepostfilter.b2frame", "test_vlmeta_prepostfilter2.b2frame"},
      {false, "test_vlmeta_prepostfilter_s.b2frame", "test_vlmeta_prepostfilter2_s.b2frame"},
  ));
}

CUTEST_TEST_TEST(vlmeta_prepostfilter) {
  CUTEST_GET_PARAMETER(params, storage_params);
  blosc2_remove_urlpath(params.urlpath);
  blosc2_remove_urlpath(params.urlpath2);

  blosc2_prefilter_params preparams = {0};
  blosc2_postfilter_params postparams = {0};
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = 1;
  cparams.prefilter = clobbering_prefilter;
  cparams.preparams = &preparams;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  dparams.postfilter = clobbering_postfilter;
  dparams.postparams = &postparams;
  blosc2_storage storage = {.contiguous = params.contiguous, .urlpath = params.urlpath,
                            .cparams = &cparams, .dparams = &dparams};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  CUTEST_ASSERT("Error creating schunk", schunk != NULL);

  // The filters are for chunk data
  int32_t *items = malloc(NITEMS * sizeof(int32_t));
  for (int i = 0; i < NITEMS; i++) {
    items[i] = i;
  }
  nprefilter = 0;
  CUTEST_ASSERT("Error appending data",
                blosc2_schunk_append_buffer(schunk, items, NITEMS * sizeof(int32_t)) == 1);
  CUTEST_ASSERT("The prefilter should run on chunk data", nprefilter > 0);
  npostfilter = 0;
  int32_t *out = malloc(NITEMS * sizeof(int32_t));
  int nbytes = blosc2_schunk_decompress_chunk(schunk, 0, out, NITEMS * sizeof(int32_t));
  free(out);
  CUTEST_ASSERT("Error decompressing", nbytes > 0);
  CUTEST_ASSERT("The postfilter should run on chunk data", npostfilter > 0);

  // Callers often pass the cparams of the super-chunk for its vlmetalayers
  nprefilter = 0;
  npostfilter = 0;
  CUTEST_ASSERT("Error adding a vlmetalayer",
                blosc2_vlmeta_add(schunk, "small", data->tiny, sizeof(data->tiny),
                                  schunk->storage->cparams) >= 0);
  CUTEST_ASSERT("Error adding a vlmetalayer",
                blosc2_vlmeta_add(schunk, "large", data->large, LARGE_LEN, schunk->storage->cparams) >= 0);
  CUTEST_ASSERT("The prefilter ran on vlmetalayer content", nprefilter == 0);
  CUTEST_ASSERT("Wrong vlmetalayers", check_both(schunk, data) == 0);
  CUTEST_ASSERT("The postfilter ran on vlmetalayer content", npostfilter == 0);

  data->large[0] ^= 1;
  CUTEST_ASSERT("Error updating a vlmetalayer",
                blosc2_vlmeta_update(schunk, "large", data->large, LARGE_LEN, schunk->storage->cparams) >= 0);
  CUTEST_ASSERT("The prefilter ran on vlmetalayer content", nprefilter == 0);
  CUTEST_ASSERT("Wrong vlmetalayers after an update", check_both(schunk, data) == 0);
  CUTEST_ASSERT("The postfilter ran on vlmetalayer content", npostfilter == 0);

  // Copies of the super-chunk and its serialized frame (which of the filters
  // apply to the chunks of a copy is not checked here)
  blosc2_storage copy_storage = {.contiguous = true};
  blosc2_schunk *copy = blosc2_schunk_copy(schunk, &copy_storage);
  CUTEST_ASSERT("Error copying", copy != NULL);
  CUTEST_ASSERT("Wrong vlmetalayers in a copy", check_both(copy, data) == 0);
  blosc2_schunk_free(copy);

  uint8_t *cframe;
  bool needs_free;
  int64_t len = blosc2_schunk_to_buffer(schunk, &cframe, &needs_free);
  CUTEST_ASSERT("Error serializing", len > 0);
  blosc2_schunk *back = blosc2_schunk_from_buffer(cframe, len, true);
  if (needs_free) {
    free(cframe);
  }
  CUTEST_ASSERT("Error deserializing", back != NULL);
  CUTEST_ASSERT("Wrong vlmetalayers in a serialized frame", check_both(back, data) == 0);
  blosc2_schunk_free(back);
  blosc2_schunk_free(schunk);

  // b2nd_copy() into other chunks recompresses the vlmetalayers; the cparams of
  // the new array (a prefilter and a lossy filter here) are for its chunks only
  int64_t shape[] = {NITEMS};
  int32_t chunkshape[] = {1000};
  int32_t blockshape[] = {100};
  int32_t chunkshape2[] = {500};
  int32_t blockshape2[] = {50};
  blosc2_cparams src_cparams = BLOSC2_CPARAMS_DEFAULTS;
  src_cparams.typesize = sizeof(int32_t);
  src_cparams.nthreads = 1;
  blosc2_storage src_storage = {.contiguous = params.contiguous, .urlpath = params.urlpath,
                                .cparams = &src_cparams};
  b2nd_context_t *ctx = b2nd_create_ctx(&src_storage, 1, shape, chunkshape, blockshape, NULL, 0, NULL, 0);
  CUTEST_ASSERT("Error creating a context", ctx != NULL);
  b2nd_array_t *src;
  blosc2_remove_urlpath(params.urlpath);
  CUTEST_ASSERT("Error creating an array",
                b2nd_from_cbuffer(ctx, &src, items, NITEMS * sizeof(int32_t)) >= 0);
  free(items);
  b2nd_free_ctx(ctx);
  CUTEST_ASSERT("Error adding a vlmetalayer",
                blosc2_vlmeta_add(src->sc, "small", data->tiny, sizeof(data->tiny), NULL) >= 0);
  CUTEST_ASSERT("Error adding a vlmetalayer",
                blosc2_vlmeta_add(src->sc, "large", data->large, LARGE_LEN, NULL) >= 0);

  blosc2_cparams dest_cparams = cparams;
  dest_cparams.filters[0] = BLOSC_TRUNC_PREC;
  dest_cparams.filters_meta[0] = 8;
  blosc2_storage dest_storage = {.contiguous = params.contiguous, .urlpath = params.urlpath2,
                                 .cparams = &dest_cparams};
  ctx = b2nd_create_ctx(&dest_storage, 1, shape, chunkshape2, blockshape2, NULL, 0, NULL, 0);
  CUTEST_ASSERT("Error creating a context", ctx != NULL);
  b2nd_array_t *dest;
  nprefilter = 0;
  CUTEST_ASSERT("Error copying an array", b2nd_copy(ctx, src, &dest) >= 0);
  CUTEST_ASSERT("The prefilter should run on the chunks of the copy", nprefilter > 0);
  b2nd_free_ctx(ctx);
  CUTEST_ASSERT("Wrong vlmetalayers in a b2nd copy", check_both(dest->sc, data) == 0);
  b2nd_free(dest);
  b2nd_free(src);

  blosc2_remove_urlpath(params.urlpath);
  blosc2_remove_urlpath(params.urlpath2);
  return 0;
}

CUTEST_TEST_TEARDOWN(vlmeta_prepostfilter) {
  free(data->large);
  blosc2_destroy();
}

int main(void) {
  CUTEST_TEST_RUN(vlmeta_prepostfilter);
}
