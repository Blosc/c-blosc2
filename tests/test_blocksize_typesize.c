/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Unit tests for the block size that is chosen for a given typesize: a requested block size
  that is not a multiple of the typesize, and typesizes above BLOSC_MAX_TYPESIZE.

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"

int tests_run = 0;

#define MAX_NBYTES (300 * 1024)

static uint8_t src[MAX_NBYTES], chunk[MAX_NBYTES + BLOSC2_MAX_OVERHEAD], out[MAX_NBYTES];


static void fill(int32_t nbytes) {
  for (int32_t i = 0; i < nbytes; i++) {
    src[i] = (uint8_t)((i * 7) ^ (i >> 5));
  }
}


/* Compress src, with a Blosc1 format chunk or not; returns the compressed size or an error */
static int compress(int codec, int filter, int32_t typesize, int32_t nbytes, int32_t blocksize,
                    int16_t nthreads, bool blosc1_format) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.compcode = (uint8_t)codec;
  cparams.clevel = 5;
  cparams.typesize = typesize;
  cparams.blocksize = blocksize;
  cparams.nthreads = nthreads;
  memset(cparams.filters, 0, sizeof(cparams.filters));
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = (uint8_t)filter;
  blosc2_context* cctx = blosc2_create_cctx(cparams);
  if (cctx == NULL) {
    return BLOSC2_ERROR_NULL_POINTER;
  }
  blosc2_ctx_set_blosc1_format(cctx, blosc1_format);
  int cbytes = blosc2_compress_ctx(cctx, src, nbytes, chunk, nbytes + BLOSC2_MAX_OVERHEAD);
  blosc2_free_ctx(cctx);
  return cbytes;
}


/* Whether the chunk decompresses to src */
static bool decompresses(int32_t cbytes, int32_t nbytes, int16_t nthreads) {
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = nthreads;
  blosc2_context* dctx = blosc2_create_dctx(dparams);
  memset(out, 0, (size_t)nbytes);
  int dbytes = blosc2_decompress_ctx(dctx, chunk, cbytes, out, nbytes);
  blosc2_free_ctx(dctx);
  return dbytes == nbytes && memcmp(src, out, (size_t)nbytes) == 0;
}


/* A requested block size is rounded to a multiple of the typesize, and the chunk says so */
static char* test_blocksize_not_a_multiple_of_typesize(void) {
  static const int codecs[] = {BLOSC_BLOSCLZ, BLOSC_LZ4};
  static const int filters[] = {BLOSC_NOFILTER, BLOSC_SHUFFLE, BLOSC_BITSHUFFLE};
  static const int32_t typesizes[] = {3, 4, 17, 255};
  static const int32_t multiples[] = {1, 40, 1000};
  static const int32_t nbyteses[] = {5000, 40001};

  for (size_t c = 0; c < sizeof(codecs) / sizeof(codecs[0]); c++) {
    for (size_t f = 0; f < sizeof(filters) / sizeof(filters[0]); f++) {
      for (size_t t = 0; t < sizeof(typesizes) / sizeof(typesizes[0]); t++) {
        for (size_t m = 0; m < sizeof(multiples) / sizeof(multiples[0]); m++) {
          for (int r = 1; r <= 2; r++) {
            int32_t typesize = typesizes[t];
            int32_t blocksize = typesize * multiples[m] + (r == 1 ? 1 : typesize - 1);
            for (size_t n = 0; n < sizeof(nbyteses) / sizeof(nbyteses[0]); n++) {
              for (int blosc1_format = 0; blosc1_format < 2; blosc1_format++) {
                for (int16_t nthreads = 1; nthreads <= 2; nthreads++) {
                  int32_t nbytes = nbyteses[n];
                  fill(nbytes);
                  int cbytes = compress(codecs[c], filters[f], typesize, nbytes, blocksize,
                                        nthreads, blosc1_format);
                  mu_assert("ERROR: compression failed", cbytes > 0);
                  int32_t hnbytes, hcbytes, hblocksize;
                  blosc2_cbuffer_sizes(chunk, &hnbytes, &hcbytes, &hblocksize);
                  if (hblocksize > typesize) {
                    mu_assert("ERROR: the block size in the header is not a multiple of the typesize",
                              hblocksize % typesize == 0);
                  }
                  mu_assert("ERROR: the chunk cannot be read back", decompresses(cbytes, nbytes, nthreads));
                }
              }
            }
          }
        }
      }
    }
  }
  return 0;
}


/* A typesize above BLOSC_MAX_TYPESIZE is stored as 1, and has to be blocked as 1 as well:
 * the tuner must not see the large typesize (it would make a block of a few bytes of a short
 * buffer, or round the block size to a multiple of the large typesize). */
static char* test_large_typesize_is_a_byte_stream(void) {
  static const int32_t typesizes[] = {256, 300, 1000};
  static const int32_t nbyteses[] = {1, 32, 200, 1000, 70000, MAX_NBYTES};
  static const int filters[] = {BLOSC_NOFILTER, BLOSC_SHUFFLE, BLOSC_BITSHUFFLE};
  static uint8_t reference[MAX_NBYTES + BLOSC2_MAX_OVERHEAD];

  for (size_t t = 0; t < sizeof(typesizes) / sizeof(typesizes[0]); t++) {
    for (size_t n = 0; n < sizeof(nbyteses) / sizeof(nbyteses[0]); n++) {
      for (size_t f = 0; f < sizeof(filters) / sizeof(filters[0]); f++) {
        for (int blosc1_format = 0; blosc1_format < 2; blosc1_format++) {
          int32_t nbytes = nbyteses[n];
          fill(nbytes);

          int ref_cbytes = compress(BLOSC_LZ4, filters[f], 1, nbytes, 0, 1, blosc1_format);
          mu_assert("ERROR: compression failed", ref_cbytes > 0);
          memcpy(reference, chunk, (size_t)ref_cbytes);

          int cbytes = compress(BLOSC_LZ4, filters[f], typesizes[t], nbytes, 0, 1, blosc1_format);
          mu_assert("ERROR: compression failed", cbytes > 0);
          mu_assert("ERROR: the typesize is not stored as 1", chunk[3] == 1);
          mu_assert("ERROR: a large typesize is not blocked like a typesize of 1",
                    cbytes == ref_cbytes && memcmp(chunk, reference, (size_t)cbytes) == 0);
          mu_assert("ERROR: the chunk cannot be read back", decompresses(cbytes, nbytes, 1));
        }
      }
    }
  }
  return 0;
}


static char* all_tests(void) {
  mu_run_test(test_blocksize_not_a_multiple_of_typesize);
  mu_run_test(test_large_typesize_is_a_byte_stream);
  return 0;
}


int main(void) {
  install_blosc_callback_test(); /* optionally install callback test */
  blosc2_init();

  char* result = all_tests();
  if (result != NULL) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  blosc2_destroy();

  return result != NULL;
}
