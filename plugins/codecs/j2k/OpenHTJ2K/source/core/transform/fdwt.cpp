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

#include <cstring>
#include "dwt.hpp"
#include "utils.hpp"
#if defined(OPENHTJ2K_ENABLE_ARM_NEON)
static fdwt_1d_filtr_func_fixed fdwt_1d_filtr_fixed[2] = {fdwt_1d_filtr_irrev97_fixed_neon,
                                                          fdwt_1d_filtr_rev53_fixed_neon};
static fdwt_ver_filtr_func_fixed fdwt_ver_sr_fixed[2]  = {fdwt_irrev_ver_sr_fixed_neon,
                                                          fdwt_rev_ver_sr_fixed_neon};
#elif defined(OPENHTJ2K_ENABLE_AVX2)
static fdwt_1d_filtr_func_fixed fdwt_1d_filtr_fixed[2] = {fdwt_1d_filtr_irrev97_fixed_avx2,
                                                          fdwt_1d_filtr_rev53_fixed_avx2};
static fdwt_ver_filtr_func_fixed fdwt_ver_sr_fixed[2]  = {fdwt_irrev_ver_sr_fixed_avx2,
                                                          fdwt_rev_ver_sr_fixed_avx2};
#else
static fdwt_1d_filtr_func_fixed fdwt_1d_filtr_fixed[2] = {fdwt_1d_filtr_irrev97_fixed,
                                                          fdwt_1d_filtr_rev53_fixed};
static fdwt_ver_filtr_func_fixed fdwt_ver_sr_fixed[2]  = {fdwt_irrev_ver_sr_fixed, fdwt_rev_ver_sr_fixed};
#endif
// irreversible FDWT
void fdwt_1d_filtr_irrev97_fixed(sprec_t *X, const int32_t left, const int32_t u_i0, const int32_t u_i1) {
  const auto i0       = static_cast<int32_t>(u_i0);
  const auto i1       = static_cast<int32_t>(u_i1);
  const int32_t start = ceil_int(i0, 2);
  const int32_t stop  = ceil_int(i1, 2);

  const int32_t offset = left + i0 % 2;
  for (int32_t n = -4 + offset, i = start - 2; i < stop + 1; i++, n += 2) {
    int32_t sum = X[n];
    sum += X[n + 2];
    X[n + 1] = static_cast<sprec_t>(X[n + 1] + ((Acoeff * sum + Aoffset) >> Ashift));
  }
  for (int32_t n = -2 + offset, i = start - 1; i < stop + 1; i++, n += 2) {
    int32_t sum = X[n - 1];
    sum += X[n + 1];
    X[n] = static_cast<sprec_t>(X[n] + ((Bcoeff * sum + Boffset) >> Bshift));
  }
  for (int32_t n = -2 + offset, i = start - 1; i < stop; i++, n += 2) {
    int32_t sum = X[n];
    sum += X[n + 2];
    X[n + 1] = static_cast<sprec_t>(X[n + 1] + ((Ccoeff * sum + Coffset) >> Cshift));
  }
  for (int32_t n = 0 + offset, i = start; i < stop; i++, n += 2) {
    int32_t sum = X[n - 1];
    sum += X[n + 1];
    X[n] = static_cast<sprec_t>(X[n] + ((Dcoeff * sum + Doffset) >> Dshift));
  }
};

// reversible FDWT
void fdwt_1d_filtr_rev53_fixed(sprec_t *X, const int32_t left, const int32_t u_i0, const int32_t u_i1) {
  const auto i0       = static_cast<int32_t>(u_i0);
  const auto i1       = static_cast<int32_t>(u_i1);
  const int32_t start = ceil_int(i0, 2);
  const int32_t stop  = ceil_int(i1, 2);
  // X += left - i0 % 2;
  const int32_t offset = left + i0 % 2;
  for (int32_t n = -2 + offset, i = start - 1; i < stop; ++i, n += 2) {
    int32_t sum = X[n];
    sum += X[n + 2];
    X[n + 1] = static_cast<sprec_t>(X[n + 1] - (sum >> 1));
  }
  for (int32_t n = 0 + offset, i = start; i < stop; ++i, n += 2) {
    int32_t sum = X[n - 1];
    sum += X[n + 1];
    X[n] = static_cast<sprec_t>(X[n] + ((sum + 2) >> 2));
  }
};

// 1-dimensional FDWT
static inline void fdwt_1d_sr_fixed(sprec_t *buf, sprec_t *in, const int32_t left, const int32_t right,
                                    const int32_t i0, const int32_t i1, const uint8_t transformation) {
  //  const uint32_t len = round_up(i1 - i0 + left + right, SIMD_PADDING);
  //  auto *Xext         = static_cast<int16_t *>(aligned_mem_alloc(sizeof(int16_t) * len, 32));
  dwt_1d_extr_fixed(buf, in, left, right, i0, i1);
  fdwt_1d_filtr_fixed[transformation](buf, left, i0, i1);
  memcpy(in, buf + left, sizeof(sprec_t) * (static_cast<size_t>(i1 - i0)));
  //  aligned_mem_free(Xext);
}

// FDWT for horizontal direction
static void fdwt_hor_sr_fixed(sprec_t *in, const int32_t u0, const int32_t u1, const int32_t v0,
                              const int32_t v1, const uint8_t transformation) {
  const int32_t stride               = u1 - u0;
  constexpr int32_t num_pse_i0[2][2] = {{4, 2}, {3, 1}};
  constexpr int32_t num_pse_i1[2][2] = {{3, 1}, {4, 2}};
  const int32_t left                 = num_pse_i0[u0 % 2][transformation];
  const int32_t right                = num_pse_i1[u1 % 2][transformation];

  if (u0 == u1 - 1) {
    // one sample case
    for (int32_t row = 0; row < v1 - v0; ++row) {
      if (u0 % 2 == 0) {
        in[row] = (transformation) ? in[row] : in[row];
      } else {
        in[row] = (transformation) ? static_cast<sprec_t>(in[row] << 1) : in[row];
      }
    }
  } else {
    // need to perform symmetric extension
    const int32_t len = u1 - u0 + left + right;
    auto *Xext        = static_cast<sprec_t *>(aligned_mem_alloc(
               sizeof(sprec_t) * static_cast<size_t>(round_up(len + SIMD_PADDING, SIMD_PADDING)), 32));
    //#pragma omp parallel for
    for (int32_t row = 0; row < v1 - v0; ++row) {
      fdwt_1d_sr_fixed(Xext, in, left, right, u0, u1, transformation);
      in += stride;
    }
    aligned_mem_free(Xext);
  }
}

void fdwt_irrev_ver_sr_fixed(sprec_t *in, const int32_t u0, const int32_t u1, const int32_t v0,
                             const int32_t v1) {
  const int32_t stride            = u1 - u0;
  constexpr int32_t num_pse_i0[2] = {4, 3};
  constexpr int32_t num_pse_i1[2] = {3, 4};
  const int32_t top               = num_pse_i0[v0 % 2];
  const int32_t bottom            = num_pse_i1[v1 % 2];

  if (v0 == v1 - 1) {
    // one sample case
    for (int32_t col = 0; col < u1 - u0; ++col) {
      if (v0 % 2) {
        in[col] <<= 0;
      }
    }
  } else {
    const int32_t len = round_up(stride, SIMD_LEN_I32);
    auto **buf        = new sprec_t *[static_cast<size_t>(top + v1 - v0 + bottom)];
    for (int32_t i = 1; i <= top; ++i) {
      buf[top - i] =
          static_cast<sprec_t *>(aligned_mem_alloc(sizeof(sprec_t) * static_cast<size_t>(len), 32));
      memcpy(buf[top - i], &in[PSEo(v0 - i, v0, v1) * stride],
             sizeof(sprec_t) * static_cast<size_t>(stride));
      // buf[top - i] = &in[(PSEo(v0 - i, v0, v1) - v0) * stride];
    }
    for (int32_t row = 0; row < v1 - v0; ++row) {
      buf[top + row] = &in[row * stride];
    }
    for (int32_t i = 1; i <= bottom; i++) {
      buf[top + (v1 - v0) + i - 1] =
          static_cast<sprec_t *>(aligned_mem_alloc(sizeof(sprec_t) * static_cast<size_t>(len), 32));
      memcpy(buf[top + (v1 - v0) + i - 1], &in[PSEo(v1 - v0 + i - 1 + v0, v0, v1) * stride],
             sizeof(sprec_t) * static_cast<size_t>(stride));
    }
    const int32_t start  = ceil_int(v0, 2);
    const int32_t stop   = ceil_int(v1, 2);
    const int32_t offset = top + v0 % 2;

    for (int32_t n = -4 + offset, i = start - 2; i < stop + 1; i++, n += 2) {
      for (int32_t col = 0; col < u1 - u0; ++col) {
        int32_t sum = buf[n][col];
        sum += buf[n + 2][col];
        buf[n + 1][col] = static_cast<sprec_t>(buf[n + 1][col] + ((Acoeff * sum + Aoffset) >> Ashift));
      }
    }
    for (int32_t n = -2 + offset, i = start - 1; i < stop + 1; i++, n += 2) {
      for (int32_t col = 0; col < u1 - u0; ++col) {
        int32_t sum = buf[n - 1][col];
        sum += buf[n + 1][col];
        buf[n][col] = static_cast<sprec_t>(buf[n][col] + ((Bcoeff * sum + Boffset) >> Bshift));
      }
    }
    for (int32_t n = -2 + offset, i = start - 1; i < stop; i++, n += 2) {
      for (int32_t col = 0; col < u1 - u0; ++col) {
        int32_t sum = buf[n][col];
        sum += buf[n + 2][col];
        buf[n + 1][col] = static_cast<sprec_t>(buf[n + 1][col] + ((Ccoeff * sum + Coffset) >> Cshift));
      }
    }
    for (int32_t n = 0 + offset, i = start; i < stop; i++, n += 2) {
      for (int32_t col = 0; col < u1 - u0; ++col) {
        int32_t sum = buf[n - 1][col];
        sum += buf[n + 1][col];
        buf[n][col] = static_cast<sprec_t>(buf[n][col] + ((Dcoeff * sum + Doffset) >> Dshift));
      }
    }

    for (int32_t i = 1; i <= top; ++i) {
      aligned_mem_free(buf[top - i]);
    }
    for (int32_t i = 1; i <= bottom; i++) {
      aligned_mem_free(buf[top + (v1 - v0) + i - 1]);
    }
    delete[] buf;
  }
}

void fdwt_rev_ver_sr_fixed(sprec_t *in, const int32_t u0, const int32_t u1, const int32_t v0,
                           const int32_t v1) {
  const int32_t stride            = u1 - u0;
  constexpr int32_t num_pse_i0[2] = {2, 1};
  constexpr int32_t num_pse_i1[2] = {1, 2};
  const int32_t top               = num_pse_i0[v0 % 2];
  const int32_t bottom            = num_pse_i1[v1 % 2];

  if (v0 == v1 - 1) {
    // one sample case
    for (int32_t col = 0; col < u1 - u0; ++col) {
      if (v0 % 2) {
        in[col] = static_cast<sprec_t>(in[col] << 1);
      }
    }
  } else {
    const int32_t len = round_up(stride, SIMD_PADDING);
    auto **buf        = new sprec_t *[static_cast<size_t>(top + v1 - v0 + bottom)];
    for (int32_t i = 1; i <= top; ++i) {
      buf[top - i] =
          static_cast<sprec_t *>(aligned_mem_alloc(sizeof(sprec_t) * static_cast<size_t>(len), 32));
      memcpy(buf[top - i], &in[PSEo(v0 - i, v0, v1) * stride],
             sizeof(sprec_t) * static_cast<size_t>(stride));
      // buf[top - i] = &in[(PSEo(v0 - i, v0, v1) - v0) * stride];
    }
    for (int32_t row = 0; row < v1 - v0; ++row) {
      buf[top + row] = &in[row * stride];
    }
    for (int32_t i = 1; i <= bottom; i++) {
      buf[top + (v1 - v0) + i - 1] =
          static_cast<sprec_t *>(aligned_mem_alloc(sizeof(sprec_t) * static_cast<size_t>(len), 32));
      memcpy(buf[top + (v1 - v0) + i - 1], &in[PSEo(v1 - v0 + i - 1 + v0, v0, v1) * stride],
             sizeof(sprec_t) * static_cast<size_t>(stride));
    }
    const int32_t start  = ceil_int(v0, 2);
    const int32_t stop   = ceil_int(v1, 2);
    const int32_t offset = top + v0 % 2;

    for (int32_t n = -2 + offset, i = start - 1; i < stop; ++i, n += 2) {
      for (int32_t col = 0; col < u1 - u0; ++col) {
        int32_t sum = buf[n][col];
        sum += buf[n + 2][col];
        buf[n + 1][col] = static_cast<sprec_t>(buf[n + 1][col] - (sum >> 1));
      }
    }
    for (int32_t n = 0 + offset, i = start; i < stop; ++i, n += 2) {
      for (int32_t col = 0; col < u1 - u0; ++col) {
        int32_t sum = buf[n - 1][col];
        sum += buf[n + 1][col];
        buf[n][col] = static_cast<sprec_t>(buf[n][col] + ((sum + 2) >> 2));
      }
    }

    for (int32_t i = 1; i <= top; ++i) {
      aligned_mem_free(buf[top - i]);
    }
    for (int32_t i = 1; i <= bottom; i++) {
      aligned_mem_free(buf[top + (v1 - v0) + i - 1]);
    }
    delete[] buf;
  }
}

// Deinterleaving to devide coefficients into subbands
static void fdwt_2d_deinterleave_fixed(sprec_t *buf, sprec_t *const LL, sprec_t *const HL,
                                       sprec_t *const LH, sprec_t *const HH, const int32_t u0,
                                       const int32_t u1, const int32_t v0, const int32_t v1) {
  const int32_t stride     = u1 - u0;
  const int32_t v_offset   = v0 % 2;
  const int32_t u_offset   = u0 % 2;
  sprec_t *dp[4]           = {LL, HL, LH, HH};
  const int32_t vstart[4]  = {ceil_int(v0, 2), ceil_int(v0, 2), v0 / 2, v0 / 2};
  const int32_t vstop[4]   = {ceil_int(v1, 2), ceil_int(v1, 2), v1 / 2, v1 / 2};
  const int32_t ustart[4]  = {ceil_int(u0, 2), u0 / 2, ceil_int(u0, 2), u0 / 2};
  const int32_t ustop[4]   = {ceil_int(u1, 2), u1 / 2, ceil_int(u1, 2), u1 / 2};
  const int32_t voffset[4] = {v_offset, v_offset, 1 - v_offset, 1 - v_offset};
  const int32_t uoffset[4] = {u_offset, 1 - u_offset, u_offset, 1 - u_offset};

#if defined(OPENHTJ2K_ENABLE_ARM_NEON)
  if ((ustop[0] - ustart[0]) != (ustop[1] - ustart[1])) {
    for (uint8_t b = 0; b < 2; ++b) {
      for (int32_t v = 0, vb = vstart[b]; vb < vstop[b]; ++vb, ++v) {
        for (int32_t u = 0, ub = ustart[b]; ub < ustop[b]; ++ub, ++u) {
          *(dp[b]++) = buf[2 * u + uoffset[b] + (2 * v + voffset[b]) * stride];
        }
      }
    }
  } else {
    sprec_t *first, *second;
    first  = LL;
    second = HL;
    if (uoffset[0] > uoffset[1]) {
      first  = HL;
      second = LL;
    }
    for (int32_t v = 0, vb = vstart[0]; vb < vstop[0]; ++vb, ++v) {
      sprec_t *sp = buf + (2 * v + voffset[0]) * stride;
      size_t len  = static_cast<size_t>(ustop[0] - ustart[0]);
      for (; len >= 8; len -= 8) {
        __builtin_prefetch(sp, 0);
        __builtin_prefetch(first, 1);
        __builtin_prefetch(second, 1);
        auto vline = vld2q_s16(sp);
        vst1q_s16(first, vline.val[0]);
        vst1q_s16(second, vline.val[1]);
        first += 8;
        second += 8;
        sp += 16;
      }
      for (; len > 0; --len) {
        *first++  = *sp++;
        *second++ = *sp++;
      }
    }
  }

  if ((ustop[2] - ustart[2]) != (ustop[3] - ustart[3])) {
    for (uint8_t b = 2; b < 4; ++b) {
      for (int32_t v = 0, vb = vstart[b]; vb < vstop[b]; ++vb, ++v) {
        for (int32_t u = 0, ub = ustart[b]; ub < ustop[b]; ++ub, ++u) {
          *(dp[b]++) = buf[2 * u + uoffset[b] + (2 * v + voffset[b]) * stride];
        }
      }
    }
  } else {
    sprec_t *first, *second;
    first  = LH;
    second = HH;
    if (uoffset[2] > uoffset[3]) {
      first  = HH;
      second = LH;
    }
    for (int32_t v = 0, vb = vstart[2]; vb < vstop[2]; ++vb, ++v) {
      sprec_t *sp = buf + (2 * v + voffset[2]) * stride;
      size_t len  = static_cast<size_t>(ustop[2] - ustart[2]);
      for (; len >= 8; len -= 8) {
        __builtin_prefetch(sp, 0);
        __builtin_prefetch(first, 1);
        __builtin_prefetch(second, 1);
        auto vline = vld2q_s16(sp);
        vst1q_s16(first, vline.val[0]);
        vst1q_s16(second, vline.val[1]);
        first += 8;
        second += 8;
        sp += 16;
      }
      for (; len > 0; --len) {
        *first++  = *sp++;
        *second++ = *sp++;
      }
    }
  }
#elif defined(OPENHTJ2K_TRY_AVX2) && defined(__AVX2__)
  if ((ustop[0] - ustart[0]) != (ustop[1] - ustart[1])) {
     for (uint8_t b = 0; b < 2; ++b) {
       for (int32_t v = 0, vb = vstart[b]; vb < vstop[b]; ++vb, ++v) {
         for (int32_t u = 0, ub = ustart[b]; ub < ustop[b]; ++ub, ++u) {
           *(dp[b]++) = buf[2 * u + uoffset[b] + (2 * v + voffset[b]) * stride];
        }
      }
    }
  } else {
     sprec_t *first, *second;
     first  = LL;
     second = HL;
     if (uoffset[0] > uoffset[1]) {
       first  = HL;
       second = LL;
    }
     const __m256i vshmask = _mm256_set_epi8(15, 14, 11, 10, 7, 6, 3, 2, 13, 12, 9, 8, 5, 4, 1, 0, 15, 14,
                                             11, 10, 7, 6, 3, 2, 13, 12, 9, 8, 5, 4, 1, 0);
     for (int32_t v = 0, vb = vstart[0]; vb < vstop[0]; ++vb, ++v) {
       sprec_t *sp = buf + (2 * v + voffset[0]) * stride;
       size_t len  = static_cast<size_t>(ustop[0] - ustart[0]);
       for (; len >= 8; len -= 8) {
         // SSE version
        // auto vline0 = _mm_loadu_si128((__m128i *)sp);
        // auto vline1 = _mm_loadu_si128((__m128i *)(sp + 8));
        // vline0      = _mm_shufflelo_epi16(vline0, _MM_SHUFFLE(3, 1, 2, 0));
        // vline0      = _mm_shufflehi_epi16(vline0, _MM_SHUFFLE(3, 1, 2, 0));
        // vline1      = _mm_shufflelo_epi16(vline1, _MM_SHUFFLE(3, 1, 2, 0));
        // vline1      = _mm_shufflehi_epi16(vline1, _MM_SHUFFLE(3, 1, 2, 0));
        // vline0      = _mm_shuffle_epi32(vline0, _MM_SHUFFLE(3, 1, 2, 0));  // A1 A2 A3 A4 B1 B2 B3 B4
        // vline1      = _mm_shuffle_epi32(vline1, _MM_SHUFFLE(3, 1, 2, 0));  // A5 A6 A7 A8 B5 B6 B7 B8
        // _mm_storeu_si128((__m128i *)first, _mm_unpacklo_epi64(vline0, vline1));
        // _mm_storeu_si128((__m128i *)second, _mm_unpackhi_epi64(vline0, vline1));

        __m256i vline = _mm256_loadu_si256((__m256i *)sp);
        vline         = _mm256_shuffle_epi8(vline, vshmask);
        vline         = _mm256_permute4x64_epi64(vline, 0xD8);
        _mm256_storeu2_m128i((__m128i *)second, (__m128i *)first, vline);
        first += 8;
        second += 8;
        sp += 16;
      }
      for (; len > 0; --len) {
         *first++  = *sp++;
         *second++ = *sp++;
      }
    }
  }

  if ((ustop[2] - ustart[2]) != (ustop[3] - ustart[3])) {
     for (uint8_t b = 2; b < 4; ++b) {
       for (int32_t v = 0, vb = vstart[b]; vb < vstop[b]; ++vb, ++v) {
         for (int32_t u = 0, ub = ustart[b]; ub < ustop[b]; ++ub, ++u) {
           *(dp[b]++) = buf[2 * u + uoffset[b] + (2 * v + voffset[b]) * stride];
        }
      }
    }
  } else {
     sprec_t *first, *second;
     first  = LH;
     second = HH;
     if (uoffset[2] > uoffset[3]) {
       first  = HH;
       second = LH;
    }
     const __m256i vshmask = _mm256_set_epi8(15, 14, 11, 10, 7, 6, 3, 2, 13, 12, 9, 8, 5, 4, 1, 0, 15, 14,
                                             11, 10, 7, 6, 3, 2, 13, 12, 9, 8, 5, 4, 1, 0);
     for (int32_t v = 0, vb = vstart[2]; vb < vstop[2]; ++vb, ++v) {
       sprec_t *sp = buf + (2 * v + voffset[2]) * stride;
       size_t len  = static_cast<size_t>(ustop[2] - ustart[2]);
       for (; len >= 8; len -= 8) {
         // SSE version
        // auto vline0 = _mm_loadu_si128((__m128i *)sp);
        // auto vline1 = _mm_loadu_si128((__m128i *)(sp + 8));
        // vline0      = _mm_shufflelo_epi16(vline0, _MM_SHUFFLE(3, 1, 2, 0));
        // vline0      = _mm_shufflehi_epi16(vline0, _MM_SHUFFLE(3, 1, 2, 0));
        // vline1      = _mm_shufflelo_epi16(vline1, _MM_SHUFFLE(3, 1, 2, 0));
        // vline1      = _mm_shufflehi_epi16(vline1, _MM_SHUFFLE(3, 1, 2, 0));
        // vline0      = _mm_shuffle_epi32(vline0, _MM_SHUFFLE(3, 1, 2, 0));  // A1 A2 A3 A4 B1 B2 B3 B4
        // vline1      = _mm_shuffle_epi32(vline1, _MM_SHUFFLE(3, 1, 2, 0));  // A5 A6 A7 A8 B5 B6 B7 B8
        // _mm_storeu_si128((__m128i *)first, _mm_unpacklo_epi64(vline0, vline1));
        // _mm_storeu_si128((__m128i *)second, _mm_unpackhi_epi64(vline0, vline1));

        __m256i vline = _mm256_loadu_si256((__m256i *)sp);
        vline         = _mm256_shuffle_epi8(vline, vshmask);
        vline         = _mm256_permute4x64_epi64(vline, 0xD8);
        _mm256_storeu2_m128i((__m128i *)second, (__m128i *)first, vline);
        first += 8;
        second += 8;
        sp += 16;
      }
      for (; len > 0; --len) {
         *first++  = *sp++;
         *second++ = *sp++;
      }
    }
  }
#else
  for (uint8_t b = 0; b < 4; ++b) {
     for (int32_t v = 0, vb = vstart[b]; vb < vstop[b]; ++vb, ++v) {
       for (int32_t u = 0, ub = ustart[b]; ub < ustop[b]; ++ub, ++u) {
         *(dp[b]++) = buf[2 * u + uoffset[b] + (2 * v + voffset[b]) * stride];
      }
    }
  }
#endif
}

// 2D FDWT function
void fdwt_2d_sr_fixed(sprec_t *previousLL, sprec_t *LL, sprec_t *HL, sprec_t *LH, sprec_t *HH,
                      const int32_t u0, const int32_t u1, const int32_t v0, const int32_t v1,
                      const uint8_t transformation) {
  sprec_t *src = previousLL;
  fdwt_ver_sr_fixed[transformation](src, u0, u1, v0, v1);
  fdwt_hor_sr_fixed(src, u0, u1, v0, v1, transformation);
  fdwt_2d_deinterleave_fixed(src, LL, HL, LH, HH, u0, u1, v0, v1);
}