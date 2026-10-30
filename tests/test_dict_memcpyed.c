/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  Data that does not compress is stored as is (memcpyed), also when the
  context compresses with a dictionary.  The copy takes the place of the
  dictionary, so the chunk must not claim one: chunks that did were rejected
  with BLOSC2_ERROR_CODEC_DICT (or read a dictionary size out of the data).
  Chunks written that way by older versions must be readable too.
*/

#include "test_common.h"

#define NBYTES (256 * 1024)

int tests_run = 0;
static uint8_t data[NBYTES];
static uint8_t chunk[NBYTES + BLOSC2_MAX_OVERHEAD];
static uint8_t result[NBYTES];


static int32_t compress_with_dict(int compcode, int32_t typesize, int16_t nthreads) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.compcode = compcode;
  cparams.clevel = 5;
  cparams.typesize = typesize;
  cparams.use_dict = 1;
  cparams.nthreads = nthreads;
  blosc2_context* cctx = blosc2_create_cctx(cparams);
  int32_t cbytes = blosc2_compress_ctx(cctx, data, NBYTES, chunk, (int32_t)sizeof(chunk));
  blosc2_free_ctx(cctx);
  return cbytes;
}


static int32_t decompress(int16_t nthreads, int32_t cbytes) {
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = nthreads;
  blosc2_context* dctx = blosc2_create_dctx(dparams);
  memset(result, 0, NBYTES);
  int32_t nbytes = blosc2_decompress_ctx(dctx, chunk, cbytes, result, NBYTES);
  blosc2_free_ctx(dctx);
  return nbytes;
}


static char* test_incompressible_with_dict(void) {
  int compcodes[] = {BLOSC_LZ4, BLOSC_LZ4HC,
#if defined(HAVE_ZSTD)
                     BLOSC_ZSTD,
#endif
  };
  int32_t typesizes[] = {1, 4};
  int16_t nthreads[] = {1, 4};
  for (size_t c = 0; c < ARRAY_SIZE(compcodes); c++) {
    for (size_t t = 0; t < ARRAY_SIZE(typesizes); t++) {
      for (size_t n = 0; n < ARRAY_SIZE(nthreads); n++) {
        int32_t cbytes = compress_with_dict(compcodes[c], typesizes[t], nthreads[n]);
        mu_assert("Cannot compress", cbytes == NBYTES + BLOSC_EXTENDED_HEADER_LENGTH);
        mu_assert("Random data must be stored as is", (chunk[BLOSC2_CHUNK_FLAGS] & BLOSC_MEMCPYED) != 0);
        mu_assert("A stored copy has no dictionary",
                  (chunk[BLOSC2_CHUNK_BLOSC2_FLAGS] & BLOSC2_USEDICT) == 0);
        for (size_t m = 0; m < ARRAY_SIZE(nthreads); m++) {
          mu_assert("Cannot decompress the copy", decompress(nthreads[m], cbytes) == NBYTES);
          mu_assert("Wrong data", memcmp(data, result, NBYTES) == 0);
        }

        // As written before: the copy still flagged as having a dictionary
        chunk[BLOSC2_CHUNK_BLOSC2_FLAGS] |= BLOSC2_USEDICT;
        for (size_t m = 0; m < ARRAY_SIZE(nthreads); m++) {
          mu_assert("Cannot decompress a copy flagged with a dictionary", decompress(nthreads[m], cbytes) == NBYTES);
          mu_assert("Wrong data", memcmp(data, result, NBYTES) == 0);
          int32_t start = 1000;
          int32_t nitems = 5000;
          blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
          dparams.nthreads = nthreads[m];
          blosc2_context* dctx = blosc2_create_dctx(dparams);
          int rc = blosc2_getitem_ctx(dctx, chunk, cbytes, start, nitems, result, NBYTES);
          blosc2_free_ctx(dctx);
          mu_assert("Cannot get items of a copy flagged with a dictionary",
                    rc == nitems * typesizes[t]);
          mu_assert("Wrong items",
                    memcmp(data + (size_t)start * typesizes[t], result, (size_t)rc) == 0);
        }
      }
    }
  }
  return EXIT_SUCCESS;
}


/* Data that compresses keeps its dictionary */
static char* test_compressible_with_dict(void) {
  for (int i = 0; i < NBYTES; i++) {
    data[i] = (uint8_t)((i / 64) % 7);
  }
  int32_t cbytes = compress_with_dict(BLOSC_LZ4, 4, 1);
  mu_assert("Cannot compress", cbytes > 0 && cbytes < NBYTES);
  mu_assert("A compressed chunk keeps its dictionary", (chunk[BLOSC2_CHUNK_BLOSC2_FLAGS] & BLOSC2_USEDICT) != 0);
  mu_assert("Cannot decompress", decompress(4, cbytes) == NBYTES);
  mu_assert("Wrong data", memcmp(data, result, NBYTES) == 0);
  return EXIT_SUCCESS;
}


static char* all_tests(void) {
  uint32_t s = 2463534242u;
  for (int i = 0; i < NBYTES; i++) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    data[i] = (uint8_t)s;
  }
  mu_run_test(test_incompressible_with_dict);
  mu_run_test(test_compressible_with_dict);
  return EXIT_SUCCESS;
}


int main(void) {
  blosc2_init();
  char* result_ = all_tests();
  if (result_ != EXIT_SUCCESS) {
    printf(" (%s)\n", result_);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);
  blosc2_destroy();
  return result_ != EXIT_SUCCESS;
}
