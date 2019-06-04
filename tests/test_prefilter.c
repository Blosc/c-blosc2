/*
  Copyright (C) 2019  The Blosc Developers
  http://blosc.org
  License: BSD (see LICENSE.txt)

*/

#include "test_common.h"

int tests_run = 0;

#define SIZE 500 * 1000
#define NTHREADS 2

// Global vars
blosc2_cparams cparams;
blosc2_dparams dparams;
blosc2_context *cctx, *dctx;
static int32_t data[SIZE];
static int32_t data2[SIZE];
static int32_t data_out[SIZE];
static int32_t data_dest[SIZE];
size_t isize = SIZE * sizeof(int32_t), osize = SIZE * sizeof(int32_t);
int dsize = SIZE * sizeof(int32_t), csize;


int prefilter_func(prefilter_params *pparams) {
  int nelems = pparams->out_size / pparams->out_typesize;
  if (pparams->ninputs == 1) {
    for (int i = 0; i < nelems; i++) {
      ((int32_t*)(pparams->out))[i] = ((int32_t*)(pparams->inputs[0]))[i];
    }
  }
  else if (pparams->ninputs == 2) {
    for (int i = 0; i < nelems; i++) {
      ((int32_t *) (pparams->out))[i] = ((int32_t *)(pparams->inputs[0]))[i] + ((int32_t *) (pparams->inputs[1]))[i];
    }
  }
  else {
    return 1;
  }
  return 0;
}


static char *test_prefilter1() {
  // Set some prefilter parameters and function
  cparams.prefilter = (prefilter_fn)prefilter_func;
  prefilter_params pparams;
  pparams.ninputs = 1;
  pparams.inputs[0] = (uint8_t*)data;
  pparams.input_typesizes[0] = cparams.typesize;
  cparams.pparams = &pparams;
  cctx = blosc2_create_cctx(cparams);

  csize = blosc2_compress_ctx(cctx, isize, data, data_out, osize);
  mu_assert("Buffer is uncompressible", csize != 0);
  mu_assert("Compression error", csize > 0);

  /* Create a context for decompression */
  dparams = BLOSC_DPARAMS_DEFAULTS;
  dparams.nthreads = NTHREADS;
  dctx = blosc2_create_dctx(dparams);

  /* Decompress  */
  dsize = blosc2_decompress_ctx(dctx, data_out, data_dest, (size_t)dsize);
  mu_assert("Decompression error", dsize > 0);

  for (int i = 0; i < SIZE; i++) {
    mu_assert("Decompressed data differs from original!", data[i] == data_dest[i]);
  }

  return 0;
}


static char *test_prefilter2() {
  // Set some prefilter parameters and function
  cparams.prefilter = (prefilter_fn)prefilter_func;
  prefilter_params pparams;
  pparams.ninputs = 2;
  pparams.inputs[0] = (uint8_t*)data;
  pparams.inputs[1] = (uint8_t*)data2;
  pparams.input_typesizes[0] = cparams.typesize;
  pparams.input_typesizes[1] = cparams.typesize;
  cparams.pparams = &pparams;
  cctx = blosc2_create_cctx(cparams);

  csize = blosc2_compress_ctx(cctx, isize, data, data_out, osize);
  mu_assert("Buffer is uncompressible", csize != 0);
  mu_assert("Compression error", csize > 0);

  /* Create a context for decompression */
  dparams = BLOSC_DPARAMS_DEFAULTS;
  dparams.nthreads = NTHREADS;
  dctx = blosc2_create_dctx(dparams);

  /* Decompress  */
  dsize = blosc2_decompress_ctx(dctx, data_out, data_dest, (size_t)dsize);
  mu_assert("Decompression error", dsize > 0);

  for (int i = 0; i < SIZE; i++) {
    if ((data[i] + data2[i]) != data_dest[i]) {
      printf("Error en pos '%d': (%d + %d) != %d\n", i, data[i], data2[i], data_dest[i]);
    }
    mu_assert("Decompressed data differs from original!", (data[i] + data2[i]) == data_dest[i]);
  }

  return 0;
}


static char *all_tests() {
  mu_run_test(test_prefilter1);
  mu_run_test(test_prefilter2);

  return 0;
}


int main() {
  /* Initialize inputs */
  for (int i = 0; i < SIZE; i++) {
    data[i] = i;
    data2[i] = i * 2;
  }

  /* Create a context for compression */
  cparams = BLOSC_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.filters[BLOSC_MAX_FILTERS - 1] = BLOSC_SHUFFLE;
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS;

  /* Run all the suite */
  char* result = all_tests();
  if (result != 0) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);


  /* Free resources */
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);

  return result != 0;
}
