/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdio.h>
#include "test_common.h"

#define SIZE (500 * 1000)
#define NTHREADS 2


int main(void) {
  blosc2_init();

  int32_t *data = malloc(SIZE * sizeof(int32_t));
  int32_t *data_out = malloc(SIZE * sizeof(int32_t));
  int32_t *data_dest = malloc(SIZE * sizeof(int32_t));
  int32_t data_subset[5];
  int32_t data_subset_ref[5] = {5, 6, 7, 8, 9};
  int32_t isize = SIZE * sizeof(int32_t), osize = SIZE * sizeof(int32_t);
  int dsize = SIZE * sizeof(int32_t), csize;
  int i, ret;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_context *cctx, *dctx;

  /* Initialize dataset */
  for (i = 0; i < SIZE; i++) {
    data[i] = i;
  }

  printf("Blosc version info: %s (%s)\n",
         BLOSC2_VERSION_STRING, BLOSC2_VERSION_DATE);

  install_blosc_callback_test(); /* optionally install callback test */

  /* Create a context for compression */
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_SHUFFLE;
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS;
  cparams.splitmode = BLOSC_AUTO_SPLIT;
  cctx = blosc2_create_cctx(cparams);

  blosc2_cparams cparams2 = {0};
  blosc2_ctx_get_cparams(cctx, &cparams2);

  if (cparams2.clevel != cparams.clevel) {
    printf("Clevels are not equal!");
    free(data);
    free(data_out);
    free(data_dest);
    return EXIT_FAILURE;
  }

  /* Compress with clevel=5 and shuffle active  */
  csize = blosc2_compress_ctx(cctx, data, isize, data_out, osize);
  blosc2_free_ctx(cctx);
  if (csize == 0) {
    printf("Buffer is incompressible.  Giving up.\n");
    free(data);
    free(data_out);
    free(data_dest);
    return EXIT_FAILURE;
  }
  if (csize < 0) {
    printf("Compression error.  Error code: %d\n", csize);
    free(data);
    free(data_out);
    free(data_dest);
    return EXIT_FAILURE;
  }

  /* Create a context for decompression */
  dparams.nthreads = NTHREADS;
  dctx = blosc2_create_dctx(dparams);

  blosc2_dparams dparams2 = {0};
  blosc2_ctx_get_dparams(dctx, &dparams2);

  if (dparams2.nthreads != dparams.nthreads) {
    printf("Nthreads are not equal!");
    free(data);
    free(data_out);
    free(data_dest);
    return EXIT_FAILURE;
  }

  ret = blosc2_getitem_ctx(dctx, data_out, csize, 5, 5, data_subset, sizeof(data_subset));
  if (ret < 0) {
    printf("Error in blosc2_getitem_ctx().  Giving up.\n");
    blosc2_free_ctx(dctx);
    free(data);
    free(data_out);
    free(data_dest);
    return EXIT_FAILURE;
  }

  for (i = 0; i < 5; i++) {
    if (data_subset[i] != data_subset_ref[i]) {
      printf("blosc2_getitem_ctx() fetched data differs from original!\n");
      blosc2_free_ctx(dctx);
      free(data);
      free(data_out);
      free(data_dest);
      return EXIT_FAILURE;
    }
  }

  /* Decompress  */
  dsize = blosc2_decompress_ctx(dctx, data_out, csize, data_dest, dsize);
  blosc2_free_ctx(dctx);
  if (dsize < 0) {
    printf("Decompression error.  Error code: %d\n", dsize);
    free(data);
    free(data_out);
    free(data_dest);
    return EXIT_FAILURE;
  }

  for (i = 0; i < SIZE; i++) {
    if (data[i] != data_dest[i]) {
      printf("Decompressed data differs from original!\n");
      free(data);
      free(data_out);
      free(data_dest);
      return EXIT_FAILURE;
    }
  }

  free(data);
  free(data_out);
  free(data_dest);

  blosc2_destroy();

  return EXIT_SUCCESS;
}
