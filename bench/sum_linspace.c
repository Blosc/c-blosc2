/*
 * Copyright (c) 2026 Blosc Development Team <blosc@blosc.org>
 * License: BSD 3-Clause (see LICENSE.txt)
 *
 * Pure C equivalent of:
 *   a = blosc2.linspace(0, 1, 10**9)   # float64, materialized/compressed
 *   a.sum()
 *
 * Build (miniexpr must already be built, including its artifact library):
 *   cmake --build ../miniexpr/build --target miniexpr miniexpr_artifact -j8
 *   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 *     -DBLOSC2_MINIEXPR_ROOT=../miniexpr
 *   cmake --build build --target sum_linspace -j8
 *   ./build/bench/sum_linspace [nitems [nthreads [repeats [loops [chunk_items [block_items]]]]]]
 *
 * Defaults: 1e9, 8, 7, 10, 2000000, 16000. Chunk/block sizes must be
 * multiples of miniexpr's 128-element pairwise leaf. The final chunk/block
 * may be short. No full-size uncompressed array is allocated.
 *
 * Creation includes allocation, miniexpr compilation, ramp generation and
 * compression. Sum timings include decompression, per-block miniexpr pairwise
 * reduction and ordered merging of leaves; one warmup is excluded. Like
 * IPython timeit, report mean/stdev of repeat averages (not the best sample).
 * Creation uses the library's b2nd_linspace() constructor. Configure
 * BLOSC2_MINIEXPR_ROOT to use miniexpr for creation as well as reduction;
 * otherwise the constructor uses its native C backend.
 */

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "blosc2.h"
#include "b2nd.h"
#include "blosc2/filters-registry.h"
#include "miniexpr_artifact.h"

#define LEAF ME_PAIRWISE_SUM_LEAF_ITEMS
#ifndef B2ND_RAMP_BACKEND
#define B2ND_RAMP_BACKEND "library configured"
#endif

typedef struct {
  int64_t nitems;
  double *leaves;
  size_t leaf_capacity;
  blosc2_context *scheduler;
  blosc2_context **decoders;
  int *errors;
  int nthreads;
  const uint8_t *source;
  int32_t source_bytes;
  int32_t source_nbytes;
  uint8_t *sink;
  int32_t sink_capacity;
  double tail[LEAF];
} bench_state;

static double wall_time(void) {
  blosc_timestamp_t now;
  static blosc_timestamp_t start;
  static int initialized;
  blosc_set_timestamp(&now);
  if (!initialized) {
    start = now;
    initialized = 1;
  }
  return blosc_elapsed_secs(start, now);
}

static int reduce_block(blosc2_prefilter_params *p) {
  bench_state *s = p->user_data;
  int32_t valid_bytes = s->source_nbytes - p->output_offset;
  if (valid_bytes <= 0) return 1;
  if (valid_bytes > p->output_size) valid_bytes = p->output_size;
  size_t n = (size_t)valid_bytes / sizeof(double);
  size_t offset = (size_t)p->output_offset / sizeof(double) / LEAF;
  size_t count = n / LEAF;
  int rc = blosc2_getitem_bytes_ctx(s->decoders[p->tid], s->source, s->source_bytes,
                                   p->output_offset, valid_bytes, p->output, p->output_size);
  if (rc != valid_bytes ||
      me_pairwise_sum_leaves(ME_FLOAT64, p->output, count * LEAF,
                            s->leaves + offset,
                            (s->leaf_capacity - offset) * sizeof(double)) != ME_ARTIFACT_SUCCESS) {
    s->errors[p->tid] = -1;
    return -1;
  }
  if (n % LEAF) memcpy(s->tail, (double *)p->output + count * LEAF, n % LEAF * sizeof(double));
  /* Public disposable-prefilter API: skip filtering/compressing this output.
   * Only thread-local decoded blocks and tiny ordered sum leaves survive. */
  return 1;
}

static int sum_array(blosc2_schunk *sc, bench_state *s, double *buffer, double *result) {
  me_pairwise_sum_state sum;
  if (me_pairwise_sum_init(&sum, ME_FLOAT64) != ME_ARTIFACT_SUCCESS) return -1;
  for (int64_t i = 0; i < sc->nchunks; ++i) {
    uint8_t *chunk = NULL;
    bool needs_free = false;
    int cbytes = blosc2_schunk_get_chunk(sc, i, &chunk, &needs_free);
    if (cbytes <= 0) return -1;
    int32_t bytes = 0, stored_bytes, blocksize;
    int rc = blosc2_cbuffer_sizes(chunk, &bytes, &stored_bytes, &blocksize);
    s->source = chunk;
    s->source_bytes = cbytes;
    int64_t remaining = s->nitems - i * (sc->chunksize / (int)sizeof(double));
    size_t valid = remaining < bytes / (int)sizeof(double) ? (size_t)remaining : (size_t)bytes / sizeof(double);
    s->source_nbytes = (int32_t)(valid * sizeof(double));
    memset(s->errors, 0, (size_t)s->nthreads * sizeof(int));
    /* Keep scheduling geometry fixed even for the tail. Blosc contexts can
     * retain a smaller tuned blocksize after compressing a short buffer. */
    if (rc >= 0) rc = blosc2_compress_ctx(s->scheduler, buffer, sc->chunksize,
                                        s->sink, s->sink_capacity);
    if (needs_free) free(chunk);
    if (rc <= 0) return -1;
    for (int t = 0; t < s->nthreads; ++t) if (s->errors[t]) return -1;
    size_t n = valid;
    size_t full = n / LEAF;
    if (me_pairwise_sum_append(&sum, s->leaves, full) != ME_ARTIFACT_SUCCESS ||
        me_pairwise_sum_update(&sum, s->tail, n % LEAF) != ME_ARTIFACT_SUCCESS) {
      return -1;
    }
  }
  return me_pairwise_sum_finish(&sum, result);
}

static int64_t parse_positive(const char *arg) {
  char *end;
  errno = 0;
  int64_t n = strtoll(arg, &end, 10);
  return errno || end == arg || *end || n <= 0 ? -1 : n;
}

int main(int argc, char **argv) {
  int64_t options[] = {1000000000, 8, 7, 10, 2000000, 16000};
  if (argc > 7) {
    fprintf(stderr, "Usage: %s [nitems [nthreads [repeats [loops [chunk_items [block_items]]]]]]\n", argv[0]);
    return EXIT_FAILURE;
  }
  for (int i = 1; i < argc; ++i) options[i - 1] = parse_positive(argv[i]);
  int64_t n = options[0], chunk_items = options[4], block_items = options[5];
  if (n <= 0 || options[1] <= 0 || options[1] > INT16_MAX ||
      options[2] <= 0 || options[2] > INT_MAX || options[3] <= 0 || options[3] > INT_MAX ||
      chunk_items <= 0 || chunk_items > (INT32_MAX - BLOSC2_MAX_OVERHEAD) / (int)sizeof(double) ||
      block_items <= 0 || block_items > chunk_items ||
      chunk_items % LEAF || block_items % LEAF || chunk_items % block_items) {
    fprintf(stderr, "Invalid arguments: positive sizes/counts required; chunk must be a multiple\n"
                    "of block, both multiples of %d, with chunk bytes fitting int32.\n", LEAF);
    return EXIT_FAILURE;
  }
  int threads = (int)options[1], repeats = (int)options[2], loops = (int)options[3];
  int32_t chunk_bytes = (int32_t)(chunk_items * sizeof(double));
  int status = EXIT_FAILURE;
  bench_state state = {.nitems = n};
  blosc2_schunk *sc = NULL;
  b2nd_array_t *array = NULL;
  b2nd_context_t *ctx = NULL;
  double *buffer = NULL, *samples = NULL;
  blosc2_init();
  double begin = wall_time();
  clock_t cpu_begin = clock();

  buffer = calloc((size_t)chunk_items, sizeof(double));
  state.leaf_capacity = (size_t)chunk_items / LEAF;
  state.leaves = malloc(state.leaf_capacity * sizeof(double));
  samples = malloc((size_t)repeats * sizeof(double));
  if (!buffer || !state.leaves || !samples) goto cleanup;

  blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
  cp.compcode = BLOSC_BLOSCLZ;
  cp.clevel = 1;
  cp.typesize = sizeof(double);
  cp.nthreads = (int16_t)threads;
  cp.blocksize = (int32_t)(block_items * sizeof(double));
  cp.splitmode = BLOSC_AUTO_SPLIT;
  memset(cp.filters, 0, sizeof(cp.filters));
  memset(cp.filters_meta, 0, sizeof(cp.filters_meta));
  cp.filters[0] = BLOSC_SHUFFLE;
  cp.filters[1] = BLOSC_FILTER_BYTEDELTA;
  cp.filters_meta[1] = sizeof(double);
  blosc2_dparams dp = BLOSC2_DPARAMS_DEFAULTS;
  dp.nthreads = (int16_t)threads;
  blosc2_storage storage = {.cparams = &cp, .dparams = &dp};
  int32_t chunks[] = {(int32_t)chunk_items}, blocks[] = {(int32_t)block_items};
  ctx = b2nd_create_ctx(&storage, 1, &n, chunks, blocks, "=f8", DTYPE_NUMPY_FORMAT, NULL, 0);
  if (!ctx || b2nd_linspace(ctx, &array, 0.0, 1.0, true) < 0) goto cleanup;
  sc = array->sc;
  double creation = wall_time() - begin;
  double creation_cpu = (double)(clock() - cpu_begin) / CLOCKS_PER_SEC;
  printf("Blosc2 %s; miniexpr %s; float64; n=%" PRId64 "; threads=%d\n",
         BLOSC2_VERSION_STRING, ME_VERSION_STRING, n, threads);
  printf("BLOSCLZ clevel=1, shuffle+bytedelta; chunk=%" PRId64 ", block=%" PRId64 " items\n",
         chunk_items, block_items);
  printf("Storage: %.3f MB -> %.3f MB (%.1fx)\n",
         sc->nbytes / 1e6, sc->cbytes / 1e6, (double)sc->nbytes / sc->cbytes);
  printf("b2nd_linspace (%s): %.3f ms wall; %.3f ms CPU\n",
         B2ND_RAMP_BACKEND, creation * 1e3, creation_cpu * 1e3);

  /* Use Blosc's worker pool to schedule block-local decode+reduce, just like
   * Python's identity-map sum fast path. This is not a second stored array. */
  state.nthreads = threads;
  state.decoders = calloc((size_t)threads, sizeof(*state.decoders));
  state.errors = calloc((size_t)threads, sizeof(*state.errors));
  state.sink_capacity = chunk_bytes + BLOSC2_MAX_OVERHEAD;
  state.sink = malloc((size_t)state.sink_capacity);
  if (!state.decoders || !state.errors || !state.sink) goto cleanup;
  dp.nthreads = 1;
  for (int t = 0; t < threads; ++t) {
    state.decoders[t] = blosc2_create_dctx(dp);
    if (!state.decoders[t]) goto cleanup;
  }
  blosc2_prefilter_params reduce_pre = {.user_data = &state, .output_is_disposable = true};
  cp.prefilter = reduce_block;
  cp.preparams = &reduce_pre;
  state.scheduler = blosc2_create_cctx(cp);
  if (!state.scheduler || blosc2_set_prefilter_full_write(state.scheduler, true) < 0) goto cleanup;

  double result, expected = n == 1 ? 0.0 : (double)n / 2;
  /* Validate endpoints independently of sum; deliberately outside the timings. */
  double first, last;
  if (blosc2_schunk_decompress_chunk(sc, 0, buffer, chunk_bytes) < 0) goto cleanup;
  first = buffer[0];
  int last_bytes = blosc2_schunk_decompress_chunk(sc, sc->nchunks - 1, buffer, chunk_bytes);
  if (last_bytes <= 0) goto cleanup;
  last = buffer[(n - 1) % chunk_items];
  if (first != 0 || fabs(last - (n == 1 ? 0 : 1)) > 2e-15) {
    fprintf(stderr, "Invalid endpoints: %.17g, %.17g\n", first, last);
    goto cleanup;
  }
  if (sum_array(sc, &state, buffer, &result)) goto cleanup;
  for (int r = 0; r < repeats; ++r) {
    begin = wall_time();
    for (int l = 0; l < loops; ++l) {
      if (sum_array(sc, &state, buffer, &result)) goto cleanup;
      if (!isfinite(result) || fabs(result - expected) > 1e-12 * fmax(1, expected)) {
        fprintf(stderr, "Incorrect sum: %.17g (expected %.17g)\n", result, expected);
        goto cleanup;
      }
    }
    samples[r] = (wall_time() - begin) / loops;
  }
  double mean = 0, variance = 0, best = samples[0];
  for (int r = 0; r < repeats; ++r) {
    mean += samples[r] / repeats;
    if (samples[r] < best) best = samples[r];
  }
  for (int r = 0; r < repeats; ++r) variance += (samples[r] - mean) * (samples[r] - mean) / repeats;
  printf("sum = %.17g (expected %.17g)\n", result, expected);
  printf("sum: %.3f ms +/- %.3f ms (%d repeats, %d loops; best %.3f ms; %.1f GB/s)\n",
         mean * 1e3, sqrt(variance) * 1e3, repeats, loops, best * 1e3, sc->nbytes / mean / 1e9);
  status = EXIT_SUCCESS;

cleanup:
  if (status != EXIT_SUCCESS) fprintf(stderr, "Benchmark failed.\n");
  if (array) b2nd_free(array);
  if (ctx) b2nd_free_ctx(ctx);
  if (state.scheduler) blosc2_free_ctx(state.scheduler);
  if (state.decoders) {
    for (int t = 0; t < threads; ++t) {
      if (state.decoders[t]) blosc2_free_ctx(state.decoders[t]);
    }
  }
  free(state.decoders);
  free(state.errors);
  free(state.sink);
  free(state.leaves);
  free(buffer);
  free(samples);
  blosc2_destroy();
  return status;
}
