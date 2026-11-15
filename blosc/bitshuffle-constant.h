/*
 * Copyright (c) 2026 Blosc Development Team <blosc@blosc.org>
 * License: BSD 3-Clause (see LICENSE.txt)
 *
 * A verified constant prefix can be emitted without transposing its bits.
 * Returning a multiple of 128 keeps the caller's vector loop aligned to its
 * normal tile size. A full constant plane may include an eight-byte tail.
 */
#ifndef BLOSC_BITSHUFFLE_CONSTANT_H
#define BLOSC_BITSHUFFLE_CONSTANT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(__AVX2__)
#include <immintrin.h>
#elif defined(__SSE2__)
#include <emmintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#endif

static inline size_t bshuf_constant_prefix(const uint8_t* input, size_t nbyte) {
  /* Eight row fills do not amortize their setup on tiny byte planes. */
  if (nbyte < 1024) return 0;
  const uint8_t value = input[0];
#if defined(__AVX2__)
  const __m256i v = _mm256_set1_epi8((char)value);
  __m256i diff = _mm256_or_si256(
      _mm256_xor_si256(_mm256_loadu_si256((const __m256i*)input), v),
      _mm256_xor_si256(_mm256_loadu_si256((const __m256i*)(input + nbyte - 32)), v));
  if (!_mm256_testz_si256(diff, diff)) return 0;
#elif defined(__SSE2__)
  const __m128i v = _mm_set1_epi8((char)value);
  __m128i diff = _mm_or_si128(
      _mm_xor_si128(_mm_loadu_si128((const __m128i*)input), v),
      _mm_xor_si128(_mm_loadu_si128((const __m128i*)(input + nbyte - 16)), v));
  if (_mm_movemask_epi8(_mm_cmpeq_epi8(diff, _mm_setzero_si128())) != 0xffff) return 0;
#elif defined(__aarch64__) || defined(_M_ARM64)
  const uint8x16_t v = vdupq_n_u8(value);
  uint8x16_t diff = vorrq_u8(veorq_u8(vld1q_u8(input), v),
                            veorq_u8(vld1q_u8(input + nbyte - 16), v));
  if (vmaxvq_u8(diff) != 0) return 0;
#else
  return 0;
#endif
  size_t j = 0;
  /* Stop at the first nonconstant tile. Its predecessors can still be emitted
   * directly, so an unsuccessful scan never requires retransposing that prefix. */
  for (; j + 127 < nbyte; j += 128) {
#if defined(__AVX2__)
    __m256i a = _mm256_or_si256(_mm256_xor_si256(_mm256_loadu_si256((const __m256i*)(input+j)), v),
                               _mm256_xor_si256(_mm256_loadu_si256((const __m256i*)(input+j+32)), v));
    __m256i b = _mm256_or_si256(_mm256_xor_si256(_mm256_loadu_si256((const __m256i*)(input+j+64)), v),
                               _mm256_xor_si256(_mm256_loadu_si256((const __m256i*)(input+j+96)), v));
    diff = _mm256_or_si256(a, b);
    if (!_mm256_testz_si256(diff, diff)) return j;
#elif defined(__SSE2__)
    diff = _mm_setzero_si128();
    for (size_t k = 0; k < 128; k += 16) {
      diff = _mm_or_si128(diff, _mm_xor_si128(_mm_loadu_si128((const __m128i*)(input+j+k)), v));
    }
    if (_mm_movemask_epi8(_mm_cmpeq_epi8(diff, _mm_setzero_si128())) != 0xffff) return j;
#elif defined(__aarch64__) || defined(_M_ARM64)
    diff = vdupq_n_u8(0);
    for (size_t k = 0; k < 128; k += 16) {
      diff = vorrq_u8(diff, veorq_u8(vld1q_u8(input+j+k), v));
    }
    if (vmaxvq_u8(diff) != 0) return j;
#endif
  }
  for (size_t k = j; k < nbyte; ++k) {
    if (input[k] != value) return j;
  }
  return nbyte;
}

static inline size_t bshuf_emit_constant_prefix(const void* in, void* out, size_t nbyte) {
  const uint8_t* input = (const uint8_t*)in;
  size_t prefix = bshuf_constant_prefix(input, nbyte);
  if (prefix != 0) {
    for (size_t bit = 0; bit < 8; ++bit) {
      memset((uint8_t*)out + bit * (nbyte / 8), (input[0] >> bit) & 1 ? 0xff : 0, prefix / 8);
    }
  }
  return prefix;
}
#endif
