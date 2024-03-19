/*********************************************************************
    Blosc - Blocked Shuffling and Compression Library

    Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
    https://blosc.org
    License: BSD 3-Clause (see LICENSE.txt)

    See LICENSE.txt for details about copyright and rights to use.

    Test program demonstrating use of the Blosc codec from C code.
    To compile this program:

    $ gcc -O test_zfp_rate_getitem.c -o test_zfp_rate_getitem -lblosc2


**********************************************************************/

#include "blosc-private.h"
#include "b2nd.h"
#include "blosc2/codecs-registry.h"
#include "blosc2.h"

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

static int test_zfp_rate_getitem_float(blosc2_schunk *schunk) {

  if (schunk->typesize != 4) {
    printf("Error: This test is only for doubles.\n");
    return 0;
  }
  int64_t nchunks = schunk->nchunks;
  int32_t chunksize = (int32_t) (schunk->chunksize);
  float *data_in = malloc(chunksize);
  int decompressed;
  int64_t csize;
  uint8_t *chunk_zfp = malloc(chunksize + BLOSC2_MAX_OVERHEAD);
  uint8_t *chunk_blosc = malloc(chunksize + BLOSC2_MAX_OVERHEAD);
  float *data_dest = malloc(chunksize);

  /* Create a context for compression */
  int8_t zfp_rate = 37;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.splitmode = BLOSC_NEVER_SPLIT;
  cparams.typesize = schunk->typesize;
  cparams.compcode = BLOSC_CODEC_ZFP_FIXED_RATE;
  cparams.compcode_meta = zfp_rate;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_NOFILTER;
  cparams.clevel = 5;
  cparams.nthreads = 1;
  cparams.blocksize = schunk->blocksize;
  cparams.schunk = schunk;
  blosc2_context *cctx;
  cctx = blosc2_create_cctx(cparams);

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  dparams.schunk = schunk;
  blosc2_context *dctx;
  dctx = blosc2_create_dctx(dparams);
  int32_t zfp_chunk_nbytes, zfp_chunk_cbytes, blosc_chunk_cbytes;
  uint8_t *lossy_chunk = malloc(chunksize + BLOSC2_MAX_OVERHEAD);

  for (int ci = 0; ci < nchunks; ci++) {
    decompressed = blosc2_schunk_decompress_chunk(schunk, ci, data_in, chunksize);
    if (decompressed < 0) {
      printf("Error decompressing chunk \n");
      return -1;
    }

    /* Compress using ZFP fixed-rate  */
    csize = blosc2_compress_ctx(cctx, data_in, chunksize, chunk_zfp, chunksize + BLOSC2_MAX_OVERHEAD);
    if (csize == 0) {
      printf("Buffer is incompressible.  Giving up.\n");
      return -1;
    } else if (csize < 0) {
      printf("Compression error.  Error code: %" PRId64 "\n", csize);
      return (int) csize;
    }
    blosc2_cbuffer_sizes(chunk_zfp, &zfp_chunk_nbytes, &zfp_chunk_cbytes, NULL);

    decompressed = blosc2_decompress_ctx(dctx, chunk_zfp, zfp_chunk_cbytes, lossy_chunk, chunksize);
    if (decompressed < 0) {
      printf("Error decompressing chunk \n");
      return -1;
    }

    /* Compress not using ZFP fixed-rate  */
    csize = blosc2_compress_ctx(schunk->cctx, lossy_chunk, chunksize, chunk_blosc, chunksize + BLOSC2_MAX_OVERHEAD);
    if (csize == 0) {
      printf("Buffer is incompressible.  Giving up.\n");
      return -1;
    } else if (csize < 0) {
      printf("Compression error.  Error code: %" PRId64 "\n", csize);
      return (int) csize;
    }
    blosc2_cbuffer_sizes(chunk_blosc, NULL, &blosc_chunk_cbytes, NULL);

    /* Get item  */
    int index, dsize_zfp, dsize_blosc;
    float item_zfp, item_blosc;
    int nelems = schunk->chunksize / schunk->typesize;
    for (int i = 0; i < 100; ++i) {
      srand(i);
      index = rand() % nelems;
      // Usual getitem
      dsize_blosc = blosc2_getitem_ctx(schunk->dctx, chunk_blosc, blosc_chunk_cbytes,
                                       index, 1, &item_blosc, sizeof(item_blosc));
      // Optimized getitem using ZFP cell machinery
      dsize_zfp = blosc2_getitem_ctx(dctx, chunk_zfp, zfp_chunk_cbytes,
                                     index, 1, &item_zfp, sizeof(item_zfp));
      if (dsize_blosc != dsize_zfp) {
        printf("Different amount of items gotten\n");
        return -1;
      }
      if (item_blosc != item_zfp) {
        printf("\nIn index %d different items extracted zfp %f blosc %f\n", index, item_zfp, item_blosc);
        return -1;
      }
    }
  }

  free(data_in);
  free(data_dest);
  free(chunk_zfp);
  free(chunk_blosc);
  free(lossy_chunk);
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);

  printf("Successful roundtrip!\n");
  return (int) (BLOSC2_ERROR_SUCCESS);
}

static int test_zfp_rate_getitem_double(blosc2_schunk *schunk) {

  if (schunk->typesize != 8) {
    printf("Error: This test is only for doubles.\n");
    return -1;
  }
  int64_t nchunks = schunk->nchunks;
  int32_t chunksize = (int32_t) (schunk->chunksize);
  double *data_in = malloc(chunksize);
  int decompressed;
  int64_t csize;
  uint8_t *chunk_zfp = malloc(chunksize + BLOSC2_MAX_OVERHEAD);
  uint8_t *chunk_blosc = malloc(chunksize + BLOSC2_MAX_OVERHEAD);
  double *data_dest = malloc(chunksize);

  /* Create a context for compression */
  int zfp_rate = 37;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.splitmode = BLOSC_NEVER_SPLIT;
  cparams.typesize = schunk->typesize;
  cparams.compcode = BLOSC_CODEC_ZFP_FIXED_RATE;
  cparams.compcode_meta = zfp_rate;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_NOFILTER;
  cparams.clevel = 5;
  cparams.nthreads = 1;
  cparams.blocksize = schunk->blocksize;
  cparams.schunk = schunk;
  blosc2_context *cctx;
  cctx = blosc2_create_cctx(cparams);

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  dparams.schunk = schunk;
  blosc2_context *dctx;
  dctx = blosc2_create_dctx(dparams);
  int32_t zfp_chunk_nbytes, zfp_chunk_cbytes, blosc_chunk_cbytes;
  uint8_t *lossy_chunk = malloc(chunksize + BLOSC2_MAX_OVERHEAD);

  for (int ci = 0; ci < nchunks; ci++) {
    decompressed = blosc2_schunk_decompress_chunk(schunk, ci, data_in, chunksize);
    if (decompressed < 0) {
      printf("Error decompressing chunk \n");
      return -1;
    }

    /* Compress using ZFP fixed-rate  */
    csize = blosc2_compress_ctx(cctx, data_in, chunksize, chunk_zfp, chunksize + BLOSC2_MAX_OVERHEAD);
    if (csize == 0) {
      printf("Buffer is incompressible.  Giving up.\n");
      return -1;
    } else if (csize < 0) {
      printf("Compression error.  Error code: %" PRId64 "\n", csize);
      return (int) csize;
    }
    blosc2_cbuffer_sizes(chunk_zfp, &zfp_chunk_nbytes, &zfp_chunk_cbytes, NULL);

    decompressed = blosc2_decompress_ctx(dctx, chunk_zfp, zfp_chunk_cbytes, lossy_chunk, chunksize);
    if (decompressed < 0) {
      printf("Error decompressing chunk \n");
      return -1;
    }

    /* Compress not using ZFP fixed-rate  */
    csize = blosc2_compress_ctx(schunk->cctx, lossy_chunk, chunksize, chunk_blosc, chunksize + BLOSC2_MAX_OVERHEAD);
    if (csize == 0) {
      printf("Buffer is incompressible.  Giving up.\n");
      return -1;
    } else if (csize < 0) {
      printf("Compression error.  Error code: %" PRId64 "\n", csize);
      return (int) csize;
    }
    blosc2_cbuffer_sizes(chunk_blosc, NULL, &blosc_chunk_cbytes, NULL);

    /* Get item  */
    int index, dsize_zfp, dsize_blosc;
    double item_zfp, item_blosc;
    int nelems = schunk->chunksize / schunk->typesize;
    for (int i = 0; i < 100; ++i) {
      srand(i);
      index = rand() % nelems;
      // Usual getitem
      dsize_blosc = blosc2_getitem_ctx(schunk->dctx, chunk_blosc, blosc_chunk_cbytes,
                                       index, 1, &item_blosc, sizeof(item_blosc));
      // Optimized getitem using ZFP cell machinery
      dsize_zfp = blosc2_getitem_ctx(dctx, chunk_zfp, zfp_chunk_cbytes,
                                     index, 1, &item_zfp, sizeof(item_zfp));
      if (dsize_blosc != dsize_zfp) {
        printf("Different amount of items gotten\n");
        return -1;
      }
      if (item_blosc != item_zfp) {
        printf("\nIn index %d different items extracted zfp %f blosc %f\n", index, item_zfp, item_blosc);
        return -1;
      }
    }
  }

  free(data_in);
  free(data_dest);
  free(chunk_zfp);
  free(chunk_blosc);
  free(lossy_chunk);
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);

  printf("Successful roundtrip!\n");
  return (int) (BLOSC2_ERROR_SUCCESS);
}


int float_cyclic() {
  int8_t ndim = 3;
  int64_t shape[] = {40, 60, 20};
  int32_t chunkshape[] = {20, 30, 16};
  int32_t blockshape[] = {11, 14, 7};
  int32_t typesize = sizeof(float);
  int64_t nelem = 1;
  for (int i = 0; i < ndim; ++i) {
    nelem *= shape[i];
  }
  int64_t size = nelem * typesize;

  float *data = malloc(size);
  for (int i = 0; i < nelem; i += 2) {
    float j = (float) i;
    data[i] = (j + j / 10 + j / 100);
    data[i + 1] = (2 + j / 10 + j / 1000);
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  blosc2_storage b2_storage = {.cparams=&cparams};
  b2_storage.contiguous = true;

  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, ndim, shape, chunkshape, blockshape, NULL, 0,
                                        NULL, 0);

  b2nd_array_t *arr;
  BLOSC_ERROR(b2nd_from_cbuffer(ctx, &arr, data, size));
  blosc2_schunk *schunk = arr->sc;

  /* Run the test. */
  int result = test_zfp_rate_getitem_float(schunk);
  BLOSC_ERROR(b2nd_free_ctx(ctx));
  BLOSC_ERROR(b2nd_free(arr));
  return result;
}

int double_same_cells() {
  int8_t ndim = 2;
  int64_t shape[] = {40, 60};
  int32_t chunkshape[] = {20, 30};
  int32_t blockshape[] = {16, 16};
  int32_t typesize = sizeof(double);
  int64_t nelem = 1;
  for (int i = 0; i < ndim; ++i) {
    nelem *= shape[i];
  }
  int64_t size = nelem * typesize;

  double *data = malloc(size);
  for (int i = 0; i < nelem; i += 4) {
    data[i] = 1.5;
    data[i + 1] = 14.7;
    data[i + 2] = 23.6;
    data[i + 3] = 3.2;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  blosc2_storage b2_storage = {.cparams=&cparams};
  b2_storage.contiguous = true;

  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, ndim, shape, chunkshape, blockshape, NULL, 0,
                                        NULL, 0);

  b2nd_array_t *arr;
  BLOSC_ERROR(b2nd_from_cbuffer(ctx, &arr, data, size));
  blosc2_schunk *schunk = arr->sc;

  /* Run the test. */
  int result = test_zfp_rate_getitem_double(schunk);
  BLOSC_ERROR(b2nd_free_ctx(ctx));
  BLOSC_ERROR(b2nd_free(arr));
  return result;
}

int item_prices() {
  blosc2_schunk *schunk = blosc2_schunk_open("example_item_prices.b2nd");
  BLOSC_ERROR_NULL(schunk, BLOSC2_ERROR_FILE_OPEN);

  /* Run the test. */
  int result = test_zfp_rate_getitem_float(schunk);
  blosc2_schunk_free(schunk);
  return result;
}


int main(void) {

  int result;
  blosc2_init();   // this is mandatory for initializing the plugin mechanism
  printf("float_cyclic: ");
  result = float_cyclic();
  if (result < 0)
    return result;
  printf("double_same_cells: ");
  result = double_same_cells();
  if (result < 0)
    return result;
  printf("item_prices: ");
  result = item_prices();
  if (result < 0)
    return result;
  blosc2_destroy();

  return BLOSC2_ERROR_SUCCESS;
}
