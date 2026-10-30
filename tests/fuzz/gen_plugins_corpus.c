/*
  Copyright (c) 2026 Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Generate structured seed corpus inputs for fuzz_plugins.c: small valid b2nd
  frames compressed with the plugin codecs / filters (zfp, ndlz, ndmean,
  ndcell, bytedelta, int_trunc).

  Run:  ./gen_plugins_corpus <output-dir>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <blosc2.h>
#include <b2nd.h>
#include <blosc2/codecs-registry.h>
#include <blosc2/filters-registry.h>

static uint32_t pattern(int64_t i) {
  return (uint32_t)(i * 2654435761u);
}

static int emit(const char *dir, const char *name, int8_t ndim,
                const int64_t *shape, uint8_t typesize, uint8_t compcode,
                uint8_t compcode_meta, uint8_t filter, uint8_t filter_meta) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  cparams.clevel = 5;
  cparams.compcode = compcode;
  cparams.compcode_meta = compcode_meta;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = filter;
  cparams.filters_meta[BLOSC2_MAX_FILTERS - 1] = filter_meta;
  cparams.nthreads = 1;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  blosc2_storage storage = {.cparams = &cparams, .dparams = &dparams,
                            .contiguous = true};

  int64_t nitems = 1;
  for (int8_t d = 0; d < ndim; d++) {
    nitems *= shape[d];
  }
  int32_t chunkshape[4] = {4, 4, 4, 4};
  int32_t blockshape[4] = {2, 2, 2, 2};

  b2nd_context_t *ctx = b2nd_create_ctx(&storage, ndim, shape, chunkshape,
                                        blockshape, NULL, 0, NULL, 0);
  if (ctx == NULL) {
    return -1;
  }
  uint32_t *data = (uint32_t *)malloc((size_t)nitems * typesize);
  int64_t words = nitems * typesize / 4;
  for (int64_t i = 0; i < words; i++) {
    data[i] = pattern(i);
  }

  b2nd_array_t *arr = NULL;
  int rc = b2nd_from_cbuffer(ctx, &arr, data, nitems * typesize);
  if (rc >= 0 && arr != NULL) {
    uint8_t *cframe = NULL;
    int64_t len = 0;
    bool needs_free = false;
    if (b2nd_to_cframe(arr, &cframe, &len, &needs_free) >= 0 && len > 0 &&
        cframe != NULL) {
      char path[512];
      snprintf(path, sizeof(path), "%s/%s", dir, name);
      FILE *f = fopen(path, "wb");
      if (f != NULL) {
        fwrite(cframe, 1, (size_t)len, f);
        fclose(f);
        printf("Successfully created %s (%lld bytes)\n", name,
               (long long)len);
      }
      if (needs_free) {
        free(cframe);
      }
    }
    b2nd_free(arr);
  }
  free(data);
  b2nd_free_ctx(ctx);
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <output-dir>\n", argv[0]);
    return 1;
  }
  const char *dir = argv[1];

  blosc2_init();
  int64_t s2[] = {8, 8};
  int64_t s1[] = {64};

  emit(dir, "plugins_zfp_acc_f4.b2nd", 2, s2, 4,
       BLOSC_CODEC_ZFP_FIXED_ACCURACY, 4, BLOSC_NOSHUFFLE, 0);
  emit(dir, "plugins_zfp_prec_f8.b2nd", 2, s2, 8,
       BLOSC_CODEC_ZFP_FIXED_PRECISION, 32, BLOSC_NOSHUFFLE, 0);
  emit(dir, "plugins_zfp_rate_f4.b2nd", 1, s1, 4,
       BLOSC_CODEC_ZFP_FIXED_RATE, 8, BLOSC_NOSHUFFLE, 0);
  emit(dir, "plugins_ndlz4_u1.b2nd", 2, s2, 1, BLOSC_CODEC_NDLZ, 4,
       BLOSC_NOSHUFFLE, 0);
  emit(dir, "plugins_ndlz8_u1.b2nd", 2, s2, 1, BLOSC_CODEC_NDLZ, 8,
       BLOSC_NOSHUFFLE, 0);
  emit(dir, "plugins_ndmean_f4.b2nd", 2, s2, 4, BLOSC_BLOSCLZ, 0,
       BLOSC_FILTER_NDMEAN, 0);
  emit(dir, "plugins_ndcell_f4.b2nd", 2, s2, 4, BLOSC_LZ4, 0,
       BLOSC_FILTER_NDCELL, 0);
  emit(dir, "plugins_bytedelta_i4.b2nd", 1, s1, 4, BLOSC_ZSTD, 0,
       BLOSC_FILTER_BYTEDELTA, 4);
  emit(dir, "plugins_inttrunc_i4.b2nd", 1, s1, 4, BLOSC_LZ4, 0,
       BLOSC_FILTER_INT_TRUNC, 2);

  blosc2_destroy();
  return 0;
}
