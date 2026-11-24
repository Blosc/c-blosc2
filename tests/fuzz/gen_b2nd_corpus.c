/*
  Copyright (c) 2026 Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Generate structured seed corpus inputs for fuzz_b2nd.c.

  Run:  ./gen_b2nd_corpus <output-dir>

  Writes a handful of small, valid b2nd frames (different dimensionalities,
  codecs, filters and a user-defined metalayer) so libFuzzer starts from
  structurally valid inputs instead of guessing the frame format.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <blosc2.h>
#include <b2nd.h>

#define BUFSIZE 1 << 20

static uint32_t pattern(int64_t i) {
  return (uint32_t)(i * 2654435761u);
}

static void write_seed(const char *dir, const char *name, uint8_t *cframe,
                       int64_t len) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", dir, name);
  FILE *f = fopen(path, "wb");
  if (f == NULL) {
    fprintf(stderr, "cannot open %s for writing\n", path);
    return;
  }
  fwrite(cframe, 1, (size_t)len, f);
  fclose(f);
  printf("Successfully created %s (%lld bytes)\n", name, (long long)len);
}

static b2nd_context_t *make_ctx(blosc2_storage *storage, int8_t ndim,
                                int64_t *shape, int32_t *chunkshape,
                                int32_t *blockshape,
                                blosc2_metalayer *metalayers,
                                int32_t nmetalayers) {
  return b2nd_create_ctx(storage, ndim, shape, chunkshape, blockshape, NULL, 0,
                         metalayers, nmetalayers);
}

static int emit(const char *dir, const char *name, int8_t ndim,
                const int64_t *shape, const int32_t *chunkshape,
                const int32_t *blockshape, uint8_t typesize,
                const char *compname, int32_t clevel, uint8_t filter,
                blosc2_metalayer *metalayers, int32_t nmetalayers) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  cparams.clevel = clevel;
  cparams.nthreads = 1;
  int compcode = blosc2_compname_to_compcode(compname);
  if (compcode < 0) {
    return -1;
  }
  cparams.compcode = (uint8_t)compcode;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = filter;

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  blosc2_storage storage = {.cparams = &cparams, .dparams = &dparams,
                            .contiguous = true};

  b2nd_context_t *ctx = make_ctx(&storage, ndim, (int64_t *)shape,
                                 (int32_t *)chunkshape, (int32_t *)blockshape,
                                 metalayers, nmetalayers);
  if (ctx == NULL) {
    return -1;
  }

  int64_t nitems = 1;
  for (int8_t d = 0; d < ndim; d++) {
    nitems *= shape[d];
  }
  size_t nbytes = (size_t)nitems * typesize;
  uint8_t *data = (uint8_t *)calloc(1, nbytes > 0 ? nbytes : 1);
  if (data == NULL) {
    b2nd_free_ctx(ctx);
    return -1;
  }
  for (size_t i = 0; i < nbytes; i++) {
    uint32_t p = pattern((int64_t)(i / 4));
    data[i] = (uint8_t)(p >> ((i % 4) * 8));
  }

  b2nd_array_t *arr = NULL;
  int rc = b2nd_from_cbuffer(ctx, &arr, data, nitems * typesize);
  if (rc < 0 || arr == NULL) {
    if (arr != NULL) {
      b2nd_free(arr);
    }
    free(data);
    b2nd_free_ctx(ctx);
    return -1;
  }

  uint8_t *cframe = NULL;
  int64_t len = 0;
  bool needs_free = false;
  if (b2nd_to_cframe(arr, &cframe, &len, &needs_free) >= 0 && len > 0 &&
      cframe != NULL) {
    write_seed(dir, name, cframe, len);
  }
  if (needs_free) {
    free(cframe);
  }
  b2nd_free(arr);
  b2nd_free_ctx(ctx);
  free(data);
  return 0;
}

int main(int argc, char **argv) {
  const char *dir = (argc >= 2) ? argv[1] : ".";

  blosc2_init();
  { int64_t shape[] = {512};
    int32_t cs[] = {128}, bs[] = {32};
    emit(dir, "b2nd_1d_i32_blosclz.b2nd", 1, shape, cs, bs, 4, "blosclz", 5,
         BLOSC_SHUFFLE, NULL, 0); }
  { int64_t shape[] = {32, 64};
    int32_t cs[] = {16, 16}, bs[] = {8, 8};
    emit(dir, "b2nd_2d_i16_lz4.b2nd", 2, shape, cs, bs, 2, "lz4", 6,
         BLOSC_SHUFFLE, NULL, 0); }
  { int64_t shape[] = {8, 8, 8};
    int32_t cs[] = {4, 4, 4}, bs[] = {2, 2, 2};
    emit(dir, "b2nd_3d_f4_zstd.b2nd", 3, shape, cs, bs, 4, "zstd", 9,
         BLOSC_NOSHUFFLE, NULL, 0); }
  { int64_t shape[] = {1, 2, 1, 2, 1, 2, 1, 2};
    int32_t cs[] = {1, 1, 1, 1, 1, 1, 1, 1};
    int32_t bs[] = {1, 1, 1, 1, 1, 1, 1, 1};
    blosc2_metalayer meta = {.name = "fz", .content = (uint8_t *)"seedmeta",
                             .content_len = 8};
    emit(dir, "b2nd_8d_u8_meta.b2nd", 8, shape, cs, bs, 1, "blosclz", 5,
         BLOSC_BITSHUFFLE, &meta, 1); }
  { int64_t shape[] = {16, 3};
    int32_t cs[] = {4, 2}, bs[] = {2, 1};
    emit(dir, "b2nd_2d_u4_delta.b2nd", 2, shape, cs, bs, 4, "lz4hc", 3,
         BLOSC_DELTA, NULL, 0); }

  blosc2_destroy();
  return 0;
}
