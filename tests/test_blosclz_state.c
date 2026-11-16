/*
 * Copyright (c) 2026 Blosc Development Team <blosc@blosc.org>
 * License: BSD 3-Clause (see LICENSE.txt).
 *
 * Cached hash scratch must preserve independent-block output across input,
 * level, table-size and offset changes, including unsuccessful calls.
 */
#include "test_common.h"
#include "blosclz.h"

static int compare_state(blosclz_state *state, int level,
                         const uint8_t *src, int size, int capacity) {
  uint8_t *reference = malloc((size_t)capacity);
  uint8_t *cached = malloc((size_t)capacity);
  uint8_t *decoded = malloc((size_t)size);
  int result = EXIT_FAILURE;
  if (!reference || !cached || !decoded) {
    goto cleanup;
  }
  int expected = blosclz_compress(level, src, size, reference, capacity, NULL);
  int actual = blosclz_compress_state(level, src, size, cached, capacity, state);
  if (actual != expected || (actual > 0 && memcmp(reference, cached, (size_t)actual) != 0)) {
    printf("Cached output differs: level=%d size=%d capacity=%d\n", level, size, capacity);
    goto cleanup;
  }
  if (actual > 0 && (blosclz_decompress(cached, actual, decoded, size) != size ||
                     memcmp(src, decoded, (size_t)size) != 0)) {
    printf("Cached output does not roundtrip\n");
    goto cleanup;
  }
  result = EXIT_SUCCESS;
cleanup:
  free(reference);
  free(cached);
  free(decoded);
  return result;
}

static int test_state_reuse(void) {
  blosclz_state *state = calloc(1, sizeof(*state));
  const int sizes[] = {16, 31, 63, 64, 65, 127, 1024, 16000, 65536, 131073};
  const int levels[] = {1, 4, 2, 9, 3, 7, 5, 6, 8};
  int result = EXIT_FAILURE;
  if (!state) {
    goto cleanup;
  }
  for (int pattern = 0; pattern < 5; pattern++) {
    for (size_t s = 0; s < ARRAY_SIZE(sizes); s++) {
      int size = sizes[s];
      /* Fresh unaligned input with no readable suffix padding. */
      uint8_t *allocation = malloc((size_t)size + 1);
      if (!allocation) {
        goto cleanup;
      }
      uint8_t *src = allocation + 1;
      uint32_t random = 42 + (uint32_t)s;
      for (int i = 0; i < size; i++) {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        switch (pattern) {
          case 0: src[i] = (uint8_t)(i % 7); break;
          case 1: src[i] = (uint8_t)random; break;
          case 2: src[i] = (uint8_t)(i < 27 ? i : (i < 335 ? 0 : 129)); break;
          case 3: src[i] = (uint8_t)(random & 3); break;
          default: src[i] = (uint8_t)((i / 257) & 255); break;
        }
      }
      for (size_t l = 0; l < ARRAY_SIZE(levels); l++) {
        if (compare_state(state, levels[l], src, size, size + size / 20 + 66) ||
            compare_state(state, levels[l], src, size, 66)) {
          free(allocation);
          goto cleanup;
        }
      }
      free(allocation);
    }
  }
  /* Force wrap first during a compression pass, then during a probe.  Old
   * positions outside the active smaller table must be cleared as well. */
  uint8_t src[16000];
  memset(src, 37, sizeof(src));
  state->base = UINT32_MAX - 8000;
  state->tagged = 1;
  state->htab[16383] = state->base - 1;
  if (compare_state(state, 4, src, sizeof(src), sizeof(src)) ||
      state->base != 16000 || state->htab[16383] != 0) {
    printf("Hash offset did not wrap safely between probe and compression\n");
    goto cleanup;
  }
  state->base = UINT32_MAX - 1;
  state->tagged = 1;
  if (compare_state(state, 9, src, sizeof(src), sizeof(src)) || state->base != 32000) {
    printf("Hash offset did not wrap safely before probing\n");
    goto cleanup;
  }
  /* A large plain-reset pass followed by a tagged pass must not expose its
   * untagged positions as references in the next buffer. */
  state->base = 0;
  state->tagged = 0;
  memset(state->htab, 0, sizeof(state->htab));
  if (compare_state(state, 1, src, sizeof(src), sizeof(src)) ||
      compare_state(state, 9, src, sizeof(src), sizeof(src))) {
    goto cleanup;
  }
  /* The adaptive reset hint must select both kernels without reusing an old
   * entropy decision or changing bytes. */
  state->tagged = 0;
  if (compare_state(state, 9, src, sizeof(src), sizeof(src)) || !state->tagged) {
    printf("Compressible data did not enable tagged resets\n");
    goto cleanup;
  }
  uint32_t random = 42;
  for (size_t i = 0; i < sizeof(src); i++) {
    random ^= random << 13;
    random ^= random >> 17;
    random ^= random << 5;
    src[i] = (uint8_t)random;
  }
  if (compare_state(state, 9, src, sizeof(src), sizeof(src)) || state->tagged ||
      compare_state(state, 9, src, sizeof(src), sizeof(src)) || state->tagged) {
    printf("Noisy data did not return to ordinary resets\n");
    goto cleanup;
  }
  result = EXIT_SUCCESS;
cleanup:
  free(state);
  return result;
}

static int test_worker_reuse(void) {
  const int size = 262147;
  uint8_t *src = malloc((size_t)size);
  uint8_t *compressed = malloc((size_t)size + BLOSC2_MAX_OVERHEAD);
  uint8_t *decoded = malloc((size_t)size);
  blosc2_context *cctx = NULL, *dctx = NULL;
  int result = EXIT_FAILURE;
  if (!src || !compressed || !decoded) {
    goto cleanup;
  }
  for (int pass = 0; pass < 27; pass++) {
    blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
    cp.compcode = pass % 3 == 1 ? BLOSC_LZ4 : BLOSC_BLOSCLZ;
    cp.clevel = 1 + pass % 9;
    cp.nthreads = pass % 2 ? 4 : 1;
    cp.typesize = 4;
    cp.blocksize = 65536;
    cp.splitmode = pass % 2 ? BLOSC_ALWAYS_SPLIT : BLOSC_NEVER_SPLIT;
    cctx = blosc2_create_cctx(cp);
    blosc2_dparams dp = BLOSC2_DPARAMS_DEFAULTS;
    dp.nthreads = cp.nthreads;
    dctx = blosc2_create_dctx(dp);
    if (!cctx || !dctx) {
      goto cleanup;
    }
    for (int change = 0; change < 3; change++) {
      for (int i = 0; i < size; i++) {
        src[i] = (uint8_t)((i / (1 + change * 127) + pass) % (7 + pass));
      }
      int cbytes = blosc2_compress_ctx(cctx, src, size, compressed, size + BLOSC2_MAX_OVERHEAD);
      if (cbytes <= 0 || blosc2_decompress_ctx(dctx, compressed, cbytes, decoded, size) != size ||
          memcmp(src, decoded, (size_t)size) != 0) {
        printf("Worker state roundtrip failed: pass=%d change=%d\n", pass, change);
        goto cleanup;
      }
    }
    blosc2_free_ctx(cctx);
    blosc2_free_ctx(dctx);
    cctx = dctx = NULL;
  }
  result = EXIT_SUCCESS;
cleanup:
  if (cctx) blosc2_free_ctx(cctx);
  if (dctx) blosc2_free_ctx(dctx);
  free(src);
  free(compressed);
  free(decoded);
  return result;
}

int main(void) {
  blosc2_init();
  int result = test_state_reuse() || test_worker_reuse();
  blosc2_destroy();
  if (!result) {
    printf("BloscLZ worker state tests passed\n");
  }
  return result;
}
