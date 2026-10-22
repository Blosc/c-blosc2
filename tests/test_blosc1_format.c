/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Unit tests for writing Blosc1 (format version 2) chunks, which C-Blosc 1.x
  can decode, with blosc2_ctx_set_blosc1_format() and BLOSC_BLOSC1_COMPAT.

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"
#include "blosc2/codecs-registry.h"

int tests_run = 0;

#define MAX_NBYTES (64 * 1024)

static const int codecs[] = {BLOSC_BLOSCLZ, BLOSC_LZ4, BLOSC_LZ4HC, BLOSC_ZLIB, BLOSC_ZSTD};
static const int filters[] = {BLOSC_NOSHUFFLE, BLOSC_SHUFFLE, BLOSC_BITSHUFFLE};


static void fill(uint8_t* buf, int32_t nbytes) {
  for (int32_t i = 0; i < nbytes; i++) {
    buf[i] = (uint8_t)((i * 7) ^ (i >> 5));
  }
}


/* Whether this build has the codec (zlib and zstd can be deactivated).  The data must not
 * be a run of zeros, which is stored without calling the codec. */
static bool codec_available(int codec) {
  uint8_t src[4096], chunk[4096 + BLOSC2_MAX_OVERHEAD];
  fill(src, sizeof(src));
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.compcode = (uint8_t)codec;
  cparams.nthreads = 1;
  blosc2_context* cctx = blosc2_create_cctx(cparams);
  int cbytes = blosc2_compress_ctx(cctx, src, sizeof(src), chunk, sizeof(chunk));
  blosc2_free_ctx(cctx);
  return cbytes != BLOSC2_ERROR_CODEC_SUPPORT;
}


/* Compress with a Blosc1-format context and check the chunk can be read back.
 * Returns the compressed size, or a negative error code. */
static int roundtrip(int codec, int filter, int32_t typesize, int32_t nbytes,
                     int32_t blocksize, int16_t nthreads, uint8_t* chunk, uint8_t* out) {
  uint8_t src[MAX_NBYTES];
  fill(src, nbytes);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.compcode = (uint8_t)codec;
  cparams.clevel = 5;
  cparams.typesize = typesize;
  cparams.blocksize = blocksize;
  cparams.nthreads = nthreads;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = (uint8_t)filter;
  blosc2_context* cctx = blosc2_create_cctx(cparams);
  if (cctx == NULL) {
    return BLOSC2_ERROR_NULL_POINTER;
  }
  int rc = blosc2_ctx_set_blosc1_format(cctx, true);
  if (rc < 0) {
    blosc2_free_ctx(cctx);
    return rc;
  }
  int cbytes = blosc2_compress_ctx(cctx, src, nbytes, chunk, nbytes + BLOSC2_MAX_OVERHEAD);
  blosc2_free_ctx(cctx);
  if (cbytes < 0) {
    return cbytes;
  }

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = nthreads;
  blosc2_context* dctx = blosc2_create_dctx(dparams);
  int dbytes = blosc2_decompress_ctx(dctx, chunk, cbytes, out, nbytes);
  blosc2_free_ctx(dctx);
  if (dbytes != nbytes || memcmp(src, out, (size_t)nbytes) != 0) {
    return BLOSC2_ERROR_FAILURE;
  }
  return cbytes;
}


/* The chunk has the Blosc1 version and header, and the default has not changed */
static char* test_header(void) {
  uint8_t src[4096], chunk[4096 + BLOSC2_MAX_OVERHEAD];
  fill(src, sizeof(src));

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 4;
  blosc2_context* cctx = blosc2_create_cctx(cparams);
  int cbytes = blosc2_compress_ctx(cctx, src, sizeof(src), chunk, sizeof(chunk));
  mu_assert("ERROR: default compression failed", cbytes > 0);
  mu_assert("ERROR: default chunks changed version", chunk[0] != BLOSC1_VERSION_FORMAT);

  mu_assert("ERROR: default context is Blosc1", !blosc2_ctx_get_blosc1_format(cctx));
  mu_assert("ERROR: cannot set the flag", blosc2_ctx_set_blosc1_format(cctx, true) == 0);
  mu_assert("ERROR: the flag is not reported", blosc2_ctx_get_blosc1_format(cctx));
  cbytes = blosc2_compress_ctx(cctx, src, sizeof(src), chunk, sizeof(chunk));
  mu_assert("ERROR: blosc1 compression failed", cbytes > 0);
  mu_assert("ERROR: not format version 2", chunk[0] == BLOSC1_VERSION_FORMAT);

  mu_assert("ERROR: cannot clear the flag", blosc2_ctx_set_blosc1_format(cctx, false) == 0);
  mu_assert("ERROR: the cleared flag is reported", !blosc2_ctx_get_blosc1_format(cctx));
  cbytes = blosc2_compress_ctx(cctx, src, sizeof(src), chunk, sizeof(chunk));
  mu_assert("ERROR: compression failed after clearing the flag", cbytes > 0);
  mu_assert("ERROR: version not restored", chunk[0] != BLOSC1_VERSION_FORMAT);
  blosc2_free_ctx(cctx);

  /* Memcpyed chunks too */
  cparams.clevel = 0;
  cctx = blosc2_create_cctx(cparams);
  blosc2_ctx_set_blosc1_format(cctx, true);
  cbytes = blosc2_compress_ctx(cctx, src, sizeof(src), chunk, sizeof(chunk));
  mu_assert("ERROR: memcpyed chunk has the wrong size", cbytes == (int)sizeof(src) + BLOSC_MIN_HEADER_LENGTH);
  mu_assert("ERROR: memcpyed chunk is not format version 2", chunk[0] == BLOSC1_VERSION_FORMAT);
  blosc2_free_ctx(cctx);
  return 0;
}


/* The flag can only be set on compression contexts */
static char* test_set_flag_errors(void) {
  mu_assert("ERROR: NULL context accepted", blosc2_ctx_set_blosc1_format(NULL, true) < 0);
  mu_assert("ERROR: NULL context is Blosc1", !blosc2_ctx_get_blosc1_format(NULL));
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_context* dctx = blosc2_create_dctx(dparams);
  mu_assert("ERROR: decompression context accepted", blosc2_ctx_set_blosc1_format(dctx, true) < 0);
  blosc2_free_ctx(dctx);
  return 0;
}


/* A typesize that is not positive is an error, not a division by zero */
static char* test_invalid_typesize(void) {
  uint8_t src[4096], chunk[4096 + BLOSC2_MAX_OVERHEAD];
  fill(src, sizeof(src));

  for (int blosc1_format = 0; blosc1_format < 2; blosc1_format++) {
    for (int32_t typesize = -1; typesize <= 0; typesize++) {
      blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
      cparams.typesize = typesize;
      cparams.nthreads = 1;
      cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_BITSHUFFLE;
      blosc2_context* cctx = blosc2_create_cctx(cparams);
      blosc2_ctx_set_blosc1_format(cctx, blosc1_format);
      int cbytes = blosc2_compress_ctx(cctx, src, sizeof(src), chunk, sizeof(chunk));
      blosc2_free_ctx(cctx);
      mu_assert("ERROR: a typesize that is not positive was accepted", cbytes < 0);
    }
  }
  return 0;
}


/* What the Blosc1 format cannot express is refused rather than written wrongly */
static char* test_unsupported(void) {
  uint8_t src[4096], chunk[4096 + BLOSC2_MAX_OVERHEAD];
  fill(src, sizeof(src));

  for (int k = 0; k < 7; k++) {
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.typesize = 4;
    switch (k) {
      case 0: cparams.filters[BLOSC2_MAX_FILTERS - 2] = BLOSC_SHUFFLE; break;  /* two filters */
      case 1: cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_DELTA; break;
      case 2: cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_TRUNC_PREC; break;
      case 3: cparams.filters_meta[BLOSC2_MAX_FILTERS - 1] = 2; break;  /* custom shuffle typesize */
      case 4: cparams.compcode = BLOSC_CODEC_NDLZ; break;
      case 5: cparams.use_dict = true; cparams.compcode = BLOSC_ZSTD; break;
      default: cparams.compcode = 3; break;  /* snappy's code, which c-blosc2 does not have */
    }
    blosc2_context* cctx = blosc2_create_cctx(cparams);
    mu_assert("ERROR: cannot create the context", cctx != NULL);
    blosc2_ctx_set_blosc1_format(cctx, true);
    int cbytes = blosc2_compress_ctx(cctx, src, sizeof(src), chunk, sizeof(chunk));
    blosc2_free_ctx(cctx);
    mu_assert("ERROR: unsupported setup was compressed", cbytes < 0);
  }

  return 0;
}


/* Every supported codec and filter round-trips through the Blosc1 header */
static char* test_roundtrip(void) {
  static uint8_t chunk[MAX_NBYTES + BLOSC2_MAX_OVERHEAD], out[MAX_NBYTES];
  static const int32_t typesizes[] = {1, 2, 3, 4, 8, 16, 17, 32, 300};
  static const int32_t sizes[] = {1, 31, 32, 33, 1000, 4097, 40001, MAX_NBYTES};

  for (size_t c = 0; c < sizeof(codecs) / sizeof(codecs[0]); c++) {
    if (!codec_available(codecs[c])) {
      continue;
    }
    for (size_t f = 0; f < sizeof(filters) / sizeof(filters[0]); f++) {
      for (size_t t = 0; t < sizeof(typesizes) / sizeof(typesizes[0]); t++) {
        for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
          int cbytes = roundtrip(codecs[c], filters[f], typesizes[t], sizes[s], 0, 1, chunk, out);
          if (cbytes < 0) {
            printf("codec %d filter %d typesize %d nbytes %d: error %d\n",
                   codecs[c], filters[f], typesizes[t], sizes[s], cbytes);
          }
          mu_assert("ERROR: Blosc1 format roundtrip failed", cbytes > 0);
          mu_assert("ERROR: not format version 2", chunk[0] == BLOSC1_VERSION_FORMAT);
        }
      }
    }
  }
  return 0;
}


/* The Blosc1 header has flags for the filter, so it can be in any slot of the pipeline */
static char* test_filter_slots(void) {
  static uint8_t src[4096], chunk[4096 + BLOSC2_MAX_OVERHEAD], out[4096];
  fill(src, sizeof(src));

  for (int slot = 0; slot < BLOSC2_MAX_FILTERS; slot++) {
    for (int filter = BLOSC_SHUFFLE; filter <= BLOSC_BITSHUFFLE; filter++) {
      blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
      cparams.typesize = 4;
      cparams.nthreads = 1;
      memset(cparams.filters, 0, sizeof(cparams.filters));
      cparams.filters[slot] = (uint8_t)filter;
      blosc2_context* cctx = blosc2_create_cctx(cparams);
      blosc2_ctx_set_blosc1_format(cctx, true);
      int cbytes = blosc2_compress_ctx(cctx, src, sizeof(src), chunk, sizeof(chunk));
      blosc2_free_ctx(cctx);
      mu_assert("ERROR: a filter in this slot was refused", cbytes > 0);
      mu_assert("ERROR: not format version 2", chunk[0] == BLOSC1_VERSION_FORMAT);
      mu_assert("ERROR: bad decompression", blosc1_decompress(chunk, out, sizeof(out)) == (int)sizeof(out));
      mu_assert("ERROR: bad roundtrip", memcmp(src, out, sizeof(src)) == 0);
    }
  }
  return 0;
}


/* Several threads, and enough blocks for them to share */
static char* test_threads(void) {
  static uint8_t chunk[MAX_NBYTES + BLOSC2_MAX_OVERHEAD], out[MAX_NBYTES];
  static const int32_t typesizes[] = {1, 4, 8, 16, 17};
  static const int32_t blocksizes[] = {0, 1024, 4096};
  static const int16_t threads[] = {2, 4};

  for (size_t c = 0; c < sizeof(codecs) / sizeof(codecs[0]); c++) {
    if (!codec_available(codecs[c])) {
      continue;
    }
    for (size_t f = 0; f < sizeof(filters) / sizeof(filters[0]); f++) {
      for (size_t t = 0; t < sizeof(typesizes) / sizeof(typesizes[0]); t++) {
        for (size_t b = 0; b < sizeof(blocksizes) / sizeof(blocksizes[0]); b++) {
          /* not a multiple of the typesize in general: it is rounded */
          int32_t blocksize = blocksizes[b];
          for (size_t n = 0; n < sizeof(threads) / sizeof(threads[0]); n++) {
            int cbytes = roundtrip(codecs[c], filters[f], typesizes[t], 40001, blocksize,
                                   threads[n], chunk, out);
            if (cbytes < 0) {
              printf("codec %d filter %d typesize %d blocksize %d threads %d: error %d\n",
                     codecs[c], filters[f], typesizes[t], blocksize, threads[n], cbytes);
            }
            mu_assert("ERROR: multi-threaded Blosc1 format roundtrip failed", cbytes > 0);
            mu_assert("ERROR: not format version 2", chunk[0] == BLOSC1_VERSION_FORMAT);
          }
        }
      }
    }
  }
  return 0;
}


/* A C-Blosc 1.x reader only splits blocks with typesize <= 16 and at least 128 elements,
 * whatever the "dont split" header flag says, so the flag must be set for all other blocks. */
static char* test_split_flag(void) {
  static const struct { int32_t typesize, blocksize; bool may_split; } cases[] = {
    {4, 4096, true}, {4, 512, true}, {4, 508, false}, {4, 256, false},
    {16, 4096, true}, {17, 17 * 256, false}, {32, 32 * 256, false}, {1, 100, false}, {1, 128, true},
  };
  static uint8_t src[MAX_NBYTES], chunk[MAX_NBYTES + BLOSC2_MAX_OVERHEAD];
  fill(src, sizeof(src));

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.compcode = BLOSC_LZ4;
    cparams.clevel = 5;
    cparams.typesize = cases[i].typesize;
    cparams.blocksize = cases[i].blocksize;
    cparams.splitmode = BLOSC_ALWAYS_SPLIT;
    cparams.nthreads = 1;
    cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_SHUFFLE;
    blosc2_context* cctx = blosc2_create_cctx(cparams);
    blosc2_ctx_set_blosc1_format(cctx, true);
    int cbytes = blosc2_compress_ctx(cctx, src, 16 * 1024, chunk, sizeof(chunk));
    blosc2_free_ctx(cctx);
    mu_assert("ERROR: compression failed", cbytes > 0);
    bool dont_split = (chunk[2] & 0x10) != 0;
    if (!cases[i].may_split) {
      mu_assert("ERROR: a block C-Blosc 1.x would not split is flagged as split", dont_split);
    }
    else {
      mu_assert("ERROR: splitting was not honored where C-Blosc 1.x supports it", !dont_split);
    }
  }
  return 0;
}


#ifndef _WIN32
/* BLOSC_BLOSC1_COMPAT makes blosc1_compress() write Blosc1 chunks, in both of its paths */
static char* test_envvar(void) {
  static uint8_t src[4096], chunk[4096 + BLOSC2_MAX_OVERHEAD], out[4096];
  fill(src, sizeof(src));

  for (int nolock = 0; nolock < 2; nolock++) {
    if (nolock) {
      setenv("BLOSC_NOLOCK", "TRUE", 1);
    }
    int cbytes = blosc1_compress(5, BLOSC_BITSHUFFLE, 4, sizeof(src), src, chunk, sizeof(chunk));
    mu_assert("ERROR: compression failed", cbytes > 0);
    mu_assert("ERROR: default chunk is format version 2", chunk[0] != BLOSC1_VERSION_FORMAT);

    setenv("BLOSC_BLOSC1_COMPAT", "TRUE", 1);
    cbytes = blosc1_compress(5, BLOSC_BITSHUFFLE, 4, sizeof(src), src, chunk, sizeof(chunk));
    mu_assert("ERROR: compression failed", cbytes > 0);
    mu_assert("ERROR: chunk is not format version 2", chunk[0] == BLOSC1_VERSION_FORMAT);
    mu_assert("ERROR: bad decompression", blosc1_decompress(chunk, out, sizeof(out)) == (int)sizeof(out));
    mu_assert("ERROR: bad roundtrip", memcmp(src, out, sizeof(src)) == 0);

    /* Delta cannot be written in this format */
    setenv("BLOSC_DELTA", "1", 1);
    cbytes = blosc1_compress(5, BLOSC_SHUFFLE, 4, sizeof(src), src, chunk, sizeof(chunk));
    mu_assert("ERROR: delta was written as Blosc1", cbytes < 0);
    unsetenv("BLOSC_DELTA");
    blosc2_set_delta(0);  /* the variable's effect is sticky */

    unsetenv("BLOSC_BLOSC1_COMPAT");
    unsetenv("BLOSC_NOLOCK");
  }
  return 0;
}
#endif


static char* all_tests(void) {
  mu_run_test(test_header);
  mu_run_test(test_set_flag_errors);
  mu_run_test(test_invalid_typesize);
  mu_run_test(test_unsupported);
  mu_run_test(test_roundtrip);
  mu_run_test(test_filter_slots);
  mu_run_test(test_threads);
  mu_run_test(test_split_flag);
#ifndef _WIN32
  mu_run_test(test_envvar);
#endif
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
