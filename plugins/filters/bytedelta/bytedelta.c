/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

// ByteDelta filter.  This is based on work by Aras Pranckevičius:
// https://aras-p.info/blog/2023/03/01/Float-Compression-7-More-Filtering-Optimization/
// SIMD implementations use Intel SSSE3 or ARM64 NEON.

#include "bytedelta.h"
#include "blosc2.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Do NOT auto-define SSSE3 based on _MSC_VER (clang-cl defines _MSC_VER). 
Require the build system to enable SSSE3 via the appropriate compiler 
flag (/arch:SSSE3 or -mssse3). */

#if defined(HAVE_SSSE3)
// SSSE3 code path for x64/x64
#define CPU_HAS_SIMD 1
#include <emmintrin.h>
#include <tmmintrin.h>
typedef __m128i bytes16;
bytes16 simd_zero() { return _mm_setzero_si128(); }
bytes16 simd_set1(uint8_t v) { return _mm_set1_epi8(v); }
bytes16 simd_load(const void* ptr) { return _mm_loadu_si128((const __m128i*)ptr); }
void simd_store(void* ptr, bytes16 x) { _mm_storeu_si128((__m128i*)ptr, x); }

bytes16 simd_concat(bytes16 hi, bytes16 lo) { return _mm_alignr_epi8(hi, lo, 15); }
bytes16 simd_add(bytes16 a, bytes16 b) { return _mm_add_epi8(a, b); }
bytes16 simd_sub(bytes16 a, bytes16 b) { return _mm_sub_epi8(a, b); }
bytes16 simd_duplane15(bytes16 x) { return _mm_shuffle_epi8(x, _mm_set1_epi8(15)); }

bytes16 simd_prefix_sum(bytes16 x)
{
  // Sklansky-style sum from https://gist.github.com/rygorous/4212be0cd009584e4184e641ca210528
  x = _mm_add_epi8(x, _mm_slli_epi64(x, 8));
  x = _mm_add_epi8(x, _mm_slli_epi64(x, 16));
  x = _mm_add_epi8(x, _mm_slli_epi64(x, 32));
  x = _mm_add_epi8(x, _mm_shuffle_epi8(x, _mm_setr_epi8(-1,-1,-1,-1,-1,-1,-1,-1,7,7,7,7,7,7,7,7)));
  return x;
}

uint8_t simd_get_last(bytes16 x) { return (_mm_extract_epi16(x, 7) >> 8) & 0xFF; }

#elif defined(__aarch64__) || defined(_M_ARM64)
// ARM v8 NEON code path
#define CPU_HAS_SIMD 1
#include <arm_neon.h>
typedef uint8x16_t bytes16;
bytes16 simd_zero() { return vdupq_n_u8(0); }
bytes16 simd_set1(uint8_t v) { return vdupq_n_u8(v); }
bytes16 simd_load(const void* ptr) { return vld1q_u8((const uint8_t*)ptr); }
void simd_store(void* ptr, bytes16 x) { vst1q_u8((uint8_t*)ptr, x); }

bytes16 simd_concat(bytes16 hi, bytes16 lo) { return vextq_u8(lo, hi, 15); }
bytes16 simd_add(bytes16 a, bytes16 b) { return vaddq_u8(a, b); }
bytes16 simd_sub(bytes16 a, bytes16 b) { return vsubq_u8(a, b); }
bytes16 simd_duplane15(bytes16 x) { return vdupq_laneq_u8(x, 15); }

bytes16 simd_prefix_sum(bytes16 x)
{
  // Kogge-Stone-style like commented out part of https://gist.github.com/rygorous/4212be0cd009584e4184e641ca210528
  bytes16 zero = vdupq_n_u8(0);
  x = vaddq_u8(x, vextq_u8(zero, x, 16 - 1));
  x = vaddq_u8(x, vextq_u8(zero, x, 16 - 2));
  x = vaddq_u8(x, vextq_u8(zero, x, 16 - 4));
  x = vaddq_u8(x, vextq_u8(zero, x, 16 - 8));
  return x;
}

uint8_t simd_get_last(bytes16 x) { return vgetq_lane_u8(x, 15); }

#endif


#if defined(__aarch64__) || defined(_M_ARM64)
/* Sixteen records remain in registers between byte transpose and delta. For
 * eight-byte records, LD4 sees alternating low/high four-byte halves; UZP
 * separates those halves into the eight original byte planes. */
static inline void load_planes16(const uint8_t *input, bytes16 *planes, int typesize) {
  if (typesize == 2) {
    uint8x16x2_t values = vld2q_u8(input);
    planes[0] = values.val[0];
    planes[1] = values.val[1];
    return;
  }
  uint8x16x4_t lo = vld4q_u8(input);
  if (typesize == 4) {
    for (int p = 0; p < 4; p++) {
      planes[p] = lo.val[p];
    }
  }
  else {
    uint8x16x4_t hi = vld4q_u8(input + 64);
    for (int p = 0; p < 4; p++) {
      planes[p] = vuzp1q_u8(lo.val[p], hi.val[p]);
      planes[p + 4] = vuzp2q_u8(lo.val[p], hi.val[p]);
    }
  }
}

static inline void store_planes16(uint8_t *output, const bytes16 *planes, int typesize) {
  if (typesize == 2) {
    uint8x16x2_t values;
    values.val[0] = planes[0];
    values.val[1] = planes[1];
    vst2q_u8(output, values);
    return;
  }
  uint8x16x4_t lo;
  if (typesize == 4) {
    for (int p = 0; p < 4; p++) {
      lo.val[p] = planes[p];
    }
    vst4q_u8(output, lo);
  }
  else {
    uint8x16x4_t hi;
    for (int p = 0; p < 4; p++) {
      lo.val[p] = vzip1q_u8(planes[p], planes[p + 4]);
      hi.val[p] = vzip2q_u8(planes[p], planes[p + 4]);
    }
    vst4q_u8(output, lo);
    vst4q_u8(output + 64, hi);
  }
}
#endif

int shuffle_bytedelta_neon_forward(const uint8_t *input, uint8_t *output, int32_t length, int typesize) {
#if defined(__aarch64__) || defined(_M_ARM64)
  if ((typesize != 2 && typesize != 4 && typesize != 8) || length < 0 || input == output) {
    return 0;
  }
  int count = length / typesize;
  int vector_count = count / 16 * 16;
  bytes16 previous[8], planes[8];
  for (int p = 0; p < typesize; p++) {
    previous[p] = simd_zero();
  }
  /* Keep plane counts compile-time constants. A variable eight-plane loop
   * makes Clang spill the transpose/carry vectors to the stack on each tile. */
  if (typesize == 2) {
    int i = 0;
    /* Two-byte groups otherwise spend disproportionate time on per-tile
     * branches and carry-register moves. Keep four tiles in flight. */
    for (; i + 64 <= vector_count; i += 64) {
      for (int tile = 0; tile < 4; tile++) {
        load_planes16(input + (i + tile * 16) * 2, planes + tile * 2, 2);
      }
      for (int p = 0; p < 2; p++) {
        bytes16 carry = previous[p];
        for (int tile = 0; tile < 4; tile++) {
          bytes16 value = planes[tile * 2 + p];
          bytes16 delta = simd_sub(value, simd_concat(value, carry));
          simd_store(output + p * count + i + tile * 16, delta);
          carry = value;
        }
        previous[p] = carry;
      }
    }
    for (; i < vector_count; i += 16) {
      load_planes16(input + i * 2, planes, 2);
      for (int p = 0; p < 2; p++) {
        bytes16 delta = simd_sub(planes[p], simd_concat(planes[p], previous[p]));
        simd_store(output + p * count + i, delta);
        previous[p] = planes[p];
      }
    }
  }
  else if (typesize == 4) {
    for (int i = 0; i < vector_count; i += 16) {
      load_planes16(input + i * 4, planes, 4);
      for (int p = 0; p < 4; p++) {
        bytes16 delta = simd_sub(planes[p], simd_concat(planes[p], previous[p]));
        simd_store(output + p * count + i, delta);
        previous[p] = planes[p];
      }
    }
  }
  else {
    for (int i = 0; i < vector_count; i += 16) {
      load_planes16(input + i * 8, planes, 8);
      for (int p = 0; p < 8; p++) {
        bytes16 delta = simd_sub(planes[p], simd_concat(planes[p], previous[p]));
        simd_store(output + p * count + i, delta);
        previous[p] = planes[p];
      }
    }
  }
  for (int p = 0; p < typesize; p++) {
    uint8_t carry = simd_get_last(previous[p]);
    for (int i = vector_count; i < count; i++) {
      uint8_t value = input[i * typesize + p];
      output[p * count + i] = (uint8_t)(value - carry);
      carry = value;
    }
  }
  /* Shuffle and the corrected BYTEDELTA both pass incomplete records through. */
  memcpy(output + count * typesize, input + count * typesize, (size_t)(length % typesize));
  return 1;
#else
  BLOSC_UNUSED_PARAM(input);
  BLOSC_UNUSED_PARAM(output);
  BLOSC_UNUSED_PARAM(length);
  BLOSC_UNUSED_PARAM(typesize);
  return 0;
#endif
}

int shuffle_bytedelta_neon_backward(const uint8_t *input, uint8_t *output, int32_t length, int typesize) {
#if defined(__aarch64__) || defined(_M_ARM64)
  if ((typesize != 2 && typesize != 4 && typesize != 8) || length < 0 || input == output) {
    return 0;
  }
  int count = length / typesize;
  int vector_count = count / 16 * 16;
  bytes16 previous[8], planes[8];
  for (int p = 0; p < typesize; p++) {
    previous[p] = simd_zero();
  }
  if (typesize == 2) {
    for (int i = 0; i < vector_count; i += 16) {
      for (int p = 0; p < 2; p++) {
        bytes16 delta = simd_load(input + p * count + i);
        planes[p] = simd_add(simd_prefix_sum(delta), simd_duplane15(previous[p]));
        previous[p] = planes[p];
      }
      store_planes16(output + i * 2, planes, 2);
    }
  }
  else if (typesize == 4) {
    for (int i = 0; i < vector_count; i += 16) {
      for (int p = 0; p < 4; p++) {
        bytes16 delta = simd_load(input + p * count + i);
        planes[p] = simd_add(simd_prefix_sum(delta), simd_duplane15(previous[p]));
        previous[p] = planes[p];
      }
      store_planes16(output + i * 4, planes, 4);
    }
  }
  else {
    for (int i = 0; i < vector_count; i += 16) {
      for (int p = 0; p < 8; p++) {
        bytes16 delta = simd_load(input + p * count + i);
        planes[p] = simd_add(simd_prefix_sum(delta), simd_duplane15(previous[p]));
        previous[p] = planes[p];
      }
      store_planes16(output + i * 8, planes, 8);
    }
  }
  for (int p = 0; p < typesize; p++) {
    uint8_t carry = simd_get_last(previous[p]);
    for (int i = vector_count; i < count; i++) {
      carry = (uint8_t)(carry + input[p * count + i]);
      output[i * typesize + p] = carry;
    }
  }
  memcpy(output + count * typesize, input + count * typesize, (size_t)(length % typesize));
  return 1;
#else
  BLOSC_UNUSED_PARAM(input);
  BLOSC_UNUSED_PARAM(output);
  BLOSC_UNUSED_PARAM(length);
  BLOSC_UNUSED_PARAM(typesize);
  return 0;
#endif
}

// Fetch 16b from N streams, compute SIMD delta
int bytedelta_forward(const uint8_t *input, uint8_t *output, int32_t length, uint8_t meta,
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
    int ip = 0;
    uint8_t _v2 = 0;
    // SIMD delta within each channel, store
#if defined(CPU_HAS_SIMD)
    bytes16 v2 = {0};
    for (; ip < stream_len - 15; ip += 16) {
      bytes16 v = simd_load(input);
      input += 16;
      bytes16 delta = simd_sub(v, simd_concat(v, v2));
      simd_store(output, delta);
      output += 16;
      v2 = v;
    }
    if (stream_len > 15) {
      _v2 = simd_get_last(v2);
    }
#endif // #if defined(CPU_HAS_SIMD)
    // scalar leftover
    for (; ip < stream_len ; ip++) {
      uint8_t v = *input;
      input++;
      *output = v - _v2;
      output++;
      _v2 = v;
    }
  }

  // When length is not a multiple of typesize, the trailing length % typesize
  // bytes do not belong to any channel (shuffle leaves them in place too).
  // Pass them through verbatim, or they would be left uninitialized in output.
  memcpy(output, input, (size_t)(length - stream_len * typesize));

  return BLOSC2_ERROR_SUCCESS;
}

// Fetch 16b from N streams, sum SIMD undelta
int bytedelta_backward(const uint8_t *input, uint8_t *output, int32_t length, uint8_t meta,
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

  const int stream_len = length / typesize;
  for (int ich = 0; ich < typesize; ++ich) {
    int ip = 0;
    uint8_t _v2 = 0;
    // SIMD fetch 16 bytes from each channel, prefix-sum un-delta
#if defined(CPU_HAS_SIMD)
    bytes16 v2 = {0};
    for (; ip < stream_len - 15; ip += 16) {
      bytes16 v = simd_load(input);
      input += 16;
      // un-delta via prefix sum
      v2 = simd_add(simd_prefix_sum(v), simd_duplane15(v2));
      simd_store(output, v2);
      output += 16;
    }
    if (stream_len > 15) {
      _v2 = simd_get_last(v2);
    }
#endif // #if defined(CPU_HAS_SIMD)
    // scalar leftover
    for (; ip < stream_len; ip++) {
      uint8_t v = *input + _v2;
      input++;
      *output = v;
      output++;
      _v2 = v;
    }
  }

  // Mirror of the forward pass: restore the trailing bytes verbatim.
  memcpy(output, input, (size_t)(length - stream_len * typesize));

  return BLOSC2_ERROR_SUCCESS;
}

// This is the original (and buggy) version of bytedelta.  It is kept here for backwards compatibility.
// See #524 for details.
// Fetch 16b from N streams, compute SIMD delta
int bytedelta_forward_buggy(const uint8_t *input, uint8_t *output, int32_t length,
                            uint8_t meta, blosc2_cparams *cparams, uint8_t id) {
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
    int ip = 0;
    // SIMD delta within each channel, store
#if defined(CPU_HAS_SIMD)
    bytes16 v2 = {0};
    for (; ip < stream_len - 15; ip += 16) {
      bytes16 v = simd_load(input);
      input += 16;
      bytes16 delta = simd_sub(v, simd_concat(v, v2));
      simd_store(output, delta);
      output += 16;
      v2 = v;
    }
#endif // #if defined(CPU_HAS_SIMD)
    // scalar leftover
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
int bytedelta_backward_buggy(const uint8_t *input, uint8_t *output, int32_t length,
                             uint8_t meta, blosc2_dparams *dparams, uint8_t id) {
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

  const int stream_len = length / typesize;
  for (int ich = 0; ich < typesize; ++ich) {
    int ip = 0;
    // SIMD fetch 16 bytes from each channel, prefix-sum un-delta
#if defined(CPU_HAS_SIMD)
    bytes16 v2 = {0};
    for (; ip < stream_len - 15; ip += 16) {
      bytes16 v = simd_load(input);
      input += 16;
      // un-delta via prefix sum
      v2 = simd_add(simd_prefix_sum(v), simd_duplane15(v2));
      simd_store(output, v2);
      output += 16;
    }
#endif // #if defined(CPU_HAS_SIMD)
    // scalar leftover
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
