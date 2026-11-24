/*********************************************************************
    Blosc - Blocked Shuffling and Compression Library

    Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
    https://blosc.org
    License: BSD 3-Clause (see LICENSE.txt)

    See LICENSE.txt for details about copyright and rights to use.

    Check that getitem on ZFP fixed-rate chunks (which decodes a single ZFP
    cell when it can) returns the same items as decompressing the whole
    chunk, for any start and number of items, rate, typesize and blockshape.

**********************************************************************/

#include "b2nd.h"
#include "blosc2/codecs-registry.h"
#include "blosc2.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int8_t ndim;
  int64_t shape[3];
  int32_t blockshape[3];
} test_shape;


static int check_getitem(int8_t ndim, const int64_t *shape, const int32_t *blockshape,
                         int32_t typesize, int8_t rate) {
  int64_t nitems = 1;
  int32_t chunkshape[3];
  int32_t blockitems = 1;
  for (int i = 0; i < ndim; i++) {
    nitems *= shape[i];
    chunkshape[i] = (int32_t) shape[i];
    blockitems *= blockshape[i];
  }
  int32_t nbytes = (int32_t) nitems * typesize;

  // Smooth data, with a constant first block (stored as a run by Blosc)
  uint8_t *data = malloc(nbytes);
  for (int64_t i = 0; i < nitems; i++) {
    double value = i < blockitems ? 1.5 : sin((double) i * 0.1) * 100. + (double) (i % 7);
    if (typesize == sizeof(float)) {
      ((float *) data)[i] = (float) value;
    } else {
      ((double *) data)[i] = value;
    }
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  cparams.compcode = BLOSC_CODEC_ZFP_FIXED_RATE;
  cparams.compcode_meta = rate;
  cparams.splitmode = BLOSC_NEVER_SPLIT;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_NOFILTER;
  cparams.nthreads = 1;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  blosc2_storage storage = {.cparams = &cparams, .dparams = &dparams};
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, ndim, shape, chunkshape, blockshape,
                                        typesize == sizeof(float) ? "<f4" : "<f8",
                                        DTYPE_NUMPY_FORMAT, NULL, 0);
  b2nd_array_t *arr;
  if (ctx == NULL || b2nd_from_cbuffer(ctx, &arr, data, nbytes) < 0) {
    printf("Cannot create the array: ndim %d, blockshape [%d %d %d], typesize %d, rate %d\n",
           ndim, blockshape[0], ndim > 1 ? blockshape[1] : 0, ndim > 2 ? blockshape[2] : 0,
           typesize, rate);
    b2nd_free_ctx(ctx);
    free(data);
    return -1;
  }

  uint8_t *chunk;
  bool needs_free;
  int cbytes = blosc2_schunk_get_chunk(arr->sc, 0, &chunk, &needs_free);
  uint8_t *ref = malloc(nbytes);
  uint8_t *items = malloc(nbytes);
  int rc = 0;
  if (cbytes < 0 || blosc2_decompress_ctx(arr->sc->dctx, chunk, cbytes, ref, nbytes) != nbytes) {
    printf("Cannot decompress the chunk\n");
    rc = -1;
    goto out;
  }

  const int32_t counts[] = {1, 2, 3, 4, 5, 8, blockitems - 1, blockitems, blockitems + 1};
  for (int64_t start = 0; start < nitems; start++) {
    for (size_t c = 0; c < sizeof(counts) / sizeof(counts[0]); c++) {
      int32_t count = counts[c];
      if (count <= 0 || start + count > nitems) {
        continue;
      }
      memset(items, 0, (size_t) count * typesize);
      int got = blosc2_getitem_ctx(arr->sc->dctx, chunk, cbytes, (int) start, count,
                                   items, count * typesize);
      if (got != count * typesize ||
          memcmp(items, ref + start * typesize, (size_t) count * typesize) != 0) {
        printf("getitem mismatch: ndim %d, blockshape [%d %d %d], typesize %d, rate %d, "
               "start %lld, nitems %d (returned %d)\n",
               ndim, blockshape[0], ndim > 1 ? blockshape[1] : 0, ndim > 2 ? blockshape[2] : 0,
               typesize, rate, (long long) start, count, got);
        rc = -1;
        goto out;
      }
    }
  }

  out:
  if (needs_free) {
    free(chunk);
  }
  free(ref);
  free(items);
  free(data);
  b2nd_free(arr);
  b2nd_free_ctx(ctx);
  return rc;
}


int main(void) {
  blosc2_init();

  const test_shape shapes[] = {
      {1, {64}, {8}},
      {1, {64}, {4}},           // a block is a single cell
      {2, {16, 16}, {8, 8}},
      {2, {16, 16}, {4, 4}},    // a block is a single cell
      {2, {16, 16}, {4, 8}},
      {2, {16, 12}, {8, 6}},    // block rows end inside a cell row (padded cells)
      {3, {8, 8, 8}, {4, 4, 4}},
      {3, {8, 8, 8}, {4, 8, 8}},
  };
  const int32_t typesizes[] = {sizeof(float), sizeof(double)};
  const int8_t rates[] = {16, 37, 50, 80};

  for (size_t s = 0; s < sizeof(shapes) / sizeof(shapes[0]); s++) {
    for (size_t t = 0; t < sizeof(typesizes) / sizeof(typesizes[0]); t++) {
      for (size_t r = 0; r < sizeof(rates) / sizeof(rates[0]); r++) {
        // ZFP cannot compress blocks with padded cells at high rates
        bool padded = false;
        for (int i = 0; i < shapes[s].ndim; i++) {
          padded |= (shapes[s].blockshape[i] % 4) != 0;
        }
        if (padded && rates[r] > 37) {
          continue;
        }
        if (check_getitem(shapes[s].ndim, shapes[s].shape, shapes[s].blockshape,
                          typesizes[t], rates[r]) < 0) {
          blosc2_destroy();
          return EXIT_FAILURE;
        }
      }
    }
  }

  blosc2_destroy();
  printf("All ZFP getitem checks passed!\n");
  return EXIT_SUCCESS;
}
