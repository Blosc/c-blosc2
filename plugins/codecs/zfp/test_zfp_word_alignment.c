/*********************************************************************
    Blosc - Blocked Shuffling and Compression Library

    Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
    https://blosc.org
    License: BSD 3-Clause (see LICENSE.txt)

    See LICENSE.txt for details about copyright and rights to use.

    Regression test for ZFP 64-bit word alignment over-read safety.

    ZFP's bitstream reader fetches 64-bit words without bounds checking.
    When a bitstream or block is not an exact multiple of 64 bits (8 bytes),
    reading the final word could touch up to 7 bytes beyond the allocation.
    These tests verify that input buffers of exact, non-word-multiple sizes
    (allocated with malloc) can be safely processed without triggering ASan
    out-of-bounds reads.
**********************************************************************/

#include "b2nd.h"
#include "blosc2.h"
#include "blosc2/codecs-registry.h"
#include "blosc2-zfp.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define TYPESIZE 4   /* float */

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

/* Test zfp_rate_decompress with an exact allocation where blocks * bits % 64 != 0 */
static int test_rate_word_alignment(void) {
  /* 1D array with 20 floats (5 blocks of 4 elements) */
  int64_t shape[1] = { 20 };
  int32_t chunkshape[1] = { 20 };
  int32_t blockshape[1] = { 20 };
  blosc2_schunk *schunk = make_schunk(1, shape, chunkshape, blockshape);
  if (schunk == NULL) {
    printf("  test_rate_word_alignment: could not create schunk -- FAIL\n");
    return -1;
  }

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.schunk = schunk;

  /* For 5 blocks with meta = 32: bits = 41. 5 * 41 = 205 bits.
   * ceil(205 / 8) = 26 bytes. 26 is not a multiple of 8 (26 % 8 == 2).
   * An exact 26-byte allocation must not trigger an ASan OOB read when
   * reading the final 64-bit word (bytes 24..31). */
  int32_t input_len = 26;
  uint8_t *input = malloc((size_t) input_len);
  if (input == NULL) {
    blosc2_schunk_free(schunk);
    return -1;
  }
  memset(input, 0, (size_t) input_len);

  float output[20];
  int rc = zfp_rate_decompress(input, input_len, (uint8_t *) output, (int32_t) sizeof(output), 32, &dparams, NULL);
  free(input);
  blosc2_schunk_free(schunk);

  if (rc <= 0) {
    printf("  test_rate_word_alignment: decompress failed (%d) -- FAIL\n", rc);
    return -1;
  }
  printf("  test_rate_word_alignment -- OK\n");
  return 0;
}

/* Test zfp_prec_decompress with an exact non-word-aligned allocation */
static int test_prec_word_alignment(void) {
  /* 1D array with 292 floats (73 blocks of 4 elements) */
  int64_t shape[1] = { 292 };
  int32_t chunkshape[1] = { 292 };
  int32_t blockshape[1] = { 292 };
  blosc2_schunk *schunk = make_schunk(1, shape, chunkshape, blockshape);
  if (schunk == NULL) {
    printf("  test_prec_word_alignment: could not create schunk -- FAIL\n");
    return -1;
  }

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.schunk = schunk;

  /* 73 blocks require min_bytes = (73 + 7) / 8 = 10 bytes.
   * 10 is not a multiple of 8 (10 % 8 == 2).
   * An exact 10-byte allocation must not trigger ASan OOB read on word 1 (bytes 8..15). */
  int32_t input_len = 10;
  uint8_t *input = malloc((size_t) input_len);
  if (input == NULL) {
    blosc2_schunk_free(schunk);
    return -1;
  }
  memset(input, 0, (size_t) input_len);

  float output[292];
  /* Will safely return without ASan buffer over-read */
  int rc = zfp_prec_decompress(input, input_len, (uint8_t *) output, (int32_t) sizeof(output), 20, &dparams, NULL);
  free(input);
  blosc2_schunk_free(schunk);

  (void) rc;
  printf("  test_prec_word_alignment -- OK\n");
  return 0;
}

/* Test zfp_acc_word_alignment with an exact non-word-aligned allocation */
static int test_acc_word_alignment(void) {
  int64_t shape[1] = { 292 };
  int32_t chunkshape[1] = { 292 };
  int32_t blockshape[1] = { 292 };
  blosc2_schunk *schunk = make_schunk(1, shape, chunkshape, blockshape);
  if (schunk == NULL) {
    printf("  test_acc_word_alignment: could not create schunk -- FAIL\n");
    return -1;
  }

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.schunk = schunk;

  int32_t input_len = 10;
  uint8_t *input = malloc((size_t) input_len);
  if (input == NULL) {
    blosc2_schunk_free(schunk);
    return -1;
  }
  memset(input, 0, (size_t) input_len);

  float output[292];
  int rc = zfp_acc_decompress(input, input_len, (uint8_t *) output, (int32_t) sizeof(output), 4, &dparams, NULL);
  free(input);
  blosc2_schunk_free(schunk);

  (void) rc;
  printf("  test_acc_word_alignment -- OK\n");
  return 0;
}

/* Test getcell / getitem with exact chunk buffer allocation */
static int test_getitem_word_alignment(void) {
  int64_t shape[1] = { 20 };
  int32_t chunkshape[1] = { 20 };
  int32_t blockshape[1] = { 20 };
  blosc2_schunk *schunk = make_schunk(1, shape, chunkshape, blockshape);
  if (schunk == NULL) {
    printf("  test_getitem_word_alignment: could not create schunk -- FAIL\n");
    return -1;
  }

  float src_data[20];
  for (int i = 0; i < 20; i++) {
    src_data[i] = (float) (i + 1) * 1.5f;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = TYPESIZE;
  cparams.compcode = BLOSC_CODEC_ZFP_FIXED_RATE;
  cparams.compcode_meta = 32;
  cparams.schunk = schunk;
  blosc2_context *cctx = blosc2_create_cctx(cparams);

  int32_t max_out = (int32_t) sizeof(src_data) + BLOSC2_MAX_OVERHEAD;
  uint8_t *tmp_chunk = malloc((size_t) max_out);
  int64_t csize = blosc2_compress_ctx(cctx, src_data, (int32_t) sizeof(src_data), tmp_chunk, max_out);
  blosc2_free_ctx(cctx);

  if (csize <= 0) {
    free(tmp_chunk);
    blosc2_schunk_free(schunk);
    printf("  test_getitem_word_alignment: compress failed -- FAIL\n");
    return -1;
  }

  /* Copy into exact-sized buffer with no trailing padding */
  uint8_t *exact_chunk = malloc((size_t) csize);
  memcpy(exact_chunk, tmp_chunk, (size_t) csize);
  free(tmp_chunk);

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.schunk = schunk;
  blosc2_context *dctx = blosc2_create_dctx(dparams);

  /* Retrieve every item, especially the last element (index 19) in the final cell */
  for (int i = 0; i < 20; i++) {
    float val = 0.0f;
    int nbytes = blosc2_getitem_ctx(dctx, exact_chunk, (int32_t) csize, i, 1, &val, (int32_t) sizeof(val));
    if (nbytes <= 0) {
      printf("  test_getitem_word_alignment: getitem at index %d failed -- FAIL\n", i);
      free(exact_chunk);
      blosc2_free_ctx(dctx);
      blosc2_schunk_free(schunk);
      return -1;
    }
  }

  free(exact_chunk);
  blosc2_free_ctx(dctx);
  blosc2_schunk_free(schunk);

  printf("  test_getitem_word_alignment -- OK\n");
  return 0;
}

int main(void) {
  int result = 0;
  blosc2_init();
  printf("ZFP word-alignment read safety tests:\n");

  result |= test_rate_word_alignment();
  result |= test_prec_word_alignment();
  result |= test_acc_word_alignment();
  result |= test_getitem_word_alignment();

  if (result == 0) {
    printf("All ZFP word-alignment checks passed.\n");
  }
  blosc2_destroy();
  return result;
}
