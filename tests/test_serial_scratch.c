/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  The thread_context for serial work (nthreads == 1, getitem, single blocks)
  belongs to the calling OS thread and is lent to one context at a time, so
  contexts do not keep one each.  Checks that many contexts share it, that it
  serves contexts with different block sizes and typesizes, that a serial job
  started from a prefilter or postfilter of another one (the thread's one is
  busy then) still works, that a thread keeps at most 16 MB (larger blocks or
  codec state stay with their context, as before) and that threads use their
  own.  Run under
  AddressSanitizer/LeakSanitizer for the thread-exit and blosc2_destroy()
  cleanup.
*/

#include <stdio.h>
#include "context.h"
#include "config.h"
#include "test_common.h"

/* ZSTD keeps codec state in the thread_context; builds without it use LZ4 */
#if defined(HAVE_ZSTD)
#define CODEC_WITH_STATE BLOSC_ZSTD
#else
#define CODEC_WITH_STATE BLOSC_LZ4
#endif

#if !defined(_WIN32)
#include <pthread.h>
#endif

int tests_run = 0;

#define NITEMS (256 * 1024)


static void fill(int32_t *buf, int32_t nitems, int32_t seed) {
  uint32_t x = (uint32_t)seed * 2654435761u + 1;
  for (int32_t i = 0; i < nitems; i++) {
    x = x * 1103515245u + 12345u;
    buf[i] = (i / 7) + (int32_t)((x >> 16) & 0xff);  // compressible, not trivially
  }
}


/* Compress `nbytes` of `src` with the given block size, typesize, codec and
   filter; returns the compressed size */
static int compress_with(const void *src, int32_t nbytes, void *dest, int32_t typesize,
                         int32_t blocksize, uint8_t compcode, uint8_t filter) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  cparams.blocksize = blocksize;
  cparams.compcode = compcode;
  cparams.clevel = 5;
  cparams.nthreads = 1;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = filter;
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  int csize = blosc2_compress_ctx(cctx, src, nbytes, dest, nbytes + BLOSC2_MAX_OVERHEAD);
  blosc2_free_ctx(cctx);
  return csize;
}


static blosc2_context *serial_dctx(void) {
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  return blosc2_create_dctx(dparams);
}


/* Many contexts working serially on one thread do not keep a thread_context each */
static char *test_contexts_share(void) {
  int32_t nbytes = NITEMS * (int32_t)sizeof(int32_t);
  int32_t *src = malloc(nbytes);
  int32_t *out = malloc(nbytes);
  uint8_t *chunk = malloc(nbytes + BLOSC2_MAX_OVERHEAD);
  fill(src, NITEMS, 1);
  int csize = compress_with(src, nbytes, chunk, 4, 32 * 1024, CODEC_WITH_STATE, BLOSC_SHUFFLE);
  mu_assert("Cannot compress", csize > 0);

  blosc2_context *dctx[8];
  for (int i = 0; i < 8; i++) {
    dctx[i] = serial_dctx();
    memset(out, 0, nbytes);
    mu_assert("Cannot decompress", blosc2_decompress_ctx(dctx[i], chunk, csize, out, nbytes) == nbytes);
    mu_assert("Wrong data", memcmp(src, out, nbytes) == 0);
    // getitem and single blocks take the same thread_context
    int32_t items[100];
    mu_assert("Cannot getitem", blosc2_getitem_ctx(dctx[i], chunk, csize, 12345, 100, items, sizeof(items)) == sizeof(items));
    mu_assert("Wrong getitem data", memcmp(items, src + 12345, sizeof(items)) == 0);
  }
#if !defined(_WIN32)
  for (int i = 0; i < 8; i++) {
    mu_assert("A serial context must not keep its own thread_context", dctx[i]->serial_context == NULL);
  }
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 4;
  cparams.nthreads = 1;
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  mu_assert("Cannot compress", blosc2_compress_ctx(cctx, src, nbytes, chunk, nbytes + BLOSC2_MAX_OVERHEAD) > 0);
  mu_assert("Serial compression must not keep a thread_context either", cctx->serial_context == NULL);
  blosc2_free_ctx(cctx);
#endif
  for (int i = 0; i < 8; i++) {
    blosc2_free_ctx(dctx[i]);
  }
  free(src);
  free(out);
  free(chunk);
  return EXIT_SUCCESS;
}


/* One thread_context serves block sizes and typesizes in any order: its
   temporaries grow to the largest need, measured in bytes */
static char *test_block_and_type_sizes(void) {
  struct {
    int32_t typesize;
    int32_t blocksize;
    uint8_t compcode;
    uint8_t filter;
  } kinds[] = {
    {4, 16 * 1024, CODEC_WITH_STATE, BLOSC_SHUFFLE},
    {8, 256 * 1024, BLOSC_LZ4, BLOSC_BITSHUFFLE},
    {1, 16 * 1024, BLOSC_BLOSCLZ, BLOSC_NOFILTER},
    {16, 16 * 1024, CODEC_WITH_STATE, BLOSC_SHUFFLE},  // same block size, larger typesize
    {4, 64 * 1024, BLOSC_LZ4HC, BLOSC_DELTA},
  };
  int nkinds = (int)(sizeof(kinds) / sizeof(kinds[0]));
  int32_t nbytes = NITEMS * (int32_t)sizeof(int32_t);
  int32_t *src = malloc(nbytes);
  int32_t *out = malloc(nbytes);
  uint8_t *chunks[5];
  int csizes[5];
  for (int k = 0; k < nkinds; k++) {
    fill(src, NITEMS, 10 + k);
    chunks[k] = malloc(nbytes + BLOSC2_MAX_OVERHEAD);
    csizes[k] = compress_with(src, nbytes, chunks[k], kinds[k].typesize, kinds[k].blocksize,
                              kinds[k].compcode, kinds[k].filter);
    mu_assert("Cannot compress", csizes[k] > 0);
  }
  blosc2_context *dctx = serial_dctx();
  int order[] = {0, 3, 1, 2, 4, 2, 0, 3, 1, 4, 0};
  for (int i = 0; i < (int)(sizeof(order) / sizeof(order[0])); i++) {
    int k = order[i];
    fill(src, NITEMS, 10 + k);
    memset(out, 0, nbytes);
    mu_assert("Cannot decompress", blosc2_decompress_ctx(dctx, chunks[k], csizes[k], out, nbytes) == nbytes);
    mu_assert("Wrong data", memcmp(src, out, nbytes) == 0);
    // ... and with a context of its own each time
    blosc2_context *dctx2 = serial_dctx();
    memset(out, 0, nbytes);
    mu_assert("Cannot decompress", blosc2_decompress_ctx(dctx2, chunks[k], csizes[k], out, nbytes) == nbytes);
    mu_assert("Wrong data", memcmp(src, out, nbytes) == 0);
    blosc2_free_ctx(dctx2);
  }
  blosc2_free_ctx(dctx);
  for (int k = 0; k < nkinds; k++) {
    free(chunks[k]);
  }
  free(src);
  free(out);
  return EXIT_SUCCESS;
}


/* A postfilter (or prefilter) that decompresses another chunk serially on the
   same thread while the thread's thread_context is lent to the outer job */
typedef struct {
  const uint8_t *chunk;
  int csize;
  const int32_t *expected;
  blosc2_context *dctx;
  int nested_errors;
} nested_params;

static int nested_decompress(nested_params *np) {
  int32_t nbytes = NITEMS * (int32_t)sizeof(int32_t);
  int32_t *buf = malloc(nbytes);
  if (blosc2_decompress_ctx(np->dctx, np->chunk, np->csize, buf, nbytes) != nbytes ||
      memcmp(buf, np->expected, nbytes) != 0) {
    np->nested_errors++;
  }
  free(buf);
  return 0;
}

static int postfilter_nested(blosc2_postfilter_params *params) {
  nested_params *np = params->user_data;
  nested_decompress(np);
  memcpy(params->output, params->input, params->size);
  return 0;
}

static int prefilter_nested(blosc2_prefilter_params *params) {
  nested_params *np = params->user_data;
  nested_decompress(np);
  memcpy(params->output, params->input, params->output_size);
  return 0;
}

static char *test_nested(void) {
  int32_t nbytes = NITEMS * (int32_t)sizeof(int32_t);
  int32_t *outer = malloc(nbytes);
  int32_t *inner = malloc(nbytes);
  int32_t *out = malloc(nbytes);
  uint8_t *outer_chunk = malloc(nbytes + BLOSC2_MAX_OVERHEAD);
  uint8_t *inner_chunk = malloc(nbytes + BLOSC2_MAX_OVERHEAD);
  fill(outer, NITEMS, 21);
  fill(inner, NITEMS, 22);
  // Different block sizes, so a shared scratch would be resized under the outer job
  int outer_csize = compress_with(outer, nbytes, outer_chunk, 4, 32 * 1024, CODEC_WITH_STATE, BLOSC_SHUFFLE);
  int inner_csize = compress_with(inner, nbytes, inner_chunk, 4, 256 * 1024, BLOSC_LZ4, BLOSC_DELTA);
  mu_assert("Cannot compress", outer_csize > 0 && inner_csize > 0);
  nested_params np = {inner_chunk, inner_csize, inner, serial_dctx(), 0};

  // Postfilter
  blosc2_postfilter_params postparams = {0};
  postparams.user_data = &np;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  dparams.postfilter = postfilter_nested;
  dparams.postparams = &postparams;
  blosc2_context *dctx = blosc2_create_dctx(dparams);
  memset(out, 0, nbytes);
  mu_assert("Cannot decompress", blosc2_decompress_ctx(dctx, outer_chunk, outer_csize, out, nbytes) == nbytes);
  mu_assert("Wrong outer data", memcmp(out, outer, nbytes) == 0);
  mu_assert("Wrong nested data", np.nested_errors == 0);
#if !defined(_WIN32)
  mu_assert("The outer job takes the thread's thread_context", dctx->serial_context == NULL);
  mu_assert("A nested job uses one of its own", np.dctx->serial_context != NULL);
#endif
  blosc2_free_ctx(dctx);

  // Prefilter
  blosc2_prefilter_params preparams = {0};
  preparams.user_data = &np;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 4;
  cparams.blocksize = 32 * 1024;
  cparams.nthreads = 1;
  cparams.prefilter = prefilter_nested;
  cparams.preparams = &preparams;
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  int csize = blosc2_compress_ctx(cctx, outer, nbytes, outer_chunk, nbytes + BLOSC2_MAX_OVERHEAD);
  mu_assert("Cannot compress", csize > 0);
  mu_assert("Wrong nested data", np.nested_errors == 0);
  blosc2_free_ctx(cctx);
  dctx = serial_dctx();
  mu_assert("Cannot decompress", blosc2_decompress_ctx(dctx, outer_chunk, csize, out, nbytes) == nbytes);
  mu_assert("Wrong data", memcmp(out, outer, nbytes) == 0);
  blosc2_free_ctx(dctx);

  blosc2_free_ctx(np.dctx);
  free(outer);
  free(inner);
  free(out);
  free(outer_chunk);
  free(inner_chunk);
  return EXIT_SUCCESS;
}


/* Blocks too large to keep a thread_context for per thread use the context's
   own, which is rebuilt (smaller) when the block size changes */
static char *test_large_blocks(void) {
  int32_t blocksize = 9 * 1024 * 1024;  // 4 temporaries of it exceed what a thread keeps
  int32_t nbytes = 2 * blocksize;
  int32_t *src = malloc(nbytes);
  int32_t *out = malloc(nbytes);
  uint8_t *chunk = malloc(nbytes + BLOSC2_MAX_OVERHEAD);
  fill(src, nbytes / 4, 31);
  int csize = compress_with(src, nbytes, chunk, 4, blocksize, BLOSC_LZ4, BLOSC_SHUFFLE);
  mu_assert("Cannot compress", csize > 0);
  blosc2_context *dctx = serial_dctx();
  mu_assert("Cannot decompress", blosc2_decompress_ctx(dctx, chunk, csize, out, nbytes) == nbytes);
  mu_assert("Wrong data", memcmp(src, out, nbytes) == 0);
  mu_assert("Large blocks use the context's own thread_context", dctx->serial_context != NULL);
  csize = compress_with(src, nbytes, chunk, 4, 32 * 1024, BLOSC_LZ4, BLOSC_SHUFFLE);
  mu_assert("Cannot compress", csize > 0);
  memset(out, 0, nbytes);
  mu_assert("Cannot decompress", blosc2_decompress_ctx(dctx, chunk, csize, out, nbytes) == nbytes);
  mu_assert("Wrong data", memcmp(src, out, nbytes) == 0);
  mu_assert("The context's own thread_context follows the block size down",
            dctx->serial_context != NULL && dctx->serial_context->tmp_nbytes < 1024 * 1024);
  blosc2_free_ctx(dctx);
  free(src);
  free(out);
  free(chunk);
  return EXIT_SUCCESS;
}


/* A thread keeps at most 16 MB: when codec state (here ZSTD compression of 2 MB
   blocks at level 9) grows its thread_context beyond that, the context that
   needed it keeps it and frees it with itself */
static char *test_large_codec_state(void) {
#if defined(HAVE_ZSTD) && !defined(_WIN32)
  int32_t blocksize = 2 * 1024 * 1024;
  int32_t nbytes = 2 * blocksize;
  int32_t *src = malloc(nbytes);
  int32_t *out = malloc(nbytes);
  uint8_t *chunk = malloc(nbytes + BLOSC2_MAX_OVERHEAD);
  fill(src, nbytes / 4, 61);
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 4;
  cparams.blocksize = blocksize;
  cparams.compcode = BLOSC_ZSTD;
  cparams.clevel = 9;
  cparams.nthreads = 1;
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  int csize = blosc2_compress_ctx(cctx, src, nbytes, chunk, nbytes + BLOSC2_MAX_OVERHEAD);
  mu_assert("Cannot compress", csize > 0);
  mu_assert("A large compression state stays with its context", cctx->serial_context != NULL);
  // The thread starts a new one for others
  blosc2_context *dctx = serial_dctx();
  mu_assert("Cannot decompress", blosc2_decompress_ctx(dctx, chunk, csize, out, nbytes) == nbytes);
  mu_assert("Wrong data", memcmp(src, out, nbytes) == 0);
  mu_assert("Other contexts still use the thread's one", dctx->serial_context == NULL);
  // ... and the context goes on with its own
  struct thread_context *own = cctx->serial_context;
  csize = blosc2_compress_ctx(cctx, src, nbytes, chunk, nbytes + BLOSC2_MAX_OVERHEAD);
  mu_assert("Cannot compress", csize > 0 && cctx->serial_context == own);
  mu_assert("Cannot decompress", blosc2_decompress_ctx(dctx, chunk, csize, out, nbytes) == nbytes);
  mu_assert("Wrong data", memcmp(src, out, nbytes) == 0);
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  free(src);
  free(out);
  free(chunk);
#endif
  return EXIT_SUCCESS;
}


#if !defined(_WIN32)
typedef struct {
  const uint8_t *chunk;
  int csize;
  const int32_t *expected;
  int errors;
} thread_params;

static void *thread_decompress(void *arg) {
  thread_params *tp = arg;
  int32_t nbytes = NITEMS * (int32_t)sizeof(int32_t);
  int32_t *out = malloc(nbytes);
  for (int i = 0; i < 20; i++) {
    blosc2_context *dctx = serial_dctx();
    if (blosc2_decompress_ctx(dctx, tp->chunk, tp->csize, out, nbytes) != nbytes ||
        memcmp(out, tp->expected, nbytes) != 0 || dctx->serial_context != NULL) {
      tp->errors++;
    }
    blosc2_free_ctx(dctx);
  }
  free(out);
  return NULL;  // LeakSanitizer reports the thread's thread_context if the exit did not free it
}
#endif

/* Threads work serially at the same time, each with its own thread_context */
static char *test_threads(void) {
#if !defined(_WIN32)
  int32_t nbytes = NITEMS * (int32_t)sizeof(int32_t);
  int32_t *src = malloc(nbytes);
  uint8_t *chunk = malloc(nbytes + BLOSC2_MAX_OVERHEAD);
  fill(src, NITEMS, 41);
  int csize = compress_with(src, nbytes, chunk, 4, 64 * 1024, CODEC_WITH_STATE, BLOSC_SHUFFLE);
  mu_assert("Cannot compress", csize > 0);
  pthread_t threads[4];
  thread_params tp[4];
  for (int i = 0; i < 4; i++) {
    tp[i] = (thread_params){chunk, csize, src, 0};
    mu_assert("Cannot create a thread", pthread_create(&threads[i], NULL, thread_decompress, &tp[i]) == 0);
  }
  for (int i = 0; i < 4; i++) {
    mu_assert("Cannot join a thread", pthread_join(threads[i], NULL) == 0);
    mu_assert("Wrong data in a thread", tp[i].errors == 0);
  }
  free(src);
  free(chunk);
#endif
  return EXIT_SUCCESS;
}


/* blosc2_free_resources() drops the calling thread's thread_context; the next
   serial job makes a new one */
static char *test_free_resources(void) {
  int32_t nbytes = NITEMS * (int32_t)sizeof(int32_t);
  int32_t *src = malloc(nbytes);
  int32_t *out = malloc(nbytes);
  uint8_t *chunk = malloc(nbytes + BLOSC2_MAX_OVERHEAD);
  fill(src, NITEMS, 51);
  int csize = compress_with(src, nbytes, chunk, 4, 32 * 1024, CODEC_WITH_STATE, BLOSC_SHUFFLE);
  mu_assert("Cannot compress", csize > 0);
  blosc2_context *dctx = serial_dctx();
  for (int i = 0; i < 2; i++) {
    memset(out, 0, nbytes);
    mu_assert("Cannot decompress", blosc2_decompress_ctx(dctx, chunk, csize, out, nbytes) == nbytes);
    mu_assert("Wrong data", memcmp(src, out, nbytes) == 0);
    mu_assert("Cannot free resources", blosc2_free_resources() == 0);
  }
  blosc2_free_ctx(dctx);
  free(src);
  free(out);
  free(chunk);
  return EXIT_SUCCESS;
}


static char *all_tests(void) {
  mu_run_test(test_contexts_share);
  mu_run_test(test_block_and_type_sizes);
  mu_run_test(test_nested);
  mu_run_test(test_large_blocks);
  mu_run_test(test_large_codec_state);
  mu_run_test(test_threads);
  mu_run_test(test_free_resources);
  return EXIT_SUCCESS;
}


int main(void) {
  blosc2_init();
  char *result = all_tests();
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
