/*
 * Copyright (c) 2026 Blosc Development Team <blosc@blosc.org>
 * License: BSD 3-Clause (see LICENSE.txt)
 */
#include "b2nd.h"
#include "blosc2/filters-registry.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
  fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

typedef struct {
  int8_t ndim;
  int64_t shape[4];
  int32_t chunks[4], blocks[4];
} geometry;

static const geometry geometries[] = {
    {0, {0}, {0}, {0}},
    {1, {0}, {0}, {0}},
    {2, {3, 0}, {2, 0}, {1, 0}},
    {1, {1}, {7}, {3}},
    {1, {129}, {64}, {16}},
    {1, {23}, {7}, {3}},
    {2, {20, 10}, {7, 5}, {3, 2}},
    {2, {1, 3}, {4, 7}, {3, 4}},
    {3, {5, 7, 9}, {3, 5, 4}, {2, 3, 3}},
    {4, {3, 2, 5, 7}, {2, 3, 3, 4}, {3, 2, 2, 3}},
};

static double read_value(const uint8_t *p, int size, bool swap) {
  uint8_t raw[8];
  for (int i = 0; i < size; ++i) raw[i] = p[swap ? size - 1 - i : i];
  if (size == 4) { float x; memcpy(&x, raw, 4); return x; }
  double x; memcpy(&x, raw, 8); return x;
}

static int check_padding(b2nd_array_t *a) {
  int size = a->sc->typesize;
  uint8_t *raw = malloc((size_t)a->sc->chunksize);
  CHECK(raw != NULL || a->nitems == 0);
  for (int64_t c = 0; c < a->sc->nchunks; ++c) {
    CHECK(blosc2_schunk_decompress_chunk(a->sc, c, raw, a->sc->chunksize) == a->sc->chunksize);
    for (int64_t i = 0; i < a->extchunknitems; ++i) {
      int64_t chunk = c, block = i / a->blocknitems, lane = i % a->blocknitems;
      bool padded = false;
      for (int d = a->ndim - 1; d >= 0; --d) {
        int64_t relative = (block % (a->extchunkshape[d] / a->blockshape[d])) * a->blockshape[d]
                           + lane % a->blockshape[d];
        int64_t coord = (chunk % (a->extshape[d] / a->chunkshape[d])) * a->chunkshape[d] + relative;
        padded |= relative >= a->chunkshape[d] || coord >= a->shape[d];
        block /= a->extchunkshape[d] / a->blockshape[d];
        chunk /= a->extshape[d] / a->chunkshape[d];
        lane /= a->blockshape[d];
      }
      if (padded) for (int b = 0; b < size; ++b) CHECK(raw[i * size + b] == 0);
    }
  }
  free(raw);
  return 0;
}

static int check_case(const geometry *g, int size, bool swap, int backend, int threads) {
  const char *path = "test_b2nd_constructors.b2frame";
  if (backend >= 2) {
    FILE *existing = fopen(path, "rb");
    if (existing != NULL) { fclose(existing); CHECK(!"test path already exists"); }
  }
  uint16_t one = 1;
  bool little = *(uint8_t *)&one == 1;
  char dtype[4] = {swap ? (little ? '>' : '<') : '=', 'f', (char)('0' + size), '\0'};
  blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
  cp.typesize = size;
  cp.nthreads = threads;
  cp.compcode = BLOSC_BLOSCLZ;
  cp.clevel = backend == 0 ? 0 : 1;
  memset(cp.filters, 0, sizeof(cp.filters));
  cp.filters[0] = BLOSC_SHUFFLE;
  cp.filters[1] = BLOSC_FILTER_BYTEDELTA;
  cp.filters_meta[1] = (uint8_t)size;
  blosc2_storage storage = {
      .cparams = &cp, .contiguous = backend == 1 || backend == 2,
      .urlpath = backend >= 2 ? (char *)path : NULL};
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, g->ndim, g->shape, g->chunks, g->blocks,
                                       dtype, DTYPE_NUMPY_FORMAT, NULL, 0);
  CHECK(ctx != NULL);
  int64_t n = 1;
  for (int d = 0; d < g->ndim; ++d) n *= g->shape[d];
  uint8_t *values = malloc((size_t)(n + 1) * size);
  CHECK(values != NULL);
  for (int mode = 0; mode < 4; ++mode) {
    b2nd_array_t *a = NULL;
    double start = mode == 3 ? 2.0 : -1.25;
    double step = mode == 3 ? -0.125 : 0.125;
    double stop = mode < 2 ? 3.5 : start + n * step;
    bool endpoint = mode == 0;
    int rc = mode < 2 ? b2nd_linspace(ctx, &a, start, stop, endpoint)
                      : b2nd_arange(ctx, &a, start, stop, step);
    CHECK(rc == 0 && a != NULL);
    CHECK(a->nitems == n && strcmp(a->dtype, dtype) == 0);
    blosc2_cparams *actual;
    CHECK(blosc2_schunk_get_cparams(a->sc, &actual) == 0);
    CHECK(actual->prefilter == NULL && actual->nthreads == threads);
    CHECK(actual->compcode == cp.compcode && actual->clevel == cp.clevel);
    CHECK(memcmp(actual->filters, cp.filters, sizeof(cp.filters)) == 0);
    free(actual);
    CHECK(b2nd_to_cbuffer(a, values, n * size) == 0);
    for (int64_t i = 0; i < n; ++i) {
      double expected;
      if (mode < 2) {
        double spacing = n > 1 ? (stop - start) / (double)(n - (endpoint ? 1 : 0)) : 0;
        volatile double offset = (double)i * spacing;
        expected = start + offset;
        if (endpoint && n > 1 && i == n - 1) expected = stop;
      }
      else expected = start + (double)i * step;
      if (size == 4) expected = (float)expected;
      double got = read_value(values + i * size, size, swap);
      if (got != expected) {
        fprintf(stderr, "ndim=%d n=%lld size=%d swap=%d backend=%d mode=%d i=%lld: %.17g != %.17g\n",
                g->ndim, (long long)n, size, swap, backend, mode, (long long)i, got, expected);
      }
      CHECK(got == expected);
    }
    CHECK(check_padding(a) == 0);
    CHECK(b2nd_free(a) == 0);
    if (backend >= 2) {
      CHECK(b2nd_open(path, &a) == 0);
      CHECK(b2nd_to_cbuffer(a, values, n * size) == 0);
      if (n) CHECK(read_value(values, size, swap) == (size == 4 ? (float)start : start));
      CHECK(b2nd_free(a) == 0);
      CHECK(blosc2_remove_urlpath(path) == 0);
    }
  }
  CHECK(b2nd_free_ctx(ctx) == 0);
  free(values);
  return 0;
}

static int noop_prefilter(blosc2_prefilter_params *p) { (void)p; return 0; }

static int check_errors(void) {
  int64_t shape[] = {4};
  int32_t chunk[] = {3}, block[] = {2};
  blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
  cp.typesize = 8;
  blosc2_storage storage = {.cparams = &cp};
  const char *dtypes[] = {"=i8", "=c8", "|S8", "=f4", "=f8"};
  for (int i = 0; i < 5; ++i) {
    b2nd_context_t *ctx = b2nd_create_ctx(&storage, 1, shape, chunk, block, dtypes[i], 0, NULL, 0);
    CHECK(ctx != NULL);
    b2nd_array_t *a = (b2nd_array_t *)1;
    if (i < 4) {
      CHECK(b2nd_linspace(ctx, &a, 0, 1, true) < 0 && a == NULL);
      CHECK(b2nd_arange(ctx, &a, 0, 4, 1) < 0 && a == NULL);
    }
    else {
      CHECK(b2nd_arange(ctx, &a, 0, 5, 1) < 0 && a == NULL);
      CHECK(b2nd_arange(ctx, &a, 0, 4, 0) < 0 && a == NULL);
      CHECK(b2nd_arange(ctx, &a, 0, 4, -1) < 0 && a == NULL);
      CHECK(b2nd_arange(ctx, &a, 0, 4, NAN) < 0 && a == NULL);
      CHECK(b2nd_arange(ctx, &a, -DBL_MAX, DBL_MAX, 1) < 0 && a == NULL);
      CHECK(b2nd_linspace(ctx, &a, NAN, 1, true) < 0 && a == NULL);
      CHECK(b2nd_linspace(ctx, &a, 0, INFINITY, true) < 0 && a == NULL);
      CHECK(b2nd_linspace(ctx, NULL, 0, 1, true) < 0);
      CHECK(b2nd_linspace(ctx, &a, 0, 1, true) == 0);
      CHECK(b2nd_free(a) == 0);
    }
    CHECK(b2nd_free_ctx(ctx) == 0);
  }
  b2nd_array_t *a = (b2nd_array_t *)1;
  CHECK(b2nd_linspace(NULL, &a, 0, 1, true) < 0 && a == NULL);
  CHECK(b2nd_arange(NULL, &a, 0, 1, 1) < 0 && a == NULL);
  cp.prefilter = noop_prefilter;
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, 1, shape, chunk, block, "=f8", 0, NULL, 0);
  CHECK(ctx != NULL);
  CHECK(b2nd_linspace(ctx, &a, 0, 1, true) < 0 && a == NULL);
  CHECK(b2nd_free_ctx(ctx) == 0);
  cp.prefilter = NULL;
  ctx = b2nd_create_ctx(&storage, 1, shape, chunk, block, "=f8", 1, NULL, 0);
  CHECK(ctx != NULL);
  CHECK(b2nd_linspace(ctx, &a, 0, 1, true) < 0 && a == NULL);
  CHECK(b2nd_free_ctx(ctx) == 0);
  shape[0] = INT64_MAX;
  chunk[0] = 1024;
  block[0] = 3;
  ctx = b2nd_create_ctx(&storage, 1, shape, chunk, block, "=f8", 0, NULL, 0);
  CHECK(ctx != NULL);
  CHECK(b2nd_linspace(ctx, &a, 0, 1, true) < 0 && a == NULL);
  CHECK(b2nd_free_ctx(ctx) == 0);
  shape[0] = 0;
  chunk[0] = 4;
  block[0] = 2;
  ctx = b2nd_create_ctx(&storage, 1, shape, chunk, block, "=f8", 0, NULL, 0);
  CHECK(ctx != NULL);
  CHECK(b2nd_arange(ctx, &a, 0, 5, -1) == 0 && a->nitems == 0);
  CHECK(b2nd_free(a) == 0);
  CHECK(b2nd_free_ctx(ctx) == 0);
  return 0;
}

static int check_extremes(void) {
  int64_t shape[] = {3};
  int32_t chunk[] = {2}, block[] = {2};
  blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
  cp.typesize = 8;
  blosc2_storage storage = {.cparams = &cp};
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, 1, shape, chunk, block, "=f8", 0, NULL, 0);
  CHECK(ctx != NULL);
  b2nd_array_t *a;
  double values[3];
  CHECK(b2nd_linspace(ctx, &a, -DBL_MAX, DBL_MAX, true) == 0);
  CHECK(b2nd_to_cbuffer(a, values, sizeof(values)) == 0);
  CHECK(values[0] == -DBL_MAX && values[1] == 0 && values[2] == DBL_MAX);
  CHECK(b2nd_free(a) == 0);
  double tiny = nextafter(0.0, 1.0);
  CHECK(b2nd_linspace(ctx, &a, 0, tiny, true) == 0);
  CHECK(b2nd_to_cbuffer(a, values, sizeof(values)) == 0);
  CHECK(values[0] == 0 && values[1] == 0 && values[2] == tiny);
  CHECK(b2nd_free(a) == 0);
  CHECK(b2nd_free_ctx(ctx) == 0);
  return 0;
}

int main(void) {
  blosc2_init();
  for (size_t g = 0; g < sizeof(geometries) / sizeof(*geometries); ++g) {
    for (int size = 4; size <= 8; size += 4) {
      for (int swap = 0; swap <= 1; ++swap) {
        for (int backend = 0; backend < 4; ++backend) {
          CHECK(check_case(&geometries[g], size, swap, backend, backend % 2 ? 2 : 1) == 0);
        }
      }
    }
  }
  CHECK(check_errors() == 0);
  CHECK(check_extremes() == 0);
  blosc2_destroy();
  puts("b2nd constructors: all checks passed");
  return 0;
}
