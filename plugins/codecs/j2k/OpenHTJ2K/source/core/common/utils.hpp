// Copyright (c) 2019 - 2021, Osamu Watanabe
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
//    modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <cstdint>
#include <cstdlib>

#define round_up(x, n) (((x) + (n)-1) & (-n))
//#define round_down(x, n) ((x) & (-n))
#define round_down(x, n) ((x) - ((x) % (n)))
#define ceil_int(a, b) ((a) + ((b)-1)) / (b)

#if defined(__INTEL_LLVM_COMPILER)
  #define __INTEL_COMPILER
#endif

#if defined(OPENHTJ2K_ENABLE_ARM_NEON)
  //#include <arm_acle.h>
  #include <arm_neon.h>
#elif defined(_MSC_VER) || defined(__MINGW64__)
  #include <intrin.h>
#elif defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
  #include <x86intrin.h>
#endif

#ifndef __clang__
  #ifndef __INTEL_COMPILER
    #if defined(__GNUC__) && (__GNUC__ < 10)
static inline void _mm256_storeu2_m128i(__m128i_u* __addr_hi, __m128i_u* __addr_lo, __m256i __a) {
  __m128i __v128;

  __v128 = _mm256_castsi256_si128(__a);
  _mm_storeu_si128(__addr_lo, __v128);
  __v128 = _mm256_extractf128_si256(__a, 1);
  _mm_storeu_si128(__addr_hi, __v128);
}
    #endif
  #endif
#endif

template <class T>
static inline T find_max(T x0, T x1, T x2, T x3) {
  T v0 = ((x0 > x1) ? x0 : x1);
  T v1 = ((x2 > x3) ? x2 : x3);
  return (v0 > v1) ? v0 : v1;
}

static inline size_t popcount32(uint32_t num) {
  size_t precision = 0;
#if defined(_MSC_VER)
  precision = __popcnt(static_cast<uint32_t>(num));
#elif defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
  precision = static_cast<size_t>(_popcnt32(num));
#elif defined(OPENHTJ2K_ENABLE_ARM_NEON)
  uint32x2_t val = vld1_dup_u32(static_cast<const uint32_t*>(&num));
  uint8_t a      = vaddv_u8(vcnt_u8(vreinterpret_u8_u32(val)));
  precision      = a >> 1;
#else
  while (num != 0) {
    if (1 == (num & 1)) {
      precision++;
    }
    num >>= 1;
  }
#endif
  return precision;
}

static inline uint32_t int_log2(const uint32_t x) {
  uint32_t y;
#if defined(_MSC_VER)
  unsigned long tmp;
  _BitScanReverse(&tmp, x);
  y = tmp;
#else
  y         = static_cast<uint32_t>(31 - __builtin_clz(x));
#endif
  return (x == 0) ? 0 : y;
}

static inline uint32_t count_leading_zeros(const uint32_t x) {
  uint32_t y;
#if defined(_MSC_VER)
  y = __lzcnt(x);
#elif defined(OPENHTJ2K_TRY_AVX2) && defined(__AVX2__) && defined(_MSC_VER) && defined(_WIN32)
  y         = _lzcnt_u32(x);
#elif defined(OPENHTJ2K_TRY_AVX2) && defined(__AVX2__) && defined(__MINGW32__)
  y              = __builtin_ia32_lzcnt_u32(x);
#elif defined(__MINGW32__) || defined(__MINGW64__)
  y = __builtin_clz(x);
#elif defined(OPENHTJ2K_ENABLE_ARM_NEON)
  y = static_cast<uint32_t>(__builtin_clz(x));
#else
  y = 31 - int_log2(x);
#endif
  return (x == 0) ? 32 : y;
}

static inline void* aligned_mem_alloc(size_t size, size_t align) {
  void* result;
#if defined(__INTEL_COMPILER)
  result = _mm_malloc(size, align);
#elif defined(_MSC_VER)
  result    = _aligned_malloc(size, align);
#elif defined(__MINGW32__) || defined(__MINGW64__)
  result         = __mingw_aligned_malloc(size, align);
#else
  if (posix_memalign(&result, align, size)) {
    result = nullptr;
  }
#endif
  return result;
}

static inline void aligned_mem_free(void* ptr) {
#if defined(__INTEL_COMPILER)
  _mm_free(ptr);
#elif defined(_MSC_VER)
  _aligned_free(ptr);
#elif defined(__MINGW32__) || defined(__MINGW64__)
  __mingw_aligned_free(ptr);
#else
  free(ptr);
#endif
}
#if ((defined(_MSVC_LANG) && _MSVC_LANG > 201103L) || __cplusplus > 201103L)
  #define MAKE_UNIQUE std::make_unique
#else
  #define MAKE_UNIQUE open_htj2k::make_unique
  #define[[maybe_unsed]] __attribute__((__unused__))
#endif

#if ((defined(_MSVC_LANG) && _MSVC_LANG <= 201103L) || __cplusplus <= 201103L)
  #include <cstddef>
  #include <memory>
  #include <type_traits>
  #include <utility>
namespace open_htj2k {
template <class T>
struct _Unique_if {
  typedef std::unique_ptr<T> _Single_object;
};

template <class T>
struct _Unique_if<T[]> {
  typedef std::unique_ptr<T[]> _Unknown_bound;
};

template <class T, size_t N>
struct _Unique_if<T[N]> {
  typedef void _Known_bound;
};

template <class T, class... Args>
typename _Unique_if<T>::_Single_object make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template <class T>
typename _Unique_if<T>::_Unknown_bound make_unique(size_t n) {
  typedef typename std::remove_extent<T>::type U;
  return std::unique_ptr<T>(new U[n]());
}

template <class T, class... Args>
typename _Unique_if<T>::_Known_bound make_unique(Args&&...) = delete;
}  // namespace open_htj2k
#endif
