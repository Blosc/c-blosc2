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

/* Compile-time plane counts keep transpose/delta stages together and allow
 * unrolling without a variable-count plane loop inside every SIMD tile. */
#define DEFINE_FORWARD(N) \
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
}

DEFINE_FORWARD(2)
DEFINE_FORWARD(4)
DEFINE_FORWARD(8)
#undef DEFINE_FORWARD

/* Record-parallel byte prefix sum for backward SHUFFLE + BYTEDELTA. */
static inline __m128i dup_hi64_128(__m128i v) {
  /* Broadcast the upper 64-bit record across both halves of the vector. */
  return _mm_unpackhi_epi64(v, v);
}

static inline __m128i dup_hi32_128(__m128i v) {
  /* Broadcast the top 32-bit record across all four lanes. */
  return _mm_shuffle_epi32(v, 0xff);
}

static int backward2(const uint8_t *input, uint8_t *output, int32_t length) {
  int count = length / 2, vector_count = count / 16 * 16;
  /* Shuffle mask broadcasting the 8th 16-bit record (bytes 14..15). */
  const __m128i mask_hi16 = _mm_setr_epi8(
      14, 15, 14, 15, 14, 15, 14, 15, 14, 15, 14, 15, 14, 15, 14, 15);
  __m128i carry_vec = _mm_setzero_si128();
  for (int i = 0; i < vector_count; i += 16) {
    /* Load 16 delta bytes from each of the 2 byte planes and interleave into records 0..15. */
    __m128i p0 = _mm_loadu_si128((const __m128i *)(input + i));
    __m128i p1 = _mm_loadu_si128((const __m128i *)(input + count + i));

    __m128i r0 = _mm_unpacklo_epi8(p0, p1);
    __m128i r1 = _mm_unpackhi_epi8(p0, p1);

    /* Kogge-Stone prefix sum across the 8 interleaved 2-byte records in r0 and r1. */
    r0 = _mm_add_epi8(r0, _mm_slli_si128(r0, 2));
    r1 = _mm_add_epi8(r1, _mm_slli_si128(r1, 2));
    r0 = _mm_add_epi8(r0, _mm_slli_si128(r0, 4));
    r1 = _mm_add_epi8(r1, _mm_slli_si128(r1, 4));
    r0 = _mm_add_epi8(r0, _mm_slli_si128(r0, 8));
    r1 = _mm_add_epi8(r1, _mm_slli_si128(r1, 8));

    /* Propagate r0's 8-record total into r1 and extract the 16-record tile sum. */
    r1 = _mm_add_epi8(r1, _mm_shuffle_epi8(r0, mask_hi16));
    __m128i tile_sum = _mm_shuffle_epi8(r1, mask_hi16);

    /* Add incoming carry, advance carry_vec, and store 16 decoded 2-byte records. */
    r0 = _mm_add_epi8(r0, carry_vec);
    r1 = _mm_add_epi8(r1, carry_vec);
    carry_vec = _mm_add_epi8(carry_vec, tile_sum);

    uint8_t *dst = output + i * 2;
    _mm_storeu_si128((__m128i *)(dst + 0), r0);
    _mm_storeu_si128((__m128i *)(dst + 16), r1);
  }
  /* Scalar tail for remaining <16 records and any incomplete trailing bytes. */
  uint8_t tail_carry[16];
  _mm_storeu_si128((__m128i *)tail_carry, carry_vec);
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

static int backward4(const uint8_t *input, uint8_t *output, int32_t length) {
  int count = length / 4, vector_count = count / 16 * 16;
  __m128i carry_vec = _mm_setzero_si128();
  for (int i = 0; i < vector_count; i += 16) {
    /* Load 16 delta bytes from each of the 4 byte planes. */
    __m128i p0 = _mm_loadu_si128((const __m128i *)(input + 0 * count + i));
    __m128i p1 = _mm_loadu_si128((const __m128i *)(input + 1 * count + i));
    __m128i p2 = _mm_loadu_si128((const __m128i *)(input + 2 * count + i));
    __m128i p3 = _mm_loadu_si128((const __m128i *)(input + 3 * count + i));

    /* Two-stage transpose interleaving 4 byte planes into 4-byte records in r0..r3. */
    __m128i a0 = _mm_unpacklo_epi8(p0, p1);
    __m128i a1 = _mm_unpackhi_epi8(p0, p1);
    __m128i a2 = _mm_unpacklo_epi8(p2, p3);
    __m128i a3 = _mm_unpackhi_epi8(p2, p3);

    __m128i r0 = _mm_unpacklo_epi16(a0, a2);
    __m128i r1 = _mm_unpackhi_epi16(a0, a2);
    __m128i r2 = _mm_unpacklo_epi16(a1, a3);
    __m128i r3 = _mm_unpackhi_epi16(a1, a3);

    /* Prefix sum across the 4 interleaved 4-byte records inside each vector. */
    r0 = _mm_add_epi8(r0, _mm_slli_si128(r0, 4));
    r1 = _mm_add_epi8(r1, _mm_slli_si128(r1, 4));
    r2 = _mm_add_epi8(r2, _mm_slli_si128(r2, 4));
    r3 = _mm_add_epi8(r3, _mm_slli_si128(r3, 4));

    r0 = _mm_add_epi8(r0, _mm_slli_si128(r0, 8));
    r1 = _mm_add_epi8(r1, _mm_slli_si128(r1, 8));
    r2 = _mm_add_epi8(r2, _mm_slli_si128(r2, 8));
    r3 = _mm_add_epi8(r3, _mm_slli_si128(r3, 8));

    /* Tree-reduce quad totals across r0..r3 and capture the 16-record tile sum. */
    r1 = _mm_add_epi8(r1, dup_hi32_128(r0));
    r3 = _mm_add_epi8(r3, dup_hi32_128(r2));
    __m128i d1 = dup_hi32_128(r1);
    __m128i r3_local = _mm_add_epi8(r3, d1);
    __m128i tile_sum = dup_hi32_128(r3_local);

    /* Fold carry_vec with d1 for r2, finalize r0..r3, and advance carry_vec. */
    __m128i d1c = _mm_add_epi8(d1, carry_vec);
    r0 = _mm_add_epi8(r0, carry_vec);
    r1 = _mm_add_epi8(r1, carry_vec);
    r2 = _mm_add_epi8(r2, d1c);
    r3 = _mm_add_epi8(r3_local, carry_vec);
    carry_vec = _mm_add_epi8(carry_vec, tile_sum);

    /* Store 16 contiguous decoded 4-byte records. */
    uint8_t *dst = output + i * 4;
    _mm_storeu_si128((__m128i *)(dst + 0), r0);
    _mm_storeu_si128((__m128i *)(dst + 16), r1);
    _mm_storeu_si128((__m128i *)(dst + 32), r2);
    _mm_storeu_si128((__m128i *)(dst + 48), r3);
  }
  /* Scalar tail for remaining <16 records and any incomplete trailing bytes. */
  uint8_t tail_carry[16];
  _mm_storeu_si128((__m128i *)tail_carry, carry_vec);
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

static int backward8(const uint8_t *input, uint8_t *output, int32_t length) {
  int count = length / 8, vector_count = count / 16 * 16;
  __m128i carry_vec = _mm_setzero_si128();
  for (int i = 0; i < vector_count; i += 16) {
    /* Load 16 delta bytes from each of the 8 byte planes. */
    __m128i p0 = _mm_loadu_si128((const __m128i *)(input + 0 * count + i));
    __m128i p1 = _mm_loadu_si128((const __m128i *)(input + 1 * count + i));
    __m128i p2 = _mm_loadu_si128((const __m128i *)(input + 2 * count + i));
    __m128i p3 = _mm_loadu_si128((const __m128i *)(input + 3 * count + i));
    __m128i p4 = _mm_loadu_si128((const __m128i *)(input + 4 * count + i));
    __m128i p5 = _mm_loadu_si128((const __m128i *)(input + 5 * count + i));
    __m128i p6 = _mm_loadu_si128((const __m128i *)(input + 6 * count + i));
    __m128i p7 = _mm_loadu_si128((const __m128i *)(input + 7 * count + i));

    /* Three-stage transpose interleaving 8 byte planes into 8-byte records in r0..r7. */
    __m128i a0 = _mm_unpacklo_epi8(p0, p1);
    __m128i a1 = _mm_unpackhi_epi8(p0, p1);
    __m128i a2 = _mm_unpacklo_epi8(p2, p3);
    __m128i a3 = _mm_unpackhi_epi8(p2, p3);
    __m128i a4 = _mm_unpacklo_epi8(p4, p5);
    __m128i a5 = _mm_unpackhi_epi8(p4, p5);
    __m128i a6 = _mm_unpacklo_epi8(p6, p7);
    __m128i a7 = _mm_unpackhi_epi8(p6, p7);

    __m128i b0 = _mm_unpacklo_epi16(a0, a2);
    __m128i b1 = _mm_unpackhi_epi16(a0, a2);
    __m128i b2 = _mm_unpacklo_epi16(a1, a3);
    __m128i b3 = _mm_unpackhi_epi16(a1, a3);
    __m128i b4 = _mm_unpacklo_epi16(a4, a6);
    __m128i b5 = _mm_unpackhi_epi16(a4, a6);
    __m128i b6 = _mm_unpacklo_epi16(a5, a7);
    __m128i b7 = _mm_unpackhi_epi16(a5, a7);

    __m128i r0 = _mm_unpacklo_epi32(b0, b4);
    __m128i r1 = _mm_unpackhi_epi32(b0, b4);
    __m128i r2 = _mm_unpacklo_epi32(b1, b5);
    __m128i r3 = _mm_unpackhi_epi32(b1, b5);
    __m128i r4 = _mm_unpacklo_epi32(b2, b6);
    __m128i r5 = _mm_unpackhi_epi32(b2, b6);
    __m128i r6 = _mm_unpacklo_epi32(b3, b7);
    __m128i r7 = _mm_unpackhi_epi32(b3, b7);

    /* Prefix sum depth 1: combine adjacent pairs of 8-byte records within each vector. */
    r0 = _mm_add_epi8(r0, _mm_slli_si128(r0, 8));
    r1 = _mm_add_epi8(r1, _mm_slli_si128(r1, 8));
    r2 = _mm_add_epi8(r2, _mm_slli_si128(r2, 8));
    r3 = _mm_add_epi8(r3, _mm_slli_si128(r3, 8));
    r4 = _mm_add_epi8(r4, _mm_slli_si128(r4, 8));
    r5 = _mm_add_epi8(r5, _mm_slli_si128(r5, 8));
    r6 = _mm_add_epi8(r6, _mm_slli_si128(r6, 8));
    r7 = _mm_add_epi8(r7, _mm_slli_si128(r7, 8));

    /* Prefix sum depth 2: combine pairs into 4-record groups. */
    r1 = _mm_add_epi8(r1, dup_hi64_128(r0));
    r3 = _mm_add_epi8(r3, dup_hi64_128(r2));
    r5 = _mm_add_epi8(r5, dup_hi64_128(r4));
    r7 = _mm_add_epi8(r7, dup_hi64_128(r6));

    /* Prefix sum depth 3: combine 4-record groups into 8-record halves. */
    __m128i d1 = dup_hi64_128(r1);
    __m128i d5 = dup_hi64_128(r5);
    r2 = _mm_add_epi8(r2, d1);
    r3 = _mm_add_epi8(r3, d1);
    r6 = _mm_add_epi8(r6, d5);
    r7 = _mm_add_epi8(r7, d5);

    /* Prefix sum depth 4: compute 16-record tile sum from r7_local. */
    __m128i d3 = dup_hi64_128(r3);
    __m128i r7_local = _mm_add_epi8(r7, d3);
    __m128i tile_sum = dup_hi64_128(r7_local);

    /* Fold carry_vec with d3 for r4..r6, finalize r0..r7, and advance carry_vec. */
    __m128i d3c = _mm_add_epi8(d3, carry_vec);
    r0 = _mm_add_epi8(r0, carry_vec);
    r1 = _mm_add_epi8(r1, carry_vec);
    r2 = _mm_add_epi8(r2, carry_vec);
    r3 = _mm_add_epi8(r3, carry_vec);
    r4 = _mm_add_epi8(r4, d3c);
    r5 = _mm_add_epi8(r5, d3c);
    r6 = _mm_add_epi8(r6, d3c);
    r7 = _mm_add_epi8(r7_local, carry_vec);
    carry_vec = _mm_add_epi8(carry_vec, tile_sum);

    /* Store 16 contiguous decoded 8-byte records. */
    uint8_t *dst = output + i * 8;
    _mm_storeu_si128((__m128i *)(dst + 0), r0);
    _mm_storeu_si128((__m128i *)(dst + 16), r1);
    _mm_storeu_si128((__m128i *)(dst + 32), r2);
    _mm_storeu_si128((__m128i *)(dst + 48), r3);
    _mm_storeu_si128((__m128i *)(dst + 64), r4);
    _mm_storeu_si128((__m128i *)(dst + 80), r5);
    _mm_storeu_si128((__m128i *)(dst + 96), r6);
    _mm_storeu_si128((__m128i *)(dst + 112), r7);
  }
  /* Scalar tail for remaining <16 records and any incomplete trailing bytes. */
  uint8_t tail_carry[16];
  _mm_storeu_si128((__m128i *)tail_carry, carry_vec);
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
