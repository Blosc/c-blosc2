/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library
  Copyright (c) 2026 Blosc Development Team <blosc@blosc.org>
  License: BSD 3-Clause (see LICENSE.txt)
**********************************************************************/

/* This translation unit alone is compiled for AVX2. The baseline shuffle
 * dispatcher must check CPUID/OSXSAVE before calling either entry point. */
#include "../plugins/filters/bytedelta/bytedelta.h"

#if defined(__AVX2__)

#include <immintrin.h>
#include <string.h>

/* Split-store low 128-bit lane to `lo` and high 128-bit lane to `hi` */
static inline void storeu2_128(void *hi, void *lo, __m256i v) {
  _mm_storeu_si128((__m128i *)lo, _mm256_castsi256_si128(v));
  _mm_storeu_si128((__m128i *)hi, _mm256_extracti128_si256(v, 1));
}

/* Forward SHUFFLE + BYTEDELTA:
 * Per-byte delta commutes with byte-plane transposition because subtraction is
 * modulo 256 per byte lane:
 *   delta(shuffle(e))[p, i] = e[i].b[p] - e[i-1].b[p] = shuffle(e -_byte prev(e))[p, i].
 * Computing the byte difference before transposing uses a single unaligned load
 * at (src - N) and a port-0/1 vpsubb, eliminating all cross-tile carry state,
 * vpalignr/vinserti128 shuffles, and register spills. */

/* Transpose 32 two-byte elements (64B) into 2 contiguous 32-byte planes and store */
static inline void transpose_store_2(uint8_t *output, int count, int i, __m256i y0, __m256i y1) {
  const __m256i mask = _mm256_set1_epi16(0x00ff);
  __m256i p0 = _mm256_permute4x64_epi64(
      _mm256_packus_epi16(_mm256_and_si256(y0, mask), _mm256_and_si256(y1, mask)), 0xd8);
  __m256i p1 = _mm256_permute4x64_epi64(
      _mm256_packus_epi16(_mm256_srli_epi16(y0, 8), _mm256_srli_epi16(y1, 8)), 0xd8);
  _mm256_storeu_si256((__m256i *)(output + i), p0);
  _mm256_storeu_si256((__m256i *)(output + count + i), p1);
}

/* Transpose 32 four-byte elements (128B) into 4 contiguous 32-byte planes and store */
static inline void transpose_store_4(uint8_t *output, int count, int i, const __m256i d[4]) {
  const __m256i mask = _mm256_setr_epi8(
      0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15,
      0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15);
  __m256i a[4], b[4];
  /* Place elements 0..15 in lane 0 and 16..31 in lane 1 so the transpose is purely in-lane. */
  a[0] = _mm256_shuffle_epi8(_mm256_permute2x128_si256(d[0], d[2], 0x20), mask);
  a[1] = _mm256_shuffle_epi8(_mm256_permute2x128_si256(d[0], d[2], 0x31), mask);
  a[2] = _mm256_shuffle_epi8(_mm256_permute2x128_si256(d[1], d[3], 0x20), mask);
  a[3] = _mm256_shuffle_epi8(_mm256_permute2x128_si256(d[1], d[3], 0x31), mask);
  b[0] = _mm256_unpacklo_epi32(a[0], a[1]);
  b[1] = _mm256_unpackhi_epi32(a[0], a[1]);
  b[2] = _mm256_unpacklo_epi32(a[2], a[3]);
  b[3] = _mm256_unpackhi_epi32(a[2], a[3]);
  _mm256_storeu_si256((__m256i *)(output + 0 * count + i), _mm256_unpacklo_epi64(b[0], b[2]));
  _mm256_storeu_si256((__m256i *)(output + 1 * count + i), _mm256_unpackhi_epi64(b[0], b[2]));
  _mm256_storeu_si256((__m256i *)(output + 2 * count + i), _mm256_unpacklo_epi64(b[1], b[3]));
  _mm256_storeu_si256((__m256i *)(output + 3 * count + i), _mm256_unpackhi_epi64(b[1], b[3]));
}

/* Transpose 32 eight-byte elements (256B) into 8 contiguous 32-byte planes and store */
static inline void transpose_store_8(uint8_t *output, int count, int i, const __m256i d[8]) {
  const __m256i mask = _mm256_setr_epi8(
      0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15,
      0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15);
  __m256i a[8], b[8];
  /* Pair d[p] and d[p+4] so elements 0..15 land in lane 0 and 16..31 in lane 1 */
  for (int p = 0; p < 4; p++) {
    b[p * 2] = _mm256_shuffle_epi8(_mm256_permute2x128_si256(d[p], d[p + 4], 0x20), mask);
    b[p * 2 + 1] = _mm256_shuffle_epi8(_mm256_permute2x128_si256(d[p], d[p + 4], 0x31), mask);
  }
  /* In-lane 16-bit, 32-bit, and 64-bit unpack stages complete the 8x16 transpose */
  for (int p = 0; p < 4; p++) {
    a[p * 2] = _mm256_unpacklo_epi16(b[p * 2], b[p * 2 + 1]);
    a[p * 2 + 1] = _mm256_unpackhi_epi16(b[p * 2], b[p * 2 + 1]);
  }
  for (int p = 0; p < 4; p++) {
    int k = p < 2 ? p : p + 2;
    b[p * 2] = _mm256_unpacklo_epi32(a[k], a[k + 2]);
    b[p * 2 + 1] = _mm256_unpackhi_epi32(a[k], a[k + 2]);
  }
  for (int p = 0; p < 4; p++) {
    _mm256_storeu_si256((__m256i *)(output + (p * 2) * count + i),
                        _mm256_unpacklo_epi64(b[p], b[p + 4]));
    _mm256_storeu_si256((__m256i *)(output + (p * 2 + 1) * count + i),
                        _mm256_unpackhi_epi64(b[p], b[p + 4]));
  }
}

/* Inverse transpose: interleave N 32-byte planes back into 32 contiguous records */
static inline void store_planes32(uint8_t *output, const __m256i *planes, int typesize) {
  if (typesize == 2) {
    __m256i lo = _mm256_unpacklo_epi8(planes[0], planes[1]);
    __m256i hi = _mm256_unpackhi_epi8(planes[0], planes[1]);
    _mm256_storeu_si256((__m256i *)output, _mm256_permute2x128_si256(lo, hi, 0x20));
    _mm256_storeu_si256((__m256i *)(output + 32), _mm256_permute2x128_si256(lo, hi, 0x31));
  }
  else if (typesize == 4) {
    __m256i a[4];
    a[0] = _mm256_unpacklo_epi8(planes[0], planes[1]);
    a[1] = _mm256_unpackhi_epi8(planes[0], planes[1]);
    a[2] = _mm256_unpacklo_epi8(planes[2], planes[3]);
    a[3] = _mm256_unpackhi_epi8(planes[2], planes[3]);
    storeu2_128(output + 64, output, _mm256_unpacklo_epi16(a[0], a[2]));
    storeu2_128(output + 80, output + 16, _mm256_unpackhi_epi16(a[0], a[2]));
    storeu2_128(output + 96, output + 32, _mm256_unpacklo_epi16(a[1], a[3]));
    storeu2_128(output + 112, output + 48, _mm256_unpackhi_epi16(a[1], a[3]));
  }
  else {
    __m256i a[8], b[8];
    for (int p = 0; p < 4; p++) {
      a[p * 2] = _mm256_unpacklo_epi8(planes[p * 2], planes[p * 2 + 1]);
      a[p * 2 + 1] = _mm256_unpackhi_epi8(planes[p * 2], planes[p * 2 + 1]);
    }
    for (int p = 0; p < 2; p++) {
      b[p * 2] = _mm256_unpacklo_epi16(a[p], a[p + 2]);
      b[p * 2 + 1] = _mm256_unpackhi_epi16(a[p], a[p + 2]);
      b[p * 2 + 4] = _mm256_unpacklo_epi16(a[p + 4], a[p + 6]);
      b[p * 2 + 5] = _mm256_unpackhi_epi16(a[p + 4], a[p + 6]);
    }
    for (int p = 0; p < 4; p++) {
      storeu2_128(output + 128 + p * 32, output + p * 32,
                  _mm256_unpacklo_epi32(b[p], b[p + 4]));
      storeu2_128(output + 128 + p * 32 + 16, output + p * 32 + 16,
                  _mm256_unpackhi_epi32(b[p], b[p + 4]));
    }
  }
}

/* Kogge-Stone inclusive byte prefix sum independently within each 128-bit lane */
static inline __m256i prefix_sum_inlane32(__m256i value) {
  value = _mm256_add_epi8(value, _mm256_slli_si256(value, 1));
  value = _mm256_add_epi8(value, _mm256_slli_si256(value, 2));
  value = _mm256_add_epi8(value, _mm256_slli_si256(value, 4));
  return _mm256_add_epi8(value, _mm256_slli_si256(value, 8));
}

static int forward2_avx2(const uint8_t *input, uint8_t *output, int32_t length) {
  int count = length / 2, vector_count = count / 32 * 32;
  if (vector_count >= 32) {
    /* First tile: zero-pad the missing predecessor of element 0 without reading before input. */
    __m256i y0 = _mm256_loadu_si256((const __m256i *)input);
    __m128i lo_prev = _mm_slli_si128(_mm256_castsi256_si128(y0), 2);
    __m128i hi_prev = _mm_loadu_si128((const __m128i *)(input + 14));
    __m256i d0 = _mm256_sub_epi8(
        y0, _mm256_inserti128_si256(_mm256_castsi128_si256(lo_prev), hi_prev, 1));
    __m256i d1 = _mm256_sub_epi8(
        _mm256_loadu_si256((const __m256i *)(input + 32)),
        _mm256_loadu_si256((const __m256i *)(input + 30)));
    transpose_store_2(output, count, 0, d0, d1);
    for (int i = 32; i < vector_count; i += 32) {
      const uint8_t *src = input + i * 2;
      d0 = _mm256_sub_epi8(_mm256_loadu_si256((const __m256i *)src),
                           _mm256_loadu_si256((const __m256i *)(src - 2)));
      d1 = _mm256_sub_epi8(_mm256_loadu_si256((const __m256i *)(src + 32)),
                           _mm256_loadu_si256((const __m256i *)(src + 30)));
      transpose_store_2(output, count, i, d0, d1);
    }
  }
  for (int p = 0; p < 2; p++) {
    uint8_t carry = vector_count > 0 ? input[(vector_count - 1) * 2 + p] : 0;
    for (int i = vector_count; i < count; i++) {
      uint8_t value = input[i * 2 + p];
      output[p * count + i] = (uint8_t)(value - carry);
      carry = value;
    }
  }
  memcpy(output + count * 2, input + count * 2, (size_t)(length % 2));
  return 1;
}

static int forward4_avx2(const uint8_t *input, uint8_t *output, int32_t length) {
  int count = length / 4, vector_count = count / 32 * 32;
  if (vector_count >= 32) {
    __m256i d[4];
    __m256i y0 = _mm256_loadu_si256((const __m256i *)input);
    __m128i lo_prev = _mm_slli_si128(_mm256_castsi256_si128(y0), 4);
    __m128i hi_prev = _mm_loadu_si128((const __m128i *)(input + 12));
    d[0] = _mm256_sub_epi8(
        y0, _mm256_inserti128_si256(_mm256_castsi128_si256(lo_prev), hi_prev, 1));
    for (int k = 1; k < 4; k++) {
      d[k] = _mm256_sub_epi8(_mm256_loadu_si256((const __m256i *)(input + k * 32)),
                             _mm256_loadu_si256((const __m256i *)(input + k * 32 - 4)));
    }
    transpose_store_4(output, count, 0, d);
    for (int i = 32; i < vector_count; i += 32) {
      const uint8_t *src = input + i * 4;
      for (int k = 0; k < 4; k++) {
        d[k] = _mm256_sub_epi8(_mm256_loadu_si256((const __m256i *)(src + k * 32)),
                               _mm256_loadu_si256((const __m256i *)(src + k * 32 - 4)));
      }
      transpose_store_4(output, count, i, d);
    }
  }
  for (int p = 0; p < 4; p++) {
    uint8_t carry = vector_count > 0 ? input[(vector_count - 1) * 4 + p] : 0;
    for (int i = vector_count; i < count; i++) {
      uint8_t value = input[i * 4 + p];
      output[p * count + i] = (uint8_t)(value - carry);
      carry = value;
    }
  }
  memcpy(output + count * 4, input + count * 4, (size_t)(length % 4));
  return 1;
}

static int forward8_avx2(const uint8_t *input, uint8_t *output, int32_t length) {
  int count = length / 8, vector_count = count / 32 * 32;
  if (vector_count >= 32) {
    __m256i d[8];
    __m256i y0 = _mm256_loadu_si256((const __m256i *)input);
    __m128i lo_prev = _mm_slli_si128(_mm256_castsi256_si128(y0), 8);
    __m128i hi_prev = _mm_loadu_si128((const __m128i *)(input + 8));
    d[0] = _mm256_sub_epi8(
        y0, _mm256_inserti128_si256(_mm256_castsi128_si256(lo_prev), hi_prev, 1));
    for (int k = 1; k < 8; k++) {
      d[k] = _mm256_sub_epi8(_mm256_loadu_si256((const __m256i *)(input + k * 32)),
                             _mm256_loadu_si256((const __m256i *)(input + k * 32 - 8)));
    }
    transpose_store_8(output, count, 0, d);
    for (int i = 32; i < vector_count; i += 32) {
      const uint8_t *src = input + i * 8;
      for (int k = 0; k < 8; k++) {
        d[k] = _mm256_sub_epi8(_mm256_loadu_si256((const __m256i *)(src + k * 32)),
                               _mm256_loadu_si256((const __m256i *)(src + k * 32 - 8)));
      }
      transpose_store_8(output, count, i, d);
    }
  }
  for (int p = 0; p < 8; p++) {
    uint8_t carry = vector_count > 0 ? input[(vector_count - 1) * 8 + p] : 0;
    for (int i = vector_count; i < count; i++) {
      uint8_t value = input[i * 8 + p];
      output[p * count + i] = (uint8_t)(value - carry);
      carry = value;
    }
  }
  memcpy(output + count * 8, input + count * 8, (size_t)(length % 8));
  return 1;
}

/* Decouple the cross-tile carry chain from the in-lane prefix sum:
 * ps_full and tile_sum depend only on the current tile, so carry[p] advances
 * via a single 1-cycle vpaddb per tile instead of waiting on vextracti128 +
 * vpshufb + vbroadcasti128. */
#define DEFINE_BACKWARD_AVX2(N) \
static int backward##N##_avx2(const uint8_t *input, uint8_t *output, int32_t length) { \
  int count = length / N, vector_count = count / 32 * 32; \
  const __m256i mask15 = _mm256_set1_epi8(15); \
  const __m128i mask15_128 = _mm_set1_epi8(15); \
  __m128i carry128[N]; \
  __m256i planes[N]; \
  for (int p = 0; p < N; p++) carry128[p] = _mm_setzero_si128(); \
  for (int i = 0; i < vector_count; i += 32) { \
    for (int p = 0; p < N; p++) { \
      __m256i delta = _mm256_loadu_si256((const __m256i *)(input + p * count + i)); \
      __m256i ps = prefix_sum_inlane32(delta); \
      __m256i lo_carry = _mm256_shuffle_epi8( \
          _mm256_permute2x128_si256(ps, ps, 0x08), mask15); \
      __m256i ps_full = _mm256_add_epi8(ps, lo_carry); \
      __m128i tile_sum = _mm_shuffle_epi8(_mm256_extracti128_si256(ps_full, 1), mask15_128); \
      planes[p] = _mm256_add_epi8(ps_full, _mm256_broadcastsi128_si256(carry128[p])); \
      carry128[p] = _mm_add_epi8(carry128[p], tile_sum); \
    } \
    store_planes32(output + i * N, planes, N); \
  } \
  for (int p = 0; p < N; p++) { \
    uint8_t carry = (uint8_t)(_mm_cvtsi128_si32(carry128[p]) & 0xff); \
    for (int i = vector_count; i < count; i++) { \
      carry = (uint8_t)(carry + input[p * count + i]); \
      output[i * N + p] = carry; \
    } \
  } \
  memcpy(output + count * N, input + count * N, (size_t)(length % N)); \
  return 1; \
}

DEFINE_BACKWARD_AVX2(2)
DEFINE_BACKWARD_AVX2(4)
DEFINE_BACKWARD_AVX2(8)
#undef DEFINE_BACKWARD_AVX2

int shuffle_bytedelta_avx2_forward(const uint8_t *input, uint8_t *output, int32_t length, int typesize) {
  switch (typesize) {
    case 2: return forward2_avx2(input, output, length);
    case 4: return forward4_avx2(input, output, length);
    case 8: return forward8_avx2(input, output, length);
    default: return 0;
  }
}

int shuffle_bytedelta_avx2_backward(const uint8_t *input, uint8_t *output, int32_t length, int typesize) {
  switch (typesize) {
    case 2: return backward2_avx2(input, output, length);
    case 4: return backward4_avx2(input, output, length);
    case 8: return backward8_avx2(input, output, length);
    default: return 0;
  }
}

#else

int shuffle_bytedelta_avx2_forward(const uint8_t *input, uint8_t *output, int32_t length, int typesize) {
  BLOSC_UNUSED_PARAM(input);
  BLOSC_UNUSED_PARAM(output);
  BLOSC_UNUSED_PARAM(length);
  BLOSC_UNUSED_PARAM(typesize);
  return 0;
}

int shuffle_bytedelta_avx2_backward(const uint8_t *input, uint8_t *output, int32_t length, int typesize) {
  BLOSC_UNUSED_PARAM(input);
  BLOSC_UNUSED_PARAM(output);
  BLOSC_UNUSED_PARAM(length);
  BLOSC_UNUSED_PARAM(typesize);
  return 0;
}

#endif /* defined(__AVX2__) */
