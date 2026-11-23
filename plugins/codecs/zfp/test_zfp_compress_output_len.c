/*********************************************************************
    Blosc - Blocked Shuffling and Compression Library

    Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
    https://blosc.org
    License: BSD 3-Clause (see LICENSE.txt)

    See LICENSE.txt for details about copyright and rights to use.

    Regression test for ZFP compressor output_len bounds checking.

    Blosc can pass output_len < input_len to codec encoders when the
    remaining destination chunk buffer is smaller than the uncompressed
    block size. Verify that zfp_acc_compress, zfp_prec_compress, and
    zfp_rate_compress return 0 without overflowing output when the
    compressed stream exceeds output_len, succeed when it fits exactly,
    and reject zero/negative output_len.
**********************************************************************/

#include "b2nd.h"
#include "blosc2.h"
#include "blosc2-zfp.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TYPESIZE 4   /* float */
#define BLOCK_DIM 8
#define CANARY_BYTE 0xA5
#define CANARY_PAD 64

typedef int (*zfp_compress_fn)(const uint8_t *input, int32_t input_len,
                               uint8_t *output, int32_t output_len,
                               uint8_t meta, blosc2_cparams *cparams,
                               const void *chunk);

static blosc2_schunk *make_schunk(int8_t ndim, const int64_t *shape,
                                  const int32_t *chunkshape, const int32_t *blockshape) {
  blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = TYPESIZE;
  storage.cparams = &cparams;
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    return NULL;
  }
  uint8_t *smeta = NULL;
  int smeta_len = b2nd_serialize_meta(ndim, shape, chunkshape, blockshape, "<f4", 0, &smeta);
  if (smeta_len < 0) {
    blosc2_schunk_free(schunk);
    return NULL;
  }
  int rc = blosc2_meta_add(schunk, "b2nd", smeta, smeta_len);
  free(smeta);
  if (rc < 0) {
    blosc2_schunk_free(schunk);
    return NULL;
  }
  return schunk;
}

static int check_compressor(const char *name, zfp_compress_fn compress_fn, uint8_t meta,
                            const uint8_t *input, int32_t input_len, blosc2_cparams *cparams) {
  /* 1. Compress with full input_len capacity to obtain exact compressed size. */
  uint8_t *full_out = malloc((size_t) input_len);
  if (full_out == NULL) {
    printf("  %-18s: malloc full_out failed -- FAIL\n", name);
    return -1;
  }
  int full_csize = compress_fn(input, input_len, full_out, input_len, meta, cparams, NULL);
  free(full_out);
  if (full_csize <= 8 || full_csize >= input_len) {
    printf("  %-18s: unexpected full_csize=%d (expected 8 < size < %d) -- FAIL\n",
           name, full_csize, input_len);
    return -1;
  }

  int32_t tight_len = (int32_t) (full_csize - 1);

  /* 2. Tight exact-sized heap buffer (catches overflow under ASan / Guard Malloc). */
  uint8_t *tight_out = malloc((size_t) tight_len);
  if (tight_out == NULL) {
    printf("  %-18s: malloc tight_out failed -- FAIL\n", name);
    return -1;
  }
  int rc = compress_fn(input, input_len, tight_out, tight_len, meta, cparams, NULL);
  free(tight_out);
  if (rc != 0) {
    printf("  %-18s: tight exact buffer returned %d (expected 0) -- FAIL\n", name, rc);
    return -1;
  }

  /* 3. Tight canary buffer (catches overflow even without sanitizers). */
  size_t canary_total = (size_t) tight_len + CANARY_PAD;
  uint8_t *canary_out = malloc(canary_total);
  if (canary_out == NULL) {
    printf("  %-18s: malloc canary_out failed -- FAIL\n", name);
    return -1;
  }
  memset(canary_out, CANARY_BYTE, canary_total);
  rc = compress_fn(input, input_len, canary_out, tight_len, meta, cparams, NULL);
  if (rc != 0) {
    printf("  %-18s: canary buffer returned %d (expected 0) -- FAIL\n", name, rc);
    free(canary_out);
    return -1;
  }
  for (size_t i = (size_t) tight_len; i < canary_total; i++) {
    if (canary_out[i] != CANARY_BYTE) {
      printf("  %-18s: canary byte overwritten at offset %zu (0x%02x) -- FAIL\n",
             name, i, canary_out[i]);
      free(canary_out);
      return -1;
    }
  }
  free(canary_out);

  /* 4. Exact-fit buffer: output_len == full_csize must succeed. */
  uint8_t *exact_out = malloc((size_t) full_csize);
  if (exact_out == NULL) {
    printf("  %-18s: malloc exact_out failed -- FAIL\n", name);
    return -1;
  }
  rc = compress_fn(input, input_len, exact_out, (int32_t) full_csize, meta, cparams, NULL);
  free(exact_out);
  if (rc != full_csize) {
    printf("  %-18s: exact-fit buffer returned %d (expected %d) -- FAIL\n",
           name, rc, full_csize);
    return -1;
  }

  /* 5. Zero and negative output_len must return 0 without writing. */
  uint8_t sentinel = CANARY_BYTE;
  rc = compress_fn(input, input_len, &sentinel, 0, meta, cparams, NULL);
  if (rc != 0 || sentinel != CANARY_BYTE) {
    printf("  %-18s: zero output_len failed (rc=%d, sentinel=0x%02x) -- FAIL\n",
           name, rc, sentinel);
    return -1;
  }
  rc = compress_fn(input, input_len, &sentinel, -1, meta, cparams, NULL);
  if (rc != 0 || sentinel != CANARY_BYTE) {
    printf("  %-18s: negative output_len failed (rc=%d, sentinel=0x%02x) -- FAIL\n",
           name, rc, sentinel);
    return -1;
  }

  printf("  %-18s: full_csize=%d -- OK\n", name, full_csize);
  return 0;
}

int main(void) {
  int result = 0;
  blosc2_init();
  printf("ZFP compressor output_len bounds tests:\n");

  int64_t shape[2] = { BLOCK_DIM, BLOCK_DIM };
  int32_t chunkshape[2] = { BLOCK_DIM, BLOCK_DIM };
  int32_t blockshape[2] = { BLOCK_DIM, BLOCK_DIM };
  blosc2_schunk *schunk = make_schunk(2, shape, chunkshape, blockshape);
  if (schunk == NULL) {
    printf("  could not build schunk -- FAIL\n");
    blosc2_destroy();
    return -1;
  }

  float input_vals[BLOCK_DIM * BLOCK_DIM];
  for (int i = 0; i < BLOCK_DIM * BLOCK_DIM; i++) {
    input_vals[i] = (float) ((i * 17 + 5) % 31) * 0.25f + (float) (i / BLOCK_DIM) * 1.5f;
  }
  int32_t input_len = (int32_t) sizeof(input_vals);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = TYPESIZE;
  cparams.schunk = schunk;

  result |= check_compressor("zfp_acc_compress", zfp_acc_compress,
                             (uint8_t) (int8_t) -2,
                             (const uint8_t *) input_vals, input_len, &cparams);
  result |= check_compressor("zfp_prec_compress", zfp_prec_compress,
                             12,
                             (const uint8_t *) input_vals, input_len, &cparams);
  result |= check_compressor("zfp_rate_compress", zfp_rate_compress,
                             50,
                             (const uint8_t *) input_vals, input_len, &cparams);

  blosc2_schunk_free(schunk);
  if (result == 0) {
    printf("All ZFP compressor output_len bounds checks passed.\n");
  }
  blosc2_destroy();
  return result;
}
