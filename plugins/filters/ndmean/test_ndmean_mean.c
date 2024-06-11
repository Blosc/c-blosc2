/*********************************************************************
    Blosc - Blocked Shuffling and Compression Library

    Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
    https://blosc.org
    License: BSD 3-Clause (see LICENSE.txt)

    See LICENSE.txt for details about copyright and rights to use.

    Test program demonstrating use of the Blosc filter from C code.
    To compile this program:

    $ gcc -O test_ndmean_mean.c -o test_ndmean_mean -lblosc2

    To run:

    $ ./test_ndmean_mean
    Blosc version info: 2.0.0a6.dev ($Date:: 2018-05-18 #$)
    Using 1 thread
    Using ZSTD compressor
    Successful roundtrip!
    Compression: 256 -> 137 (1.9x)
    2_rows_matches: 119 obtained

    Successful roundtrip!
    Compression: 128 -> 110 (1.2x)
    same_cells: 18 obtained

    Successful roundtrip!
    Compression: 448 -> 259 (1.7x)
    some_matches: 189 obtained


**********************************************************************/

#include "ndmean.h"
#include "blosc2/filters-registry.h"
#include "b2nd.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>

#define EPSILON (float) (1)

static bool is_close(double d1, double d2) {

  double aux = 1;
  if (fabs(d1) < fabs(d2)) {
    if (fabs(d1) > 0) {
      aux = fabs(d2);
    }
  } else {
    if (fabs(d2) > 0) {
      aux = fabs(d1);
    }
  }

  return fabs(d1 - d2) < aux * EPSILON;
}


static int test_ndmean(blosc2_schunk *schunk) {

  int8_t ndim;
  int64_t *shape = malloc(8 * sizeof(int64_t));
  int32_t *chunkshape = malloc(8 * sizeof(int32_t));
  int32_t *blockshape = malloc(8 * sizeof(int32_t));
  uint8_t cellshape = 4;
  uint8_t *smeta;
  int32_t smeta_len;
  if (blosc2_meta_get(schunk, "b2nd", &smeta, &smeta_len) < 0) {
    printf("Blosc error");
    return 0;
  }
  b2nd_deserialize_meta(smeta, smeta_len, &ndim, shape, chunkshape, blockshape, NULL, NULL);
  free(smeta);

  if (ndim != 1) {
    fprintf(stderr, "This test only works for ndim = 1");
    return -1;
  }

  int32_t typesize = schunk->typesize;
  int64_t nchunks = schunk->nchunks;
  int32_t chunksize = (int32_t) (schunk->chunksize);
  //   int isize = (int) array->extchunknitems * typesize;
  uint8_t *data_in = malloc(chunksize);
  int decompressed;
  int64_t csize;
  int64_t dsize;
  int64_t csize_f = 0;
  uint8_t *data_out = malloc(chunksize + BLOSC2_MAX_OVERHEAD);
  uint8_t *data_dest = malloc(chunksize);

  /* Create a context for compression */
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.splitmode = BLOSC_ALWAYS_SPLIT;
  cparams.typesize = typesize;
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.filters[4] = BLOSC_FILTER_NDMEAN;
  cparams.filters_meta[4] = cellshape;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_SHUFFLE;
  cparams.clevel = 9;
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

  double cell_mean;
  for (int ci = 0; ci < nchunks; ci++) {

    decompressed = blosc2_schunk_decompress_chunk(schunk, ci, data_in, chunksize);
    if (decompressed < 0) {
      printf("Error decompressing chunk \n");
      return -1;
    }

    /* Compress with clevel=5 and shuffle active  */
    csize = blosc2_compress_ctx(cctx, data_in, chunksize, data_out, chunksize + BLOSC2_MAX_OVERHEAD);
    if (csize == 0) {
      printf("Buffer is incompressible.  Giving up.\n");
      return -1;
    } else if (csize < 0) {
      printf("Compression error.  Error code: %" PRId64 "\n", csize);
      return (int) csize;
    }
    csize_f += csize;

    /* Decompress  */
    dsize = blosc2_decompress_ctx(dctx, data_out, chunksize + BLOSC2_MAX_OVERHEAD, data_dest, chunksize);
    if (dsize <= 0) {
      printf("Decompression error.  Error code: %" PRId64 "\n", dsize);
      return (int) dsize;
    }

    int chunk_shape = chunkshape[0];
    if ((ci == nchunks - 1) && (shape[0] % chunkshape[0] != 0)) {
      chunk_shape = (int) (shape[0] % chunkshape[0]);
    }
    int nblocks = (chunk_shape + blockshape[0] - 1) / blockshape[0];

    for (int bi = 0; bi < nblocks; bi++) {
      int block_shape = blockshape[0];
      if ((bi == nblocks - 1) && (chunk_shape % blockshape[0] != 0)) {
        block_shape = chunk_shape % blockshape[0];
      }
      int ncells = (block_shape + cellshape - 1) / cellshape;

      for (int cei = 0; cei < ncells; cei++) {
        int ind = bi * blockshape[0] + cei * cellshape;
        cell_mean = 0;
        int cell_shape = cellshape;
        if ((cei == ncells - 1) && (block_shape % cell_shape != 0)) {
          cell_shape = block_shape % cell_shape;
        }
        switch (typesize) {
          case 4:
            for (int i = 0; i < cell_shape; i++) {
              cell_mean += ((float *) data_in)[ind + i];
            }
            cell_mean /= (float) cell_shape;
            for (int i = 0; i < cell_shape; i++) {
              if (!is_close(cell_mean, ((float *) data_dest)[ind + i])) {
                printf("i: %d, cell_mean %.9f, dest %.9f", ind + i, cell_mean, ((float *) data_dest)[ind + i]);
                printf("\n Decompressed data differs from original!\n");
                return -1;
              }
            }
            break;
          case 8:
            for (int i = 0; i < cell_shape; i++) {
              cell_mean += ((double *) data_in)[ind + i];
            }
            cell_mean /= (double) cell_shape;
            for (int i = 0; i < cell_shape; i++) {
              if (!is_close(cell_mean, ((double *) data_dest)[ind + i])) {
                printf("i: %d, cell_mean %.9f, dest %.9f", ind + i, cell_mean, ((double *) data_dest)[ind + i]);
                printf("\n Decompressed data differs from original!\n");
                return -1;
              }
            }
            break;
          default :
            break;
        }
      }
    }
  }
  csize_f = csize_f / nchunks;

  free(data_in);
  free(data_out);
  free(data_dest);
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  free(shape);
  free(chunkshape);
  free(blockshape);

  printf("Successful roundtrip!\n");
  printf("Compression: %d -> %" PRId64 " (%.1fx)\n", chunksize, csize_f, (1. * chunksize) / (double) csize_f);
  return (int) (chunksize - csize_f);
}


int rows_matches() {
  int8_t ndim = 1;
  int typesize = 8;
  int64_t shape[] = {512};
  int32_t chunkshape[] = {32};
  int32_t blockshape[] = {16};
  int64_t isize = 1;
  for (int i = 0; i < ndim; ++i) {
    isize *= (int) (shape[i]);
  }
  int64_t nbytes = typesize * isize;
  double *data = malloc(nbytes);
  for (int64_t i = 0; i < isize; i += 4) {
    if ((i <= 20) || ((i >= 48) && (i <= 68)) || ((i >= 96) && (i <= 116))) {
      data[i] = 0;
      data[i + 1] = 1;
      data[i + 2] = 2;
      data[i + 3] = 3;
    } else if (((i >= 24) && (i <= 44)) || ((i >= 72) && (i <= 92)) || ((i >= 120) && (i <= 140))) {
      data[i] = i;
      data[i + 1] = i + 1;
      data[i + 2] = i + 2;
      data[i + 3] = i + 3;
    } else {
      data[i] = i;
      data[i + 1] = i;
      data[i + 2] = i;
      data[i + 3] = i;
    }
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  blosc2_storage b2_storage = {.cparams=&cparams};
  b2_storage.contiguous = true;

  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, ndim, shape, chunkshape, blockshape, NULL, 0,
                                        NULL, 0);

  b2nd_array_t *arr;
  BLOSC_ERROR(b2nd_from_cbuffer(ctx, &arr, data, nbytes));
  blosc2_schunk *schunk = arr->sc;

  /* Run the test. */
  int result = test_ndmean(schunk);
  BLOSC_ERROR(b2nd_free_ctx(ctx));
  BLOSC_ERROR(b2nd_free(arr));
  free(data);
  return result;
}

int same_cells() {
  int8_t ndim = 1;
  int typesize = 8;
  int64_t shape[] = {512};
  int32_t chunkshape[] = {32};
  int32_t blockshape[] = {16};
  int64_t isize = 1;
  for (int i = 0; i < ndim; ++i) {
    isize *= (int) (shape[i]);
  }
  int64_t nbytes = typesize * isize;
  double *data = malloc(nbytes);
  for (int64_t i = 0; i < isize; i += 4) {
    data[i] = 0;
    data[i + 1] = 1111111;
    data[i + 2] = 2;
    data[i + 3] = 1111111;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  blosc2_storage b2_storage = {.cparams=&cparams};
  b2_storage.contiguous = true;

  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, ndim, shape, chunkshape, blockshape, NULL, 0,
                                        NULL, 0);

  b2nd_array_t *arr;
  BLOSC_ERROR(b2nd_from_cbuffer(ctx, &arr, data, nbytes));
  blosc2_schunk *schunk = arr->sc;

  /* Run the test. */
  int result = test_ndmean(schunk);
  BLOSC_ERROR(b2nd_free_ctx(ctx));
  BLOSC_ERROR(b2nd_free(arr));
  free(data);
  return result;
}

int some_matches() {
  int8_t ndim = 1;
  int typesize = 8;
  int64_t shape[] = {512};
  int32_t chunkshape[] = {48};
  int32_t blockshape[] = {14};
  int64_t isize = 1;
  for (int i = 0; i < ndim; ++i) {
    isize *= (int) (shape[i]);
  }
  int64_t nbytes = typesize * isize;
  double *data = malloc(nbytes);
  for (int64_t i = 0; i < (isize / 2); i++) {
    data[i] = (double) i;
  }
  for (int64_t i = (isize / 2); i < isize; i++) {
    data[i] = (double) 1;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  blosc2_storage b2_storage = {.cparams=&cparams};
  b2_storage.contiguous = true;

  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, ndim, shape, chunkshape, blockshape, NULL, 0,
                                        NULL, 0);

  b2nd_array_t *arr;
  BLOSC_ERROR(b2nd_from_cbuffer(ctx, &arr, data, nbytes));
  blosc2_schunk *schunk = arr->sc;

  /* Run the test. */
  int result = test_ndmean(schunk);
  BLOSC_ERROR(b2nd_free_ctx(ctx));
  BLOSC_ERROR(b2nd_free(arr));
  free(data);
  return result;
}


int main(void) {

  int result;
  blosc2_init();
  result = rows_matches();
  printf("2_rows_matches: %d obtained \n \n", result);
  if (result <= 0)
    return -1;
  result = same_cells();
  printf("same_cells: %d obtained \n \n", result);
  if (result <= 0)
    return -1;
  result = some_matches();
  printf("some_matches: %d obtained \n \n", result);
  if (result <= 0)
    return -1;
  blosc2_destroy();
  return BLOSC2_ERROR_SUCCESS;
}
