/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

// ByteDelta filter.  This is based on work by Aras Pranckevičius:
// https://aras-p.info/blog/2023/03/01/Float-Compression-7-More-Filtering-Optimization/
// This requires Intel SSE4.1 and ARM64 NEON, which should be widely available by now.

#include <blosc2.h>
#include "bytedelta.h"
#include <stdio.h>
#include "../plugins/plugin_utils.h"
#include "../include/blosc2/filters-registry.h"

#if defined(__x86_64__) || defined(_M_X64)
#	define CPU_ARCH_X64 1
#	include <emmintrin.h>
#	include <tmmintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#	define CPU_ARCH_ARM64 1
#	include <arm_neon.h>
#else
#   error Unsupported platform (SSE4.1/NEON required)
#endif

#if CPU_ARCH_X64
typedef __m128i Bytes16;
Bytes16 SimdZero() { return _mm_setzero_si128(); }
Bytes16 SimdSet1(uint8_t v) { return _mm_set1_epi8(v); }
Bytes16 SimdLoad(const void* ptr) { return _mm_loadu_si128((const __m128i*)ptr); }
void SimdStore(void* ptr, Bytes16 x) { _mm_storeu_si128((__m128i*)ptr, x); }

Bytes16 SimdConcat(Bytes16 hi, Bytes16 lo) { return _mm_alignr_epi8(hi, lo, 15); }
Bytes16 SimdAdd(Bytes16 a, Bytes16 b) { return _mm_add_epi8(a, b); }
Bytes16 SimdSub(Bytes16 a, Bytes16 b) { return _mm_sub_epi8(a, b); }
Bytes16 SimdShuffle(Bytes16 x, Bytes16 table) { return _mm_shuffle_epi8(x, table); }

Bytes16 SimdPrefixSum(Bytes16 x)
{
  // Sklansky-style sum from https://gist.github.com/rygorous/4212be0cd009584e4184e641ca210528
  x = _mm_add_epi8(x, _mm_slli_epi64(x, 8));
  x = _mm_add_epi8(x, _mm_slli_epi64(x, 16));
  x = _mm_add_epi8(x, _mm_slli_epi64(x, 32));
  x = _mm_add_epi8(x, _mm_shuffle_epi8(x, _mm_setr_epi8(-1,-1,-1,-1,-1,-1,-1,-1,7,7,7,7,7,7,7,7)));
  return x;
}

#elif CPU_ARCH_ARM64
typedef uint8x16_t Bytes16;
Bytes16 SimdZero() { return vdupq_n_u8(0); }
Bytes16 SimdSet1(uint8_t v) { return vdupq_n_u8(v); }
Bytes16 SimdLoad(const void* ptr) { return vld1q_u8((const uint8_t*)ptr); }
void SimdStore(void* ptr, Bytes16 x) { vst1q_u8((uint8_t*)ptr, x); }

Bytes16 SimdConcat(Bytes16 hi, Bytes16 lo) { return vextq_u8(lo, hi, 15); }
Bytes16 SimdAdd(Bytes16 a, Bytes16 b) { return vaddq_u8(a, b); }
Bytes16 SimdSub(Bytes16 a, Bytes16 b) { return vsubq_u8(a, b); }
Bytes16 SimdShuffle(Bytes16 x, Bytes16 table) { return vqtbl1q_u8(x, table); }

Bytes16 SimdPrefixSum(Bytes16 x)
{
  // Kogge-Stone-style like commented out part of https://gist.github.com/rygorous/4212be0cd009584e4184e641ca210528
  Bytes16 zero = vdupq_n_u8(0);
  x = vaddq_u8(x, vextq_u8(zero, x, 16 - 1));
  x = vaddq_u8(x, vextq_u8(zero, x, 16 - 2));
  x = vaddq_u8(x, vextq_u8(zero, x, 16 - 4));
  x = vaddq_u8(x, vextq_u8(zero, x, 16 - 8));
  return x;
}

#endif


// Fetch 16b from N streams, compute SIMD delta
int bytedelta_encoder(const uint8_t *input, uint8_t *output, int32_t length, uint8_t meta,
                      blosc2_cparams *cparams, uint8_t id) {
  BLOSC_UNUSED_PARAM(id);

  int typesize = meta;
  if (typesize == 0) {
    if (cparams->schunk == NULL) {
      BLOSC_TRACE_ERROR("When meta is 0, you need to be on a schunk!");
      BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
    }
    blosc2_schunk* schunk = (blosc2_schunk*)(cparams->schunk);
    typesize = schunk->typesize;
  }

  const int stream_len = length / typesize;
  for (int ich = 0; ich < typesize; ++ich) {
    // delta within each channel, store
    Bytes16 v2 = {};
    int ip = 0;
    for (; ip < stream_len - 15; ip += 16) {
      Bytes16 v = SimdLoad(input);
      input += 16;
      Bytes16 delta = SimdSub(v, SimdConcat(v, v2));
      SimdStore(output, delta);
      output += 16;
      v2 = v;
    }
    // leftover (should never happen, but anyway)
    uint8_t _v2 = 0;
    for (; ip < stream_len ; ip++) {
      uint8_t v = *input;
      input++;
      *output = v - _v2;
      output++;
      _v2 = v;
    }
  }

  return BLOSC2_ERROR_SUCCESS;
}

// Fetch 16b from N streams, sum SIMD undelta
int bytedelta_decoder(const uint8_t *input, uint8_t *output, int32_t length, uint8_t meta,
                      blosc2_dparams *dparams, uint8_t id) {
  BLOSC_UNUSED_PARAM(id);

  int typesize = meta;
  if (typesize == 0) {
    if (dparams->schunk == NULL) {
      BLOSC_TRACE_ERROR("When meta is 0, you need to be on a schunk!");
      BLOSC_ERROR(BLOSC2_ERROR_FAILURE);
    }
    blosc2_schunk* schunk = (blosc2_schunk*)(dparams->schunk);
    typesize = schunk->typesize;
  }

  const Bytes16 hibyte = SimdSet1(15);
  const int stream_len = length / typesize;
  for (int ich = 0; ich < typesize; ++ich) {
    // fetch 16 bytes from each channel, prefix-sum un-delta
    Bytes16 v2 = {};
    int ip = 0;
    for (; ip < stream_len - 15; ip += 16) {
      Bytes16 v = SimdLoad(input);
      input += 16;
      // un-delta via prefix sum
      v2 = SimdAdd(SimdPrefixSum(v), SimdShuffle(v2, hibyte));
      SimdStore(output, v2);
      output += 16;
    }
    // leftover (should never happen, but anyway)
    uint8_t _v2 = 0;
    for (; ip < stream_len; ip++) {
      uint8_t v = *input + _v2;
      input++;
      *output = v;
      output++;
      _v2 = v;
    }
  }

  return BLOSC2_ERROR_SUCCESS;
}
