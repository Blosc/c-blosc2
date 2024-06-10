/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Unit tests for BLOSC_COMPRESSOR environment variable in Blosc.

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"

int tests_run = 0;

/* Global vars */
void *src, *srcmasked, *srcmasked2, *dest, *dest2;
int nbytes, cbytes;
int16_t nthreads = 1;
int clevel = 1;
int doshuffle = 1;
#define size (1000 * 1000)
#define typesize 8
const int bytesize = size * typesize;
const int blocksize = 32 * 1024;
bool *maskout;
bool *maskout2;
int nblocks;


// Check decompression without mask
static char *test_nomask(void) {
  blosc2_context *dctx = blosc2_create_dctx(BLOSC2_DPARAMS_DEFAULTS);
  nbytes = blosc2_decompress_ctx(dctx, dest, cbytes, dest2, bytesize);
  blosc2_free_ctx(dctx);

  mu_assert("ERROR: nbytes is not correct", nbytes == bytesize);

  int64_t* _src = src;
  int64_t* _dst = dest2;
  for (int i = 0; i < size; i++) {
      mu_assert("ERROR: wrong values in dest", _dst[i] == _src[i]);
  }
  return 0;
}


// Check decompression with mask
static char *test_mask(void) {
  blosc2_context *dctx = blosc2_create_dctx(BLOSC2_DPARAMS_DEFAULTS);

  memset(dest2, 0, bytesize);
  mu_assert("ERROR: setting maskout", blosc2_set_maskout(dctx, maskout, nblocks) == 0);
  nbytes = blosc2_decompress_ctx(dctx, dest, cbytes, dest2, bytesize);
  blosc2_free_ctx(dctx);
  mu_assert("ERROR: nbytes is not correct", nbytes == bytesize);

  int64_t* _src = srcmasked;
  int64_t* _dst = dest2;
  for (int i = 0; i < size; i++) {
    mu_assert("ERROR: wrong values in dest", _dst[i] == _src[i]);
  }
  return 0;
}


// Check decompression with mask, and no mask afterwards
static char *test_mask_nomask(void) {
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = nthreads;
  blosc2_context *dctx = blosc2_create_dctx(dparams);

  memset(dest2, 0, bytesize);
  mu_assert("ERROR: setting maskout", blosc2_set_maskout(dctx, maskout, nblocks) == 0);
  nbytes = blosc2_decompress_ctx(dctx, dest, cbytes, dest2, bytesize);
  mu_assert("ERROR: nbytes is not correct w/ mask", nbytes == bytesize);

  int64_t* _src = srcmasked;  // masked source
  int64_t* _dst = dest2;
  for (int i = 0; i < size; i++) {
    mu_assert("ERROR: wrong values in dest", _dst[i] == _src[i]);
  }

  memset(dest2, 0, bytesize);
  nbytes = blosc2_decompress_ctx(dctx, dest, cbytes, dest2, bytesize);
  mu_assert("ERROR: nbytes is not correct w/out mask", nbytes == bytesize);

  _src = src;   // original source
  _dst = dest2;
  for (int i = 0; i < size; i++) {
    mu_assert("ERROR: wrong values in dest", _dst[i] == _src[i]);
  }
  blosc2_free_ctx(dctx);
  return 0;
}


// Check decompression with mask, no mask, and then a different mask at last
static char *test_mask_nomask_mask(void) {
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = nthreads;
  blosc2_context *dctx = blosc2_create_dctx(dparams);

  memset(dest2, 0, bytesize);
  mu_assert("ERROR: setting maskout", blosc2_set_maskout(dctx, maskout, nblocks) == 0);
  nbytes = blosc2_decompress_ctx(dctx, dest, cbytes, dest2, bytesize);
  mu_assert("ERROR: nbytes is not correct w/ mask", nbytes == bytesize);

  int64_t* _src = srcmasked;  // masked source
  int64_t* _dst = dest2;
  for (int i = 0; i < size; i++) {
    mu_assert("ERROR: wrong values in dest", _dst[i] == _src[i]);
  }

  memset(dest2, 0, bytesize);
  nbytes = blosc2_decompress_ctx(dctx, dest, cbytes, dest2, bytesize);
  mu_assert("ERROR: nbytes is not correct w/out mask", nbytes == bytesize);

  _src = src;   // original source
  _dst = dest2;
  for (int i = 0; i < size; i++) {
    mu_assert("ERROR: wrong values in dest", _dst[i] == _src[i]);
  }

  memset(dest2, 0, bytesize);
  mu_assert("ERROR: setting maskout", blosc2_set_maskout(dctx, maskout2, nblocks) == 0);
  nbytes = blosc2_decompress_ctx(dctx, dest, cbytes, dest2, bytesize);
  mu_assert("ERROR: nbytes is not correct w/out mask", nbytes == bytesize);

  _src = srcmasked2;  // masked source
  _dst = dest2;
  for (int i = 0; i < size; i++) {
    mu_assert("ERROR: wrong values in dest", _dst[i] == _src[i]);
  }
  blosc2_free_ctx(dctx);
  return 0;
}


static char *all_tests(void) {
  nthreads = 1;
  mu_run_test(test_nomask);
  nthreads = 2;
  mu_run_test(test_nomask);
  nthreads = 1;
  mu_run_test(test_mask);
  nthreads = 2;
  mu_run_test(test_mask);
  nthreads = 1;
  mu_run_test(test_mask_nomask);
  nthreads = 2;
  mu_run_test(test_mask_nomask);
  nthreads = 1;
  mu_run_test(test_mask_nomask_mask);
  nthreads = 2;  // TODO: fix this case
  mu_run_test(test_mask_nomask_mask);

  return 0;
}

#define BUFFER_ALIGN_SIZE   32

int main(void) {
  blosc2_init();

  char *result;

  nblocks = bytesize / blocksize;
  if (nblocks * blocksize < bytesize) {
    nblocks++;
  }

  /* Initialize buffers */
  src = blosc_test_malloc(BUFFER_ALIGN_SIZE, bytesize);
  srcmasked = blosc_test_malloc(BUFFER_ALIGN_SIZE, bytesize);
  srcmasked2 = blosc_test_malloc(BUFFER_ALIGN_SIZE, bytesize);
  dest = blosc_test_malloc(BUFFER_ALIGN_SIZE, bytesize + BLOSC2_MAX_OVERHEAD);
  dest2 = blosc_test_malloc(BUFFER_ALIGN_SIZE, bytesize);
  maskout = blosc_test_malloc(BUFFER_ALIGN_SIZE, nblocks);
  maskout2 = blosc_test_malloc(BUFFER_ALIGN_SIZE, nblocks);

  int64_t* _src = src;
  for (int i=0; i < size; i++) {
    _src[i] = (int64_t)i;
  }

  // Get a compressed chunk
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.blocksize = blocksize;
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  cbytes = blosc2_compress_ctx(cctx, src, bytesize, dest, bytesize + BLOSC2_MAX_OVERHEAD);
  blosc2_free_ctx(cctx);

  // Build a mask
  for (int i=0; i < nblocks; i++) {
    maskout[i] = (i % 3) ? true : false;
  }

  // Initialize masked values
  int64_t* _srcmasked = srcmasked;
  for (int64_t i=0; i < size; i++) {
    _srcmasked[i] = maskout[(i * typesize) / blocksize] ? 0 : i;
  }

  // Build a second mask
  for (int i=0; i < nblocks; i++) {
    maskout2[i] = (i % 2) ? true : false;
  }

  // Initialize masked values
  int64_t* _srcmasked2 = srcmasked2;
  for (int64_t i=0; i < size; i++) {
    _srcmasked2[i] = maskout2[(i * typesize) / blocksize] ? 0 : i;
  }

  /* Run all the suite */
  result = all_tests();
  if (result != 0) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  blosc_test_free(src);
  blosc_test_free(srcmasked);
  blosc_test_free(srcmasked2);
  blosc_test_free(dest);
  blosc_test_free(dest2);
  blosc_test_free(maskout);
  blosc_test_free(maskout2);

  blosc2_destroy();
  return result != 0;
}
