/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  Run detection in compression: a stream whose bytes are all equal is stored
  as a run (a stream length <= 0) instead of compressed data.  This checks it
  against a byte-by-byte reference for many stream lengths, with one differing
  byte at every position and with random differences.  The stream ends where
  its heap buffer ends, so reading past it shows up under ASan.
*/

#include "test_common.h"

#define BLOCKSIZE 4096
#define NRUNBLOCKS 2     /* run blocks in front keep every chunk compressible */
#define MAXOFFSET 7

int tests_run = 0;
static uint8_t stream[BLOCKSIZE];
static uint8_t chunk[NRUNBLOCKS * BLOCKSIZE + BLOCKSIZE + BLOSC2_MAX_OVERHEAD];
static uint8_t dest[NRUNBLOCKS * BLOCKSIZE + BLOCKSIZE];
static blosc2_context* cctx;
static blosc2_context* dctx;


static int32_t read_i32_le(const uint8_t* p) {
  return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}


static bool reference_is_run(const uint8_t* p, int32_t len) {
  for (int32_t i = 1; i < len; i++) {
    if (p[i] != p[0]) {
      return false;
    }
  }
  return true;
}


/* Compress NRUNBLOCKS run blocks followed by the first `len` bytes of
   `stream`, which become the last (leftover) block, i.e. one stream.  The
   source starts `offset` bytes into a heap buffer that ends with the stream. */
static char* check_stream(int32_t len, int offset) {
  int32_t nbytes = NRUNBLOCKS * BLOCKSIZE + len;
  uint8_t* buf = malloc((size_t)(offset + nbytes));
  uint8_t* src = buf + offset;
  memset(src, (uint8_t)(stream[0] + 1), NRUNBLOCKS * BLOCKSIZE);
  memcpy(src + NRUNBLOCKS * BLOCKSIZE, stream, (size_t)len);
  int32_t cbytes = blosc2_compress_ctx(cctx, src, nbytes, chunk, (int32_t)sizeof(chunk));
  char* msg = EXIT_SUCCESS;
  if (cbytes <= 0 || (chunk[BLOSC2_CHUNK_FLAGS] & BLOSC_MEMCPYED) != 0 ||
      read_i32_le(chunk + BLOSC2_CHUNK_BLOCKSIZE) != BLOCKSIZE) {
    msg = "Cannot compress into the expected blocks";
    goto out;
  }
  /* bstarts follow the header */
  int32_t bstart = read_i32_le(chunk + BLOSC_EXTENDED_HEADER_LENGTH + NRUNBLOCKS * (int32_t)sizeof(int32_t));
  if (bstart <= 0 || bstart + (int32_t)sizeof(int32_t) > cbytes) {
    msg = "Bad block start";
    goto out;
  }
  bool stored_as_run = read_i32_le(chunk + bstart) <= 0;
  if (stored_as_run != reference_is_run(stream, len)) {
    fprintf(stderr, "len %d, offset %d: stored as run: %d\n", len, offset, stored_as_run);
    msg = "A stream must be stored as a run exactly when all its bytes are equal";
    goto out;
  }
  int32_t dbytes = blosc2_decompress_ctx(dctx, chunk, cbytes, dest, (int32_t)sizeof(dest));
  if (dbytes != nbytes || memcmp(src, dest, (size_t)nbytes) != 0) {
    msg = "Wrong data after decompression";
  }
out:
  free(buf);
  return msg;
}


/* A run of a random byte, then the same with one bit changed at every
   position, then streams with a few random differences; random start
   alignments */
static char* check_length(int32_t len) {
  uint8_t byte = (uint8_t)rand();
  for (int32_t pos = -1; pos < len; pos++) {
    memset(stream, byte, (size_t)len);
    if (pos >= 0) {
      stream[pos] ^= (uint8_t)(1 << (rand() % 8));
    }
    char* msg = check_stream(len, rand() % (MAXOFFSET + 1));
    if (msg != EXIT_SUCCESS) {
      return msg;
    }
  }
  for (int i = 0; i < 16; i++) {
    memset(stream, byte, (size_t)len);
    for (int k = rand() % 4; k > 0; k--) {
      stream[rand() % len] = (uint8_t)rand();
    }
    char* msg = check_stream(len, rand() % (MAXOFFSET + 1));
    if (msg != EXIT_SUCCESS) {
      return msg;
    }
  }
  return EXIT_SUCCESS;
}


/* Lengths 1..300: zero to four 64-byte steps, with every remainder */
static char* test_short_streams(void) {
  for (int32_t len = 1; len <= 300; len++) {
    char* msg = check_length(len);
    if (msg != EXIT_SUCCESS) {
      return msg;
    }
  }
  return EXIT_SUCCESS;
}


static char* test_long_streams(void) {
  int32_t lens[] = {511, 512, 513, 1000, 2047, 4095};
  for (size_t i = 0; i < ARRAY_SIZE(lens); i++) {
    char* msg = check_length(lens[i]);
    if (msg != EXIT_SUCCESS) {
      return msg;
    }
  }
  return EXIT_SUCCESS;
}


static char* all_tests(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.compcode = BLOSC_LZ4;
  cparams.typesize = 1;
  cparams.blocksize = BLOCKSIZE;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_NOFILTER;
  cctx = blosc2_create_cctx(cparams);
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dctx = blosc2_create_dctx(dparams);
  srand(1);

  mu_run_test(test_short_streams);
  mu_run_test(test_long_streams);

  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
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
  printf("\tTests run: %d\n", tests_run);
  blosc2_destroy();
  return result != EXIT_SUCCESS;
}
