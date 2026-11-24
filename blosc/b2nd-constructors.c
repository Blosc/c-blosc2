/*********************************************************************
  Copyright (c) 2026 Blosc Development Team <blosc@blosc.org>
  License: BSD 3-Clause (see LICENSE.txt)
**********************************************************************/

#include "b2nd.h"
#include "context.h"
#include "blosc-checked.h"

#include <math.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef BLOSC2_HAVE_MINIEXPR
#include "miniexpr_artifact.h"
#endif

typedef struct {
  b2nd_array_t *array;
  double start, stop, step;
  bool endpoint, interpolate, swap;
  int itemsize;
  int64_t nchunk;
#ifdef BLOSC2_HAVE_MINIEXPR
  me_artifact *kernel;
#endif
} ramp_state;

/* Only accept explicit floating NumPy dtypes, not a size-compatible integer or
 * opaque dtype. b2nd itself intentionally treats dtype as opaque metadata. */
static int ramp_dtype(b2nd_context_t *ctx, int *itemsize, bool *swap) {
  if (ctx->dtype_format != DTYPE_NUMPY_FORMAT || ctx->dtype == NULL) {
    return BLOSC2_ERROR_INVALID_PARAM;
  }
  const char *dtype = ctx->dtype;
  char order = '=';
  if (strchr("<>=|", dtype[0]) && dtype[0]) order = *dtype++;
  if (strcmp(dtype, "f4") == 0) *itemsize = 4;
  else if (strcmp(dtype, "f8") == 0) *itemsize = 8;
  else return BLOSC2_ERROR_INVALID_PARAM;
  if (ctx->b2_storage->cparams->typesize != *itemsize ||
      ctx->b2_storage->cparams->prefilter != NULL) return BLOSC2_ERROR_INVALID_PARAM;
  const uint16_t one = 1;
  bool little = *(const uint8_t *)&one == 1;
  *swap = (order == '<' && !little) || (order == '>' && little);
  return 0;
}

static void store_value(ramp_state *s, uint8_t *output, double value) {
  if (s->itemsize == 4) {
    float f = (float)value;
    memcpy(output, &f, sizeof(f));
  }
  else memcpy(output, &value, sizeof(value));
}

static int ramp_span(ramp_state *s, uint8_t *output, int64_t origin, int64_t count) {
  b2nd_array_t *a = s->array;
#ifdef BLOSC2_HAVE_MINIEXPR
  if (s->kernel != NULL) {
    me_artifact_eval_descriptor d = {
        .struct_size = sizeof(d), .version = ME_ARTIFACT_EVAL_DESCRIPTOR_VERSION,
        .nitems = (size_t)count, .output_capacity = (size_t)count * s->itemsize,
        .ndim = 1, .logical_shape = &a->nitems,
        .block_origin = &origin, .block_extent = &count};
    if (me_artifact_eval_ex(s->kernel, NULL, 0, output, &d, NULL) != ME_ARTIFACT_SUCCESS) {
      return BLOSC2_ERROR_FAILURE;
    }
  }
  else
#endif
  {
    for (int64_t j = 0; j < count; ++j) {
      int64_t i = origin + j;
      double value;
      if (s->interpolate) {
        double t = (double)i / (double)(a->nitems - (s->endpoint ? 1 : 0));
        value = (1.0 - t) * s->start + t * s->stop;
      }
      else {
        /* Keep the multiply and add separate, matching miniexpr's strict ramp. */
        double offset = (double)i * s->step;
        value = s->start + offset;
      }
      store_value(s, output + j * s->itemsize, value);
    }
  }
  if (origin == 0) store_value(s, output, s->start);
  if (s->endpoint && a->nitems > 1 && origin + count == a->nitems) {
    store_value(s, output + (count - 1) * s->itemsize, s->stop);
  }
  if (s->swap) {
    for (int64_t i = 0; i < count; ++i) {
      uint8_t *p = output + i * s->itemsize;
      for (int j = 0; j < s->itemsize / 2; ++j) {
        uint8_t tmp = p[j];
        p[j] = p[s->itemsize - 1 - j];
        p[s->itemsize - 1 - j] = tmp;
      }
    }
  }
  return 0;
}

/* Physical chunks contain C-ordered blocks, each itself in C order. Generate
 * contiguous logical rows into those blocks; padding never advances the ramp. */
static int ramp_prefilter(blosc2_prefilter_params *p) {
  ramp_state *s = p->user_data;
  b2nd_array_t *a = s->array;
  int ndim = a->ndim;
  int64_t chunk = s->nchunk, block = p->nblock;
  int64_t base[B2ND_MAX_DIM], valid[B2ND_MAX_DIM];
  int64_t rows = 1;
  bool padded = false;
  for (int d = ndim - 1; d >= 0; --d) {
    int64_t chunkgrid = a->extshape[d] / a->chunkshape[d];
    int64_t blockgrid = a->extchunkshape[d] / a->blockshape[d];
    int64_t relative = (block % blockgrid) * a->blockshape[d];
    block /= blockgrid;
    base[d] = (chunk % chunkgrid) * a->chunkshape[d] + relative;
    chunk /= chunkgrid;
    int64_t live = a->shape[d] - base[d];
    int64_t in_chunk = a->chunkshape[d] - relative;
    if (live > in_chunk) live = in_chunk;
    if (live > a->blockshape[d]) live = a->blockshape[d];
    valid[d] = live > 0 ? live : 0;
    padded |= valid[d] != a->blockshape[d];
    if (d < ndim - 1) rows *= a->blockshape[d];
  }
  if (padded) memset(p->output, 0, (size_t)p->output_size);
  if (ndim == 0) return ramp_span(s, p->output, 0, 1);
  if (valid[ndim - 1] == 0) return 0;
  for (int64_t row = 0; row < rows; ++row) {
    int64_t r = row, origin = base[ndim - 1];
    bool live = true;
    for (int d = ndim - 2; d >= 0; --d) {
      int64_t pos = r % a->blockshape[d];
      r /= a->blockshape[d];
      live &= pos < valid[d];
      origin += (base[d] + pos) * a->item_array_strides[d];
    }
    if (live) {
      int rc = ramp_span(s, p->output + row * a->blockshape[ndim - 1] * s->itemsize,
                         origin, valid[ndim - 1]);
      if (rc < 0) return rc;
    }
  }
  return 0;
}

#ifdef BLOSC2_HAVE_MINIEXPR
/* snprintf follows LC_NUMERIC, but Menudet source always uses a decimal dot.
 * Never change the process locale from inside the library. */
static void ramp_literal(double value, char out[128]) {
  snprintf(out, 128, "%.17g", value);
  const char *decimal = localeconv()->decimal_point;
  char *point = decimal[0] ? strstr(out, decimal) : NULL;
  if (point != NULL && strcmp(decimal, ".") != 0) {
    size_t len = strlen(decimal);
    *point = '.';
    memmove(point + 1, point + len, strlen(point + len) + 1);
  }
  if (strpbrk(out, ".eE") == NULL) strcat(out, ".0");
}

static int ramp_compile(ramp_state *s) {
  if (s->interpolate) return 0;
  char start[128], step[128], json[2048];
  ramp_literal(s->start, start);
  ramp_literal(s->step, step);
  snprintf(json, sizeof(json),
      "{\"schema_version\":\"0.1\",\"language\":{\"name\":\"menudet\",\"version\":\"0.1\"},"
      "\"requires\":[\"numeric\",\"nd-context\"],"
      "\"source\":\"def ramp():\\n    return %s + _flat_idx * %s\\n\","
      "\"entry_point\":\"ramp\",\"inputs\":[],\"constants\":[],"
      "\"output\":{\"dtype\":\"float%d\",\"contract\":\"elementwise\"},"
      "\"context\":{\"ndim\":1},\"semantics\":{\"fp\":\"strict\","
      "\"numeric\":\"numpy-2.5\",\"casting\":\"unsafe\"}}", start, step, s->itemsize * 8);
  return me_artifact_load(json, strlen(json), ME_JIT_OFF, &s->kernel, NULL) == ME_ARTIFACT_SUCCESS
             ? 0 : BLOSC2_ERROR_FAILURE;
}
#endif

static int ramp_create(b2nd_context_t *ctx, b2nd_array_t **out, double start,
                       double stop, double step, bool endpoint, bool linspace) {
  if (out == NULL) return BLOSC2_ERROR_NULL_POINTER;
  *out = NULL;
  if (ctx == NULL) return BLOSC2_ERROR_NULL_POINTER;
  ramp_state s = {.start = start, .stop = stop, .step = step, .endpoint = endpoint};
  int rc = ramp_dtype(ctx, &s.itemsize, &s.swap);
  if (rc < 0) return rc;
  if (!isfinite(start) || !isfinite(stop) || (!linspace && (!isfinite(step) || step == 0))) {
    return BLOSC2_ERROR_INVALID_PARAM;
  }
  int64_t n = 1;
  for (int d = 0; d < ctx->ndim; ++d) {
    if (!blosc2_checked_mul_i64_nonneg(n, ctx->shape[d], &n) ||
        (ctx->shape[d] > 0 && (ctx->chunkshape[d] <= 0 || ctx->blockshape[d] <= 0))) {
      return BLOSC2_ERROR_INVALID_PARAM;
    }
  }
  if (n > 0) {
    int64_t physical_items = 1;
    for (int d = 0; d < ctx->ndim; ++d) {
      int64_t chunks = ctx->shape[d] / ctx->chunkshape[d] +
                       (ctx->shape[d] % ctx->chunkshape[d] != 0);
      int64_t blocks = ctx->chunkshape[d] / ctx->blockshape[d] +
                       (ctx->chunkshape[d] % ctx->blockshape[d] != 0);
      int64_t extent = blocks * ctx->blockshape[d];
      if (!blosc2_checked_mul_i64_nonneg(physical_items, chunks, &physical_items) ||
          !blosc2_checked_mul_i64_nonneg(physical_items, extent, &physical_items) ||
          physical_items > INT64_MAX / s.itemsize) {
        return BLOSC2_ERROR_INVALID_PARAM;
      }
    }
  }
  if (!linspace) {
    double quotient = (stop - start) / step;
    if (!isfinite(quotient) || (long double)fmax(0, ceil(quotient)) != (long double)n) {
      return BLOSC2_ERROR_INVALID_PARAM;
    }
  }
  else {
    double delta = stop - start;
    s.step = n > 1 ? delta / (double)(n - (endpoint ? 1 : 0)) : 0;
    s.interpolate = n > 1 && (!isfinite(delta) || (s.step == 0 && delta != 0));
  }
  /* Validation precedes allocation/storage creation, especially for bad arange
   * lengths. Use a temporary compressor so no callback or stack state escapes. */
  rc = b2nd_uninit(ctx, &s.array);
  if (rc < 0) return rc;
  blosc2_context *compressor = NULL;
  uint8_t *dummy = NULL, *chunk = NULL;
  if (n == 0) goto success;
#ifdef BLOSC2_HAVE_MINIEXPR
  rc = ramp_compile(&s);
  if (rc < 0) goto cleanup;
#endif
  blosc2_cparams cp = *ctx->b2_storage->cparams;
  blosc2_prefilter_params pre = {.user_data = &s};
  cp.prefilter = ramp_prefilter;
  cp.preparams = &pre;
  cp.schunk = s.array->sc;
  compressor = blosc2_create_cctx(cp);
  if (compressor == NULL) { rc = BLOSC2_ERROR_MEMORY_ALLOC; goto cleanup; }
  rc = blosc2_set_prefilter_full_write(compressor, true);
  if (rc < 0) goto cleanup;
  int32_t bytes = s.array->sc->chunksize;
  dummy = calloc(1, (size_t)bytes);
  chunk = malloc((size_t)bytes + BLOSC2_MAX_OVERHEAD);
  if (dummy == NULL || chunk == NULL) { rc = BLOSC2_ERROR_MEMORY_ALLOC; goto cleanup; }
  for (s.nchunk = 0; s.nchunk < s.array->sc->nchunks; ++s.nchunk) {
    int cbytes = blosc2_compress_ctx(compressor, dummy, bytes, chunk, bytes + BLOSC2_MAX_OVERHEAD);
    if (cbytes <= 0) { rc = cbytes < 0 ? cbytes : BLOSC2_ERROR_FAILURE; goto cleanup; }
    if (blosc2_schunk_update_chunk(s.array->sc, s.nchunk, chunk, true) < 0) {
      rc = BLOSC2_ERROR_FAILURE;
      goto cleanup;
    }
  }
success:
  *out = s.array;
  s.array = NULL;
  rc = 0;
cleanup:
  if (compressor != NULL) blosc2_free_ctx(compressor);
#ifdef BLOSC2_HAVE_MINIEXPR
  me_artifact_free(s.kernel);
#endif
  free(dummy);
  free(chunk);
  if (s.array != NULL) b2nd_free(s.array);
  return rc;
}

int b2nd_linspace(b2nd_context_t *ctx, b2nd_array_t **array,
                  double start, double stop, bool endpoint) {
  return ramp_create(ctx, array, start, stop, 0, endpoint, true);
}

int b2nd_arange(b2nd_context_t *ctx, b2nd_array_t **array,
                double start, double stop, double step) {
  return ramp_create(ctx, array, start, stop, step, false, false);
}
