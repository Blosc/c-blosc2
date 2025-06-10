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
void *src, *srccpy, *dest, *dest2;
int nbytes, cbytes;
int clevel = 1;
int doshuffle = 1;
size_t typesize = 8;
int32_t size = 8 * 1000 * 1000;  /* must be divisible by typesize */


/* Check compressor */
static char *test_compressor(void) {
  const char* compressor;

  /* Before any blosc1_compress() the compressor must be blosclz */
  compressor = blosc1_get_compressor();
  mu_assert("ERROR: get_compressor (compress, before) incorrect",
	    strcmp(compressor, "blosclz") == 0);

  /* Activate the BLOSC_COMPRESSOR variable */
  setenv("BLOSC_COMPRESSOR", "lz4", 0);

  /* Get a compressed buffer */
  cbytes = blosc1_compress(clevel, doshuffle, typesize, size, src,
                           dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cbytes is not correct", cbytes < size);

  compressor = blosc1_get_compressor();
  mu_assert("ERROR: get_compressor (compress, after) incorrect",
	    strcmp(compressor, "lz4") == 0);

  /* Reset envvar */
  unsetenv("BLOSC_COMPRESSOR");
  return 0;
}


/* Check compressing + decompressing */
static char *test_compress_decompress(void) {
  const char* compressor;

  /* Activate the BLOSC_COMPRESSOR variable */
  setenv("BLOSC_COMPRESSOR", "lz4", 0);

  compressor = blosc1_get_compressor();
  mu_assert("ERROR: get_compressor incorrect",
	    strcmp(compressor, "lz4") == 0);

  /* Get a compressed buffer */
  cbytes = blosc1_compress(clevel, doshuffle, typesize, size, src,
                           dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cbytes is not correct", cbytes < size);

  compressor = blosc1_get_compressor();
  mu_assert("ERROR: get_compressor incorrect",
	    strcmp(compressor, "lz4") == 0);

  /* Decompress the buffer */
  nbytes = blosc1_decompress(dest, dest2, size);
  mu_assert("ERROR: nbytes incorrect(1)", nbytes == size);

  compressor = blosc1_get_compressor();
  mu_assert("ERROR: get_compressor incorrect",
	    strcmp(compressor, "lz4") == 0);

  /* Reset envvar */
  unsetenv("BLOSC_COMPRESSOR");
  return 0;
}


/* Check compression level */
static char *test_clevel(void) {
  int cbytes2;

  /* Get a compressed buffer */
  cbytes = blosc1_compress(clevel, doshuffle, typesize, size, src,
                           dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cbytes is not correct", cbytes < size);

  /* Activate the BLOSC_CLEVEL variable */
  setenv("BLOSC_CLEVEL", "9", 0);
  cbytes2 = blosc1_compress(clevel, doshuffle, typesize, size, src,
                            dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: BLOSC_CLEVEL does not work correctly", cbytes2 != cbytes);

  /* Reset envvar */
  unsetenv("BLOSC_CLEVEL");
  return 0;
}

/* Check noshuffle */
static char *test_noshuffle(void) {
  int cbytes2;

  /* Get a compressed buffer */
  cbytes = blosc1_compress(clevel, doshuffle, typesize, size, src,
                           dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cbytes is not correct", cbytes < size);

  /* Activate the BLOSC_SHUFFLE variable */
  setenv("BLOSC_SHUFFLE", "NOSHUFFLE", 0);
  cbytes2 = blosc1_compress(clevel, doshuffle, typesize, size, src,
                            dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: BLOSC_SHUFFLE=NOSHUFFLE does not work correctly",
            cbytes2 > cbytes);

  /* Reset env var */
  unsetenv("BLOSC_SHUFFLE");
  return 0;
}


/* Check regular shuffle */
static char *test_shuffle(void) {
  int cbytes2;

  /* Get a compressed buffer */
  cbytes = blosc1_compress(clevel, doshuffle, typesize, size, src,
                           dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cbytes is not 0", cbytes < size);

  /* Activate the BLOSC_SHUFFLE variable */
  setenv("BLOSC_SHUFFLE", "SHUFFLE", 0);
  cbytes2 = blosc1_compress(clevel, doshuffle, typesize, size, src,
                            dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: BLOSC_SHUFFLE=SHUFFLE does not work correctly",
            cbytes2 == cbytes);

  /* Reset env var */
  unsetenv("BLOSC_SHUFFLE");
  return 0;
}

/* Check bitshuffle */
static char *test_bitshuffle(void) {
  int cbytes2;

  /* Get a compressed buffer */
  if (blosc1_set_compressor("zlib") == -1) {
    /* If zlib is not here, just skip the test */
    return 0;
  };
  cbytes = blosc1_compress(clevel, doshuffle, typesize, size, src,
                           dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cbytes is not 0", cbytes < size);

  /* Activate the BLOSC_BITSHUFFLE variable */
  setenv("BLOSC_SHUFFLE", "BITSHUFFLE", 0);
  cbytes2 = blosc1_compress(clevel, doshuffle, typesize, size, src,
                            dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: BLOSC_SHUFFLE=BITSHUFFLE does not work correctly",
            cbytes2 < cbytes);

  /* Reset env var */
  unsetenv("BLOSC_SHUFFLE");
  return 0;
}


/* Check delta conding */
static char *test_delta(void) {
  int cbytes2;

  /* Get a compressed buffer */
  blosc1_set_compressor("blosclz");  /* avoid lz4 here for now (see #168) */
  blosc2_set_delta(0);
  cbytes = blosc1_compress(clevel, doshuffle, typesize, size, src,
                           dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cbytes is not 0", cbytes < size);

  /* Activate the BLOSC_DELTA variable */
  setenv("BLOSC_DELTA", "1", 0);
  cbytes2 = blosc1_compress(clevel, doshuffle, typesize, size, src,
                            dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: BLOSC_DELTA=1 does not work correctly",
            cbytes2 < 3 * cbytes / 4);

  /* Reset env var */
  unsetenv("BLOSC_DELTA");
  return 0;
}


/* Check typesize */
static char *test_typesize(void) {
  int cbytes2;

  /* Get a compressed buffer */
  cbytes = blosc1_compress(clevel, doshuffle, typesize, size, src,
                           dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cbytes is not correct", cbytes < size);

  /* Activate the BLOSC_TYPESIZE variable */
  setenv("BLOSC_TYPESIZE", "9", 0);
  cbytes2 = blosc1_compress(clevel, doshuffle, typesize, size, src,
                            dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: BLOSC_TYPESIZE does not work correctly", cbytes2 > cbytes);

  /* Reset envvar */
  unsetenv("BLOSC_TYPESIZE");
  return 0;
}

/* Check small blocksize */
static char *test_small_blocksize(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.blocksize = 2;
  cparams.typesize = 1;
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_context *dctx = blosc2_create_dctx(dparams);
  size = 8;
  /* Get a compressed buffer */
  cbytes = blosc2_compress_ctx(cctx, src, size, dest, size + BLOSC2_MAX_OVERHEAD);
  nbytes = blosc2_decompress_ctx(dctx, dest, size + BLOSC2_MAX_OVERHEAD, src, size);
  mu_assert("ERROR: nbytes is not correct", nbytes == size);

  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  return 0;
}


/* Check small buffer */
static char *test_small_buffer(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_context *dctx = blosc2_create_dctx(dparams);
  size = 2;
  uint8_t *src2 = calloc(size, 1);
  for (int i = 0; i < size; i++) {
    src2[i] = (uint8_t)i;
  }

  /* Using contexts */
  cbytes = blosc2_compress_ctx(cctx, src2, size, dest, size + BLOSC2_MAX_OVERHEAD);
  nbytes = blosc2_decompress_ctx(dctx, dest, size + BLOSC2_MAX_OVERHEAD, src, size);
  mu_assert("ERROR: nbytes is not correct", nbytes == size);

  /* Not using contexts */
  cbytes = blosc2_compress(9, 1, cparams.typesize, src2, size, dest, size + BLOSC2_MAX_OVERHEAD);
  nbytes = blosc2_decompress(dest, size + BLOSC2_MAX_OVERHEAD, src, size);
  mu_assert("ERROR: nbytes is not correct", nbytes == size);

  /* Using Blosc1 interface */
  cbytes = blosc1_compress(9, 1, cparams.typesize, size, src2, dest, size + BLOSC2_MAX_OVERHEAD);
  nbytes = blosc1_decompress(dest, src, size);
  mu_assert("ERROR: nbytes is not correct", nbytes == size);

  free(src2);
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  return 0;
}


static char *all_tests(void) {
  mu_run_test(test_compressor);
  mu_run_test(test_compress_decompress);
  mu_run_test(test_clevel);
  mu_run_test(test_noshuffle);
  mu_run_test(test_shuffle);
  mu_run_test(test_bitshuffle);
  mu_run_test(test_delta);
  mu_run_test(test_typesize);
  mu_run_test(test_small_blocksize);
  mu_run_test(test_small_buffer);

  return 0;
}

#define BUFFER_ALIGN_SIZE   32

int main(void) {
  int64_t *_src;
  char *result;
  size_t i;

  blosc2_init();
  blosc1_set_compressor("blosclz");

  /* Initialize buffers */
  src = blosc_test_malloc(BUFFER_ALIGN_SIZE, size);
  srccpy = blosc_test_malloc(BUFFER_ALIGN_SIZE, size);
  dest = blosc_test_malloc(BUFFER_ALIGN_SIZE, size + BLOSC2_MAX_OVERHEAD);
  dest2 = blosc_test_malloc(BUFFER_ALIGN_SIZE, size);
  _src = (int64_t *)src;
  for (i=0; i < (size / sizeof(int64_t)); i++) {
    _src[i] = (int64_t)i;
  }
  memcpy(srccpy, src, size);

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
  blosc_test_free(srccpy);
  blosc_test_free(dest);
  blosc_test_free(dest2);

  blosc2_destroy();

  return result != 0;
}
