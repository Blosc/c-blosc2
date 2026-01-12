/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Unit tests for BLOSC_COMPRESSOR environment variable in Blosc.

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"
#include "config.h" // so have access to HAVE_OPENZL variable


int tests_run = 0;

/* Global vars */
void *src, *srccpy, *dest, *dest2;
int nbytes, cbytes, cbytes2;
int clevel = 5;
int doshuffle = 1;
size_t typesize = 8;
int32_t size = 8 * 1000 * 1000;  /* must be divisible by typesize */
uint8_t compcode_meta = 7; // SH_BD_LZ4
uint8_t compcode_metas[8] = {0, 1, 2, 3, 6, 7, 14, 15};
/* compcode_meta values
ZSTD = 0,
LZ4 = 1,
SH_ZSTD = 2,
SH_LZ4 = 3,
SH_BD_ZSTD = 6,
SH_BD_LZ4 = 7,
SH_BD_SPLIT_ZSTD = 14,
SH_BD_SPLIT_LZ4 = 15,
For all these options, enable Checksum via +16
*/


/* Check compressing + decompressing */
static char *test_checksum(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_context *cctx, *dctx;
  cparams.compcode = BLOSC_OPENZL;
  cparams.clevel = clevel;
  cparams.typesize = typesize;
  cparams.compcode_meta = compcode_meta;
  cctx = blosc2_create_cctx(cparams);
  dctx = blosc2_create_dctx(dparams);

  cbytes = blosc2_compress_ctx(cctx, src, (int32_t)size, dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cbytes is not correct", cbytes < size);

  /* Decompress the buffer */
  nbytes = blosc2_decompress_ctx(dctx, dest, cbytes, dest2, (int32_t)size);
  mu_assert("ERROR: nbytes incorrect(1)", nbytes == size);

  blosc2_cparams cparams2 = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams2 = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_context *cctx2, *dctx2;
  cparams2.compcode = BLOSC_OPENZL;
  cparams2.clevel = clevel;
  cparams2.typesize = typesize;
  cparams2.compcode_meta = compcode_meta + 16; // with checksum
  cctx2 = blosc2_create_cctx(cparams2);
  dctx2 = blosc2_create_dctx(dparams2);

  cbytes2 = blosc2_compress_ctx(cctx2, src, (int32_t)size, dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cbytes is not correct", cbytes2 > cbytes);

  /* Decompress the buffer */
  nbytes = blosc2_decompress_ctx(dctx2, dest, cbytes2, dest2, (int32_t)size);
  mu_assert("ERROR: nbytes incorrect(1)", nbytes == size);

  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  blosc2_free_ctx(cctx2);
  blosc2_free_ctx(dctx2);

  return 0;
}


/* Check compression level */
static char *test_clevel(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_cparams cparams2 = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_context *cctx, *cctx2;
  cparams.compcode = BLOSC_OPENZL;
  cparams.clevel = 1;
  cparams.typesize = typesize;
  cparams.compcode_meta = 1; // plain LZ4
  cctx = blosc2_create_cctx(cparams);
  cparams2.compcode = BLOSC_OPENZL;
  cparams2.clevel = 5;
  cparams2.typesize = typesize;
  cparams2.compcode_meta = 1; // plain LZ4
  cctx2 = blosc2_create_cctx(cparams2);

  cbytes = blosc2_compress_ctx(cctx, src, (int32_t)size, dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: cbytes is not correct", cbytes < size);

  cbytes2 = blosc2_compress_ctx(cctx2, src, (int32_t)size, dest, size + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: increasing clevel does not increase compression", cbytes2 < cbytes);
  
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(cctx2);
  return 0;
}

/* Check different compression profiles */
static char *test_profiles(void) {
  for( int idx = 0; idx < 8; idx++){
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
    blosc2_context *cctx, *dctx;
    cparams.compcode = BLOSC_OPENZL;
    cparams.clevel = clevel;
    cparams.typesize = typesize;
    cparams.compcode_meta = compcode_metas[idx]; // different profiles
    dctx = blosc2_create_dctx(dparams);
    cctx = blosc2_create_cctx(cparams);
    
    cbytes = blosc2_compress_ctx(cctx, src, (int32_t)size, dest, size + BLOSC2_MAX_OVERHEAD);
    mu_assert("ERROR: cbytes is not correct", cbytes < size);

    /* Decompress the buffer */
    nbytes = blosc2_decompress_ctx(dctx, dest, cbytes, dest2, (int32_t)size);
    mu_assert("ERROR: nbytes incorrect(1)", nbytes == size);
    blosc2_free_ctx(cctx);
    blosc2_free_ctx(dctx);
  }
    return 0;
}

static char *all_tests(void) {
  mu_run_test(test_checksum);
  mu_run_test(test_clevel);
  mu_run_test(test_profiles);
  return 0;
}

#define BUFFER_ALIGN_SIZE   32

int main(void) {
  int64_t *_src;
  char *result;
  size_t i;

  blosc2_init();
  
  #if defined(HAVE_OPENZL)
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
  #else 
    result = 0;
  #endif
  blosc2_destroy();

  return result != 0;
}
