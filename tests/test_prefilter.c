/*
  Copyright (C) 2019  The Blosc Developers
  http://blosc.org
  License: BSD (see LICENSE.txt)

*/

#include "test_common.h"

int tests_run = 0;

#define SIZE 500 * 1000
#define NTHREADS 2

typedef struct {
    int ninputs;
    uint8_t *inputs[2];
    int32_t input_typesizes[2];
} test_pparams;

// Global vars
blosc2_cparams cparams;
blosc2_dparams dparams;
blosc2_context *cctx, *dctx;
static int32_t data[SIZE];
static int32_t data2[SIZE];
static int32_t data_out[SIZE + BLOSC_MAX_OVERHEAD / sizeof(int32_t)];
static int32_t data_dest[SIZE];
int32_t isize = SIZE * sizeof(int32_t);
int32_t osize = SIZE * sizeof(int32_t) + BLOSC_MAX_OVERHEAD;
int dsize = SIZE * sizeof(int32_t);
int csize;


int prefilter_func(blosc2_prefilter_params *pparams) {
  test_pparams *tpparams = pparams->user_data;
  int nelems = pparams->out_size / pparams->out_typesize;
  if (tpparams->ninputs == 1) {
    int32_t *input0 = ((int32_t *)(tpparams->inputs[0] + pparams->out_offset));
    for (int i = 0; i < nelems; i++) {
      ((int32_t*)(pparams->out))[i] = input0[i];
    }
  }
  else if (tpparams->ninputs == 2) {
    int32_t *input0 = ((int32_t *)(tpparams->inputs[0] + pparams->out_offset));
    int32_t *input1 = ((int32_t *)(tpparams->inputs[1] + pparams->out_offset));
    for (int i = 0; i < nelems; i++) {
      ((int32_t *) (pparams->out))[i] = input0[i] + input1[i];
    }
  }
  else {
    return 1;
  }
  return 0;
}


static char *test_prefilter1(void) {
  // Set some prefilter parameters and function
  cparams.prefilter = (blosc2_prefilter_fn)prefilter_func;
  // We need to zero the contents of the pparams.  TODO: make a constructor for ppparams.
  blosc2_prefilter_params pparams = {0};
  test_pparams tpparams = {0};
  tpparams.ninputs = 1;
  tpparams.inputs[0] = (uint8_t*)data;
  tpparams.input_typesizes[0] = cparams.typesize;
  pparams.user_data = (void*)&tpparams;
  cparams.pparams = &pparams;
  cctx = blosc2_create_cctx(cparams);

  csize = blosc2_compress_ctx(cctx, data, isize, data_out, (size_t)osize);
  mu_assert("Compression error", csize > 0);

  /* Create a context for decompression */
  dctx = blosc2_create_dctx(dparams);

  /* Decompress  */
  dsize = blosc2_decompress_ctx(dctx, data_out, csize, data_dest, (size_t)dsize);
  mu_assert("Decompression error", dsize > 0);

  for (int i = 0; i < SIZE; i++) {
    mu_assert("Decompressed data differs from original!", data[i] == data_dest[i]);
  }

  /* Free resources */
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);

  return 0;
}


static char *test_prefilter2(void) {
  // Set some prefilter parameters and function
  cparams.prefilter = (blosc2_prefilter_fn)prefilter_func;
  // We need to zero the contents of the pparams.  TODO: make a constructor for ppparams.
  blosc2_prefilter_params pparams = {0};
  test_pparams tpparams = {0};
  tpparams.ninputs = 2;
  tpparams.inputs[0] = (uint8_t*)data;
  tpparams.inputs[1] = (uint8_t*)data2;
  tpparams.input_typesizes[0] = cparams.typesize;
  tpparams.input_typesizes[1] = cparams.typesize;
  pparams.user_data = (void*)&tpparams;
  cparams.pparams = &pparams;
  cctx = blosc2_create_cctx(cparams);

  csize = blosc2_compress_ctx(cctx, data, isize, data_out, (size_t)osize);
  mu_assert("Buffer is uncompressible", csize != 0);
  mu_assert("Compression error", csize > 0);

  /* Create a context for decompression */
  dctx = blosc2_create_dctx(dparams);

  /* Decompress  */
  dsize = blosc2_decompress_ctx(dctx, data_out, csize, data_dest, (size_t)dsize);
  mu_assert("Decompression error", dsize > 0);

  for (int i = 0; i < SIZE; i++) {
    if ((data[i] + data2[i]) != data_dest[i]) {
      printf("Error in pos '%d': (%d + %d) != %d\n", i, data[i], data2[i], data_dest[i]);
    }
    mu_assert("Decompressed data differs from original!", (data[i] + data2[i]) == data_dest[i]);
  }

  /* Free resources */
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);

  return 0;
}


static char *all_tests(void) {
  // Check with an assortment of clevels and nthreads
  cparams.clevel = 0;
  cparams.nthreads = 1;
  dparams.nthreads = NTHREADS;
  mu_run_test(test_prefilter1);
  cparams.clevel = 1;
  cparams.nthreads = 1;
  mu_run_test(test_prefilter1);
  cparams.clevel = 7;
  cparams.nthreads = NTHREADS;
  mu_run_test(test_prefilter1);
  cparams.clevel = 0;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = 1;
  mu_run_test(test_prefilter2);
  cparams.clevel = 5;
  cparams.nthreads = 1;
  mu_run_test(test_prefilter2);
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  mu_run_test(test_prefilter2);

  return 0;
}


int main(void) {
  /* Initialize inputs */
  for (int i = 0; i < SIZE; i++) {
    data[i] = i;
    data2[i] = i * 2;
  }

  install_blosc_callback_test(); /* optionally install callback test */

  /* Create a context for compression */
  cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_SHUFFLE;
  dparams = BLOSC2_DPARAMS_DEFAULTS;

  /* Run all the suite */
  char* result = all_tests();
  if (result != 0) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  return result != 0;
}
