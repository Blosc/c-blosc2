/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Tests for the shared managed thread-pool backend.

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdio.h>
#include <string.h>

#include "test_common.h"
#include "../blosc/context.h"

#define CHUNKSIZE  (64 * 1024)
#define TYPESIZE   8

int tests_run = 0;

static char *test_roundtrip_shuffle_multithreaded(void);
static char *test_roundtrip_delta_multithreaded(void);
static char *test_roundtrip_bitshuffle_multithreaded(void);

static char *roundtrip_with_filter(int16_t nthreads, uint8_t filter) {
  static int64_t data[CHUNKSIZE / TYPESIZE];
  static int64_t dest[CHUNKSIZE / TYPESIZE];
  static uint8_t cdata[CHUNKSIZE * 2];
  int32_t isize = (int32_t)sizeof(data);

  for (int i = 0; i < (int)ARRAY_SIZE(data); ++i) {
    data[i] = i;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = nthreads;
  cparams.typesize = TYPESIZE;
  cparams.clevel = 5;
  memset(cparams.filters, BLOSC_NOFILTER, BLOSC2_MAX_FILTERS);
  memset(cparams.filters_meta, 0, BLOSC2_MAX_FILTERS);
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = filter;

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = nthreads;

  blosc2_context *cctx = blosc2_create_cctx(cparams);
  mu_assert("create_cctx failed", cctx != NULL);
  int cbytes = blosc2_compress_ctx(cctx, data, isize, cdata, (int32_t)sizeof(cdata));
  blosc2_free_ctx(cctx);
  mu_assert("compress_ctx failed", cbytes > 0);

  blosc2_context *dctx = blosc2_create_dctx(dparams);
  mu_assert("create_dctx failed", dctx != NULL);
  int dbytes = blosc2_decompress_ctx(dctx, cdata, cbytes, dest, isize);
  blosc2_free_ctx(dctx);
  mu_assert("decompress_ctx failed", dbytes == isize);
  mu_assert("roundtrip mismatch", memcmp(data, dest, (size_t)isize) == 0);

  return NULL;
}

static char *test_nthreads1_no_shared_pool(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 1;
  cparams.typesize = TYPESIZE;

  blosc2_context *cctx = blosc2_create_cctx(cparams);
  mu_assert("create_cctx failed", cctx != NULL);
  mu_assert("serial context should stay serial", cctx->thread_backend == BLOSC_BACKEND_SERIAL);
  mu_assert("serial context should not have shared pool", cctx->thread_pool == NULL);
  blosc2_free_ctx(cctx);

  return NULL;
}

static char *test_same_nthreads_share_pool(void) {
  static int64_t data[CHUNKSIZE / TYPESIZE];
  static uint8_t cbuf1[CHUNKSIZE * 2];
  static uint8_t cbuf2[CHUNKSIZE * 2];

  for (int i = 0; i < (int)ARRAY_SIZE(data); ++i) {
    data[i] = i;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 4;
  cparams.typesize = TYPESIZE;

  blosc2_context *ctx1 = blosc2_create_cctx(cparams);
  blosc2_context *ctx2 = blosc2_create_cctx(cparams);
  mu_assert("create ctx1 failed", ctx1 != NULL);
  mu_assert("create ctx2 failed", ctx2 != NULL);

  mu_assert("ctx1 compress failed",
            blosc2_compress_ctx(ctx1, data, (int32_t)sizeof(data), cbuf1, (int32_t)sizeof(cbuf1)) > 0);
  mu_assert("ctx2 compress failed",
            blosc2_compress_ctx(ctx2, data, (int32_t)sizeof(data), cbuf2, (int32_t)sizeof(cbuf2)) > 0);
  mu_assert("ctx1 should use shared pool", ctx1->thread_backend == BLOSC_BACKEND_SHARED_POOL);
  mu_assert("ctx2 should use shared pool", ctx2->thread_backend == BLOSC_BACKEND_SHARED_POOL);
  mu_assert("same nthreads contexts should share pool", ctx1->thread_pool == ctx2->thread_pool);
  blosc2_free_ctx(ctx1);
  blosc2_free_ctx(ctx2);
  return NULL;
}

static char *test_different_nthreads_different_pools(void) {
  static int64_t data[CHUNKSIZE / TYPESIZE];
  static uint8_t cbuf2[CHUNKSIZE * 2];
  static uint8_t cbuf4[CHUNKSIZE * 2];

  for (int i = 0; i < (int)ARRAY_SIZE(data); ++i) {
    data[i] = i;
  }

  blosc2_cparams cp2 = BLOSC2_CPARAMS_DEFAULTS;
  cp2.nthreads = 2;
  cp2.typesize = TYPESIZE;

  blosc2_cparams cp4 = BLOSC2_CPARAMS_DEFAULTS;
  cp4.nthreads = 4;
  cp4.typesize = TYPESIZE;

  blosc2_context *ctx2 = blosc2_create_cctx(cp2);
  blosc2_context *ctx4 = blosc2_create_cctx(cp4);
  mu_assert("create ctx2 failed", ctx2 != NULL);
  mu_assert("create ctx4 failed", ctx4 != NULL);

  mu_assert("ctx2 compress failed",
            blosc2_compress_ctx(ctx2, data, (int32_t)sizeof(data), cbuf2, (int32_t)sizeof(cbuf2)) > 0);
  mu_assert("ctx4 compress failed",
            blosc2_compress_ctx(ctx4, data, (int32_t)sizeof(data), cbuf4, (int32_t)sizeof(cbuf4)) > 0);
  mu_assert("different nthreads should not share pool", ctx2->thread_pool != ctx4->thread_pool);

  blosc2_free_ctx(ctx2);
  blosc2_free_ctx(ctx4);
  return NULL;
}

static char *test_dynamic_nthreads_rebind(void) {
  static int64_t data[CHUNKSIZE / TYPESIZE];
  static uint8_t cbuf[CHUNKSIZE * 2];

  for (int i = 0; i < (int)ARRAY_SIZE(data); ++i) {
    data[i] = i;
  }

  blosc2_cparams cp2 = BLOSC2_CPARAMS_DEFAULTS;
  cp2.nthreads = 2;
  cp2.typesize = TYPESIZE;

  blosc2_cparams cp4 = BLOSC2_CPARAMS_DEFAULTS;
  cp4.nthreads = 4;
  cp4.typesize = TYPESIZE;

  blosc2_context *anchor = blosc2_create_cctx(cp2);
  blosc2_context *ctx = blosc2_create_cctx(cp4);
  mu_assert("create anchor failed", anchor != NULL);
  mu_assert("create ctx failed", ctx != NULL);

  mu_assert("anchor compress failed",
            blosc2_compress_ctx(anchor, data, (int32_t)sizeof(data), cbuf, (int32_t)sizeof(cbuf)) > 0);
  mu_assert("ctx compress failed",
            blosc2_compress_ctx(ctx, data, (int32_t)sizeof(data), cbuf, (int32_t)sizeof(cbuf)) > 0);

  struct blosc_shared_pool *pool2 = anchor->thread_pool;
  mu_assert("expected distinct pools before rebind", ctx->thread_pool != pool2);

  ctx->new_nthreads = 2;
  mu_assert("ctx compress after rebind failed",
            blosc2_compress_ctx(ctx, data, (int32_t)sizeof(data), cbuf, (int32_t)sizeof(cbuf)) > 0);
  mu_assert("rebound context should attach to 2-thread pool", ctx->thread_pool == pool2);

  blosc2_free_ctx(ctx);
  blosc2_free_ctx(anchor);
  return NULL;
}

static char *test_live_context_survives_destroy(void) {
  static int64_t data[CHUNKSIZE / TYPESIZE];
  static int64_t dest[CHUNKSIZE / TYPESIZE];
  static uint8_t cdata[CHUNKSIZE * 2];
  int32_t isize = (int32_t)sizeof(data);

  for (int i = 0; i < (int)ARRAY_SIZE(data); ++i) {
    data[i] = i;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 4;
  cparams.typesize = TYPESIZE;

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 4;

  blosc2_context *cctx = blosc2_create_cctx(cparams);
  blosc2_context *dctx = blosc2_create_dctx(dparams);
  mu_assert("create cctx failed", cctx != NULL);
  mu_assert("create dctx failed", dctx != NULL);

  int cbytes = blosc2_compress_ctx(cctx, data, isize, cdata, (int32_t)sizeof(cdata));
  mu_assert("initial compress failed", cbytes > 0);
  mu_assert("cctx should use shared pool", cctx->thread_pool != NULL);

  blosc2_destroy();

  cbytes = blosc2_compress_ctx(cctx, data, isize, cdata, (int32_t)sizeof(cdata));
  mu_assert("compress after blosc2_destroy failed", cbytes > 0);
  mu_assert("decompress after blosc2_destroy failed",
            blosc2_decompress_ctx(dctx, cdata, cbytes, dest, isize) == isize);
  mu_assert("post-destroy roundtrip mismatch", memcmp(data, dest, (size_t)isize) == 0);

  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  blosc2_init();

  return NULL;
}

static char *test_many_contexts_share_pool(void) {
  enum { N_CTX = 8 };
  static int64_t data[CHUNKSIZE / TYPESIZE];
  static uint8_t cbuf[N_CTX][CHUNKSIZE * 2];
  blosc2_context *ctxs[N_CTX];

  for (int i = 0; i < (int)ARRAY_SIZE(data); ++i) {
    data[i] = i;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 4;
  cparams.typesize = TYPESIZE;

  for (int i = 0; i < N_CTX; ++i) {
    ctxs[i] = blosc2_create_cctx(cparams);
    mu_assert("create_cctx failed", ctxs[i] != NULL);
    mu_assert("compress_ctx failed",
              blosc2_compress_ctx(ctxs[i], data, (int32_t)sizeof(data), cbuf[i], (int32_t)sizeof(cbuf[i])) > 0);
  }

  struct blosc_shared_pool *pool = ctxs[0]->thread_pool;
  mu_assert("expected shared pool", pool != NULL);
  for (int i = 1; i < N_CTX; ++i) {
    mu_assert("all contexts should share one pool", ctxs[i]->thread_pool == pool);
  }

  for (int i = 0; i < N_CTX; ++i) {
    blosc2_free_ctx(ctxs[i]);
  }

  return NULL;
}

static char *all_tests(void) {
#if defined(_WIN32)
  printf("Windows uses per-context worker threads instead of the shared-pool backend.\n");
  return EXIT_SUCCESS;
#endif

  mu_run_test(test_nthreads1_no_shared_pool);
  mu_run_test(test_same_nthreads_share_pool);
  mu_run_test(test_different_nthreads_different_pools);
  mu_run_test(test_dynamic_nthreads_rebind);
  mu_run_test(test_live_context_survives_destroy);
  mu_run_test(test_many_contexts_share_pool);
  mu_run_test(test_roundtrip_shuffle_multithreaded);
  mu_run_test(test_roundtrip_delta_multithreaded);
  mu_run_test(test_roundtrip_bitshuffle_multithreaded);
  return NULL;
}

static char *test_roundtrip_shuffle_multithreaded(void) {
  return roundtrip_with_filter(4, BLOSC_SHUFFLE);
}

static char *test_roundtrip_delta_multithreaded(void) {
  return roundtrip_with_filter(4, BLOSC_DELTA);
}

static char *test_roundtrip_bitshuffle_multithreaded(void) {
  return roundtrip_with_filter(4, BLOSC_BITSHUFFLE);
}

int main(void) {
  char *result;

  printf("Blosc version info: %s (%s)\n", BLOSC2_VERSION_STRING, BLOSC2_VERSION_DATE);
  install_blosc_callback_test();
  blosc2_init();

  result = all_tests();
  if (result != NULL) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED\n");
  }
  printf("\tTests run: %d\n", tests_run);

  blosc2_destroy();
  return result != NULL;
}
