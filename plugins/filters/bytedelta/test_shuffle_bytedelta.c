/* Byte-identical pair kernels and cross-decoding through staged filter aliases. */
#include "bytedelta.h"
#include "blosc2/filters-registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    exit(1); \
  } \
} while (0)

static void check_kernel(int typesize, int length, int pattern) {
  uint8_t *allocation = malloc((size_t)length + 1);
  uint8_t *scratch = malloc((size_t)length + 32);
  uint8_t *expected_base = malloc((size_t)length + 1);
  uint8_t *encoded_base = malloc((size_t)length + 32);
  uint8_t *decoded_base = malloc((size_t)length + 32);
  CHECK(allocation && scratch && expected_base && encoded_base && decoded_base);
  uint8_t *input = allocation + 1;  /* Unaligned, with no readable suffix padding. */
  uint8_t *expected = expected_base + 1;
  uint8_t *encoded = encoded_base + 3, *decoded = decoded_base + 5;
  uint32_t random = 1234567;
  for (int i = 0; i < length; i++) {
    random = random * 1664525U + 1013904223U;
    input[i] = pattern == 0 ? (uint8_t)(random >> 24) :
               pattern == 1 ? (uint8_t)((i / typesize) % 251) : 0;
  }
  memset(encoded_base, 0xcd, (size_t)length + 32);
  memset(decoded_base, 0xcd, (size_t)length + 32);
  blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dp = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_shuffle(typesize, length, input, scratch);
  CHECK(bytedelta_forward(scratch, expected, length, (uint8_t)typesize, &cp, 0) == 0);
  /* Independent bytewise oracle, not just agreement between two SIMD paths. */
  int count = length / typesize;
  for (int p = 0; p < typesize; p++) {
    uint8_t carry = 0;
    for (int i = 0; i < count; i++) {
      uint8_t value = input[i * typesize + p];
      decoded[p * count + i] = (uint8_t)(value - carry);
      carry = value;
    }
  }
  memcpy(decoded + count * typesize, input + count * typesize, (size_t)(length % typesize));
  CHECK(memcmp(expected, decoded, (size_t)length) == 0);
  memset(decoded_base, 0xcd, (size_t)length + 32);
  int handled = shuffle_bytedelta_forward(input, encoded, length, typesize);
  CHECK(handled == shuffle_bytedelta_available());
  if (handled) {
    CHECK(memcmp(encoded, expected, (size_t)length) == 0);
    CHECK(shuffle_bytedelta_backward(expected, decoded, length, typesize) == 1);
    CHECK(memcmp(decoded, input, (size_t)length) == 0);
    CHECK(bytedelta_backward(encoded, scratch, length, (uint8_t)typesize, &dp, 0) == 0);
    blosc2_unshuffle(typesize, length, scratch, expected);
    CHECK(memcmp(expected, input, (size_t)length) == 0);
    for (int i = 0; i < 3; i++) CHECK(encoded_base[i] == 0xcd);
    for (int i = length + 3; i < length + 32; i++) CHECK(encoded_base[i] == 0xcd);
    for (int i = 0; i < 5; i++) CHECK(decoded_base[i] == 0xcd);
    for (int i = length + 5; i < length + 32; i++) CHECK(decoded_base[i] == 0xcd);
  }
  free(allocation);
  free(scratch);
  free(expected_base);
  free(encoded_base);
  free(decoded_base);
}

static int copy_postfilter(blosc2_postfilter_params *params) {
  if (*(int *)params->user_data) return -1;
  memcpy(params->output, params->input, (size_t)params->size);
  return 0;
}

static void check_pipeline(int typesize, int threads, int level, int split, int slot, int mode) {
  int legacy = mode == 4;
  /* DELTA and the historical buggy filter require whole records. Test partial
   * records on the corrected pair without those unrelated legacy constraints. */
  int length = typesize * 1032 + (legacy || mode == 5 ? 0 : 3);
  uint8_t *input = malloc((size_t)length);
  uint8_t *decoded = malloc((size_t)length);
  uint8_t *fused = malloc((size_t)length + BLOSC2_MAX_OVERHEAD);
  uint8_t *staged = malloc((size_t)length + BLOSC2_MAX_OVERHEAD);
  CHECK(input && decoded && fused && staged);
  for (int i = 0; i < 1032; i++) {
    double value = (double)i / 1031;
    if (typesize == 2) {
      uint16_t integer = (uint16_t)(i * 37);
      memcpy(input + i * typesize, &integer, 2);
    }
    else if (typesize == 8) memcpy(input + i * typesize, &value, 8);
    else {
      float single = (float)value;
      memcpy(input + i * typesize, &single, 4);
    }
  }
  for (int i = typesize * 1032; i < length; i++) input[i] = (uint8_t)i;
  blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
  cp.typesize = typesize;
  cp.compcode = BLOSC_LZ4;
  cp.blocksize = typesize * 137;
  cp.nthreads = (int16_t)threads;
  cp.clevel = (uint8_t)level;
  cp.splitmode = split ? BLOSC_ALWAYS_SPLIT : BLOSC_NEVER_SPLIT;
  memset(cp.filters, 0, sizeof(cp.filters));
  int delta_slot = slot + 1 + (mode == 9 && slot + 2 < BLOSC2_MAX_FILTERS ? 1 : 0);
  cp.filters[slot] = BLOSC_SHUFFLE;
  cp.filters[delta_slot] = legacy ? BLOSC_FILTER_BYTEDELTA_BUGGY : BLOSC_FILTER_BYTEDELTA;
  cp.filters_meta[delta_slot] = mode == 1 ? 0 : (uint8_t)typesize;
  if (mode == 2) cp.filters_meta[slot] = (uint8_t)(typesize / 2);  /* Mismatch: fallback. */
  if (mode == 3) {  /* Matching grouping overrides need not equal record typesize. */
    cp.filters_meta[slot] = 4;
    cp.filters_meta[delta_slot] = 4;
  }
  if (mode == 8) {  /* Matching but unsupported group size: staged fallback. */
    cp.filters_meta[slot] = 3;
    cp.filters_meta[delta_slot] = 3;
  }
  if (mode == 5 && slot > 0) cp.filters[slot - 1] = BLOSC_DELTA;
  if (mode == 6 && slot + 2 < BLOSC2_MAX_FILTERS) cp.filters[slot + 2] = BLOSC_BITSHUFFLE;
  blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
  storage.cparams = &cp;
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  CHECK(schunk != NULL);
  cp.schunk = schunk;
  blosc2_context *encoder = blosc2_create_cctx(cp);
  uint8_t reference_id = legacy ? 248 : 249;
  cp.filters[delta_slot] = reference_id;  /* Alias prevents pair recognition. */
  blosc2_context *reference = blosc2_create_cctx(cp);
  CHECK(encoder && reference);
  int bytes = blosc2_compress_ctx(encoder, input, length, fused, length + BLOSC2_MAX_OVERHEAD);
  int reference_bytes = blosc2_compress_ctx(reference, input, length, staged, length + BLOSC2_MAX_OVERHEAD);
  CHECK(bytes > 0 && bytes == reference_bytes);
  if (threads == 1) {
    if (memcmp(fused + BLOSC_EXTENDED_HEADER_LENGTH, staged + BLOSC_EXTENDED_HEADER_LENGTH,
               (size_t)bytes - BLOSC_EXTENDED_HEADER_LENGTH) != 0) {
      fprintf(stderr, "payload mismatch: size=%d threads=%d level=%d split=%d slot=%d mode=%d\n",
              typesize, threads, level, split, slot, mode);
    }
    CHECK(memcmp(fused + BLOSC_EXTENDED_HEADER_LENGTH, staged + BLOSC_EXTENDED_HEADER_LENGTH,
                 (size_t)bytes - BLOSC_EXTENDED_HEADER_LENGTH) == 0);
  }
  int fail = 0;
  blosc2_postfilter_params postparams = {0};
  postparams.user_data = &fail;
  blosc2_dparams dp = BLOSC2_DPARAMS_DEFAULTS;
  dp.schunk = schunk;
  dp.nthreads = (int16_t)threads;
  if (mode == 7) {
    dp.postfilter = copy_postfilter;
    dp.postparams = &postparams;
  }
  blosc2_context *decoder = blosc2_create_dctx(dp);
  CHECK(decoder != NULL);
  /* Fused bytes read through the original staged inverse, and vice versa. */
  for (int direction = 0; direction < 2; direction++) {
    uint8_t *chunk = direction == 0 ? fused : staged;
    chunk[BLOSC2_CHUNK_FILTER_CODES + delta_slot] = direction == 0 ? reference_id :
        (legacy ? BLOSC_FILTER_BYTEDELTA_BUGGY : BLOSC_FILTER_BYTEDELTA);
    CHECK(blosc2_decompress_ctx(decoder, chunk, bytes, decoded, length) == length);
    CHECK(memcmp(decoded, input, (size_t)length) == 0);
  }
  if (mode == 7) {
    fail = 1;
    CHECK(blosc2_decompress_ctx(decoder, staged, bytes, decoded, length) < 0);
  }
  blosc2_free_ctx(encoder);
  blosc2_free_ctx(reference);
  blosc2_free_ctx(decoder);
  blosc2_schunk_free(schunk);
  free(input);
  free(decoded);
  free(fused);
  free(staged);
}

static void check_missing_schunk(void) {
  blosc2_cparams cp = BLOSC2_CPARAMS_DEFAULTS;
  cp.typesize = 8;
  cp.compcode = BLOSC_LZ4;
  cp.nthreads = 1;
  memset(cp.filters, 0, sizeof(cp.filters));
  cp.filters[0] = BLOSC_SHUFFLE;
  cp.filters[1] = BLOSC_FILTER_BYTEDELTA;
  /* BYTEDELTA meta=0 requires a schunk even when the adjacent shuffle agrees. */
  uint8_t input[4096], output[4096 + BLOSC2_MAX_OVERHEAD];
  for (size_t i = 0; i < sizeof(input); i++) input[i] = (uint8_t)(i % 251);
  blosc2_context *encoder = blosc2_create_cctx(cp);
  CHECK(encoder != NULL);
  CHECK(blosc2_compress_ctx(encoder, input, sizeof(input), output, sizeof(output)) < 0);
  blosc2_free_ctx(encoder);
}

int main(void) {
  blosc2_init();
  blosc2_filter aliases[] = {
    {.id = 249, .name = "staged_bytedelta", .version = 1,
     .forward = bytedelta_forward, .backward = bytedelta_backward},
    {.id = 248, .name = "staged_bytedelta_buggy", .version = 1,
     .forward = bytedelta_forward_buggy, .backward = bytedelta_backward_buggy}
  };
  CHECK(blosc2_register_filter(&aliases[0]) == 0);
  CHECK(blosc2_register_filter(&aliases[1]) == 0);
  printf("Runtime fused kernels available: %d\n", shuffle_bytedelta_available());
  for (int typesize = 2; typesize <= 8; typesize *= 2) {
    for (int pattern = 0; pattern < 3; pattern++) {
      for (int length = 0; length < 130 * typesize; length++) check_kernel(typesize, length, pattern);
      check_kernel(typesize, 80000, pattern);
      check_kernel(typesize, 400000, pattern);
      check_kernel(typesize, 34119, pattern);
    }
    for (int threads = 1; threads <= 4; threads += 3)
      for (int level = 0; level <= 5; level += 5)
        for (int split = 0; split < 2; split++)
          for (int slot = 0; slot <= 4; slot += 2)
            for (int mode = 0; mode < 10; mode++) check_pipeline(typesize, threads, level, split, slot, mode);
  }
  uint8_t data[64] = {0}, output[64];
  CHECK(shuffle_bytedelta_forward(data, output, sizeof(data), 3) == 0);
  CHECK(shuffle_bytedelta_backward(data, output, sizeof(data), 3) == 0);
  CHECK(shuffle_bytedelta_forward(data, data, sizeof(data), 4) == 0);
  CHECK(shuffle_bytedelta_backward(data, data, sizeof(data), 4) == 0);
  CHECK(shuffle_bytedelta_forward(data, output, -1, 4) == 0);
  CHECK(shuffle_bytedelta_backward(data, output, -1, 4) == 0);
  check_missing_schunk();
  blosc2_destroy();
  puts("SHUFFLE+BYTEDELTA kernels and staged/fused cross-decoding passed");
  return 0;
}
