/*
 * Copyright (c) 2026 Blosc Development Team <blosc@blosc.org>
 * License: BSD 3-Clause (see LICENSE.txt).
 *
 * Zero-initialization remains the default; only an explicit context opt-in
 * permits a callback to see dirty scratch before fully overwriting it.
 */
#include "test_common.h"

typedef struct {
  int phase;
  bool check_dirty;
  bool dirty[4];
} full_write_state;

static int full_write_prefilter(blosc2_prefilter_params *params) {
  full_write_state *state = params->user_data;
  if (state->phase == 0) {
    memset(params->output, 0xa5, (size_t)params->output_size);
  } else if (state->phase == 1) {
    params->output[0] = 0x7f;  /* Legacy partial writer: remaining bytes must be zero. */
  } else if (state->phase == 2) {
    bool dirty = true;
    for (int i = 0; state->check_dirty && i < params->output_size; ++i) {
      /* A worker can reuse the output of an earlier block in this call. */
      dirty &= params->output[i] == 0xa5 || params->output[i] == 0x3c;
    }
    state->dirty[params->nblock] = dirty;
    memset(params->output, 0x3c, (size_t)params->output_size);
  } else {
    return -1;
  }
  return 0;
}

#define FULL_WRITE_CHECK(test) do { \
  if (!(test)) { \
    printf("Full-write check failed at line %d\n", __LINE__); \
    goto cleanup; \
  } \
} while (0)

static int test_full_write(int clevel, int nthreads) {
  uint8_t src[4096] = {0}, compressed[4096 + BLOSC2_MAX_OVERHEAD], decoded[4096];
  full_write_state state = {0};
  /* Parallel jobs may use a different worker on the second call. Only serial
   * scratch has a deterministic previous value; parallel calls still roundtrip. */
  state.check_dirty = nthreads == 1;
  blosc2_prefilter_params params = {0};
  params.user_data = &state;
  blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
  cp.typesize = 1;
  cp.blocksize = 1024;
  cp.clevel = clevel;
  cp.nthreads = nthreads;
  cp.prefilter = full_write_prefilter;
  cp.preparams = &params;
  memset(cp.filters, 0, sizeof(cp.filters));
  blosc2_dparams dp = BLOSC2_DPARAMS_DEFAULTS;
  dp.nthreads = nthreads;
  blosc2_context *cc = blosc2_create_cctx(cp);
  blosc2_context *dc = blosc2_create_dctx(dp);
  blosc2_context *clone = NULL;
  int result = EXIT_FAILURE, size;
  FULL_WRITE_CHECK(cc != NULL && dc != NULL);
  FULL_WRITE_CHECK(blosc2_set_prefilter_full_write(NULL, true) == BLOSC2_ERROR_INVALID_PARAM);
  FULL_WRITE_CHECK(blosc2_set_prefilter_full_write(dc, true) == BLOSC2_ERROR_INVALID_PARAM);
  FULL_WRITE_CHECK(blosc2_set_prefilter_full_write(dc, false) == BLOSC2_ERROR_INVALID_PARAM);

  /* Poison every worker's output, then prove enabling avoids the memset. */
  size = blosc2_compress_ctx(cc, src, sizeof(src), compressed, sizeof(compressed));
  FULL_WRITE_CHECK(size > 0);
  FULL_WRITE_CHECK(blosc2_set_prefilter_full_write(cc, true) == 0);
  state.phase = 2;
  size = blosc2_compress_ctx(cc, src, sizeof(src), compressed, sizeof(compressed));
  FULL_WRITE_CHECK(size > 0);
  FULL_WRITE_CHECK(blosc2_decompress_ctx(dc, compressed, size, decoded, sizeof(decoded)) == sizeof(decoded));
  for (int i = 0; i < 4; ++i) {
    FULL_WRITE_CHECK(state.dirty[i]);
  }
  for (size_t i = 0; i < sizeof(decoded); ++i) {
    FULL_WRITE_CHECK(decoded[i] == 0x3c);
  }

  /* Recreating a context from public parameters must not inherit the opt-in. */
  FULL_WRITE_CHECK(blosc2_ctx_get_cparams(cc, &cp) == 0);
  clone = blosc2_create_cctx(cp);
  FULL_WRITE_CHECK(clone != NULL);
  state.phase = 0;
  FULL_WRITE_CHECK(blosc2_compress_ctx(clone, src, sizeof(src), compressed, sizeof(compressed)) > 0);
  state.phase = 1;
  size = blosc2_compress_ctx(clone, src, sizeof(src), compressed, sizeof(compressed));
  FULL_WRITE_CHECK(size > 0);
  FULL_WRITE_CHECK(blosc2_decompress_ctx(dc, compressed, size, decoded, sizeof(decoded)) == sizeof(decoded));
  for (size_t i = 0; i < sizeof(decoded); ++i) {
    FULL_WRITE_CHECK(decoded[i] == (i % 1024 == 0 ? 0x7f : 0));
  }

  /* Disabling the opt-in restores partial-write behavior, also after failure. */
  state.phase = 3;
  FULL_WRITE_CHECK(blosc2_compress_ctx(cc, src, sizeof(src), compressed, sizeof(compressed)) < 0);
  FULL_WRITE_CHECK(blosc2_set_prefilter_full_write(cc, false) == 0);
  state.phase = 1;
  size = blosc2_compress_ctx(cc, src, sizeof(src), compressed, sizeof(compressed));
  FULL_WRITE_CHECK(size > 0);
  FULL_WRITE_CHECK(blosc2_decompress_ctx(dc, compressed, size, decoded, sizeof(decoded)) == sizeof(decoded));
  for (size_t i = 0; i < sizeof(decoded); ++i) {
    FULL_WRITE_CHECK(decoded[i] == (i % 1024 == 0 ? 0x7f : 0));
  }
  result = EXIT_SUCCESS;
cleanup:
  if (clone != NULL) blosc2_free_ctx(clone);
  if (cc != NULL) blosc2_free_ctx(cc);
  if (dc != NULL) blosc2_free_ctx(dc);
  return result;
}

int main(void) {
  blosc2_init();
  blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_context *plain = blosc2_create_cctx(cp);
  if (plain == NULL ||
      blosc2_set_prefilter_full_write(plain, true) != BLOSC2_ERROR_INVALID_PARAM ||
      blosc2_set_prefilter_full_write(plain, false) != 0) {
    if (plain != NULL) blosc2_free_ctx(plain);
    blosc2_destroy();
    return EXIT_FAILURE;
  }
  blosc2_free_ctx(plain);
  int result = test_full_write(0, 1) || test_full_write(5, 1) ||
               test_full_write(0, 2) || test_full_write(5, 2);
  blosc2_destroy();
  if (!result) printf("Prefilter full-write tests passed\n");
  return result;
}
