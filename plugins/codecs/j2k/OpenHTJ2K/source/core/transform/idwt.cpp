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
static idwt_1d_filtd_func_fixed idwt_1d_filtr_fixed[2] = {idwt_1d_filtr_irrev97_fixed_neon,
                                                          idwt_1d_filtr_rev53_fixed_neon};
static idwt_ver_filtd_func_fixed idwt_ver_sr_fixed[2]  = {idwt_irrev_ver_sr_fixed_neon,
                                                          idwt_rev_ver_sr_fixed_neon};
#elif defined(OPENHTJ2K_ENABLE_AVX2)
static idwt_1d_filtd_func_fixed idwt_1d_filtr_fixed[2] = {idwt_1d_filtr_irrev97_fixed_avx2,
                                                          idwt_1d_filtr_rev53_fixed_avx2};
static idwt_ver_filtd_func_fixed idwt_ver_sr_fixed[2]  = {idwt_irrev_ver_sr_fixed_avx2,
                                                          idwt_rev_ver_sr_fixed_avx2};
#else
static idwt_1d_filtd_func_fixed idwt_1d_filtr_fixed[2] = {idwt_1d_filtr_irrev97_fixed,
                                                          idwt_1d_filtr_rev53_fixed};
static idwt_ver_filtd_func_fixed idwt_ver_sr_fixed[2]  = {idwt_irrev_ver_sr_fixed, idwt_rev_ver_sr_fixed};
#endif

void idwt_1d_filtr_irrev97_fixed(sprec_t *X, const int32_t left, const int32_t u_i0, const int32_t u_i1) {
  const auto i0        = static_cast<int32_t>(u_i0);
  const auto i1        = static_cast<int32_t>(u_i1);
  const int32_t start  = i0 / 2;
  const int32_t stop   = i1 / 2;
  const int32_t offset = left - i0 % 2;

  int32_t sum;
  /* K and 1/K have been already done by dequantization */
  for (int32_t n = -2 + offset, i = start - 1; i < stop + 2; i++, n += 2) {
    sum = X[n - 1];
    sum += X[n + 1];
    X[n] = static_cast<sprec_t>(X[n] - ((Dcoeff * sum + Doffset) >> Dshift));
  }
  int16_t a[16];
  memcpy(a, X - 2 + offset, sizeof(int16_t) * 16);
  for (int32_t n = -2 + offset, i = start - 1; i < stop + 1; i++, n += 2) {
    sum = X[n];
    sum += X[n + 2];
    X[n + 1] = static_cast<sprec_t>(X[n + 1] - ((Ccoeff * sum + Coffset) >> Cshift));
  }
  for (int32_t n = 0 + offset, i = start; i < stop + 1; i++, n += 2) {
    sum = X[n - 1];
    sum += X[n + 1];
    X[n] = static_cast<sprec_t>(X[n] - ((Bcoeff * sum + Boffset) >> Bshift));
  }
  for (int32_t n = 0 + offset, i = start; i < stop; i++, n += 2) {
    sum = X[n];
    sum += X[n + 2];
    X[n + 1] = static_cast<sprec_t>(X[n + 1] - ((Acoeff * sum + Aoffset) >> Ashift));
  }
}

void idwt_1d_filtr_rev53_fixed(sprec_t *X, const int32_t left, const int32_t u_i0, const int32_t u_i1) {
  const auto i0        = static_cast<int32_t>(u_i0);
  const auto i1        = static_cast<int32_t>(u_i1);
  const int32_t start  = i0 / 2;
  const int32_t stop   = i1 / 2;
  const int32_t offset = left - i0 % 2;

  for (int32_t n = 0 + offset, i = start; i < stop + 1; ++i, n += 2) {
    X[n] = static_cast<sprec_t>(X[n] - ((X[n - 1] + X[n + 1] + 2) >> 2));
  }

  for (int32_t n = 0 + offset, i = start; i < stop; ++i, n += 2) {
    X[n + 1] = static_cast<sprec_t>(X[n + 1] + ((X[n] + X[n + 2]) >> 1));
  }
}

static void idwt_1d_sr_fixed(sprec_t *buf, sprec_t *in, const int32_t left, const int32_t right,
                             const int32_t i0, const int32_t i1, const uint8_t transformation) {
  dwt_1d_extr_fixed(buf, in, left, right, i0, i1);
  idwt_1d_filtr_fixed[transformation](buf, left, i0, i1);
  memcpy(in, buf + left, sizeof(sprec_t) * (static_cast<size_t>(i1 - i0)));
}

static void idwt_hor_sr_fixed(sprec_t *in, const int32_t u0, const int32_t u1, const int32_t v0,
                              const int32_t v1, const uint8_t transformation) {
  const int32_t stride               = u1 - u0;
  constexpr int32_t num_pse_i0[2][2] = {{3, 1}, {4, 2}};
  constexpr int32_t num_pse_i1[2][2] = {{4, 2}, {3, 1}};
  const int32_t left                 = num_pse_i0[u0 % 2][transformation];
  const int32_t right                = num_pse_i1[u1 % 2][transformation];

  if (u0 == u1 - 1) {
    // one sample case
    for (int32_t row = 0; row < v1 - v0; ++row) {
      //      in[row] = in[row];
      if (u0 % 2 != 0 && transformation) {
        in[row] = static_cast<sprec_t>(in[row] >> 1);
      }
    }
  } else {
    // need to perform symmetric extension
    const int32_t len = u1 - u0 + left + right;
    auto *Yext        = static_cast<sprec_t *>(aligned_mem_alloc(
               sizeof(sprec_t) * static_cast<size_t>(round_up(len + SIMD_PADDING, SIMD_PADDING)), 32));
    for (int32_t row = 0; row < v1 - v0; ++row) {
      idwt_1d_sr_fixed(Yext, in, left, right, u0, u1, transformation);
      in += stride;
    }
    aligned_mem_free(Yext);
  }
}

void idwt_irrev_ver_sr_fixed(sprec_t *in, const int32_t u0, const int32_t u1, const int32_t v0,
                             const int32_t v1) {
  const int32_t stride            = u1 - u0;
  constexpr int32_t num_pse_i0[2] = {3, 4};
  constexpr int32_t num_pse_i1[2] = {4, 3};
  const int32_t top               = num_pse_i0[v0 % 2];
  const int32_t bottom            = num_pse_i1[v1 % 2];
  if (v0 == v1 - 1) {
    // one sample case
    for (int32_t col = 0; col < u1 - u0; ++col) {
      in[col] >>= (v0 % 2 == 0) ? 0 : 0;
    }
  } else {
    const int32_t len = round_up(stride, SIMD_PADDING);
    auto **buf        = new sprec_t *[static_cast<size_t>(top + v1 - v0 + bottom)];
    for (int32_t i = 1; i <= top; ++i) {
      buf[top - i] =
          static_cast<sprec_t *>(aligned_mem_alloc(sizeof(sprec_t) * static_cast<size_t>(len), 32));
      memcpy(buf[top - i], &in[PSEo(v0 - i, v0, v1) * stride],
             sizeof(sprec_t) * static_cast<size_t>(stride));
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
    const int32_t start  = v0 / 2;
    const int32_t stop   = v1 / 2;
    const int32_t offset = top - v0 % 2;

    for (int32_t n = -2 + offset, i = start - 1; i < stop + 2; i++, n += 2) {
      for (int32_t col = 0; col < u1 - u0; ++col) {
        int32_t sum = buf[n - 1][col];
        sum += buf[n + 1][col];
        buf[n][col] = static_cast<sprec_t>(buf[n][col] - ((Dcoeff * sum + Doffset) >> Dshift));
      }
    }
    for (int32_t n = -2 + offset, i = start - 1; i < stop + 1; i++, n += 2) {
      for (int32_t col = 0; col < u1 - u0; ++col) {
        int32_t sum = buf[n][col];
        sum += buf[n + 2][col];
        buf[n + 1][col] = static_cast<sprec_t>(buf[n + 1][col] - ((Ccoeff * sum + Coffset) >> Cshift));
      }
    }
    for (int32_t n = 0 + offset, i = start; i < stop + 1; i++, n += 2) {
      for (int32_t col = 0; col < u1 - u0; ++col) {
        int32_t sum = buf[n - 1][col];
        sum += buf[n + 1][col];
        buf[n][col] = static_cast<sprec_t>(buf[n][col] - ((Bcoeff * sum + Boffset) >> Bshift));
      }
    }
    for (int32_t n = 0 + offset, i = start; i < stop; i++, n += 2) {
      for (int32_t col = 0; col < u1 - u0; ++col) {
        int32_t sum = buf[n][col];
        sum += buf[n + 2][col];
        buf[n + 1][col] = static_cast<sprec_t>(buf[n + 1][col] - ((Acoeff * sum + Aoffset) >> Ashift));
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

void idwt_rev_ver_sr_fixed(sprec_t *in, const int32_t u0, const int32_t u1, const int32_t v0,
                           const int32_t v1) {
  const int32_t stride            = u1 - u0;
  constexpr int32_t num_pse_i0[2] = {1, 2};
  constexpr int32_t num_pse_i1[2] = {2, 1};
  const int32_t top               = num_pse_i0[v0 % 2];
  const int32_t bottom            = num_pse_i1[v1 % 2];
  if (v0 == v1 - 1 && (v0 % 2)) {
    // one sample case
    for (int32_t col = 0; col < u1 - u0; ++col) {
      in[col] = static_cast<sprec_t>(in[col] >> 1);
    }
  } else {
    const int32_t len = round_up(stride, SIMD_PADDING);
    auto **buf        = new sprec_t *[static_cast<size_t>(top + v1 - v0 + bottom)];
    for (int32_t i = 1; i <= top; ++i) {
      buf[top - i] =
          static_cast<sprec_t *>(aligned_mem_alloc(sizeof(sprec_t) * static_cast<size_t>(len), 32));
      memcpy(buf[top - i], &in[PSEo(v0 - i, v0, v1) * stride],
             sizeof(sprec_t) * static_cast<size_t>(stride));
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
    const int32_t start  = v0 / 2;
    const int32_t stop   = v1 / 2;
    const int32_t offset = top - v0 % 2;

    for (int32_t n = 0 + offset, i = start; i < stop + 1; ++i, n += 2) {
      for (int32_t col = 0; col < u1 - u0; ++col) {
        int32_t sum = buf[n - 1][col];
        sum += buf[n + 1][col];
        buf[n][col] = static_cast<sprec_t>(buf[n][col] - ((sum + 2) >> 2));
      }
    }
    for (int32_t n = 0 + offset, i = start; i < stop; ++i, n += 2) {
      for (int32_t col = 0; col < u1 - u0; ++col) {
        int32_t sum = buf[n][col];
        sum += buf[n + 2][col];
        buf[n + 1][col] = static_cast<sprec_t>(buf[n + 1][col] + (sum >> 1));
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

static void idwt_2d_interleave_fixed(sprec_t *buf, sprec_t *LL, sprec_t *HL, sprec_t *LH, sprec_t *HH,
                                     int32_t u0, int32_t u1, int32_t v0, int32_t v1) {
  const int32_t stride     = u1 - u0;
  const int32_t v_offset   = v0 % 2;
  const int32_t u_offset   = u0 % 2;
  sprec_t *sp[4]           = {LL, HL, LH, HH};
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
          buf[2 * u + uoffset[b] + (2 * v + voffset[b]) * stride] = *(sp[b]++);
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
    __builtin_prefetch(first);
    __builtin_prefetch(second);
    int16x8_t vfirst0, vfirst1, vsecond0, vsecond1;
    int16x8x2_t vdst0, vdst1;
    for (int32_t v = 0, vb = vstart[0]; vb < vstop[0]; ++vb, ++v) {
      sprec_t *dp = buf + (2 * v + voffset[0]) * stride;
      size_t len  = static_cast<size_t>(ustop[0] - ustart[0]);
      vfirst0     = vld1q_s16(first);
      //      vfirst1      = vld1q_s16(first + 8);
      vsecond0 = vld1q_s16(second);
      //      vsecond1     = vld1q_s16(second + 8);
      for (; len >= 16; len -= 16) {
        //        vfirst0      = vld1q_s16(first);
        vfirst1 = vld1q_s16(first + 8);
        //        vsecond0     = vld1q_s16(second);
        vsecond1     = vld1q_s16(second + 8);
        vdst0.val[0] = vfirst0;
        vdst0.val[1] = vsecond0;
        vdst1.val[0] = vfirst1;
        vdst1.val[1] = vsecond1;
        vst2q_s16(dp, vdst0);
        vst2q_s16(dp + 16, vdst1);
        first += 16;
        second += 16;
        __builtin_prefetch(first);
        __builtin_prefetch(second);
        dp += 32;
        vfirst0 = vld1q_s16(first);
        //        vfirst1      = vld1q_s16(first + 8);
        vsecond0 = vld1q_s16(second);
        //        vsecond1     = vld1q_s16(second + 8);
      }
      for (; len > 0; --len) {
        *dp++ = *first++;
        *dp++ = *second++;
      }
    }
  }

  if ((ustop[2] - ustart[2]) != (ustop[3] - ustart[3])) {
    for (uint8_t b = 2; b < 4; ++b) {
      for (int32_t v = 0, vb = vstart[b]; vb < vstop[b]; ++vb, ++v) {
        for (int32_t u = 0, ub = ustart[b]; ub < ustop[b]; ++ub, ++u) {
          buf[2 * u + uoffset[b] + (2 * v + voffset[b]) * stride] = *(sp[b]++);
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
    __builtin_prefetch(first);
    __builtin_prefetch(second);
    int16x8_t vfirst0, vfirst1, vsecond0, vsecond1;
    int16x8x2_t vdst0, vdst1;
    for (int32_t v = 0, vb = vstart[2]; vb < vstop[2]; ++vb, ++v) {
      sprec_t *dp = buf + (2 * v + voffset[2]) * stride;
      size_t len  = static_cast<size_t>(ustop[2] - ustart[2]);
      vfirst0     = vld1q_s16(first);
      //      vfirst1      = vld1q_s16(first + 8);
      vsecond0 = vld1q_s16(second);
      //      vsecond1     = vld1q_s16(second + 8);
      for (; len >= 16; len -= 16) {
        //        vfirst0      = vld1q_s16(first);
        vfirst1 = vld1q_s16(first + 8);
        //        vsecond0     = vld1q_s16(second);
        vsecond1     = vld1q_s16(second + 8);
        vdst0.val[0] = vfirst0;
        vdst0.val[1] = vsecond0;
        vdst1.val[0] = vfirst1;
        vdst1.val[1] = vsecond1;
        vst2q_s16(dp, vdst0);
        vst2q_s16(dp + 16, vdst1);
        first += 16;
        second += 16;
        __builtin_prefetch(first);
        __builtin_prefetch(second);
        dp += 32;
        vfirst0 = vld1q_s16(first);
        //        vfirst1      = vld1q_s16(first + 8);
        vsecond0 = vld1q_s16(second);
        //        vsecond1     = vld1q_s16(second + 8);
      }
      for (; len > 0; --len) {
        *dp++ = *first++;
        *dp++ = *second++;
      }
    }
  }
#elif defined(OPENHTJ2K_TRY_AVX2) && defined(__AVX2__)
  if ((ustop[0] - ustart[0]) != (ustop[1] - ustart[1])) {
     for (uint8_t b = 0; b < 2; ++b) {
       for (int32_t v = 0, vb = vstart[b]; vb < vstop[b]; ++vb, ++v) {
         for (int32_t u = 0, ub = ustart[b]; ub < ustop[b]; ++ub, ++u) {
           buf[2 * u + uoffset[b] + (2 * v + voffset[b]) * stride] = *(sp[b]++);
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
       sprec_t *dp = buf + (2 * v + voffset[0]) * stride;
       size_t len  = static_cast<size_t>(ustop[0] - ustart[0]);
       // SSE version
       //  for (; len >= 8; len -= 8) {
       //    auto vfirst  = _mm_loadu_si128((__m128i *)first);
       //    auto vsecond = _mm_loadu_si128((__m128i *)second);
       //    auto vtmp0   = _mm_unpacklo_epi16(vfirst, vsecond);
       //    auto vtmp1   = _mm_unpackhi_epi16(vfirst, vsecond);
       //    _mm_storeu_si128((__m128i *)dp, vtmp0);
       //    _mm_storeu_si128((__m128i *)(dp + 8), vtmp1);
       //    first += 8;
       //    second += 8;
       //    dp += 16;
       // }

       // AVX2 version
       __m256i vfirst, vsecond;
       vfirst  = _mm256_loadu_si256((__m256i *)first);
       vsecond = _mm256_loadu_si256((__m256i *)second);
       for (; len >= 16; len -= 16) {
         auto vtmp0 = _mm256_unpacklo_epi16(vfirst, vsecond);
         auto vtmp1 = _mm256_unpackhi_epi16(vfirst, vsecond);

         _mm256_storeu_si256((__m256i *)dp, _mm256_permute2x128_si256(vtmp0, vtmp1, 0x20));
         _mm256_storeu_si256((__m256i *)dp + 1, _mm256_permute2x128_si256(vtmp0, vtmp1, 0x31));
         first += 16;
         second += 16;
         dp += 32;
         vfirst  = _mm256_loadu_si256((__m256i *)first);
         vsecond = _mm256_loadu_si256((__m256i *)second);
      }
       for (; len > 0; --len) {
         *dp++ = *first++;
         *dp++ = *second++;
      }
    }
  }

  if ((ustop[2] - ustart[2]) != (ustop[3] - ustart[3])) {
     for (uint8_t b = 2; b < 4; ++b) {
       for (int32_t v = 0, vb = vstart[b]; vb < vstop[b]; ++vb, ++v) {
         for (int32_t u = 0, ub = ustart[b]; ub < ustop[b]; ++ub, ++u) {
           buf[2 * u + uoffset[b] + (2 * v + voffset[b]) * stride] = *(sp[b]++);
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
       sprec_t *dp = buf + (2 * v + voffset[2]) * stride;
       size_t len  = static_cast<size_t>(ustop[2] - ustart[2]);
       // SSE version
       //  for (; len >= 8; len -= 8) {
       //    auto vfirst  = _mm_loadu_si128((__m128i *)first);
       //    auto vsecond = _mm_loadu_si128((__m128i *)second);
       //    auto vtmp0   = _mm_unpacklo_epi16(vfirst, vsecond);
       //    auto vtmp1   = _mm_unpackhi_epi16(vfirst, vsecond);
       //    _mm_storeu_si128((__m128i *)dp, vtmp0);
       //    _mm_storeu_si128((__m128i *)(dp + 8), vtmp1);
       //    first += 8;
       //    second += 8;
       //    dp += 16;
       // }

       // AVX2 version
       __m256i vfirst, vsecond;
       vfirst  = _mm256_loadu_si256((__m256i *)first);
       vsecond = _mm256_loadu_si256((__m256i *)second);
       for (; len >= 16; len -= 16) {
         auto vtmp0 = _mm256_unpacklo_epi16(vfirst, vsecond);
         auto vtmp1 = _mm256_unpackhi_epi16(vfirst, vsecond);

         _mm256_storeu_si256((__m256i *)dp, _mm256_permute2x128_si256(vtmp0, vtmp1, 0x20));
         _mm256_storeu_si256((__m256i *)dp + 1, _mm256_permute2x128_si256(vtmp0, vtmp1, 0x31));
         first += 16;
         second += 16;
         dp += 32;
         vfirst  = _mm256_loadu_si256((__m256i *)first);
         vsecond = _mm256_loadu_si256((__m256i *)second);
      }
       for (; len > 0; --len) {
         *dp++ = *first++;
         *dp++ = *second++;
      }
    }
  }
#else
  for (uint8_t b = 0; b < 4; ++b) {
     for (int32_t v = 0, vb = vstart[b]; vb < vstop[b]; ++vb, ++v) {
       for (int32_t u = 0, ub = ustart[b]; ub < ustop[b]; ++ub, ++u) {
         buf[2 * u + uoffset[b] + (2 * v + voffset[b]) * stride] = *(sp[b]++);
      }
    }
  }
#endif
}

void idwt_2d_sr_fixed(sprec_t *nextLL, sprec_t *LL, sprec_t *HL, sprec_t *LH, sprec_t *HH, const int32_t u0,
                      const int32_t u1, const int32_t v0, const int32_t v1, const uint8_t transformation,
                      uint8_t normalizing_upshift) {
  const int32_t buf_length = (u1 - u0) * (v1 - v0);
  sprec_t *src             = nextLL;
  idwt_2d_interleave_fixed(src, LL, HL, LH, HH, u0, u1, v0, v1);
  idwt_hor_sr_fixed(src, u0, u1, v0, v1, transformation);
  idwt_ver_sr_fixed[transformation](src, u0, u1, v0, v1);

  // scaling for 16bit width fixed-point representation
  if (transformation != 1 && normalizing_upshift) {
    int32_t len = buf_length;
#if defined(OPENHTJ2K_ENABLE_ARM_NEON)
    int16x8_t vshift = vdupq_n_s16(normalizing_upshift);
    int16x8_t in0, in1;
    in0 = vld1q_s16(src);
    in1 = vld1q_s16(src + 8);
    for (; len >= 16; len -= 16) {
      in0 = vshlq_s16(in0, vshift);
      in1 = vshlq_s16(in1, vshift);
      vst1q_s16(src, in0);
      vst1q_s16(src + 8, in1);
      src += 16;
      in0 = vld1q_s16(src);
      in1 = vld1q_s16(src + 8);
    }
    for (; len > 0; --len) {
      *src = static_cast<sprec_t>(*src << normalizing_upshift);
      src++;
    }
#elif defined(OPENHTJ2K_TRY_AVX2) && defined(__AVX2__)
    for (; len >= 16; len -= 16) {
       __m256i tmp0 = _mm256_load_si256((__m256i *)src);
       __m256i tmp1 = _mm256_slli_epi16(tmp0, static_cast<int32_t>(normalizing_upshift));
       _mm256_store_si256((__m256i *)src, tmp1);
       src += 16;
    }
    for (; len > 0; --len) {
       // cast to unsigned to avoid undefined behavior
      *src = static_cast<sprec_t>(static_cast<usprec_t>(*src) << normalizing_upshift);
      src++;
    }
#else
    for (; len > 0; --len) {
       // cast to unsigned to avoid undefined behavior
      *src = static_cast<sprec_t>(static_cast<usprec_t>(*src) << normalizing_upshift);
      src++;
    }
#endif
  }
}
