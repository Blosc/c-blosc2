/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Tests for the shared managed thread-pool paradigm.

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdio.h>
#include <string.h>
#include "test_common.h"
#include "../blosc/context.h"

#define CHUNKSIZE  (64 * 1024)   /* 64 KiB – large enough for multi-block */
#define TYPESIZE   8

/* Global vars */
int tests_run = 0;


/* Helper: compress then decompress a small buffer and verify round-trip */
static char *roundtrip(int16_t nthreads, uint8_t *filters, uint8_t *filters_meta,
                       int32_t typesize, int clevel)
{
  static int64_t data[CHUNKSIZE / TYPESIZE];
  static int64_t dest[CHUNKSIZE / TYPESIZE];
  static uint8_t cdata[CHUNKSIZE * 2];
  const int32_t isize = (int32_t)sizeof(data);

  for (int i = 0; i < (int)(sizeof(data) / sizeof(data[0])); i++) {
    data[i] = i;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads  = nthreads;
  cparams.typesize  = typesize;
  cparams.clevel    = clevel;
  memcpy(cparams.filters,      filters,      BLOSC2_MAX_FILTERS);
  memcpy(cparams.filters_meta, filters_meta, BLOSC2_MAX_FILTERS);

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = nthreads;

  blosc2_context *cctx = blosc2_create_cctx(cparams);
  mu_assert("create_cctx failed", cctx != NULL);

  int cbytes = blosc2_compress_ctx(cctx, data, isize, cdata, (int32_t)sizeof(cdata));
  mu_assert("compress_ctx failed", cbytes > 0);
  blosc2_free_ctx(cctx);

  blosc2_context *dctx = blosc2_create_dctx(dparams);
  mu_assert("create_dctx failed", dctx != NULL);

  int dbytes = blosc2_decompress_ctx(dctx, cdata, cbytes, dest, isize);
  mu_assert("decompress_ctx failed", dbytes == isize);
  blosc2_free_ctx(dctx);

  mu_assert("data mismatch", memcmp(data, dest, isize) == 0);
  return EXIT_SUCCESS;
}

/* Build a default filter/meta pair with a given filter slot */
static void make_filters(uint8_t *filters, uint8_t *filters_meta, uint8_t filter)
{
  memset(filters,      BLOSC_NOFILTER, BLOSC2_MAX_FILTERS);
  memset(filters_meta, 0,              BLOSC2_MAX_FILTERS);
  filters[BLOSC2_MAX_FILTERS - 1] = filter;
}


/* ------------------------------------------------------------------ */
/* Test 1: nthreads=1 never allocates a pool                          */
/* ------------------------------------------------------------------ */
static char *test_nthreads1_no_pool(void)
{
  uint8_t f[BLOSC2_MAX_FILTERS], fm[BLOSC2_MAX_FILTERS];
  make_filters(f, fm, BLOSC_SHUFFLE);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 1;
  cparams.typesize = TYPESIZE;
  memcpy(cparams.filters,      f,  BLOSC2_MAX_FILTERS);
  memcpy(cparams.filters_meta, fm, BLOSC2_MAX_FILTERS);

  static int64_t data[CHUNKSIZE / TYPESIZE];
  static uint8_t cbuf[CHUNKSIZE * 2];
  for (int i = 0; i < (int)(sizeof(data)/sizeof(data[0])); i++) data[i] = i;

  blosc2_context *cctx = blosc2_create_cctx(cparams);
  mu_assert("create_cctx failed", cctx != NULL);
  int r = blosc2_compress_ctx(cctx, data, (int32_t)sizeof(data), cbuf, (int32_t)sizeof(cbuf));
  mu_assert("compress failed", r > 0);
  mu_assert("nthreads=1 should have no pool", cctx->thread_pool == NULL);
  blosc2_free_ctx(cctx);

  return EXIT_SUCCESS;
}


/* ------------------------------------------------------------------ */
/* Test 2: contexts with the same nthreads share one pool             */
/* ------------------------------------------------------------------ */
static char *test_same_nthreads_share_pool(void)
{
  blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
  cp.nthreads = 4;

  static int64_t data[CHUNKSIZE / TYPESIZE];
  static uint8_t cbuf1[CHUNKSIZE * 2], cbuf2[CHUNKSIZE * 2];
  for (int i = 0; i < (int)(sizeof(data)/sizeof(data[0])); i++) data[i] = i;

  blosc2_context *ctx1 = blosc2_create_cctx(cp);
  blosc2_context *ctx2 = blosc2_create_cctx(cp);
  mu_assert("create_cctx1 failed", ctx1 != NULL);
  mu_assert("create_cctx2 failed", ctx2 != NULL);

  int cb1 = blosc2_compress_ctx(ctx1, data, (int32_t)sizeof(data), cbuf1, (int32_t)sizeof(cbuf1));
  int cb2 = blosc2_compress_ctx(ctx2, data, (int32_t)sizeof(data), cbuf2, (int32_t)sizeof(cbuf2));
  mu_assert("compress1 failed", cb1 > 0);
  mu_assert("compress2 failed", cb2 > 0);

  mu_assert("ctx1 should have a pool", ctx1->thread_pool != NULL);
  mu_assert("ctx2 should have a pool", ctx2->thread_pool != NULL);
  mu_assert("contexts should share the same pool",
            ctx1->thread_pool == ctx2->thread_pool);

  blosc2_free_ctx(ctx1);
  blosc2_free_ctx(ctx2);

  return EXIT_SUCCESS;
}


/* ------------------------------------------------------------------ */
/* Test 3: different nthreads → different pools                        */
/* ------------------------------------------------------------------ */
static char *test_different_nthreads_different_pools(void)
{
  static int64_t data[CHUNKSIZE / TYPESIZE];
  static uint8_t cb2[CHUNKSIZE * 2], cb4[CHUNKSIZE * 2];
  for (int i = 0; i < (int)(sizeof(data)/sizeof(data[0])); i++) data[i] = i;

  blosc2_cparams cp2 = BLOSC2_CPARAMS_DEFAULTS; cp2.nthreads = 2;
  blosc2_cparams cp4 = BLOSC2_CPARAMS_DEFAULTS; cp4.nthreads = 4;

  blosc2_context *ctx2 = blosc2_create_cctx(cp2);
  blosc2_context *ctx4 = blosc2_create_cctx(cp4);

  int r2 = blosc2_compress_ctx(ctx2, data, (int32_t)sizeof(data), cb2, (int32_t)sizeof(cb2));
  int r4 = blosc2_compress_ctx(ctx4, data, (int32_t)sizeof(data), cb4, (int32_t)sizeof(cb4));
  mu_assert("compress(2) failed", r2 > 0);
  mu_assert("compress(4) failed", r4 > 0);

  mu_assert("2-thread ctx should have a pool", ctx2->thread_pool != NULL);
  mu_assert("4-thread ctx should have a pool", ctx4->thread_pool != NULL);
  mu_assert("different nthreads must use different pools",
            ctx2->thread_pool != ctx4->thread_pool);

  blosc2_free_ctx(ctx2);
  blosc2_free_ctx(ctx4);

  return EXIT_SUCCESS;
}


/* ------------------------------------------------------------------ */
/* Test 4: dynamic nthreads change triggers pool rebind               */
/* ------------------------------------------------------------------ */
static char *test_dynamic_nthreads_rebind(void)
{
  static int64_t data[CHUNKSIZE / TYPESIZE];
  static uint8_t cbuf[CHUNKSIZE * 2];
  for (int i = 0; i < (int)(sizeof(data)/sizeof(data[0])); i++) data[i] = i;

  /* Create a 2-thread context to anchor the 2-thread pool */
  blosc2_cparams cp2 = BLOSC2_CPARAMS_DEFAULTS;
  cp2.nthreads = 2;
  blosc2_context *anchor = blosc2_create_cctx(cp2);
  int r = blosc2_compress_ctx(anchor, data, (int32_t)sizeof(data), cbuf, (int32_t)sizeof(cbuf));
  mu_assert("anchor compress failed", r > 0);
  struct blosc_shared_pool *pool2 = anchor->thread_pool;
  mu_assert("2-thread pool should exist", pool2 != NULL);

  /* Create a 4-thread context and verify it uses a different pool */
  blosc2_cparams cp4 = BLOSC2_CPARAMS_DEFAULTS;
  cp4.nthreads = 4;
  blosc2_context *ctx = blosc2_create_cctx(cp4);
  r = blosc2_compress_ctx(ctx, data, (int32_t)sizeof(data), cbuf, (int32_t)sizeof(cbuf));
  mu_assert("initial 4-thread compress failed", r > 0);
  mu_assert("4-thread pool should differ from 2-thread pool",
            ctx->thread_pool != pool2);

  /* Rebind the 4-thread context to 2 threads */
  ctx->new_nthreads = 2;
  r = blosc2_compress_ctx(ctx, data, (int32_t)sizeof(data), cbuf, (int32_t)sizeof(cbuf));
  mu_assert("post-rebind compress failed", r > 0);

  /* After rebind, ctx should share the anchor's 2-thread pool */
  mu_assert("ctx should join the existing 2-thread pool",
            ctx->thread_pool == pool2);

  blosc2_free_ctx(ctx);
  blosc2_free_ctx(anchor);

  return EXIT_SUCCESS;
}


/* ------------------------------------------------------------------ */
/* Test 5: round-trip with shuffle filter, multi-threaded             */
/* ------------------------------------------------------------------ */
static char *test_roundtrip_shuffle_multithreaded(void)
{
  uint8_t f[BLOSC2_MAX_FILTERS], fm[BLOSC2_MAX_FILTERS];
  make_filters(f, fm, BLOSC_SHUFFLE);
  return roundtrip(4, f, fm, TYPESIZE, 5);
}


/* ------------------------------------------------------------------ */
/* Test 6: round-trip with delta filter, multi-threaded               */
/* ------------------------------------------------------------------ */
static char *test_roundtrip_delta_multithreaded(void)
{
  uint8_t f[BLOSC2_MAX_FILTERS], fm[BLOSC2_MAX_FILTERS];
  make_filters(f, fm, BLOSC_DELTA);
  return roundtrip(4, f, fm, TYPESIZE, 5);
}


/* ------------------------------------------------------------------ */
/* Test 7: round-trip with bitshuffle filter, multi-threaded          */
/* ------------------------------------------------------------------ */
static char *test_roundtrip_bitshuffle_multithreaded(void)
{
  uint8_t f[BLOSC2_MAX_FILTERS], fm[BLOSC2_MAX_FILTERS];
  make_filters(f, fm, BLOSC_BITSHUFFLE);
  return roundtrip(4, f, fm, TYPESIZE, 5);
}


/* ------------------------------------------------------------------ */
/* Test 8: pool ref-count drops to 0 on last free (no crash)          */
/* ------------------------------------------------------------------ */
static char *test_pool_refcount_and_destroy(void)
{
  static int64_t data[CHUNKSIZE / TYPESIZE];
  static uint8_t cbuf[CHUNKSIZE * 2];
  for (int i = 0; i < (int)(sizeof(data)/sizeof(data[0])); i++) data[i] = i;

  blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
  cp.nthreads = 3;

  blosc2_context *ctx = blosc2_create_cctx(cp);
  int r = blosc2_compress_ctx(ctx, data, (int32_t)sizeof(data), cbuf, (int32_t)sizeof(cbuf));
  mu_assert("compress failed", r > 0);
  mu_assert("pool not acquired", ctx->thread_pool != NULL);

  blosc2_free_ctx(ctx);
  /* After free, the pool pointer is gone; test passes if no crash */

  return EXIT_SUCCESS;
}


/* ------------------------------------------------------------------ */
/* Test 9: compression + decompression with delta, nthreads=1         */
/* ------------------------------------------------------------------ */
static char *test_roundtrip_delta_serial(void)
{
  uint8_t f[BLOSC2_MAX_FILTERS], fm[BLOSC2_MAX_FILTERS];
  make_filters(f, fm, BLOSC_DELTA);
  return roundtrip(1, f, fm, TYPESIZE, 5);
}


/* ------------------------------------------------------------------ */
/* Test 10: many contexts share one pool                              */
/* ------------------------------------------------------------------ */
static char *test_many_contexts_share_pool(void)
{
#define N_CTX 8
  static int64_t data[CHUNKSIZE / TYPESIZE];
  static uint8_t cbuf[N_CTX][CHUNKSIZE * 2];
  for (int i = 0; i < (int)(sizeof(data)/sizeof(data[0])); i++) data[i] = i;

  blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
  cp.nthreads = 4;

  blosc2_context *ctxs[N_CTX];
  for (int i = 0; i < N_CTX; i++) {
    ctxs[i] = blosc2_create_cctx(cp);
    mu_assert("create_cctx failed", ctxs[i] != NULL);
    int r = blosc2_compress_ctx(ctxs[i], data, (int32_t)sizeof(data),
                                cbuf[i], (int32_t)sizeof(cbuf[i]));
    mu_assert("compress failed", r > 0);
  }

  /* All should share the same pool */
  struct blosc_shared_pool *pool = ctxs[0]->thread_pool;
  mu_assert("pool NULL", pool != NULL);
  for (int i = 1; i < N_CTX; i++) {
    mu_assert("all contexts should share pool",
              ctxs[i]->thread_pool == pool);
  }

  /* Free all contexts */
  for (int i = 0; i < N_CTX; i++) {
    blosc2_free_ctx(ctxs[i]);
  }
  /* All freed; pool is destroyed (no crash) */

  return EXIT_SUCCESS;
#undef N_CTX
}


static char *all_tests(void)
{
  printf("Blosc version info: %s (%s)\n",
         BLOSC2_VERSION_STRING, BLOSC2_VERSION_DATE);

  mu_run_test(test_nthreads1_no_pool);
  mu_run_test(test_same_nthreads_share_pool);
  mu_run_test(test_different_nthreads_different_pools);
  mu_run_test(test_dynamic_nthreads_rebind);
  mu_run_test(test_roundtrip_shuffle_multithreaded);
  mu_run_test(test_roundtrip_delta_multithreaded);
  mu_run_test(test_roundtrip_bitshuffle_multithreaded);
  mu_run_test(test_pool_refcount_and_destroy);
  mu_run_test(test_roundtrip_delta_serial);
  mu_run_test(test_many_contexts_share_pool);

  return EXIT_SUCCESS;
}


int main(int argc, char **argv)
{
  char *result;

  if (argc > 0) {
    printf("STARTING TESTS for %s", argv[0]);
  }

  install_blosc_callback_test();
  blosc2_init();

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
