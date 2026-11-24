/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include "test_common.h"

int tests_run = 0;

enum { NBLOCKS = 8, BLOCKSIZE = 4096, INPUT_SIZE = NBLOCKS * BLOCKSIZE };

static int32_t read_i32_le(const uint8_t* src) {
  return (int32_t)((uint32_t)src[0] | ((uint32_t)src[1] << 8) |
                   ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24));
}

/* A roundtrip cannot detect corrupted dictionary samples: compression and
 * decompression use the same embedded bytes.  With no filters and one thread,
 * the samples are the concatenated input, so check the dictionary directly.
 * Its size exceeds the bstarts table, ensuring that both the move destination
 * and the dictionary-size field overlap the original samples. */
static char* check_dict_content(bool vlblocks) {
  uint8_t input[INPUT_SIZE];
  uint8_t output[INPUT_SIZE];
  uint8_t chunk[INPUT_SIZE + BLOSC2_MAX_OVERHEAD + BLOSC2_MAXDICTSIZE];
  const void* srcs[NBLOCKS];
  int32_t srcsizes[NBLOCKS];
  int32_t total = 0;
  const int codecs[] = {BLOSC_LZ4, BLOSC_LZ4HC};

  for (int i = 0; i < INPUT_SIZE; i++) {
    input[i] = (uint8_t)(i % 251);
  }
  for (int i = 0; i < NBLOCKS; i++) {
    srcs[i] = input + total;
    srcsizes[i] = BLOCKSIZE - (vlblocks ? 16 * i : 0);
    total += srcsizes[i];
  }

  for (int ci = 0; ci < (int)ARRAY_SIZE(codecs); ci++) {
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.compcode = codecs[ci];
    cparams.typesize = 1;
    cparams.clevel = 5;
    cparams.use_dict = 1;
    cparams.nthreads = 1;
    cparams.blocksize = BLOCKSIZE;
    cparams.splitmode = BLOSC_NEVER_SPLIT;
    memset(cparams.filters, 0, sizeof(cparams.filters));
    blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
    dparams.nthreads = 1;
    blosc2_context* cctx = blosc2_create_cctx(cparams);
    blosc2_context* dctx = blosc2_create_dctx(dparams);
    mu_assert("ERROR: cannot create contexts", cctx != NULL && dctx != NULL);

    int32_t cbytes = vlblocks ?
        blosc2_vlcompress_ctx(cctx, srcs, srcsizes, NBLOCKS, chunk, (int32_t)sizeof(chunk)) :
        blosc2_compress_ctx(cctx, input, total, chunk, (int32_t)sizeof(chunk));
    mu_assert("ERROR: dictionary compression failed", cbytes > 0);
    mu_assert("ERROR: dictionary flag missing",
              (chunk[BLOSC2_CHUNK_BLOSC2_FLAGS] & BLOSC2_USEDICT) != 0);
    mu_assert("ERROR: unexpected memcpy fallback", (chunk[BLOSC2_CHUNK_FLAGS] & BLOSC_MEMCPYED) == 0);
    mu_assert("ERROR: unexpected block layout",
              read_i32_le(chunk + BLOSC2_CHUNK_BLOCKSIZE) == (vlblocks ? NBLOCKS : BLOCKSIZE));

    int32_t dict_offset = BLOSC_EXTENDED_HEADER_LENGTH + NBLOCKS * (int32_t)sizeof(int32_t);
    mu_assert("ERROR: missing dictionary size", cbytes >= dict_offset + (int32_t)sizeof(int32_t));
    int32_t dict_size = read_i32_le(chunk + dict_offset);
    /* Both inputs are large enough that the 5% cap, not sample availability,
     * determines the raw LZ4 dictionary size. */
    mu_assert("ERROR: unexpected dictionary size", dict_size == total / 20);
    dict_offset += (int32_t)sizeof(int32_t);
    mu_assert("ERROR: truncated dictionary", cbytes >= dict_offset + dict_size);
    mu_assert("ERROR: embedded dictionary differs from original samples",
              memcmp(chunk + dict_offset, input, (size_t)dict_size) == 0);

    int32_t dbytes = blosc2_decompress_ctx(dctx, chunk, cbytes, output, (int32_t)sizeof(output));
    mu_assert("ERROR: decompressed size mismatch", dbytes == total);
    mu_assert("ERROR: roundtrip mismatch", memcmp(output, input, (size_t)total) == 0);
    blosc2_free_ctx(cctx);
    blosc2_free_ctx(dctx);
  }
  return EXIT_SUCCESS;
}

static char* test_dict_content(void) {
  return check_dict_content(false);
}

static char* test_vl_dict_content(void) {
  return check_dict_content(true);
}

static char* test_reused_lz4_state(void) {
  // Reused LZ4 worker state must not reference an earlier stream/block/chunk,
  // including after an incompressible or output-capacity fallback.
  for (int threads = 1; threads <= 4; threads += 3) {
    for (int split = 0; split < 2; split++) {
      blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
      blosc2_dparams dp = BLOSC2_DPARAMS_DEFAULTS;
      cp.compcode = BLOSC_LZ4;
      cp.typesize = 8;
      cp.nthreads = threads;
      cp.blocksize = BLOCKSIZE;
      cp.splitmode = split ? BLOSC_ALWAYS_SPLIT : BLOSC_NEVER_SPLIT;
      dp.nthreads = threads;
      uint8_t input[INPUT_SIZE], output[INPUT_SIZE];
      uint8_t chunk[INPUT_SIZE + BLOSC2_MAX_OVERHEAD];
      uint32_t random = 1234567;
      blosc2_context *encoder = blosc2_create_cctx(cp);
      blosc2_context *decoder = blosc2_create_dctx(dp);
      for (int round = 0; round < 12; round++) {
        int size = INPUT_SIZE - round * 8;  // Include partial last blocks.
        for (int i = 0; i < size; i++) {
          random = random * 1664525U + 1013904223U;
          input[i] = round % 3 == 0 ? (uint8_t)(random >> 24) : (uint8_t)((i + round) % 251);
        }
        if (round % 3 == 0) {
          mu_assert("Small output should fail or fall back",
                    blosc2_compress_ctx(encoder, input, size, chunk, BLOSC2_MAX_OVERHEAD) <= 0);
        }
        int bytes = blosc2_compress_ctx(encoder, input, size, chunk, sizeof(chunk));
        mu_assert("Reused stream compression failed", bytes > 0);
        mu_assert("Reused stream decompression failed",
                  blosc2_decompress_ctx(decoder, chunk, bytes, output, sizeof(output)) == size);
        mu_assert("Reused stream referenced prior data", memcmp(input, output, size) == 0);
      }
      blosc2_free_ctx(encoder);
      blosc2_free_ctx(decoder);
    }
  }
  return EXIT_SUCCESS;
}

static char* all_tests(void) {
  mu_run_test(test_reused_lz4_state);
  mu_run_test(test_dict_content);
  mu_run_test(test_vl_dict_content);
  return EXIT_SUCCESS;
}

int main(void) {
  blosc2_init();
  char* result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf(" Tests run: %d\n", tests_run);
  blosc2_destroy();
  return result != EXIT_SUCCESS;
}
