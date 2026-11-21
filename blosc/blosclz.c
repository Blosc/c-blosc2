/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/*********************************************************************
  The code in this file is heavily based on FastLZ, a lightning-fast
  lossless compression library.  See LICENSES/FASTLZ.txt for details.
**********************************************************************/


#include "blosclz.h"
#include "fastcopy.h"
#include "shuffle.h"
#include "blosc2/blosc2-common.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if defined(__AVX2__)
#include <immintrin.h>
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#endif

/*
 * Give hints to the compiler for branch prediction optimization.
 * This is not necessary anymore with modern CPUs.
 */
#if 0 && defined(__GNUC__) && (__GNUC__ > 2)
#define BLOSCLZ_LIKELY(c)    (__builtin_expect((c), 1))
#define BLOSCLZ_UNLIKELY(c)  (__builtin_expect((c), 0))
#else
#define BLOSCLZ_LIKELY(c)    (c)
#define BLOSCLZ_UNLIKELY(c)  (c)
#endif

/*
 * Use inlined functions for supported systems.
 */
#if defined(_MSC_VER) && !defined(__cplusplus)   /* Visual Studio */
#define inline __inline  /* Visual C is not C99, but supports some kind of inline */
#endif

#define MAX_COPY 32U
#define MAX_DISTANCE 8191
#define MAX_FARDISTANCE (65535 + MAX_DISTANCE - 1)

#ifdef BLOSC_STRICT_ALIGN
  #define BLOSCLZ_READU16(p) ((p)[0] | (p)[1]<<8)
  #define BLOSCLZ_READU32(p) ((p)[0] | (p)[1]<<8 | (p)[2]<<16 | (p)[3]<<24)
#else
  #define BLOSCLZ_READU16(p) *((const uint16_t*)(p))
  #define BLOSCLZ_READU32(p) *((const uint32_t*)(p))
#endif

#define HASH_LOG (14U)

// This is used in LZ4 and seems to work pretty well here too
#define HASH_FUNCTION(v, s, h) {      \
  (v) = ((s) * 2654435761U) >> (32U - (h)); \
}

#if defined(_MSC_VER)
#define BLOSCLZ_ALWAYS_INLINE static __forceinline
#else
#define BLOSCLZ_ALWAYS_INLINE static inline __attribute__((always_inline))
#endif

#if defined(__GNUC__) || defined(__clang__)
/* 32-bit ctz is only used by x86 movemask paths; guard to avoid unused warnings on ARM64 */
#if defined(__SSE2__) || defined(__AVX2__)
static inline unsigned blosclz_ctz32(uint32_t v) {
  return (unsigned)__builtin_ctz(v);
}
#define BLOSCLZ_HAVE_CTZ32 1
#endif
static inline unsigned blosclz_ctz64(uint64_t v) {
  return (unsigned)__builtin_ctzll(v);
}
#define BLOSCLZ_HAVE_CTZ64 1
#elif defined(_MSC_VER)
/* 32-bit ctz is only used by x86 movemask paths; guard to avoid unused warnings on ARM64 */
#if defined(__SSE2__) || defined(__AVX2__)
static inline unsigned blosclz_ctz32(uint32_t v) {
  unsigned long idx;
  _BitScanForward(&idx, v);
  return (unsigned)idx;
}
#define BLOSCLZ_HAVE_CTZ32 1
#endif
#if defined(_M_X64) || defined(_M_ARM64)
static inline unsigned blosclz_ctz64(uint64_t v) {
  unsigned long idx;
  _BitScanForward64(&idx, v);
  return (unsigned)idx;
}
#define BLOSCLZ_HAVE_CTZ64 1
#endif
#endif

#if defined(BLOSCLZ_AVX2_ENABLED) && !defined(__AVX2__) && !defined(BLOSCLZ_BUILD_AVX2)
int blosclz_compress_avx2(const int clevel, const void* input, int length,
                          void* output, int maxout, blosc2_context* ctx);
int blosclz_compress_state_avx2(const int clevel, const void* input, int length,
                                void* output, int maxout, blosclz_state* state);
#endif

#if defined(__AVX2__)
BLOSCLZ_ALWAYS_INLINE uint8_t *get_run_32(uint8_t *ip, const uint8_t *ip_bound, const uint8_t *ref) {
  uint8_t x = ip[-1];
  /* Broadcast the run byte across all 32 lanes once outside the loop */
  const __m256i value = _mm256_set1_epi8((char)x);
  /* Scan long runs in 128-byte (4x32B) groups with a single reduction */
  while (ip_bound - ip >= 128) {
    __m256i cmp0 = _mm256_cmpeq_epi8(value, _mm256_loadu_si256((const __m256i *)ref));
    __m256i cmp1 = _mm256_cmpeq_epi8(value, _mm256_loadu_si256((const __m256i *)(ref + 32)));
    __m256i cmp2 = _mm256_cmpeq_epi8(value, _mm256_loadu_si256((const __m256i *)(ref + 64)));
    __m256i cmp3 = _mm256_cmpeq_epi8(value, _mm256_loadu_si256((const __m256i *)(ref + 96)));
    __m256i eq01 = _mm256_and_si256(cmp0, cmp1);
    __m256i eq23 = _mm256_and_si256(cmp2, cmp3);
    if ((uint32_t)_mm256_movemask_epi8(_mm256_and_si256(eq01, eq23)) != 0xFFFFFFFFU) {
      /* Pinpoint the first differing byte directly from the compare vectors */
      if ((uint32_t)_mm256_movemask_epi8(eq01) != 0xFFFFFFFFU) {
        uint32_t m0 = (uint32_t)_mm256_movemask_epi8(cmp0);
        if (m0 != 0xFFFFFFFFU) {
#if defined(BLOSCLZ_HAVE_CTZ32)
          return ip + blosclz_ctz32(~m0);
#else
          while (*ref++ == x) ip++;
          return ip;
#endif
        }
        uint32_t m1 = (uint32_t)_mm256_movemask_epi8(cmp1);
#if defined(BLOSCLZ_HAVE_CTZ32)
        return ip + 32 + blosclz_ctz32(~m1);
#else
        ip += 32;
        ref += 32;
        while (*ref++ == x) ip++;
        return ip;
#endif
      }
      uint32_t m2 = (uint32_t)_mm256_movemask_epi8(cmp2);
      if (m2 != 0xFFFFFFFFU) {
#if defined(BLOSCLZ_HAVE_CTZ32)
        return ip + 64 + blosclz_ctz32(~m2);
#else
        ip += 64;
        ref += 64;
        while (*ref++ == x) ip++;
        return ip;
#endif
      }
      uint32_t m3 = (uint32_t)_mm256_movemask_epi8(cmp3);
#if defined(BLOSCLZ_HAVE_CTZ32)
      return ip + 96 + blosclz_ctz32(~m3);
#else
      ip += 96;
      ref += 96;
      while (*ref++ == x) ip++;
      return ip;
#endif
    }
    ip += 128;
    ref += 128;
  }
  /* Narrow 32-byte and 16-byte vector tails */
  while (ip_bound - ip >= 32) {
    __m256i cmp = _mm256_cmpeq_epi8(value, _mm256_loadu_si256((const __m256i *)ref));
    uint32_t mask = (uint32_t)_mm256_movemask_epi8(cmp);
    if (mask != 0xFFFFFFFFU) {
#if defined(BLOSCLZ_HAVE_CTZ32)
      return ip + blosclz_ctz32(~mask);
#else
      while (*ref++ == x) ip++;
      return ip;
#endif
    }
    ip += 32;
    ref += 32;
  }
  if (ip_bound - ip >= 16) {
    __m128i cmp = _mm_cmpeq_epi8(_mm256_castsi256_si128(value),
                                 _mm_loadu_si128((const __m128i *)ref));
    uint32_t mask = (uint32_t)_mm_movemask_epi8(cmp);
    if (mask != 0xFFFFU) {
#if defined(BLOSCLZ_HAVE_CTZ32)
      return ip + blosclz_ctz32(~mask);
#else
      while (*ref++ == x) ip++;
      return ip;
#endif
    }
    ip += 16;
    ref += 16;
  }
  /* Keep the original run byte: ref trails ip by one byte, so ip[-1] has
   * not necessarily been checked when a vector iteration ends. */
  while ((ip < ip_bound) && (*ref++ == x)) ip++;
  return ip;
}

BLOSCLZ_ALWAYS_INLINE uint8_t *get_match_32(uint8_t *ip, const uint8_t *ip_bound, const uint8_t *ref) {
  /* Most matches in floating-point byte deltas are short. Avoid wide vector
   * comparisons for these; a little-endian 64-bit scan locates a mismatch
   * without a byte-at-a-time loop. */
  if (ip_bound - ip >= 8) {
    uint64_t a, b;
    memcpy(&a, ip, sizeof(a));
    memcpy(&b, ref, sizeof(b));
    uint64_t diff = a ^ b;
    if (diff) {
#if defined(BLOSCLZ_HAVE_CTZ64)
      return ip + (blosclz_ctz64(diff) >> 3) + 1;
#else
      while (*ref++ == *ip++) {}
      return ip;
#endif
    }
    ip += 8;
    ref += 8;
  }
  /* Scan long matches in 128-byte (4x32B) groups with a single reduction */
  while (ip_bound - ip >= 128) {
    __m256i cmp0 = _mm256_cmpeq_epi8(_mm256_loadu_si256((const __m256i *)ip),
                                     _mm256_loadu_si256((const __m256i *)ref));
    __m256i cmp1 = _mm256_cmpeq_epi8(_mm256_loadu_si256((const __m256i *)(ip + 32)),
                                     _mm256_loadu_si256((const __m256i *)(ref + 32)));
    __m256i cmp2 = _mm256_cmpeq_epi8(_mm256_loadu_si256((const __m256i *)(ip + 64)),
                                     _mm256_loadu_si256((const __m256i *)(ref + 64)));
    __m256i cmp3 = _mm256_cmpeq_epi8(_mm256_loadu_si256((const __m256i *)(ip + 96)),
                                     _mm256_loadu_si256((const __m256i *)(ref + 96)));
    __m256i eq01 = _mm256_and_si256(cmp0, cmp1);
    __m256i eq23 = _mm256_and_si256(cmp2, cmp3);
    if ((uint32_t)_mm256_movemask_epi8(_mm256_and_si256(eq01, eq23)) != 0xFFFFFFFFU) {
      /* Pinpoint the first mismatch (+1 matches the scalar post-increment contract) */
      if ((uint32_t)_mm256_movemask_epi8(eq01) != 0xFFFFFFFFU) {
        uint32_t m0 = (uint32_t)_mm256_movemask_epi8(cmp0);
        if (m0 != 0xFFFFFFFFU) {
#if defined(BLOSCLZ_HAVE_CTZ32)
          return ip + blosclz_ctz32(~m0) + 1;
#else
          while (*ref++ == *ip++) {}
          return ip;
#endif
        }
        uint32_t m1 = (uint32_t)_mm256_movemask_epi8(cmp1);
#if defined(BLOSCLZ_HAVE_CTZ32)
        return ip + 32 + blosclz_ctz32(~m1) + 1;
#else
        ip += 32;
        ref += 32;
        while (*ref++ == *ip++) {}
        return ip;
#endif
      }
      uint32_t m2 = (uint32_t)_mm256_movemask_epi8(cmp2);
      if (m2 != 0xFFFFFFFFU) {
#if defined(BLOSCLZ_HAVE_CTZ32)
        return ip + 64 + blosclz_ctz32(~m2) + 1;
#else
        ip += 64;
        ref += 64;
        while (*ref++ == *ip++) {}
        return ip;
#endif
      }
      uint32_t m3 = (uint32_t)_mm256_movemask_epi8(cmp3);
#if defined(BLOSCLZ_HAVE_CTZ32)
      return ip + 96 + blosclz_ctz32(~m3) + 1;
#else
      ip += 96;
      ref += 96;
      while (*ref++ == *ip++) {}
      return ip;
#endif
    }
    ip += 128;
    ref += 128;
  }
  /* Narrow 32-byte, 16-byte, and scalar tails */
  while (ip_bound - ip >= 32) {
    __m256i cmp = _mm256_cmpeq_epi8(_mm256_loadu_si256((const __m256i *)ip),
                                    _mm256_loadu_si256((const __m256i *)ref));
    uint32_t mask = (uint32_t)_mm256_movemask_epi8(cmp);
    if (mask != 0xFFFFFFFFU) {
#if defined(BLOSCLZ_HAVE_CTZ32)
      return ip + blosclz_ctz32(~mask) + 1;
#else
      while (*ref++ == *ip++) {}
      return ip;
#endif
    }
    ip += 32;
    ref += 32;
  }
  if (ip_bound - ip >= 16) {
    __m128i cmp = _mm_cmpeq_epi8(_mm_loadu_si128((const __m128i *)ip),
                                 _mm_loadu_si128((const __m128i *)ref));
    uint32_t mask = (uint32_t)_mm_movemask_epi8(cmp);
    if (mask != 0xFFFFU) {
#if defined(BLOSCLZ_HAVE_CTZ32)
      return ip + blosclz_ctz32(~mask) + 1;
#else
      while (*ref++ == *ip++) {}
      return ip;
#endif
    }
    ip += 16;
    ref += 16;
  }
  while ((ip < ip_bound) && (*ref++ == *ip++)) {}
  return ip;
}
#endif

#if defined(__SSE2__) && !defined(__AVX2__)
BLOSCLZ_ALWAYS_INLINE uint8_t *get_run_16(uint8_t *ip, const uint8_t *ip_bound, const uint8_t *ref) {
  uint8_t x = ip[-1];
  /* Broadcast the run byte across all 16 lanes once outside the loop */
  const __m128i value = _mm_set1_epi8((char)x);
  /* Scan long runs in 64-byte (4x16B) groups with a single reduction */
  while (ip_bound - ip >= 64) {
    __m128i cmp0 = _mm_cmpeq_epi8(value, _mm_loadu_si128((const __m128i *)ref));
    __m128i cmp1 = _mm_cmpeq_epi8(value, _mm_loadu_si128((const __m128i *)(ref + 16)));
    __m128i cmp2 = _mm_cmpeq_epi8(value, _mm_loadu_si128((const __m128i *)(ref + 32)));
    __m128i cmp3 = _mm_cmpeq_epi8(value, _mm_loadu_si128((const __m128i *)(ref + 48)));
    __m128i eq01 = _mm_and_si128(cmp0, cmp1);
    __m128i eq23 = _mm_and_si128(cmp2, cmp3);
    if (_mm_movemask_epi8(_mm_and_si128(eq01, eq23)) != 0xFFFF) {
      /* Pinpoint the first differing byte directly from the compare vectors */
      if (_mm_movemask_epi8(eq01) != 0xFFFF) {
        uint32_t m0 = (uint32_t)_mm_movemask_epi8(cmp0);
        if (m0 != 0xFFFFU) {
#if defined(BLOSCLZ_HAVE_CTZ32)
          return ip + blosclz_ctz32(~m0);
#else
          while (*ref++ == x) ip++;
          return ip;
#endif
        }
        uint32_t m1 = (uint32_t)_mm_movemask_epi8(cmp1);
#if defined(BLOSCLZ_HAVE_CTZ32)
        return ip + 16 + blosclz_ctz32(~m1);
#else
        ip += 16;
        ref += 16;
        while (*ref++ == x) ip++;
        return ip;
#endif
      }
      uint32_t m2 = (uint32_t)_mm_movemask_epi8(cmp2);
      if (m2 != 0xFFFFU) {
#if defined(BLOSCLZ_HAVE_CTZ32)
        return ip + 32 + blosclz_ctz32(~m2);
#else
        ip += 32;
        ref += 32;
        while (*ref++ == x) ip++;
        return ip;
#endif
      }
      uint32_t m3 = (uint32_t)_mm_movemask_epi8(cmp3);
#if defined(BLOSCLZ_HAVE_CTZ32)
      return ip + 48 + blosclz_ctz32(~m3);
#else
      ip += 48;
      ref += 48;
      while (*ref++ == x) ip++;
      return ip;
#endif
    }
    ip += 64;
    ref += 64;
  }
  /* Narrow 16-byte vector tail */
  while (ip_bound - ip >= 16) {
    __m128i cmp = _mm_cmpeq_epi8(value, _mm_loadu_si128((const __m128i *)ref));
    uint32_t mask = (uint32_t)_mm_movemask_epi8(cmp);
    if (mask != 0xFFFFU) {
#if defined(BLOSCLZ_HAVE_CTZ32)
      return ip + blosclz_ctz32(~mask);
#else
      while (*ref++ == x) ip++;
      return ip;
#endif
    }
    ip += 16;
    ref += 16;
  }
  /* Keep the original run byte: ref trails ip by one byte, so ip[-1] has
   * not necessarily been checked when a vector iteration ends. */
  while ((ip < ip_bound) && (*ref++ == x)) ip++;
  return ip;
}
#endif


#if !defined(__aarch64__) && !defined(_M_ARM64) && !defined(__SSE2__) && !defined(__AVX2__)
static uint8_t *get_run(uint8_t *ip, const uint8_t *ip_bound, const uint8_t *ref) {
  uint8_t x = ip[-1];
  int64_t value, value2;
  /* Broadcast the value for every byte in a 64-bit register */
  memset(&value, x, 8);
  /* safe because the outer check against ip limit */
  while (ip < (ip_bound - sizeof(int64_t))) {
#if defined(BLOSC_STRICT_ALIGN)
    memcpy(&value2, ref, 8);
#else
    value2 = ((int64_t*)ref)[0];
#endif
    if (value != value2) {
      /* Return the byte that starts to differ */
      while (*ref++ == x) ip++;
      return ip;
    }
    else {
      ip += 8;
      ref += 8;
    }
  }
  /* Look into the remainder */
  while ((ip < ip_bound) && (*ref++ == x)) ip++;
  return ip;
}
#endif


#if !defined(BLOSCLZ_BUILD_AVX2)
/* Return the byte that starts to differ */
uint8_t *get_match(uint8_t *ip, const uint8_t *ip_bound, const uint8_t *ref) {
#if !defined(BLOSC_STRICT_ALIGN)
  while (ip < (ip_bound - sizeof(int64_t))) {
    if (*(int64_t*)ref != *(int64_t*)ip) {
      /* Return the byte that starts to differ */
      while (*ref++ == *ip++) {}
      return ip;
    }
    else {
      ip += sizeof(int64_t);
      ref += sizeof(int64_t);
    }
  }
#endif
  /* Look into the remainder */
  while ((ip < ip_bound) && (*ref++ == *ip++)) {}
  return ip;
}
#endif


#if defined(__SSE2__) && !defined(__AVX2__)
BLOSCLZ_ALWAYS_INLINE uint8_t *get_match_16(uint8_t *ip, const uint8_t *ip_bound, const uint8_t *ref) {
  /* Most matches in floating-point byte deltas are short. Avoid wide vector
   * comparisons for these; a little-endian 64-bit scan locates a mismatch
   * without a byte-at-a-time loop. */
  if (ip_bound - ip >= 8) {
    uint64_t a, b;
    memcpy(&a, ip, sizeof(a));
    memcpy(&b, ref, sizeof(b));
    uint64_t diff = a ^ b;
    if (diff) {
#if defined(BLOSCLZ_HAVE_CTZ64)
      return ip + (blosclz_ctz64(diff) >> 3) + 1;
#else
      while (*ref++ == *ip++) {}
      return ip;
#endif
    }
    ip += 8;
    ref += 8;
  }
  /* Scan long matches in 64-byte (4x16B) groups with a single reduction */
  while (ip_bound - ip >= 64) {
    __m128i cmp0 = _mm_cmpeq_epi8(_mm_loadu_si128((const __m128i *)ip),
                                  _mm_loadu_si128((const __m128i *)ref));
    __m128i cmp1 = _mm_cmpeq_epi8(_mm_loadu_si128((const __m128i *)(ip + 16)),
                                  _mm_loadu_si128((const __m128i *)(ref + 16)));
    __m128i cmp2 = _mm_cmpeq_epi8(_mm_loadu_si128((const __m128i *)(ip + 32)),
                                  _mm_loadu_si128((const __m128i *)(ref + 32)));
    __m128i cmp3 = _mm_cmpeq_epi8(_mm_loadu_si128((const __m128i *)(ip + 48)),
                                  _mm_loadu_si128((const __m128i *)(ref + 48)));
    __m128i eq01 = _mm_and_si128(cmp0, cmp1);
    __m128i eq23 = _mm_and_si128(cmp2, cmp3);
    if (_mm_movemask_epi8(_mm_and_si128(eq01, eq23)) != 0xFFFF) {
      /* Pinpoint the first mismatch (+1 matches the scalar post-increment contract) */
      if (_mm_movemask_epi8(eq01) != 0xFFFF) {
        uint32_t m0 = (uint32_t)_mm_movemask_epi8(cmp0);
        if (m0 != 0xFFFFU) {
#if defined(BLOSCLZ_HAVE_CTZ32)
          return ip + blosclz_ctz32(~m0) + 1;
#else
          while (*ref++ == *ip++) {}
          return ip;
#endif
        }
        uint32_t m1 = (uint32_t)_mm_movemask_epi8(cmp1);
#if defined(BLOSCLZ_HAVE_CTZ32)
        return ip + 16 + blosclz_ctz32(~m1) + 1;
#else
        ip += 16;
        ref += 16;
        while (*ref++ == *ip++) {}
        return ip;
#endif
      }
      uint32_t m2 = (uint32_t)_mm_movemask_epi8(cmp2);
      if (m2 != 0xFFFFU) {
#if defined(BLOSCLZ_HAVE_CTZ32)
        return ip + 32 + blosclz_ctz32(~m2) + 1;
#else
        ip += 32;
        ref += 32;
        while (*ref++ == *ip++) {}
        return ip;
#endif
      }
      uint32_t m3 = (uint32_t)_mm_movemask_epi8(cmp3);
#if defined(BLOSCLZ_HAVE_CTZ32)
      return ip + 48 + blosclz_ctz32(~m3) + 1;
#else
      ip += 48;
      ref += 48;
      while (*ref++ == *ip++) {}
      return ip;
#endif
    }
    ip += 64;
    ref += 64;
  }
  while (ip_bound - ip >= 16) {
    __m128i cmp = _mm_cmpeq_epi8(_mm_loadu_si128((const __m128i *)ip),
                                 _mm_loadu_si128((const __m128i *)ref));
    uint32_t mask = (uint32_t)_mm_movemask_epi8(cmp);
    if (mask != 0xFFFFU) {
#if defined(BLOSCLZ_HAVE_CTZ32)
      return ip + blosclz_ctz32(~mask) + 1;
#else
      while (*ref++ == *ip++) {}
      return ip;
#endif
    }
    ip += 16;
    ref += 16;
  }
  /* Look into the remainder */
  while ((ip < ip_bound) && (*ref++ == *ip++)) {}
  return ip;
}
#endif


#if defined(__aarch64__) || defined(_M_ARM64)
/* Reduce 4x32-bit lanes instead of 16x8-bit lanes (umaxv.4s is faster than uminv.16b) */
static inline bool blosclz_neon_any_nonzero(uint8x16_t v) {
  return vmaxvq_u32(vreinterpretq_u32_u8(v)) != 0;
}

/* Check long matches in cache-line-sized groups.  Reduce just once per group,
 * then use the narrow loop to locate the first differing byte. */
static uint8_t *get_match_neon(uint8_t *ip, const uint8_t *bound, const uint8_t *ref) {
  /* Most matches in floating-point byte deltas are short.  Avoid four vector
   * comparisons for these; AArch64's little-endian word scan locates a mismatch
   * without a byte-at-a-time loop. */
  if (bound - ip >= 8) {
    uint64_t a, b;
    memcpy(&a, ip, sizeof(a));
    memcpy(&b, ref, sizeof(b));
    uint64_t diff = a ^ b;
    if (diff) {
#if defined(BLOSCLZ_HAVE_CTZ64) && !defined(__AARCH64EB__)
      return ip + (blosclz_ctz64(diff) >> 3) + 1;
#else
      while (*ref++ == *ip++) {}
      return ip;
#endif
    }
    ip += 8;
    ref += 8;
  }
  while (bound - ip >= 64) {
    /* Accumulate bytewise differences via XOR+OR to test 64 bytes with one 32-bit max reduction */
    uint8x16_t d0 = veorq_u8(vld1q_u8(ip), vld1q_u8(ref));
    uint8x16_t d1 = veorq_u8(vld1q_u8(ip + 16), vld1q_u8(ref + 16));
    uint8x16_t d2 = veorq_u8(vld1q_u8(ip + 32), vld1q_u8(ref + 32));
    uint8x16_t d3 = veorq_u8(vld1q_u8(ip + 48), vld1q_u8(ref + 48));
    uint8x16_t d01 = vorrq_u8(d0, d1);
    uint8x16_t d23 = vorrq_u8(d2, d3);
    if (blosclz_neon_any_nonzero(vorrq_u8(d01, d23))) {
      break;
    }
    ip += 64;
    ref += 64;
  }
  while (bound - ip >= 16) {
    /* Test 16 bytes for any difference via XOR and 4-lane u32 reduction */
    if (blosclz_neon_any_nonzero(veorq_u8(vld1q_u8(ip), vld1q_u8(ref)))) {
      while (*ref++ == *ip++) {}
      return ip;
    }
    ip += 16;
    ref += 16;
  }
  while (ip < bound && *ref++ == *ip++) {}
  return ip;
}

static uint8_t *get_run_neon(uint8_t *ip, const uint8_t *bound, const uint8_t *ref) {
  uint8_t x = ip[-1];
  uint8x16_t value = vdupq_n_u8(x);
  while (bound - ip >= 64) {
    /* Accumulate differences from the run byte via XOR+OR with one 32-bit max reduction */
    uint8x16_t d0 = veorq_u8(value, vld1q_u8(ref));
    uint8x16_t d1 = veorq_u8(value, vld1q_u8(ref + 16));
    uint8x16_t d2 = veorq_u8(value, vld1q_u8(ref + 32));
    uint8x16_t d3 = veorq_u8(value, vld1q_u8(ref + 48));
    uint8x16_t d01 = vorrq_u8(d0, d1);
    uint8x16_t d23 = vorrq_u8(d2, d3);
    if (blosclz_neon_any_nonzero(vorrq_u8(d01, d23))) {
      break;
    }
    ip += 64;
    ref += 64;
  }
  while (bound - ip >= 16) {
    /* Check 16-byte tail chunks via XOR and 4-lane u32 reduction before scalar scan */
    if (blosclz_neon_any_nonzero(veorq_u8(value, vld1q_u8(ref)))) {
      while (*ref++ == x) ip++;
      return ip;
    }
    ip += 16;
    ref += 16;
  }
  /* Keep the original run byte: ref trails ip by one byte, so ip[-1] has
   * not necessarily been checked when a vector iteration ends. */
  while (ip < bound && *ref++ == x) ip++;
  return ip;
}
#endif

static inline uint8_t* get_run_or_match(uint8_t* ip, uint8_t* ip_bound, const uint8_t* ref, bool run) {
  if (BLOSCLZ_UNLIKELY(run)) {
#if defined(__AVX2__)
    ip = get_run_32(ip, ip_bound, ref);
#elif defined(__SSE2__)
    ip = get_run_16(ip, ip_bound, ref);
#elif defined(__aarch64__) || defined(_M_ARM64)
    ip = get_run_neon(ip, ip_bound, ref);
#else
    ip = get_run(ip, ip_bound, ref);
#endif
  }
  else {
#if defined(__AVX2__)
    ip = get_match_32(ip, ip_bound, ref);
#elif defined(__SSE2__)
    ip = get_match_16(ip, ip_bound, ref);
#elif defined(__aarch64__) || defined(_M_ARM64)
    ip = get_match_neon(ip, ip_bound, ref);
#else
    ip = get_match(ip, ip_bound, ref);
#endif
  }

  return ip;
}

#define LITERAL(ip, op, op_limit, anchor, copy) {       \
  if (BLOSCLZ_UNLIKELY((op_limit) - (op) < 2))          \
    goto out;                                           \
  *(op)++ = *(anchor)++;                                \
  (ip) = (anchor);                                      \
  (copy)++;                                             \
  if (BLOSCLZ_UNLIKELY((copy) == MAX_COPY)) {           \
    (copy) = 0;                                         \
    *(op)++ = MAX_COPY-1;                               \
  }                                                     \
}

#define LITERAL2(ip, anchor, copy) {                    \
  oc++; (anchor)++;                                     \
  (ip) = (anchor);                                      \
  (copy)++;                                             \
  if (BLOSCLZ_UNLIKELY((copy) == MAX_COPY)) {           \
    (copy) = 0;                                         \
    oc++;                                               \
  }                                                     \
}

#define MATCH_SHORT(op, op_limit, len, distance) {        \
  if (BLOSCLZ_UNLIKELY((op_limit) - (op) < 2))            \
    goto out;                                             \
  *(op)++ = (uint8_t)(((len) << 5U) + ((distance) >> 8U));\
  *(op)++ = (uint8_t)(((distance) & 255U));               \
}

#define MATCH_LONG(op, op_limit, len, distance) {       \
  if (BLOSCLZ_UNLIKELY((op_limit) - (op) < 1))          \
    goto out;                                           \
  *(op)++ = (uint8_t)((7U << 5U) + ((distance) >> 8U)); \
  for ((len) -= 7; (len) >= 255; (len) -= 255) {        \
    if (BLOSCLZ_UNLIKELY((op_limit) - (op) < 1))        \
      goto out;                                         \
    *(op)++ = 255;                                      \
  }                                                     \
  if (BLOSCLZ_UNLIKELY((op_limit) - (op) < 2))          \
    goto out;                                           \
  *(op)++ = (uint8_t)(len);                             \
  *(op)++ = (uint8_t)(((distance) & 255U));             \
}

#define MATCH_SHORT_FAR(op, op_limit, len, distance) {      \
  if (BLOSCLZ_UNLIKELY((op_limit) - (op) < 4))              \
    goto out;                                               \
  *(op)++ = (uint8_t)(((len) << 5U) + 31);                  \
  *(op)++ = 255;                                            \
  *(op)++ = (uint8_t)((distance) >> 8U);                    \
  *(op)++ = (uint8_t)((distance) & 255U);                   \
}

#define MATCH_LONG_FAR(op, op_limit, len, distance) {       \
  if (BLOSCLZ_UNLIKELY((op_limit) - (op) < 1))              \
    goto out;                                               \
  *(op)++ = (7U << 5U) + 31;                                \
  for ((len) -= 7; (len) >= 255; (len) -= 255) {            \
    if (BLOSCLZ_UNLIKELY((op_limit) - (op) < 1))            \
      goto out;                                             \
    *(op)++ = 255;                                          \
  }                                                         \
  if (BLOSCLZ_UNLIKELY((op_limit) - (op) < 4))              \
    goto out;                                               \
  *(op)++ = (uint8_t)(len);                                 \
  *(op)++ = 255;                                            \
  *(op)++ = (uint8_t)((distance) >> 8U);                    \
  *(op)++ = (uint8_t)((distance) & 255U);                   \
}

/* Hash entries are stored as position + base.  Entries below base belong to
 * a previous pass and read as position 0, exactly like a zeroed table.  With
 * no state, base is always 0 and the table is memset as usual. */
#define HTAB_GET(entry, base) (tagged ? ((entry) >= (base) ? (entry) - (base) : 0) : (entry))

// Get a guess for the compressed size of a buffer
BLOSCLZ_ALWAYS_INLINE double get_cratio(uint8_t* ibase, int maxlen, int minlen, int ipshift, uint32_t htab[],
                                        int8_t hashlog, uint32_t base, const int tagged) {
  uint8_t* ip = ibase;
  int32_t oc = 0;
  const uint16_t hashlen = (1U << (uint8_t)hashlog);
  uint32_t hval;
  uint32_t seq;
  uint8_t copy;
  // Make a tradeoff between testing too much and too little
  uint16_t limit = (maxlen > hashlen) ? hashlen : maxlen;
  uint8_t* ip_bound = ibase + limit - 1;
  uint8_t* ip_limit = ibase + limit - 12;


  /* we start with literal copy */
  copy = 4;
  oc += 5;

  /* main loop */
  while (BLOSCLZ_LIKELY(ip < ip_limit)) {
    const uint8_t* ref;
    unsigned distance;
    uint8_t* anchor = ip;    /* comparison starting-point */

    /* find potential match */
    seq = BLOSCLZ_READU32(ip);
    HASH_FUNCTION(hval, seq, hashlog)
    ref = ibase + HTAB_GET(htab[hval], base);

    /* calculate distance to the match */
    distance = (unsigned int)(anchor - ref);

    /* update hash table */
    htab[hval] = (uint32_t) (anchor - ibase) + base;

    if (distance == 0 || (distance >= MAX_FARDISTANCE)) {
      LITERAL2(ip, anchor, copy)
      continue;
    }

    /* is this a match? check the first 4 bytes */
    if (BLOSCLZ_READU32(ref) == BLOSCLZ_READU32(ip)) {
      ref += 4;
    }
    else {
      /* no luck, copy as a literal */
      LITERAL2(ip, anchor, copy)
      continue;
    }

    /* last matched byte */
    ip = anchor + 4;

    /* distance is biased */
    distance--;

    /* get runs or matches; zero distance means a run */
    ip = get_run_or_match(ip, ip_bound, ref, !distance);

    ip -= ipshift;
    int len = (int)(ip - anchor);
    if (len < minlen) {
      LITERAL2(ip, anchor, copy)
      continue;
    }

    /* if we haven't copied anything, adjust the output counter */
    if (!copy)
      oc--;
    /* reset literal counter */
    copy = 0;

    /* encode the match */
    if (distance < MAX_DISTANCE) {
      if (len >= 7) {
        oc += ((len - 7) / 255) + 1;
      }
      oc += 2;
    }
    else {
      /* far away, but not yet in the another galaxy... */
      if (len >= 7) {
        oc += ((len - 7) / 255) + 1;
      }
      oc += 4;
    }

    /* update the hash at match boundary */
    seq = BLOSCLZ_READU32(ip);
    HASH_FUNCTION(hval, seq, hashlog)
    htab[hval] = (uint32_t)(ip++ - ibase) + base;
    ip++;
    /* assuming literal copy */
    oc++;
  }

  double ic = (double)(ip - ibase);
  return ic / (double)oc;
}


BLOSCLZ_ALWAYS_INLINE uint32_t htab_reset(uint32_t htab[], uint8_t hashlog, blosclz_state* state, int span,
                                          const int tagged) {
  if (!tagged) {
    memset(htab, 0, (1U << hashlog) * sizeof(uint32_t));
    /* untagged entries (< span) must look stale to the next tagged pass */
    if (state != NULL && state->base < (uint32_t)span) {
      state->base = (uint32_t)span;
    }
    return 0;
  }
  if (state->base > UINT32_MAX - (uint32_t)span) {
    memset(state->htab, 0, sizeof(state->htab));
    state->base = 0;
  }
  uint32_t base = state->base;
  state->base += (uint32_t)span;
  return base;
}

BLOSCLZ_ALWAYS_INLINE int compress_impl(const int clevel, const void* input, int length, void* output,
                                        int maxout, uint32_t htab[], blosclz_state* state, const int tagged) {
  uint8_t* ibase = (uint8_t*)input;

  /* Keep match coverage and boundary positioning consistent across levels.
   * A shift of 4 truncates matches further and can disrupt periodic byte deltas,
   * reducing both compression ratio and speed on floating-point ramps. */
  unsigned ipshift = 3;
  /* Retain the higher-level minimum: accepting more short matches can prevent
   * finding longer matches and increase decompression work. */
  unsigned minlen = clevel == 1 ? 3 : 4;

  /* Grow the table gradually from 1 KB at level 1 to 64 KB at level 7.
   * Smaller tables also bound the entropy probe, reducing low-level work on
   * the small alphabets produced by filtered ramps. */
  uint8_t hashlog_[10] = {0, HASH_LOG - 6, HASH_LOG - 5, HASH_LOG - 4, HASH_LOG - 3,
                          HASH_LOG - 2, HASH_LOG - 1, HASH_LOG, HASH_LOG, HASH_LOG};
  uint8_t hashlog = hashlog_[clevel];

  // Experiments say that checking 1/4 of the buffer is enough to figure out approx cratio
  // UPDATE: new experiments with ERA5 datasets (float32) say that checking the whole buffer
  // is better (specially when combined with bitshuffle).
  // The loss in speed for checking the whole buffer is pretty negligible too.
  int maxlen = length;
  if (clevel < 2) {
    maxlen /= 8;
  }
  else if (clevel < 4) {
    maxlen /= 4;
  }
  else if (clevel < 7) {
    maxlen /= 2;
  }
  // Start probing somewhere inside the buffer
  int shift = length - maxlen;
  // Actual entropy probing!
  uint32_t base = htab_reset(htab, hashlog, state, maxlen, tagged);
  double cratio = get_cratio(ibase + shift, maxlen, minlen, ipshift, htab, hashlog, base, tagged);
  if (state != NULL) {
    /* Tagged lookups cost a bit per hash probe; they only pay off when skipping
     * the (large) table clear matters, i.e. on very compressible data. */
    state->tagged = clevel >= 4 && cratio >= 8.0;
  }
  // discard probes with small compression ratios (too expensive)
  double cratio_[10] = {0, 2, 1.5, 1.2, 1.2, 1.2, 1.2, 1.15, 1.1, 1.0};
  if (cratio < cratio_[clevel]) {
      goto out;
  }

  uint8_t* ip = ibase;
  uint8_t* ip_bound = ibase + length - 1;
  uint8_t* ip_limit = ibase + length - 12;
  uint8_t* op = (uint8_t*)output;
  const uint8_t* op_limit = op + maxout;
  uint32_t seq;
  uint8_t copy;
  uint32_t hval;

  /* input and output buffer cannot be less than 16 and 66 bytes or we can get into trouble */
  if (length < 16 || maxout < 66) {
    return 0;
  }

  // Initialize the hash table
  base = htab_reset(htab, hashlog, state, length, tagged);

  /* we start with literal copy */
  copy = 4;
  *op++ = MAX_COPY - 1;
  *op++ = *ip++;
  *op++ = *ip++;
  *op++ = *ip++;
  *op++ = *ip++;

  /* main loop */
  while (BLOSCLZ_LIKELY(ip < ip_limit)) {
    const uint8_t* ref;
    unsigned distance;
    uint8_t* anchor = ip;    /* comparison starting-point */

    /* find potential match */
    seq = BLOSCLZ_READU32(ip);
    HASH_FUNCTION(hval, seq, hashlog)
    ref = ibase + HTAB_GET(htab[hval], base);
    /* With no pending literals, try the position following the previous
     * occurrence of ip[-1..2].  Its alignment can break a chain of short
     * matches on periodic data even when the ordinary candidate also matches.
     * Read the old entry before updating it, then reload the ordinary candidate:
     * the two hashes can share a slot. */
    if (copy == 0 && ip > ibase + 4 && ip_bound - ip >= 8) {
      uint32_t prevhash;
      HASH_FUNCTION(prevhash, BLOSCLZ_READU32(ip - 1), hashlog)
      const uint8_t *candidate = ibase + HTAB_GET(htab[prevhash], base) + 1;
      htab[prevhash] = (uint32_t)(ip - 1 - ibase) + base;
      ref = ibase + HTAB_GET(htab[hval], base);
      if (candidate < ip && ip - candidate < MAX_FARDISTANCE &&
          memcmp(candidate, ip, 8) == 0) {
        ref = candidate;
      }
    }

    /* calculate distance to the match */
    distance = (unsigned int)(anchor - ref);

    /* update hash table */
    htab[hval] = (uint32_t) (anchor - ibase) + base;

    if (distance == 0 || (distance >= MAX_FARDISTANCE)) {
      LITERAL(ip, op, op_limit, anchor, copy)
      continue;
    }

    /* is this a match? check the first 4 bytes */
    if (BLOSCLZ_UNLIKELY(BLOSCLZ_READU32(ref) == BLOSCLZ_READU32(ip))) {
      ref += 4;
    } else {
      /* no luck, copy as a literal */
      LITERAL(ip, op, op_limit, anchor, copy)
      continue;
    }

    /* last matched byte */
    ip = anchor + 4;

    /* distance is biased */
    distance--;

    /* get runs or matches; zero distance means a run */
    ip = get_run_or_match(ip, ip_bound, ref, !distance);

    /* length is biased, '1' means a match of 3 bytes */
    ip -= ipshift;

    unsigned len = (int)(ip - anchor);

    // Encoding short lengths is expensive during decompression
    if (len < minlen || (len <= 5 && distance >= MAX_DISTANCE)) {
      LITERAL(ip, op, op_limit, anchor, copy)
      continue;
    }

    /* if we have copied something, adjust the copy count */
    if (copy)
      /* copy is biased, '0' means 1 byte copy */
      *(op - copy - 1) = (uint8_t)(copy - 1);
    else
      /* back, to overwrite the copy count */
      op--;
    /* reset literal counter */
    copy = 0;

    /* encode the match */
    if (distance < MAX_DISTANCE) {
      if (len < 7) {
        MATCH_SHORT(op, op_limit, len, distance)
      } else {
        MATCH_LONG(op, op_limit, len, distance)
      }
    } else {
      /* far away, but not yet in the another galaxy... */
      distance -= MAX_DISTANCE;
      if (len < 7) {
        MATCH_SHORT_FAR(op, op_limit, len, distance)
      } else {
        MATCH_LONG_FAR(op, op_limit, len, distance)
      }
    }

    /* Advance past the biased match length.  Update the boundary hashes on
     * the next iteration, preserving the old ip-1 entry for the extra candidate. */
    ip += 2;

    if (BLOSCLZ_UNLIKELY(op_limit - op < 1))
      goto out;

    /* assuming literal copy */
    *op++ = MAX_COPY - 1;
  }

  /* left-over as literal copy */
  while (BLOSCLZ_UNLIKELY(ip <= ip_bound)) {
    if (BLOSCLZ_UNLIKELY(op_limit - op < 2)) goto out;
    *op++ = *ip++;
    copy++;
    if (BLOSCLZ_UNLIKELY(copy == MAX_COPY)) {
      copy = 0;
      *op++ = MAX_COPY - 1;
    }
  }

  /* if we have copied something, adjust the copy length */
  if (copy)
    *(op - copy - 1) = (uint8_t)(copy - 1);
  else
    op--;

  /* marker for blosclz */
  *(uint8_t*)output |= (1U << 5U);

  return (int)(op - (uint8_t*)output);

  out:
  return 0;
}

#if defined(BLOSCLZ_BUILD_AVX2)

#if defined(__AVX2__)
int blosclz_compress_avx2(const int clevel, const void* input, int length,
                          void* output, int maxout, blosc2_context* ctx) {
  BLOSC_UNUSED_PARAM(ctx);
  uint32_t htab[1U << HASH_LOG];
  return compress_impl(clevel, input, length, output, maxout, htab, NULL, 0);
}

int blosclz_compress_state_avx2(const int clevel, const void* input, int length,
                                void* output, int maxout, blosclz_state* state) {
  if (state->tagged) {
    return compress_impl(clevel, input, length, output, maxout, state->htab, state, 1);
  }
  return compress_impl(clevel, input, length, output, maxout, state->htab, state, 0);
}

/* Check whether [ip, ip_bound) is a constant-byte stream (128 B/iter AVX2) */
bool blosc_get_run_avx2(const uint8_t* ip, const uint8_t* ip_bound) {
  uint8_t x = *ip;
  /* Broadcast the reference byte across all 32 lanes */
  const __m256i vx = _mm256_set1_epi8((char)x);
  while (ip_bound - ip >= 128) {
    const __m256i* p = (const __m256i*)ip;
    __m256i eq01 = _mm256_and_si256(_mm256_cmpeq_epi8(_mm256_loadu_si256(p), vx),
                                    _mm256_cmpeq_epi8(_mm256_loadu_si256(p + 1), vx));
    __m256i eq23 = _mm256_and_si256(_mm256_cmpeq_epi8(_mm256_loadu_si256(p + 2), vx),
                                    _mm256_cmpeq_epi8(_mm256_loadu_si256(p + 3), vx));
    if ((uint32_t)_mm256_movemask_epi8(_mm256_and_si256(eq01, eq23)) != 0xFFFFFFFFU) {
      return false;
    }
    ip += 128;
  }
  /* Check remaining 32-byte vectors and scalar tail */
  while (ip_bound - ip >= 32) {
    if ((uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_loadu_si256((const __m256i*)ip), vx)) != 0xFFFFFFFFU) {
      return false;
    }
    ip += 32;
  }
  while (ip < ip_bound) {
    if (*ip != x) {
      return false;
    }
    ip++;
  }
  return true;
}
#endif /* defined(__AVX2__) */

#else /* !defined(BLOSCLZ_BUILD_AVX2) */

int blosclz_compress(const int clevel, const void* input, int length,
                     void* output, int maxout, blosc2_context* ctx) {
#if defined(BLOSCLZ_AVX2_ENABLED) && !defined(__AVX2__)
  if (blosc_cpu_has_avx2()) {
    return blosclz_compress_avx2(clevel, input, length, output, maxout, ctx);
  }
#endif
  BLOSC_UNUSED_PARAM(ctx);
  uint32_t htab[1U << HASH_LOG];
  return compress_impl(clevel, input, length, output, maxout, htab, NULL, 0);
}

int blosclz_compress_state(const int clevel, const void* input, int length,
                           void* output, int maxout, blosclz_state* state) {
#if defined(BLOSCLZ_AVX2_ENABLED) && !defined(__AVX2__)
  if (blosc_cpu_has_avx2()) {
    return blosclz_compress_state_avx2(clevel, input, length, output, maxout, state);
  }
#endif
  if (state->tagged) {
    return compress_impl(clevel, input, length, output, maxout, state->htab, state, 1);
  }
  return compress_impl(clevel, input, length, output, maxout, state->htab, state, 0);
}

// See https://habr.com/en/company/yandex/blog/457612/
#if defined(__AVX2__)

#if defined(_MSC_VER)
#define ALIGNED_(x) __declspec(align(x))
#else
#if defined(__GNUC__)
#define ALIGNED_(x) __attribute__ ((aligned(x)))
#endif
#endif
#define ALIGNED_TYPE_(t, x) t ALIGNED_(x)

static unsigned char* copy_match_16(unsigned char *op, const unsigned char *match, int32_t len)
{
  size_t offset = op - match;
  while (len >= 16) {

    static const ALIGNED_TYPE_(uint8_t, 16) masks[] =
      {
                0,  1,  2,  1,  4,  1,  4,  2,  8,  7,  6,  5,  4,  3,  2,  1, // offset = 0, not used as mask, but for shift
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // offset = 1
                0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,
                0,  1,  2,  0,  1,  2,  0,  1,  2,  0,  1,  2,  0,  1,  2,  0,
                0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,
                0,  1,  2,  3,  4,  0,  1,  2,  3,  4,  0,  1,  2,  3,  4,  0,
                0,  1,  2,  3,  4,  5,  0,  1,  2,  3,  4,  5,  0,  1,  2,  3,
                0,  1,  2,  3,  4,  5,  6,  0,  1,  2,  3,  4,  5,  6,  0,  1,
                0,  1,  2,  3,  4,  5,  6,  7,  0,  1,  2,  3,  4,  5,  6,  7,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  0,  1,  2,  3,  4,  5,  6,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  1,  2,  3,  4,  5,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,  0,  1,  2,  3,  4,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11,  0,  1,  2,  3,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12,  0,  1,  2,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13,  0,  1,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,  0,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,  15, // offset = 16
      };

    _mm_storeu_si128((__m128i *)(op),
                     _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *)(match)),
                                      _mm_load_si128((const __m128i *)(masks) + offset)));

    match += masks[offset];

    op += 16;
    len -= 16;
  }
  // Deal with remainders
  for (; len > 0; len--) {
    *op++ = *match++;
  }
  return op;
}
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
/* ARM64 NEON match copier: uses 64B/32B/16B vector copies with exact
 * overlapping 16B tails (so end-of-stream matches need no slack and do not
 * fall back to scalar copy_match), inline NEON short RLE fills, and 16B
 * doubling for 8..15-byte overlap distances. */
BLOSCLZ_ALWAYS_INLINE uint8_t *blosclz_copy_match_neon(
    uint8_t *op, const uint8_t *ref, int32_t len, uint8_t *op_limit) {
  ptrdiff_t dist = op - ref;
  uint8_t *end = op + len;

  if (dist >= 16) {
    if (len <= 16 && op_limit - op >= 16) {
      vst1q_u8(op, vld1q_u8(ref));
      return end;
    }
    if (dist >= 32) {
      if (len <= 32 && op_limit - op >= 32) {
        uint8x16_t v0 = vld1q_u8(ref);
        uint8x16_t v1 = vld1q_u8(ref + 16);
        vst1q_u8(op, v0);
        vst1q_u8(op + 16, v1);
        return end;
      }
      if (len >= 16) {
        uint8_t *cur = op;
        const uint8_t *src = ref;
        while (cur + 64 <= end) {
          uint8x16_t v0 = vld1q_u8(src);
          uint8x16_t v1 = vld1q_u8(src + 16);
          vst1q_u8(cur, v0);
          vst1q_u8(cur + 16, v1);
          uint8x16_t v2 = vld1q_u8(src + 32);
          uint8x16_t v3 = vld1q_u8(src + 48);
          vst1q_u8(cur + 32, v2);
          vst1q_u8(cur + 48, v3);
          cur += 64;
          src += 64;
        }
        if (cur + 32 <= end) {
          uint8x16_t v0 = vld1q_u8(src);
          uint8x16_t v1 = vld1q_u8(src + 16);
          vst1q_u8(cur, v0);
          vst1q_u8(cur + 16, v1);
          cur += 32;
          src += 32;
        }
        if (cur + 16 <= end) {
          vst1q_u8(cur, vld1q_u8(src));
          cur += 16;
        }
        if (cur < end) {
          vst1q_u8(end - 16, vld1q_u8(ref + len - 16));
        }
        return end;
      }
      return copy_match(op, ref, (unsigned)len);
    }
    /* 16 <= dist < 32 */
    if (len >= 16) {
      uint8_t *cur = op;
      const uint8_t *src = ref;
      while (cur + 32 <= end) {
        vst1q_u8(cur, vld1q_u8(src));
        vst1q_u8(cur + 16, vld1q_u8(src + 16));
        cur += 32;
        src += 32;
      }
      if (cur + 16 <= end) {
        vst1q_u8(cur, vld1q_u8(src));
        cur += 16;
      }
      if (cur < end) {
        vst1q_u8(end - 16, vld1q_u8(ref + len - 16));
      }
      return end;
    }
    return copy_match(op, ref, (unsigned)len);
  }

  if (dist == 1) {
    if (len <= 32 && op_limit - op >= 32) {
      uint8x16_t v = vdupq_n_u8(*ref);
      vst1q_u8(op, v);
      if (len > 16) {
        vst1q_u8(op + 16, v);
      }
      return end;
    }
    memset(op, *ref, (size_t)len);
    return end;
  }

  if (dist >= 8) {
    if (len <= dist && op_limit - op >= 16) {
      vst1q_u8(op, vld1q_u8(ref));
      return end;
    }
    if (len <= 16 && op_limit - op >= 16) {
      memcpy(op, ref, 8);
      memcpy(op + 8, ref + 8, 8);
      return end;
    }
    if (len > 16) {
      /* Seed first dist bytes so wide_dist = 2 * dist >= 16. */
      vst1q_u8(op, vld1q_u8(ref));
      uint8_t *cur = op + dist;
      const uint8_t *src = ref;
      ptrdiff_t wide_dist = dist * 2;
      while (cur + 16 <= end) {
        vst1q_u8(cur, vld1q_u8(src));
        cur += 16;
        src += 16;
      }
      if (cur < end) {
        if (cur > op + dist) {
          vst1q_u8(end - 16, vld1q_u8(end - 16 - wide_dist));
        }
        else {
          while (cur < end) {
            *cur++ = *src++;
          }
        }
      }
      return end;
    }
    return copy_match(op, ref, (unsigned)len);
  }

  return copy_match(op, ref, (unsigned)len);
}
#endif

#if !defined(__aarch64__) && !defined(_M_ARM64)
// LZ4 wildCopy which can reach excellent copy bandwidth (even if insecure)
static inline void wild_copy(uint8_t *out, const uint8_t* from, uint8_t* end) {
  uint8_t* d = out;
  const uint8_t* s = from;
  uint8_t* const e = end;

  do { memcpy(d,s,8); d+=8; s+=8; } while (d<e);
}
#endif

#if defined(__aarch64__) || defined(_M_ARM64) || defined(__x86_64__) || defined(_M_X64)
#define BLOSCLZ_WIDE_DECODE
#endif

#if defined(BLOSCLZ_WIDE_DECODE) && !defined(__aarch64__) && !defined(_M_ARM64)
/* The caller guarantees a distance and output slack of at least 16 bytes.
 * Keep each load/store ordered so overlapping matches propagate correctly. */
static inline void wild_copy_16(uint8_t *out, const uint8_t *from, uint8_t *end) {
  do { memcpy(out, from, 16); out += 16; from += 16; } while (out < end);
}
#endif

int blosclz_decompress(const void* input, int length, void* output, int maxout) {
  const uint8_t* ip = (const uint8_t*)input;
  const uint8_t* ip_limit = ip + length;
  uint8_t* op = (uint8_t*)output;
  uint32_t ctrl;
  uint8_t* op_limit = op + maxout;
  if (BLOSCLZ_UNLIKELY(length == 0)) {
    return 0;
  }
  ctrl = (*ip++) & 31U;

  while (1) {
    if (ctrl >= 32) {
      // match
      int32_t len = (int32_t)(ctrl >> 5U) - 1 ;
      int32_t ofs = (int32_t)(ctrl & 31U) << 8U;
      uint8_t code;
      const uint8_t* ref = op - ofs;

      if (len == 7 - 1) {
#ifdef BLOSCLZ_WIDE_DECODE
        /* Long runs encode their lengths as consecutive 255-byte extensions.
         * Consume eight together, keeping an offset byte in reserve. */
        while (ip_limit - ip >= 9) {
          uint64_t extensions;
          memcpy(&extensions, ip, sizeof(extensions));
          if (extensions != UINT64_MAX) {
            break;
          }
          if (BLOSCLZ_UNLIKELY(len > maxout - 8 * 255)) {
            return 0;
          }
          len += 8 * 255;
          ip += 8;
        }
#endif
        do {
          if (BLOSCLZ_UNLIKELY(ip + 1 >= ip_limit)) {
            return 0;
          }
          code = *ip++;
          /* Guard before the addition so that `len` can never wrap past
             INT32_MAX.  The subsequent `len += 3` adds at most 3 more,
             so capping at maxout here is safe and sufficient. */
          if (BLOSCLZ_UNLIKELY(len > maxout - code)) {
            return 0;
          }
          len += code;
        } while (code == 255);
      }
      else {
        if (BLOSCLZ_UNLIKELY(ip + 1 >= ip_limit)) {
          return 0;
        }
      }
      code = *ip++;
      len += 3;
      ref -= code;

      /* match from 16-bit distance */
      if (BLOSCLZ_UNLIKELY(code == 255)) {
        if (ofs == (31U << 8U)) {
          if (ip + 1 >= ip_limit) {
            return 0;
          }
          ofs = (*ip++) << 8U;
          ofs += *ip++;
          ref = op - ofs - MAX_DISTANCE;
        }
      }

      if (BLOSCLZ_UNLIKELY(len > op_limit - op)) {
        return 0;
      }

      if (BLOSCLZ_UNLIKELY(ref <= (uint8_t*)output)) {
        return 0;
      }

      if (BLOSCLZ_UNLIKELY(ip >= ip_limit)) break;
      ctrl = *ip++;

      ref--;
#if defined(__aarch64__) || defined(_M_ARM64)
      op = blosclz_copy_match_neon(op, ref, len, op_limit);
#else
      if (ref == op - 1) {
        /* optimized copy for a run */
        memset(op, *ref, len);
        op += len;
      }
#ifdef BLOSCLZ_WIDE_DECODE
      else if ((op - ref >= 16) && (op_limit - op - len >= 16)) {
        wild_copy_16(op, ref, op + len);
        op += len;
      }
#endif
      else if ((op - ref >= 8) && (op_limit - op - len >= 8)) {
        // copy with an overlap not larger than 8
        wild_copy(op, ref, op + len);
        op += len;
      }
      else {
        // general copy with any overlap
#if 0 && defined(__AVX2__)
        if (op - ref <= 16) {
          // This is not faster on a combination of compilers (clang, gcc, icc) or machines, but
          // it is not too slower either.
          op = copy_match_16(op, ref, len);
        }
        else {
#endif
          op = copy_match(op, ref, (unsigned) len);
#if 0 && defined(__AVX2__)
        }
#endif
      }
#endif
    }
    else {
      // literal
      ctrl++;
      if (BLOSCLZ_UNLIKELY(ctrl > op_limit - op)) {
        return 0;
      }
      if (BLOSCLZ_UNLIKELY(ctrl > ip_limit - ip)) {
        return 0;
      }

#ifdef BLOSCLZ_WIDE_DECODE
      /* Fixed-size copies can inline.  Overcopy only within both buffers;
       * subsequent tokens overwrite the extra output bytes. */
      if (ctrl <= 16 && op_limit - op >= 16 && ip_limit - ip >= 16) {
        memcpy(op, ip, 16);
      }
      else if (op_limit - op >= 32 && ip_limit - ip >= 32) {
        memcpy(op, ip, 32);
      }
      else
#endif
      {
        memcpy(op, ip, ctrl);
      }
      op += ctrl; ip += ctrl;
      // On GCC-6, fastcopy this is still faster than plain memcpy
      // However, using recent CLANG/LLVM 9.0, there is almost no difference
      // in performance.
      // And starting on CLANG/LLVM 10 and GCC 9, memcpy is generally faster.
      // op = fastcopy(op, ip, (unsigned) ctrl); ip += ctrl;

      if (BLOSCLZ_UNLIKELY(ip >= ip_limit)) break;
      ctrl = *ip++;
    }
  }

  return (int)(op - (uint8_t*)output);
}

#endif /* !defined(BLOSCLZ_BUILD_AVX2) */
