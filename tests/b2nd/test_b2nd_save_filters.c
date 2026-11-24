/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  b2nd_save(), b2nd_to_cframe() and b2nd_save_append() of an array that has a
  postfilter store the chunks as they are (b2nd_save() also keeps the layout,
  contiguous or sparse), whereas b2nd_copy() applies the postfilter.
*/

#include "test_common.h"

#define NITEMS (4 * 10 * 1000)
#define POSTFILTER_ADD 1000000

typedef struct {
  bool contiguous;
  char *urlpath;
} save_filters_params;

static int add_postfilter(blosc2_postfilter_params *params) {
  const int32_t *in = (const int32_t *)params->input;
  int32_t *out = (int32_t *)params->output;
  for (int32_t i = 0; i < params->size / params->typesize; i++) {
    out[i] = in[i] + POSTFILTER_ADD;
  }
  return 0;
}

/* `array` reads `i + add` */
static int check_values(b2nd_array_t *array, int32_t add, int32_t *buffer) {
  B2ND_TEST_ASSERT(b2nd_to_cbuffer(array, buffer, NITEMS * sizeof(int32_t)));
  for (int32_t i = 0; i < NITEMS; i++) {
    CUTEST_ASSERT("Wrong value", buffer[i] == i + add);
  }
  return 0;
}

/* The chunks of `other` are those of `array`, byte for byte */
static int check_same_chunks(b2nd_array_t *array, b2nd_array_t *other) {
  CUTEST_ASSERT("nchunks differ", other->sc->nchunks == array->sc->nchunks);
  for (int64_t nchunk = 0; nchunk < array->sc->nchunks; nchunk++) {
    uint8_t *chunk, *other_chunk;
    bool needs_free, other_needs_free;
    int cbytes = blosc2_schunk_get_chunk(array->sc, nchunk, &chunk, &needs_free);
    int other_cbytes = blosc2_schunk_get_chunk(other->sc, nchunk, &other_chunk, &other_needs_free);
    CUTEST_ASSERT("Error getting a chunk", cbytes > 0 && other_cbytes > 0);
    bool same = other_cbytes == cbytes && memcmp(chunk, other_chunk, (size_t)cbytes) == 0;
    if (needs_free) {
      free(chunk);
    }
    if (other_needs_free) {
      free(other_chunk);
    }
    CUTEST_ASSERT("A chunk differs from the stored one", same);
  }
  return 0;
}

CUTEST_TEST_SETUP(save_filters) {
  blosc2_init();

  CUTEST_PARAMETRIZE(params, save_filters_params, CUTEST_DATA(
      {true, NULL},
      {false, NULL},
      {true, "test_save_filters.b2nd"},
      {false, "test_save_filters_s.b2nd"},
  ));
}

CUTEST_TEST_TEST(save_filters) {
  CUTEST_GET_PARAMETER(params, save_filters_params);
  int32_t *buffer = malloc(NITEMS * sizeof(int32_t));

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_postfilter_params postparams = {0};
  dparams.postfilter = add_postfilter;
  dparams.postparams = &postparams;
  blosc2_storage storage = {.contiguous = params.contiguous, .urlpath = params.urlpath,
                            .cparams = &cparams, .dparams = &dparams};
  int64_t shape[] = {NITEMS};
  int32_t chunkshape[] = {NITEMS / 4};
  int32_t blockshape[] = {NITEMS / 16};
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, 1, shape, chunkshape, blockshape, "<i4",
                                        DTYPE_NUMPY_FORMAT, NULL, 0);
  CUTEST_ASSERT("Error creating the context", ctx != NULL);

  for (int32_t i = 0; i < NITEMS; i++) {
    buffer[i] = i;
  }
  blosc2_remove_urlpath(params.urlpath);
  b2nd_array_t *src;
  B2ND_TEST_ASSERT(b2nd_from_cbuffer(ctx, &src, buffer, NITEMS * sizeof(int32_t)));
  CUTEST_ASSERT("The postfilter is not applied", check_values(src, POSTFILTER_ADD, buffer) == 0);

  // b2nd_save(): the chunks as stored, in the same layout
  char *fname = "test_save_filters_out.b2nd";
  blosc2_remove_urlpath(fname);
  B2ND_TEST_ASSERT(b2nd_save(src, fname));
  b2nd_array_t *saved;
  B2ND_TEST_ASSERT(b2nd_open(fname, &saved));
  CUTEST_ASSERT("b2nd_save() must keep the layout", saved->sc->storage->contiguous == params.contiguous);
  CUTEST_ASSERT("b2nd_save() must store the chunks as stored", check_same_chunks(src, saved) == 0);
  CUTEST_ASSERT("b2nd_save() must store the stored values", check_values(saved, 0, buffer) == 0);
  B2ND_TEST_ASSERT(b2nd_free(saved));
  blosc2_remove_urlpath(fname);

  // b2nd_to_cframe() and b2nd_save_append() store the same values
  uint8_t *cframe;
  int64_t cframe_len;
  bool needs_free;
  B2ND_TEST_ASSERT(b2nd_to_cframe(src, &cframe, &cframe_len, &needs_free));
  b2nd_array_t *back;
  B2ND_TEST_ASSERT(b2nd_from_cframe(cframe, cframe_len, true, &back));
  if (needs_free) {
    free(cframe);
  }
  CUTEST_ASSERT("b2nd_to_cframe() must store the stored values", check_values(back, 0, buffer) == 0);
  B2ND_TEST_ASSERT(b2nd_free(back));
  CUTEST_ASSERT("Error appending", b2nd_save_append(src, fname) == 0);
  B2ND_TEST_ASSERT(b2nd_open_offset(fname, &back, 0));
  CUTEST_ASSERT("b2nd_save_append() must store the stored values", check_values(back, 0, buffer) == 0);
  B2ND_TEST_ASSERT(b2nd_free(back));
  blosc2_remove_urlpath(fname);

  // b2nd_copy() applies the postfilter
  blosc2_cparams copy_cparams = BLOSC2_CPARAMS_DEFAULTS;
  copy_cparams.typesize = sizeof(int32_t);
  blosc2_storage copy_storage = {.contiguous = true, .cparams = &copy_cparams};
  b2nd_context_t *copy_ctx = b2nd_create_ctx(&copy_storage, 1, shape, chunkshape, blockshape, "<i4",
                                             DTYPE_NUMPY_FORMAT, NULL, 0);
  b2nd_array_t *copy;
  B2ND_TEST_ASSERT(b2nd_copy(copy_ctx, src, &copy));
  CUTEST_ASSERT("A copy must hold the values as read", check_values(copy, POSTFILTER_ADD, buffer) == 0);
  B2ND_TEST_ASSERT(b2nd_free(copy));
  B2ND_TEST_ASSERT(b2nd_free_ctx(copy_ctx));

  B2ND_TEST_ASSERT(b2nd_free(src));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));
  blosc2_remove_urlpath(params.urlpath);
  free(buffer);
  return 0;
}

CUTEST_TEST_TEARDOWN(save_filters) {
  blosc2_destroy();
}

int main(void) {
  CUTEST_TEST_RUN(save_filters);
}
