/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include "test_common.h"

int tests_run = 0;

#define SIZE (500 * 1000)
#define NTHREADS 2

typedef struct {
    int ninputs;
    uint8_t *inputs[2];
    int32_t input_typesizes[2];
} test_preparams;

// Global vars
blosc2_cparams cparams;
blosc2_dparams dparams;
blosc2_context *cctx, *dctx;
static int32_t data[SIZE];
static int32_t data2[SIZE];
static int32_t data_out[SIZE + BLOSC2_MAX_OVERHEAD / sizeof(int32_t)];
static int32_t data_dest[SIZE];
int32_t isize = SIZE * sizeof(int32_t);
int32_t osize = SIZE * sizeof(int32_t) + BLOSC2_MAX_OVERHEAD;
int dsize = SIZE * sizeof(int32_t);
int csize;


int prefilter_func(blosc2_prefilter_params *preparams) {
  test_preparams *tpreparams = preparams->user_data;
  int nelems = preparams->output_size / preparams->output_typesize;
  if (tpreparams->ninputs == 0) {
    int32_t *input0 = (int32_t *)preparams->input;
    for (int i = 0; i < nelems; i++) {
      ((int32_t*)(preparams->output))[i] = input0[i] * 2;
    }
  }
  else if (tpreparams->ninputs == 1) {
    int32_t *input0 = ((int32_t *)(tpreparams->inputs[0] + preparams->output_offset));
    for (int i = 0; i < nelems; i++) {
      ((int32_t*)(preparams->output))[i] = input0[i] * 3;
    }
  }
  else if (tpreparams->ninputs == 2) {
    int32_t *input0 = ((int32_t *)(tpreparams->inputs[0] + preparams->output_offset));
    int32_t *input1 = ((int32_t *)(tpreparams->inputs[1] + preparams->output_offset));
    for (int i = 0; i < nelems; i++) {
      ((int32_t *) (preparams->output))[i] = input0[i] + input1[i];
    }
  }
  else {
    return 1;
  }
  return 0;
}


static char *test_prefilter0(void) {
  // Set some prefilter parameters and function
  cparams.prefilter = (blosc2_prefilter_fn)prefilter_func;
  // We need to zero the contents of the preparams.  TODO: make a constructor for ppreparams.
  blosc2_prefilter_params preparams = {0};
  test_preparams tpreparams = {0};
  preparams.user_data = (void*)&tpreparams;
  cparams.preparams = &preparams;
  cctx = blosc2_create_cctx(cparams);

  csize = blosc2_compress_ctx(cctx, data, isize, data_out, osize);
  mu_assert("Compression error", csize > 0);

  /* Create a context for decompression */
  dctx = blosc2_create_dctx(dparams);

  /* Decompress  */
  dsize = blosc2_decompress_ctx(dctx, data_out, csize, data_dest, dsize);
  mu_assert("Decompression error", dsize >= 0);

  for (int i = 0; i < SIZE; i++) {
    mu_assert("Decompressed data differs from original!", data[i] * 2 == data_dest[i]);
  }

  /* Free resources */
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);

  return 0;
}


static char *test_prefilter1(void) {
  // Set some prefilter parameters and function
  cparams.prefilter = (blosc2_prefilter_fn)prefilter_func;
  // We need to zero the contents of the preparams.  TODO: make a constructor for ppreparams.
  blosc2_prefilter_params preparams = {0};
  test_preparams tpreparams = {0};
  tpreparams.ninputs = 1;
  tpreparams.inputs[0] = (uint8_t*)data;
  tpreparams.input_typesizes[0] = cparams.typesize;
  preparams.user_data = (void*)&tpreparams;
  cparams.preparams = &preparams;
  cctx = blosc2_create_cctx(cparams);

  csize = blosc2_compress_ctx(cctx, data, isize, data_out, osize);
  mu_assert("Compression error", csize > 0);

  /* Create a context for decompression */
  dctx = blosc2_create_dctx(dparams);

  /* Decompress  */
  dsize = blosc2_decompress_ctx(dctx, data_out, csize, data_dest, dsize);
  mu_assert("Decompression error", dsize >= 0);

  for (int i = 0; i < SIZE; i++) {
    mu_assert("Decompressed data differs from original!", data[i] * 3 == data_dest[i]);
  }

  /* Free resources */
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);

  return 0;
}


static char *test_prefilter2(void) {
  // Set some prefilter parameters and function
  cparams.prefilter = (blosc2_prefilter_fn)prefilter_func;
  // We need to zero the contents of the preparams.  TODO: make a constructor for ppreparams.
  blosc2_prefilter_params preparams = {0};
  test_preparams tpreparams = {0};
  tpreparams.ninputs = 2;
  tpreparams.inputs[0] = (uint8_t*)data;
  tpreparams.inputs[1] = (uint8_t*)data2;
  tpreparams.input_typesizes[0] = cparams.typesize;
  tpreparams.input_typesizes[1] = cparams.typesize;
  preparams.user_data = (void*)&tpreparams;
  cparams.preparams = &preparams;
  cctx = blosc2_create_cctx(cparams);

  csize = blosc2_compress_ctx(cctx, data, isize, data_out, osize);
  mu_assert("Buffer is incompressible", csize != 0);
  mu_assert("Compression error", csize > 0);

  /* Create a context for decompression */
  dctx = blosc2_create_dctx(dparams);

  /* Decompress  */
  dsize = blosc2_decompress_ctx(dctx, data_out, csize, data_dest, dsize);
  mu_assert("Decompression error", dsize >= 0);

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
  mu_run_test(test_prefilter0);
  cparams.clevel = 1;
  cparams.nthreads = 1;
  mu_run_test(test_prefilter0);
  cparams.clevel = 7;
  cparams.nthreads = NTHREADS;
  mu_run_test(test_prefilter0);
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  mu_run_test(test_prefilter0);

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
  blosc2_init();

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

  blosc2_destroy();
  return result != 0;
}
