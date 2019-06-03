/*
  Copyright (C) 2019  The Blosc Developers
  http://blosc.org
  License: BSD (see LICENSE.txt)

*/

#include <stdio.h>
#include "test_common.h"

#define SIZE 500 * 1000
#define NTHREADS 2

int prefilter_example(prefilter_params* pparams) {
  if (pparams->ninputs == 1) {
    memcpy(pparams->out, pparams->inputs[0], pparams->out_size);
  }
  else {
    return 1;
  }
  return 0;
}

int main() {
  static int32_t data[SIZE];
  static int32_t data_out[SIZE];
  static int32_t data_dest[SIZE];
  int32_t data_subset[5];
  int32_t data_subset_ref[5] = {5, 6, 7, 8, 9};
  size_t isize = SIZE * sizeof(int32_t), osize = SIZE * sizeof(int32_t);
  int dsize = SIZE * sizeof(int32_t), csize;
  int i, ret;
  blosc2_cparams cparams = BLOSC_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC_DPARAMS_DEFAULTS;
  blosc2_context *cctx, *dctx;

  /* Initialize dataset */
  for (i = 0; i < SIZE; i++) {
    data[i] = i;
  }

  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  /* Create a context for compression */
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.filters[BLOSC_MAX_FILTERS - 1] = BLOSC_SHUFFLE;
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS;

  // Set some prefilter parameters and function
  cparams.prefilter = (prefilter_fn)prefilter_example;
  prefilter_params pparams;
  pparams.ninputs = 1;
  pparams.inputs[0] = (uint8_t*)data;
  pparams.typesizes[0] = cparams.typesize;
  cparams.pparams = &pparams;

  cctx = blosc2_create_cctx(cparams);

  /* Compress with clevel=5 and shuffle active  */
  csize = blosc2_compress_ctx(cctx, isize, data, data_out, osize);
  if (csize == 0) {
    printf("Buffer is uncompressible.  Giving up.\n");
    return EXIT_FAILURE;
  }
  if (csize < 0) {
    printf("Compression error.  Error code: %d\n", csize);
    return EXIT_FAILURE;
  }

  /* Create a context for decompression */
  dparams.nthreads = NTHREADS;
  dctx = blosc2_create_dctx(dparams);

  ret = blosc2_getitem_ctx(dctx, data_out, 5, 5, data_subset);
  if (ret < 0) {
    printf("Error in blosc2_getitem_ctx().  Giving up.\n");
    return EXIT_FAILURE;
  }

  for (i = 0; i < 5; i++) {
    if (data_subset[i] != data_subset_ref[i]) {
      printf("blosc2_getitem_ctx() fetched data differs from original!\n");
      return EXIT_FAILURE;
    }
  }

  /* Decompress  */
  dsize = blosc2_decompress_ctx(dctx, data_out, data_dest, (size_t)dsize);
  if (dsize < 0) {
    printf("Decompression error.  Error code: %d\n", dsize);
    return EXIT_FAILURE;
  }

  for (i = 0; i < SIZE; i++) {
    if (data[i] != data_dest[i]) {
      printf("Decompressed data differs from original!\n");
      return EXIT_FAILURE;
    }
  }

  /* Free resources */
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);

  return EXIT_SUCCESS;
}
