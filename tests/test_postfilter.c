/*
  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
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
} test_postparams;

// Global vars
blosc2_cparams cparams;
blosc2_dparams dparams;
int32_t* data;
int32_t* data2;
int32_t* data_out;
int32_t* data_dest;
int32_t isize = SIZE * sizeof(int32_t);
int32_t osize = SIZE * sizeof(int32_t) + BLOSC_MAX_OVERHEAD;
int dsize = SIZE * sizeof(int32_t);
int csize;
bool data_cnt = false;


int init_data(void) {
  data = malloc(SIZE * sizeof(int32_t));
  data2 = malloc(SIZE * sizeof(int32_t));
  data_out = malloc(SIZE * sizeof(int32_t) + BLOSC_MAX_OVERHEAD);
  data_dest = malloc(SIZE * sizeof(int32_t));

  /* Initialize inputs */
  for (int i = 0; i < SIZE; i++) {
    data[i] = data_cnt ? 0 : i;  // important to have a zero here for testing special chunks!
    data2[i] = data_cnt ? 2 : i * 2;
  }

  return 0;
}


void free_data(void) {
  free(data);
  free(data2);
  free(data_out);
  free(data_dest);
}


int postfilter_func(blosc2_postfilter_params *postparams) {
  test_postparams *tpostparams = postparams->user_data;
  int nelems = postparams->size / postparams->typesize;
  if (tpostparams->ninputs == 0) {
    int32_t *input0 = (int32_t *)postparams->in;
    for (int i = 0; i < nelems; i++) {
      ((int32_t*)(postparams->out))[i] = input0[i] * 2;
    }
  }
  else if (tpostparams->ninputs == 1) {
    int32_t *input0 = ((int32_t *)(tpostparams->inputs[0] + postparams->offset));
    for (int i = 0; i < nelems; i++) {
      ((int32_t*)(postparams->out))[i] = input0[i] * 3;
    }
  }
  else if (tpostparams->ninputs == 2) {
    int32_t *input0 = ((int32_t *)(tpostparams->inputs[0] + postparams->offset));
    int32_t *input1 = ((int32_t *)(tpostparams->inputs[1] + postparams->offset));
    for (int i = 0; i < nelems; i++) {
      ((int32_t *) (postparams->out))[i] = input0[i] + input1[i];
    }
  }
  else {
    return 1;
  }
  return 0;
}


static char *test_postfilter0(void) {
  blosc2_context *cctx, *dctx;
  cctx = blosc2_create_cctx(cparams);
  init_data();

  csize = blosc2_compress_ctx(cctx, data, isize, data_out, osize);
  mu_assert("Compression error", csize > 0);

  // Set some postfilter parameters and function
  dparams.postfilter = (blosc2_postfilter_fn)postfilter_func;
  // We need to zero the contents of the postparams.  TODO: make a constructor for postparams?
  blosc2_postfilter_params postparams = {0};
  // In this case we are not passing any additional input in tpostparams
  test_postparams tpostparams = {0};
  postparams.user_data = (void*)&tpostparams;
  dparams.postparams = &postparams;

  /* Create a context for decompression */
  dctx = blosc2_create_dctx(dparams);

  /* Decompress  */
  dsize = blosc2_decompress_ctx(dctx, data_out, csize, data_dest, dsize);
  mu_assert("Decompression error", dsize >= 0);
  for (int i = 0; i < SIZE; i++) {
    mu_assert("Decompressed data differs from original!", data[i] * 2 == data_dest[i]);
  }

  /* getitem */
  int start = 3;
  int nitems = 10;
  int dsize_ = blosc2_getitem_ctx(dctx, data_out, csize, start, nitems, data_dest, dsize);
  mu_assert("getitem error", dsize_ >= 0);
  for (int i = start; i < start + nitems; i++) {
    mu_assert("getitem data differs from original!", data[i] * 2 == data_dest[i - start]);
  }

  /* Free resources */
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  free_data();

  return 0;
}


static char *test_postfilter1(void) {
  blosc2_context *cctx, *dctx;
  cctx = blosc2_create_cctx(cparams);
  init_data();

  csize = blosc2_compress_ctx(cctx, data, isize, data_out, osize);
  mu_assert("Compression error", csize > 0);

  // Set some postfilter parameters and function
  dparams.postfilter = (blosc2_postfilter_fn)postfilter_func;
  // We need to zero the contents of the postparams.  TODO: make a constructor for postparams?
  blosc2_postfilter_params postparams = {0};
  test_postparams tpostparams = {0};
  tpostparams.ninputs = 1;
  tpostparams.inputs[0] = (uint8_t*)data;
  tpostparams.input_typesizes[0] = cparams.typesize;
  postparams.user_data = (void*)&tpostparams;
  dparams.postparams = &postparams;

  /* Create a context for decompression */
  dctx = blosc2_create_dctx(dparams);

  /* Decompress  */
  dsize = blosc2_decompress_ctx(dctx, data_out, csize, data_dest, dsize);
  mu_assert("getitem error", dsize >= 0);
  for (int i = 0; i < SIZE; i++) {
    mu_assert("getitem data differs from original!", data[i] * 3 == data_dest[i]);
  }

  /* getitem */
  int start = 3;
  int nitems = SIZE - start;
  int dsize_ = blosc2_getitem_ctx(dctx, data_out, csize, start, nitems, data_dest, dsize);
  mu_assert("getitem error", dsize_ >= 0);
  for (int i = start; i < start + nitems; i++) {
    mu_assert("getitem data differs from original!", data[i] * 3 == data_dest[i - start]);
  }

  /* Free resources */
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  free_data();

  return 0;
}


static char *test_postfilter2(void) {
  blosc2_context *cctx, *dctx;
  cctx = blosc2_create_cctx(cparams);
  init_data();

  csize = blosc2_compress_ctx(cctx, data, isize, data_out, osize);
  mu_assert("Buffer is uncompressible", csize != 0);
  mu_assert("Compression error", csize > 0);

  // Set some postfilter parameters and function
  dparams.postfilter = (blosc2_postfilter_fn)postfilter_func;
  // We need to zero the contents of the postparams.  TODO: make a constructor for ppostparams.
  blosc2_postfilter_params postparams = {0};
  test_postparams tpostparams = {0};
  tpostparams.ninputs = 2;
  tpostparams.inputs[0] = (uint8_t*)data;
  tpostparams.inputs[1] = (uint8_t*)data2;
  tpostparams.input_typesizes[0] = cparams.typesize;
  tpostparams.input_typesizes[1] = cparams.typesize;
  postparams.user_data = (void*)&tpostparams;
  dparams.postparams = &postparams;

  /* Create a context for decompression */
  dctx = blosc2_create_dctx(dparams);

  /* Decompress  */
  dsize = blosc2_decompress_ctx(dctx, data_out, csize, data_dest, dsize);
  mu_assert("Decompression error", dsize >= 0);
  for (int i = 0; i < SIZE; i++) {
    if ((data[i] + data2[i]) != data_dest[i]) {
      printf("Error in pos '%d': (%d + %d) != %d\n", i, data[i], data2[i], data_dest[i]);
    }
    mu_assert("Decompressed data differs from original!",
              (data[i] + data2[i]) == data_dest[i]);
  }

  /* getitem */
  int start = 0;
  int nitems = SIZE - start;
  int dsize_ = blosc2_getitem_ctx(dctx, data_out, csize, start, nitems, data_dest, dsize);
  mu_assert("getitem error", dsize_ >= 0);
  for (int i = start; i < start + nitems; i++) {
    mu_assert("getitem data differs from original!",
              (data[i] + data2[i]) == data_dest[i - start]);
  }

  /* Free resources */
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  free_data();

  return 0;
}


static char *all_tests(void) {
  // Check with an assortment of clevels and nthreads

  cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_SHUFFLE;
  cparams.clevel = 0;
  cparams.nthreads = 1;
  dparams.nthreads = NTHREADS;
  mu_run_test(test_postfilter0);
  cparams.clevel = 1;
  cparams.nthreads = 1;
  dparams.nthreads = 1;
  mu_run_test(test_postfilter0);
  cparams.clevel = 7;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = 1;
  mu_run_test(test_postfilter0);
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  mu_run_test(test_postfilter0);

  cparams.clevel = 0;
  cparams.nthreads = 1;
  dparams.nthreads = NTHREADS;
  mu_run_test(test_postfilter1);
  cparams.clevel = 1;
  cparams.nthreads = 1;
  mu_run_test(test_postfilter1);
  cparams.clevel = 7;
  cparams.nthreads = NTHREADS;
  mu_run_test(test_postfilter1);

  cparams.clevel = 0;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = 1;
  mu_run_test(test_postfilter2);

  // Activate special chunks from now on
  data_cnt = true;

  cparams.clevel = 5;
  cparams.nthreads = 1;
  mu_run_test(test_postfilter0);
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  mu_run_test(test_postfilter2);

  // This exposes a bug that has been fixed
  cparams.clevel = 9;
  cparams.nthreads = 1;
  dparams.nthreads = 1;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_NOSHUFFLE;
  mu_run_test(test_postfilter0);

  return 0;
}


int main(void) {
  install_blosc_callback_test(); /* optionally install callback test */

  /* Create a context for compression */
  cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.blocksize = 2048;
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
