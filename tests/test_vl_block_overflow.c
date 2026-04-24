/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Regression test for VL-block cumulative-size integer overflow.

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
**********************************************************************/

#include "test_common.h"

#include <stdint.h>

static int32_t read_i32(const uint8_t* src) {
  int32_t value;
  memcpy(&value, src, sizeof(value));
  return value;
}

static void write_i32(uint8_t* dst, int32_t value) {
  memcpy(dst, &value, sizeof(value));
}

int main(void) {
  const int32_t block_nbytes = 1024 * 1024;
  const int32_t nblocks = 4097;
  const size_t template_cap = 4 * 1024 * 1024;
  int rc = EXIT_SUCCESS;
  uint8_t* block = NULL;
  uint8_t* template_chunk = NULL;
  uint8_t* malformed = NULL;
  uint8_t* out = NULL;
  blosc2_context* cctx = NULL;

  blosc2_init();

  block = calloc(1, (size_t)block_nbytes);
  if (block == NULL) {
    rc = EXIT_FAILURE;
    goto cleanup;
  }

  const void* srcs[1] = {block};
  int32_t srcsizes[1] = {block_nbytes};
  template_chunk = malloc(template_cap);
  if (template_chunk == NULL) {
    rc = EXIT_FAILURE;
    goto cleanup;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  cparams.clevel = 9;
  cctx = blosc2_create_cctx(cparams);
  if (cctx == NULL) {
    rc = EXIT_FAILURE;
    goto cleanup;
  }

  int32_t template_cbytes = blosc2_vlcompress_ctx(cctx, srcs, srcsizes, 1, template_chunk, (int32_t)template_cap);
  if (template_cbytes < 0) {
    rc = EXIT_FAILURE;
    goto cleanup;
  }

  const int32_t header_overhead = BLOSC_EXTENDED_HEADER_LENGTH;
  int32_t bstart0 = read_i32(template_chunk + header_overhead);
  if (bstart0 <= 0 || bstart0 >= template_cbytes) {
    rc = EXIT_FAILURE;
    goto cleanup;
  }

  int32_t block_span = template_cbytes - bstart0;
  if (block_span <= 4) {
    rc = EXIT_FAILURE;
    goto cleanup;
  }

  size_t bstarts_bytes = (size_t)nblocks * sizeof(int32_t);
  size_t data_start = (size_t)header_overhead + bstarts_bytes;
  size_t malformed_cbytes_size = data_start + (size_t)nblocks * (size_t)block_span;
  if (malformed_cbytes_size > INT32_MAX) {
    rc = EXIT_FAILURE;
    goto cleanup;
  }
  int32_t malformed_cbytes = (int32_t)malformed_cbytes_size;

  malformed = malloc(malformed_cbytes_size);
  out = malloc((size_t)block_nbytes);
  if (malformed == NULL || out == NULL) {
    rc = EXIT_FAILURE;
    goto cleanup;
  }

  memcpy(malformed, template_chunk, (size_t)header_overhead);
  write_i32(malformed + BLOSC2_CHUNK_NBYTES, block_nbytes);
  write_i32(malformed + BLOSC2_CHUNK_BLOCKSIZE, nblocks);
  write_i32(malformed + BLOSC2_CHUNK_CBYTES, malformed_cbytes);

  for (int32_t i = 0; i < nblocks; ++i) {
    int32_t off = (int32_t)(data_start + (size_t)i * (size_t)block_span);
    write_i32(malformed + header_overhead + (size_t)i * sizeof(int32_t), off);
  }

  uint8_t* cursor = malformed + data_start;
  for (int32_t i = 0; i < nblocks; ++i) {
    memcpy(cursor, template_chunk + bstart0, (size_t)block_span);
    cursor += block_span;
  }

  rc = blosc2_decompress(malformed, malformed_cbytes, out, block_nbytes);
  if (rc >= 0) {
    printf("Expected malformed VL chunk to be rejected, got rc=%d\n", rc);
    rc = EXIT_FAILURE;
    goto cleanup;
  }

  rc = EXIT_SUCCESS;

cleanup:
  if (cctx != NULL) {
    blosc2_free_ctx(cctx);
  }
  free(block);
  free(template_chunk);
  free(malformed);
  free(out);
  blosc2_destroy();
  return rc;
}
