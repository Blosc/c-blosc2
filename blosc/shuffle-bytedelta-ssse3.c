/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library
  Copyright (c) 2026 Blosc Development Team <blosc@blosc.org>
  License: BSD 3-Clause (see LICENSE.txt)
**********************************************************************/

/* This translation unit alone is compiled for SSSE3. The baseline shuffle
 * dispatcher must check CPUID before calling either entry point. */
#include "../plugins/filters/bytedelta/bytedelta.h"

#include <tmmintrin.h>
#include <string.h>

static inline void load_planes16(const uint8_t *input, __m128i *planes, int typesize) {
  if (typesize == 2) {
    const __m128i mask = _mm_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15);
    __m128i a = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *)input), mask);
    __m128i b = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *)(input + 16)), mask);
    planes[0] = _mm_unpacklo_epi64(a, b);
    planes[1] = _mm_unpackhi_epi64(a, b);
  }
  else if (typesize == 4) {
    const __m128i mask = _mm_setr_epi8(0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15);
    __m128i a[4], b[4];
    for (int p = 0; p < 4; p++) {
      a[p] = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *)(input + p * 16)), mask);
    }
    b[0] = _mm_unpacklo_epi32(a[0], a[1]);
    b[1] = _mm_unpackhi_epi32(a[0], a[1]);
    b[2] = _mm_unpacklo_epi32(a[2], a[3]);
    b[3] = _mm_unpackhi_epi32(a[2], a[3]);
    planes[0] = _mm_unpacklo_epi64(b[0], b[2]);
    planes[1] = _mm_unpackhi_epi64(b[0], b[2]);
    planes[2] = _mm_unpacklo_epi64(b[1], b[3]);
    planes[3] = _mm_unpackhi_epi64(b[1], b[3]);
  }
  else {
    const __m128i mask = _mm_setr_epi8(0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15);
    __m128i a[8], b[8];
    for (int p = 0; p < 8; p++) {
      b[p] = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *)(input + p * 16)), mask);
    }
    for (int p = 0; p < 4; p++) {
      a[p * 2] = _mm_unpacklo_epi16(b[p * 2], b[p * 2 + 1]);
      a[p * 2 + 1] = _mm_unpackhi_epi16(b[p * 2], b[p * 2 + 1]);
    }
    for (int p = 0; p < 4; p++) {
      int k = p < 2 ? p : p + 2;
      b[p * 2] = _mm_unpacklo_epi32(a[k], a[k + 2]);
      b[p * 2 + 1] = _mm_unpackhi_epi32(a[k], a[k + 2]);
    }
    for (int p = 0; p < 4; p++) {
      planes[p * 2] = _mm_unpacklo_epi64(b[p], b[p + 4]);
      planes[p * 2 + 1] = _mm_unpackhi_epi64(b[p], b[p + 4]);
    }
  }
}

static inline void store_planes16(uint8_t *output, const __m128i *planes, int typesize) {
  if (typesize == 2) {
    _mm_storeu_si128((__m128i *)output, _mm_unpacklo_epi8(planes[0], planes[1]));
    _mm_storeu_si128((__m128i *)(output + 16), _mm_unpackhi_epi8(planes[0], planes[1]));
  }
  else if (typesize == 4) {
    __m128i a[4];
    a[0] = _mm_unpacklo_epi8(planes[0], planes[1]);
    a[1] = _mm_unpackhi_epi8(planes[0], planes[1]);
    a[2] = _mm_unpacklo_epi8(planes[2], planes[3]);
    a[3] = _mm_unpackhi_epi8(planes[2], planes[3]);
    _mm_storeu_si128((__m128i *)output, _mm_unpacklo_epi16(a[0], a[2]));
    _mm_storeu_si128((__m128i *)(output + 16), _mm_unpackhi_epi16(a[0], a[2]));
    _mm_storeu_si128((__m128i *)(output + 32), _mm_unpacklo_epi16(a[1], a[3]));
    _mm_storeu_si128((__m128i *)(output + 48), _mm_unpackhi_epi16(a[1], a[3]));
  }
  else {
    __m128i a[8], b[8];
    for (int p = 0; p < 4; p++) {
      a[p * 2] = _mm_unpacklo_epi8(planes[p * 2], planes[p * 2 + 1]);
      a[p * 2 + 1] = _mm_unpackhi_epi8(planes[p * 2], planes[p * 2 + 1]);
    }
    for (int p = 0; p < 2; p++) {
      b[p * 2] = _mm_unpacklo_epi16(a[p], a[p + 2]);
      b[p * 2 + 1] = _mm_unpackhi_epi16(a[p], a[p + 2]);
      b[p * 2 + 4] = _mm_unpacklo_epi16(a[p + 4], a[p + 6]);
      b[p * 2 + 5] = _mm_unpackhi_epi16(a[p + 4], a[p + 6]);
    }
    for (int p = 0; p < 4; p++) {
      _mm_storeu_si128((__m128i *)(output + p * 32), _mm_unpacklo_epi32(b[p], b[p + 4]));
      _mm_storeu_si128((__m128i *)(output + p * 32 + 16), _mm_unpackhi_epi32(b[p], b[p + 4]));
    }
  }
}

static inline __m128i prefix_sum16(__m128i value) {
  value = _mm_add_epi8(value, _mm_slli_si128(value, 1));
  value = _mm_add_epi8(value, _mm_slli_si128(value, 2));
  value = _mm_add_epi8(value, _mm_slli_si128(value, 4));
  return _mm_add_epi8(value, _mm_slli_si128(value, 8));
}

/* Compile-time plane counts keep transpose/delta stages together and allow
 * unrolling without a variable-count plane loop inside every SIMD tile. */
#define DEFINE_PAIR(N) \
static int forward##N(const uint8_t *input, uint8_t *output, int32_t length) { \
  int count = length / N, vector_count = count / 16 * 16; \
  __m128i previous[N], planes[N]; \
  for (int p = 0; p < N; p++) previous[p] = _mm_setzero_si128(); \
  for (int i = 0; i < vector_count; i += 16) { \
    load_planes16(input + i * N, planes, N); \
    for (int p = 0; p < N; p++) { \
      __m128i delta = _mm_sub_epi8(planes[p], _mm_alignr_epi8(planes[p], previous[p], 15)); \
      _mm_storeu_si128((__m128i *)(output + p * count + i), delta); \
      previous[p] = planes[p]; \
    } \
  } \
  for (int p = 0; p < N; p++) { \
    uint8_t carry = (uint8_t)(_mm_extract_epi16(previous[p], 7) >> 8); \
    for (int i = vector_count; i < count; i++) { \
      uint8_t value = input[i * N + p]; \
      output[p * count + i] = (uint8_t)(value - carry); \
      carry = value; \
    } \
  } \
  memcpy(output + count * N, input + count * N, (size_t)(length % N)); \
  return 1; \
} \
static int backward##N(const uint8_t *input, uint8_t *output, int32_t length) { \
  int count = length / N, vector_count = count / 16 * 16; \
  __m128i previous[N], planes[N]; \
  for (int p = 0; p < N; p++) previous[p] = _mm_setzero_si128(); \
  for (int i = 0; i < vector_count; i += 16) { \
    for (int p = 0; p < N; p++) { \
      __m128i delta = _mm_loadu_si128((const __m128i *)(input + p * count + i)); \
      planes[p] = _mm_add_epi8(prefix_sum16(delta), _mm_shuffle_epi8(previous[p], _mm_set1_epi8(15))); \
      previous[p] = planes[p]; \
    } \
    store_planes16(output + i * N, planes, N); \
  } \
  for (int p = 0; p < N; p++) { \
    uint8_t carry = (uint8_t)(_mm_extract_epi16(previous[p], 7) >> 8); \
    for (int i = vector_count; i < count; i++) { \
      carry = (uint8_t)(carry + input[p * count + i]); \
      output[i * N + p] = carry; \
    } \
  } \
  memcpy(output + count * N, input + count * N, (size_t)(length % N)); \
  return 1; \
}

DEFINE_PAIR(2)
DEFINE_PAIR(4)
DEFINE_PAIR(8)
#undef DEFINE_PAIR

int shuffle_bytedelta_ssse3_forward(const uint8_t *input, uint8_t *output, int32_t length, int typesize) {
  switch (typesize) {
    case 2: return forward2(input, output, length);
    case 4: return forward4(input, output, length);
    case 8: return forward8(input, output, length);
    default: return 0;
  }
}

int shuffle_bytedelta_ssse3_backward(const uint8_t *input, uint8_t *output, int32_t length, int typesize) {
  switch (typesize) {
    case 2: return backward2(input, output, length);
    case 4: return backward4(input, output, length);
    case 8: return backward8(input, output, length);
    default: return 0;
  }
}
