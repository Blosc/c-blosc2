/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Focused tests for shared managed thread pools.

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "test_common.h"

#define NITEMS (300 * 1000)
#define NTHREADS_A 4
#define NTHREADS_B 3
#define NLOOPS 8

typedef struct {
  blosc2_context *cctx;
  blosc2_context *dctx;
  const int32_t *src;
  uint8_t *compressed;
  int32_t *decompressed;
  int compressed_cap;
  int src_nbytes;
  int rc;
} worker_state;

static void* roundtrip_worker(void *arg) {
  worker_state *state = (worker_state*)arg;

  for (int i = 0; i < NLOOPS; ++i) {
    int cbytes = blosc2_compress_ctx(state->cctx, state->src, state->src_nbytes,
                                     state->compressed, state->compressed_cap);
    if (cbytes <= 0) {
      state->rc = cbytes <= 0 ? cbytes : -1;
      return NULL;
    }

    int dbytes = blosc2_decompress_ctx(state->dctx, state->compressed, cbytes,
                                       state->decompressed, state->src_nbytes);
    if (dbytes != state->src_nbytes) {
      state->rc = dbytes;
      return NULL;
    }
    if (memcmp(state->src, state->decompressed, (size_t)state->src_nbytes) != 0) {
      state->rc = -1;
      return NULL;
    }
  }

  state->rc = 0;
  return NULL;
}

static int must_roundtrip(blosc2_context *cctx, blosc2_context *dctx, const int32_t *src,
                          int32_t *dest, uint8_t *compressed, int src_nbytes, int compressed_cap) {
  int cbytes = blosc2_compress_ctx(cctx, src, src_nbytes, compressed, compressed_cap);
  if (cbytes <= 0) {
    printf("Compression failed: %d\n", cbytes);
    return EXIT_FAILURE;
  }

  int dbytes = blosc2_decompress_ctx(dctx, compressed, cbytes, dest, src_nbytes);
  if (dbytes != src_nbytes) {
    printf("Decompression failed: %d\n", dbytes);
    return EXIT_FAILURE;
  }

  if (memcmp(src, dest, (size_t)src_nbytes) != 0) {
    for (int i = 0; i < src_nbytes / (int)sizeof(int32_t); ++i) {
      if (src[i] != dest[i]) {
        printf("Roundtrip mismatch at %d: %d != %d\n", i, src[i], dest[i]);
        break;
      }
    }
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int main(void) {
  int rc = EXIT_SUCCESS;
  int src_nbytes = NITEMS * (int)sizeof(int32_t);
  int compressed_cap = src_nbytes + BLOSC2_MAX_OVERHEAD;
  int32_t *src = malloc((size_t)src_nbytes);
  int32_t *dest_a = malloc((size_t)src_nbytes);
  int32_t *dest_b = malloc((size_t)src_nbytes);
  int32_t *dest_c = malloc((size_t)src_nbytes);
  uint8_t *compressed_a = malloc((size_t)compressed_cap);
  uint8_t *compressed_b = malloc((size_t)compressed_cap);
  uint8_t *compressed_c = malloc((size_t)compressed_cap);
  blosc2_context *cctx_a = NULL, *dctx_a = NULL, *cctx_b = NULL, *dctx_b = NULL, *cctx_c = NULL;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_pthread_t th_a, th_b;
  worker_state state_a = {0}, state_b = {0};

  if (src == NULL || dest_a == NULL || dest_b == NULL || dest_c == NULL ||
      compressed_a == NULL || compressed_b == NULL || compressed_c == NULL) {
    printf("Allocation failure.\n");
    rc = EXIT_FAILURE;
    goto cleanup;
  }

  for (int i = 0; i < NITEMS; ++i) {
    src[i] = i * 3 + 1;
  }

  cparams.typesize = sizeof(int32_t);
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS_A;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_SHUFFLE;
  dparams.nthreads = NTHREADS_A;

  cctx_a = blosc2_create_cctx(cparams);
  dctx_a = blosc2_create_dctx(dparams);
  cctx_b = blosc2_create_cctx(cparams);
  dctx_b = blosc2_create_dctx(dparams);

  cparams.nthreads = NTHREADS_B;
  cctx_c = blosc2_create_cctx(cparams);

  if (cctx_a == NULL || dctx_a == NULL || cctx_b == NULL || dctx_b == NULL || cctx_c == NULL) {
    printf("Context creation failed.\n");
    rc = EXIT_FAILURE;
    goto cleanup;
  }

  if (must_roundtrip(cctx_a, dctx_a, src, dest_a, compressed_a, src_nbytes, compressed_cap) != EXIT_SUCCESS) {
    rc = EXIT_FAILURE;
    goto cleanup;
  }

  if (must_roundtrip(cctx_b, dctx_b, src, dest_b, compressed_b, src_nbytes, compressed_cap) != EXIT_SUCCESS) {
    rc = EXIT_FAILURE;
    goto cleanup;
  }

  if (blosc2_compress_ctx(cctx_c, src, src_nbytes, compressed_c, compressed_cap) <= 0) {
    printf("Attach for alternate thread count failed.\n");
    rc = EXIT_FAILURE;
    goto cleanup;
  }

#ifndef _WIN32
  if (cctx_a->thread_pool == NULL || cctx_b->thread_pool == NULL || dctx_a->thread_pool == NULL || dctx_b->thread_pool == NULL) {
    printf("Shared pools were not attached.\n");
    rc = EXIT_FAILURE;
    goto cleanup;
  }
  if (cctx_a->thread_pool != cctx_b->thread_pool || dctx_a->thread_pool != dctx_b->thread_pool) {
    printf("Contexts with identical nthreads did not share pools.\n");
    rc = EXIT_FAILURE;
    goto cleanup;
  }
  if (cctx_c->thread_pool == NULL || cctx_c->thread_pool == cctx_a->thread_pool) {
    printf("Contexts with different nthreads did not get separate pools.\n");
    rc = EXIT_FAILURE;
    goto cleanup;
  }
#endif  /* !_WIN32 */

  blosc2_init();
  blosc2_destroy();

  if (must_roundtrip(cctx_a, dctx_a, src, dest_a, compressed_a, src_nbytes, compressed_cap) != EXIT_SUCCESS) {
    printf("Live context failed after blosc2_destroy().\n");
    rc = EXIT_FAILURE;
    goto cleanup;
  }

  state_a.cctx = cctx_a;
  state_a.dctx = dctx_a;
  state_a.src = src;
  state_a.compressed = compressed_a;
  state_a.decompressed = dest_a;
  state_a.compressed_cap = compressed_cap;
  state_a.src_nbytes = src_nbytes;

  state_b.cctx = cctx_b;
  state_b.dctx = dctx_b;
  state_b.src = src;
  state_b.compressed = compressed_b;
  state_b.decompressed = dest_b;
  state_b.compressed_cap = compressed_cap;
  state_b.src_nbytes = src_nbytes;

  if (blosc2_pthread_create(&th_a, NULL, roundtrip_worker, &state_a) != 0 ||
      blosc2_pthread_create(&th_b, NULL, roundtrip_worker, &state_b) != 0) {
    printf("Thread creation failed.\n");
    rc = EXIT_FAILURE;
    goto cleanup;
  }

  blosc2_pthread_join(th_a, NULL);
  blosc2_pthread_join(th_b, NULL);

  if (state_a.rc != 0 || state_b.rc != 0) {
    printf("Concurrent roundtrip failed: %d %d\n", state_a.rc, state_b.rc);
    rc = EXIT_FAILURE;
  }

cleanup:
  if (cctx_a != NULL) blosc2_free_ctx(cctx_a);
  if (dctx_a != NULL) blosc2_free_ctx(dctx_a);
  if (cctx_b != NULL) blosc2_free_ctx(cctx_b);
  if (dctx_b != NULL) blosc2_free_ctx(dctx_b);
  if (cctx_c != NULL) blosc2_free_ctx(cctx_c);
  free(src);
  free(dest_a);
  free(dest_b);
  free(dest_c);
  free(compressed_a);
  free(compressed_b);
  free(compressed_c);
  return rc;
}
