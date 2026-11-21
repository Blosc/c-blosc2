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

/* Record-parallel byte prefix sum helpers for backward SHUFFLE + BYTEDELTA.
 * Transposing into interleaved records first lets us run a tree prefix sum
 * across complete records in parallel, eliminating per-plane 32-byte Kogge-Stone
 * loops and reducing carry state from N registers (which spilled on typesize=8)
 * down to a single 128-bit register. */
static inline __m256i dup_hi64_256(__m256i v) {
  /* Broadcast the upper 64-bit record of each 128-bit lane across that lane. */
  return _mm256_unpackhi_epi64(v, v);
}

static inline __m256i dup_hi32_256(__m256i v) {
  /* Broadcast the top 32-bit record of each 128-bit lane across that lane. */
  return _mm256_shuffle_epi32(v, 0xff);
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

static int backward2_avx2(const uint8_t *input, uint8_t *output, int32_t length) {
  int count = length / 2, vector_count = count / 32 * 32;
  /* Shuffle mask broadcasting the 8th 16-bit record (bytes 14..15) in each 128-bit lane. */
  const __m256i mask_hi16 = _mm256_setr_epi8(
      14, 15, 14, 15, 14, 15, 14, 15, 14, 15, 14, 15, 14, 15, 14, 15,
      14, 15, 14, 15, 14, 15, 14, 15, 14, 15, 14, 15, 14, 15, 14, 15);
  const __m128i mask_hi16_128 = _mm256_castsi256_si128(mask_hi16);
  __m128i carry128 = _mm_setzero_si128();
  for (int i = 0; i < vector_count; i += 32) {
    /* Load 32 delta bytes from each of the 2 byte planes. */
    __m256i p0 = _mm256_loadu_si256((const __m256i *)(input + i));
    __m256i p1 = _mm256_loadu_si256((const __m256i *)(input + count + i));

    /* Interleave into 2-byte records: lo lanes hold 0..7 and 8..15, hi lanes 16..23 and 24..31. */
    __m256i r0 = _mm256_unpacklo_epi8(p0, p1);
    __m256i r1 = _mm256_unpackhi_epi8(p0, p1);

    /* Kogge-Stone prefix sum across the 8 interleaved 2-byte records in each 128-bit lane. */
    r0 = _mm256_add_epi8(r0, _mm256_slli_si256(r0, 2));
    r1 = _mm256_add_epi8(r1, _mm256_slli_si256(r1, 2));
    r0 = _mm256_add_epi8(r0, _mm256_slli_si256(r0, 4));
    r1 = _mm256_add_epi8(r1, _mm256_slli_si256(r1, 4));
    r0 = _mm256_add_epi8(r0, _mm256_slli_si256(r0, 8));
    r1 = _mm256_add_epi8(r1, _mm256_slli_si256(r1, 8));

    /* Propagate each 8-record half-lane total from r0 into r1. */
    r1 = _mm256_add_epi8(r1, _mm256_shuffle_epi8(r0, mask_hi16));

    /* Build per-lane tile carry [carry, carry + sum_lo] and advance running carry128. */
    __m128i sum_lo = _mm_shuffle_epi8(_mm256_castsi256_si128(r1), mask_hi16_128);
    __m128i sum_hi = _mm_shuffle_epi8(_mm256_extracti128_si256(r1, 1), mask_hi16_128);
    __m128i carry_hi = _mm_add_epi8(carry128, sum_lo);
    __m256i tile_carry = _mm256_inserti128_si256(_mm256_castsi128_si256(carry128), carry_hi, 1);
    carry128 = _mm_add_epi8(carry128, _mm_add_epi8(sum_lo, sum_hi));

    /* Apply incoming cross-tile and cross-lane carry to all 32 records. */
    r0 = _mm256_add_epi8(r0, tile_carry);
    r1 = _mm256_add_epi8(r1, tile_carry);

    /* Reorder lanes into two contiguous 32-byte stores (records 0..15, then 16..31). */
    uint8_t *dst = output + i * 2;
    _mm256_storeu_si256((__m256i *)dst, _mm256_permute2x128_si256(r0, r1, 0x20));
    _mm256_storeu_si256((__m256i *)(dst + 32), _mm256_permute2x128_si256(r0, r1, 0x31));
  }
  /* Scalar tail for remaining <32 records and any incomplete trailing bytes. */
  uint8_t tail_carry[16];
  _mm_storeu_si128((__m128i *)tail_carry, carry128);
  for (int p = 0; p < 2; p++) {
    uint8_t carry = tail_carry[p];
    for (int i = vector_count; i < count; i++) {
      carry = (uint8_t)(carry + input[p * count + i]);
      output[i * 2 + p] = carry;
    }
  }
  memcpy(output + count * 2, input + count * 2, (size_t)(length % 2));
  return 1;
}

static int backward4_avx2(const uint8_t *input, uint8_t *output, int32_t length) {
  int count = length / 4, vector_count = count / 32 * 32;
  __m128i carry128 = _mm_setzero_si128();
  for (int i = 0; i < vector_count; i += 32) {
    /* Load 32 delta bytes from each of the 4 byte planes. */
    __m256i p0 = _mm256_loadu_si256((const __m256i *)(input + 0 * count + i));
    __m256i p1 = _mm256_loadu_si256((const __m256i *)(input + 1 * count + i));
    __m256i p2 = _mm256_loadu_si256((const __m256i *)(input + 2 * count + i));
    __m256i p3 = _mm256_loadu_si256((const __m256i *)(input + 3 * count + i));

    /* Stage 1 & 2 in-lane transpose: interleave 4 planes into 4-byte records in r0..r3. */
    __m256i a0 = _mm256_unpacklo_epi8(p0, p1);
    __m256i a1 = _mm256_unpackhi_epi8(p0, p1);
    __m256i a2 = _mm256_unpacklo_epi8(p2, p3);
    __m256i a3 = _mm256_unpackhi_epi8(p2, p3);

    __m256i r0 = _mm256_unpacklo_epi16(a0, a2);
    __m256i r1 = _mm256_unpackhi_epi16(a0, a2);
    __m256i r2 = _mm256_unpacklo_epi16(a1, a3);
    __m256i r3 = _mm256_unpackhi_epi16(a1, a3);

    /* Prefix sum across the 4 interleaved 4-byte records within each 128-bit lane. */
    r0 = _mm256_add_epi8(r0, _mm256_slli_si256(r0, 4));
    r1 = _mm256_add_epi8(r1, _mm256_slli_si256(r1, 4));
    r2 = _mm256_add_epi8(r2, _mm256_slli_si256(r2, 4));
    r3 = _mm256_add_epi8(r3, _mm256_slli_si256(r3, 4));

    r0 = _mm256_add_epi8(r0, _mm256_slli_si256(r0, 8));
    r1 = _mm256_add_epi8(r1, _mm256_slli_si256(r1, 8));
    r2 = _mm256_add_epi8(r2, _mm256_slli_si256(r2, 8));
    r3 = _mm256_add_epi8(r3, _mm256_slli_si256(r3, 8));

    /* Tree-reduce quad totals across r0..r3 to get full 16-record local sums per lane. */
    r1 = _mm256_add_epi8(r1, dup_hi32_256(r0));
    r3 = _mm256_add_epi8(r3, dup_hi32_256(r2));
    __m256i d1 = dup_hi32_256(r1);
    __m256i r3_local = _mm256_add_epi8(r3, d1);

    /* Build per-lane tile carry [carry, carry + sum_lo] and advance running carry128. */
    __m128i sum_lo = _mm_shuffle_epi32(_mm256_castsi256_si128(r3_local), 0xff);
    __m128i sum_hi = _mm_shuffle_epi32(_mm256_extracti128_si256(r3_local, 1), 0xff);
    __m128i carry_hi = _mm_add_epi8(carry128, sum_lo);
    __m256i tile_carry = _mm256_inserti128_si256(_mm256_castsi128_si256(carry128), carry_hi, 1);
    carry128 = _mm_add_epi8(carry128, _mm_add_epi8(sum_lo, sum_hi));

    /* Fold tile_carry with d1 for r2 and apply carries to all 4 vectors. */
    __m256i d1c = _mm256_add_epi8(d1, tile_carry);
    r0 = _mm256_add_epi8(r0, tile_carry);
    r1 = _mm256_add_epi8(r1, tile_carry);
    r2 = _mm256_add_epi8(r2, d1c);
    r3 = _mm256_add_epi8(r3_local, tile_carry);

    /* Reorder lanes into four contiguous 32-byte stores (records 0..31). */
    uint8_t *dst = output + i * 4;
    _mm256_storeu_si256((__m256i *)(dst + 0), _mm256_permute2x128_si256(r0, r1, 0x20));
    _mm256_storeu_si256((__m256i *)(dst + 32), _mm256_permute2x128_si256(r2, r3, 0x20));
    _mm256_storeu_si256((__m256i *)(dst + 64), _mm256_permute2x128_si256(r0, r1, 0x31));
    _mm256_storeu_si256((__m256i *)(dst + 96), _mm256_permute2x128_si256(r2, r3, 0x31));
  }
  /* Scalar tail for remaining <32 records and any incomplete trailing bytes. */
  uint8_t tail_carry[16];
  _mm_storeu_si128((__m128i *)tail_carry, carry128);
  for (int p = 0; p < 4; p++) {
    uint8_t carry = tail_carry[p];
    for (int i = vector_count; i < count; i++) {
      carry = (uint8_t)(carry + input[p * count + i]);
      output[i * 4 + p] = carry;
    }
  }
  memcpy(output + count * 4, input + count * 4, (size_t)(length % 4));
  return 1;
}

static int backward8_avx2(const uint8_t *input, uint8_t *output, int32_t length) {
  int count = length / 8, vector_count = count / 32 * 32;
  __m128i carry128 = _mm_setzero_si128();
  for (int i = 0; i < vector_count; i += 32) {
    /* Load 32 delta bytes from each of the 8 byte planes. */
    __m256i p0 = _mm256_loadu_si256((const __m256i *)(input + 0 * count + i));
    __m256i p1 = _mm256_loadu_si256((const __m256i *)(input + 1 * count + i));
    __m256i p2 = _mm256_loadu_si256((const __m256i *)(input + 2 * count + i));
    __m256i p3 = _mm256_loadu_si256((const __m256i *)(input + 3 * count + i));
    __m256i p4 = _mm256_loadu_si256((const __m256i *)(input + 4 * count + i));
    __m256i p5 = _mm256_loadu_si256((const __m256i *)(input + 5 * count + i));
    __m256i p6 = _mm256_loadu_si256((const __m256i *)(input + 6 * count + i));
    __m256i p7 = _mm256_loadu_si256((const __m256i *)(input + 7 * count + i));

    /* Stage 1 (8-bit), Stage 2 (16-bit), Stage 3 (32-bit) in-lane transpose into 8-byte records. */
    __m256i a0 = _mm256_unpacklo_epi8(p0, p1);
    __m256i a1 = _mm256_unpackhi_epi8(p0, p1);
    __m256i a2 = _mm256_unpacklo_epi8(p2, p3);
    __m256i a3 = _mm256_unpackhi_epi8(p2, p3);
    __m256i a4 = _mm256_unpacklo_epi8(p4, p5);
    __m256i a5 = _mm256_unpackhi_epi8(p4, p5);
    __m256i a6 = _mm256_unpacklo_epi8(p6, p7);
    __m256i a7 = _mm256_unpackhi_epi8(p6, p7);

    __m256i b0 = _mm256_unpacklo_epi16(a0, a2);
    __m256i b1 = _mm256_unpackhi_epi16(a0, a2);
    __m256i b2 = _mm256_unpacklo_epi16(a1, a3);
    __m256i b3 = _mm256_unpackhi_epi16(a1, a3);
    __m256i b4 = _mm256_unpacklo_epi16(a4, a6);
    __m256i b5 = _mm256_unpackhi_epi16(a4, a6);
    __m256i b6 = _mm256_unpacklo_epi16(a5, a7);
    __m256i b7 = _mm256_unpackhi_epi16(a5, a7);

    __m256i r0 = _mm256_unpacklo_epi32(b0, b4);
    __m256i r1 = _mm256_unpackhi_epi32(b0, b4);
    __m256i r2 = _mm256_unpacklo_epi32(b1, b5);
    __m256i r3 = _mm256_unpackhi_epi32(b1, b5);
    __m256i r4 = _mm256_unpacklo_epi32(b2, b6);
    __m256i r5 = _mm256_unpackhi_epi32(b2, b6);
    __m256i r6 = _mm256_unpacklo_epi32(b3, b7);
    __m256i r7 = _mm256_unpackhi_epi32(b3, b7);

    /* Prefix sum depth 1: combine adjacent pairs of 8-byte records within each 128-bit lane. */
    r0 = _mm256_add_epi8(r0, _mm256_slli_si256(r0, 8));
    r1 = _mm256_add_epi8(r1, _mm256_slli_si256(r1, 8));
    r2 = _mm256_add_epi8(r2, _mm256_slli_si256(r2, 8));
    r3 = _mm256_add_epi8(r3, _mm256_slli_si256(r3, 8));
    r4 = _mm256_add_epi8(r4, _mm256_slli_si256(r4, 8));
    r5 = _mm256_add_epi8(r5, _mm256_slli_si256(r5, 8));
    r6 = _mm256_add_epi8(r6, _mm256_slli_si256(r6, 8));
    r7 = _mm256_add_epi8(r7, _mm256_slli_si256(r7, 8));

    /* Prefix sum depth 2: combine pairs into 4-record groups. */
    r1 = _mm256_add_epi8(r1, dup_hi64_256(r0));
    r3 = _mm256_add_epi8(r3, dup_hi64_256(r2));
    r5 = _mm256_add_epi8(r5, dup_hi64_256(r4));
    r7 = _mm256_add_epi8(r7, dup_hi64_256(r6));

    /* Prefix sum depth 3: combine 4-record groups into 8-record halves. */
    __m256i d1 = dup_hi64_256(r1);
    __m256i d5 = dup_hi64_256(r5);
    r2 = _mm256_add_epi8(r2, d1);
    r3 = _mm256_add_epi8(r3, d1);
    r6 = _mm256_add_epi8(r6, d5);
    r7 = _mm256_add_epi8(r7, d5);

    /* Prefix sum depth 4: compute 16-record local totals in r7_local before applying carries. */
    __m256i d3 = dup_hi64_256(r3);
    __m256i r7_local = _mm256_add_epi8(r7, d3);

    /* Build per-lane tile carry [carry, carry + sum_lo] and advance running carry128. */
    __m128i r7_lo = _mm256_castsi256_si128(r7_local);
    __m128i r7_hi = _mm256_extracti128_si256(r7_local, 1);
    __m128i sum_lo = _mm_unpackhi_epi64(r7_lo, r7_lo);
    __m128i sum_hi = _mm_unpackhi_epi64(r7_hi, r7_hi);
    __m128i carry_hi = _mm_add_epi8(carry128, sum_lo);
    __m256i tile_carry = _mm256_inserti128_si256(_mm256_castsi128_si256(carry128), carry_hi, 1);
    carry128 = _mm_add_epi8(carry128, _mm_add_epi8(sum_lo, sum_hi));

    /* Fold tile_carry with d3 for r4..r6 to save vector adds, then finalize r0..r7. */
    __m256i d3c = _mm256_add_epi8(d3, tile_carry);
    r0 = _mm256_add_epi8(r0, tile_carry);
    r1 = _mm256_add_epi8(r1, tile_carry);
    r2 = _mm256_add_epi8(r2, tile_carry);
    r3 = _mm256_add_epi8(r3, tile_carry);
    r4 = _mm256_add_epi8(r4, d3c);
    r5 = _mm256_add_epi8(r5, d3c);
    r6 = _mm256_add_epi8(r6, d3c);
    r7 = _mm256_add_epi8(r7_local, tile_carry);

    /* Split-store low 128-bit lanes (records 0..15) and high lanes (16..31) without vperm2i128. */
    uint8_t *dst = output + i * 8;
    _mm_storeu_si128((__m128i *)(dst + 0), _mm256_castsi256_si128(r0));
    _mm_storeu_si128((__m128i *)(dst + 16), _mm256_castsi256_si128(r1));
    _mm_storeu_si128((__m128i *)(dst + 32), _mm256_castsi256_si128(r2));
    _mm_storeu_si128((__m128i *)(dst + 48), _mm256_castsi256_si128(r3));
    _mm_storeu_si128((__m128i *)(dst + 64), _mm256_castsi256_si128(r4));
    _mm_storeu_si128((__m128i *)(dst + 80), _mm256_castsi256_si128(r5));
    _mm_storeu_si128((__m128i *)(dst + 96), _mm256_castsi256_si128(r6));
    _mm_storeu_si128((__m128i *)(dst + 112), _mm256_castsi256_si128(r7));
    _mm_storeu_si128((__m128i *)(dst + 128), _mm256_extracti128_si256(r0, 1));
    _mm_storeu_si128((__m128i *)(dst + 144), _mm256_extracti128_si256(r1, 1));
    _mm_storeu_si128((__m128i *)(dst + 160), _mm256_extracti128_si256(r2, 1));
    _mm_storeu_si128((__m128i *)(dst + 176), _mm256_extracti128_si256(r3, 1));
    _mm_storeu_si128((__m128i *)(dst + 192), _mm256_extracti128_si256(r4, 1));
    _mm_storeu_si128((__m128i *)(dst + 208), _mm256_extracti128_si256(r5, 1));
    _mm_storeu_si128((__m128i *)(dst + 224), _mm256_extracti128_si256(r6, 1));
    _mm_storeu_si128((__m128i *)(dst + 240), _mm256_extracti128_si256(r7, 1));
  }
  /* Scalar tail for remaining <32 records and any incomplete trailing bytes. */
  uint8_t tail_carry[16];
  _mm_storeu_si128((__m128i *)tail_carry, carry128);
  for (int p = 0; p < 8; p++) {
    uint8_t carry = tail_carry[p];
    for (int i = vector_count; i < count; i++) {
      carry = (uint8_t)(carry + input[p * count + i]);
      output[i * 8 + p] = carry;
    }
  }
  memcpy(output + count * 8, input + count * 8, (size_t)(length % 8));
  return 1;
}

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
