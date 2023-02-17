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

#include "utils.hpp"
#if defined(OPENHTJ2K_TRY_AVX2) && defined(__AVX2__)
  #include "dwt.hpp"
  #include <cstring>
/********************************************************************************
 * horizontal transforms
 *******************************************************************************/
// irreversible IDWT
auto idwt_irrev97_fixed_avx2_hor_step0 = [](const int32_t init_pos, const int32_t simdlen, int16_t *const X,
                                            const int32_t n0, const int32_t n1) {
  auto vcoeff = _mm256_set1_epi16(Dcoeff_simd);
  for (int32_t n = init_pos, i = 0; i < simdlen; i += 8, n += 16) {
    auto xin0 = _mm256_loadu_si256((__m256i *)(X + n + n0));
    auto xin2 = _mm256_loadu_si256((__m256i *)(X + n + n1));
    auto xsum = _mm256_add_epi16(xin0, xin2);
    xsum      = _mm256_blend_epi16(xsum, _mm256_setzero_si256(), 0xAA);
    xsum      = _mm256_mulhrs_epi16(xsum, vcoeff);
    xsum      = _mm256_slli_si256(xsum, 2);
    xin0      = _mm256_sub_epi16(xin0, xsum);
    _mm256_storeu_si256((__m256i *)(X + n + n0), xin0);
  }
};

auto idwt_irrev97_fixed_avx2_hor_step1 = [](const int32_t init_pos, const int32_t simdlen, int16_t *const X,
                                            const int32_t n0, const int32_t n1) {
  auto vcoeff = _mm256_set1_epi16(Ccoeff_simd);
  for (int32_t n = init_pos, i = 0; i < simdlen; i += 8, n += 16) {
    auto xin0 = _mm256_loadu_si256((__m256i *)(X + n + n0));
    auto xin2 = _mm256_loadu_si256((__m256i *)(X + n + n1));
    auto xsum = _mm256_add_epi16(xin0, xin2);
    xsum      = _mm256_blend_epi16(xsum, _mm256_setzero_si256(), 0xAA);
    xsum      = _mm256_mulhrs_epi16(xsum, vcoeff);
    xsum      = _mm256_slli_si256(xsum, 2);
    xin0      = _mm256_sub_epi16(xin0, xsum);
    _mm256_storeu_si256((__m256i *)(X + n + n0), xin0);
  }
};

auto idwt_irrev97_fixed_avx2_hor_step2 = [](const int32_t init_pos, const int32_t simdlen, int16_t *const X,
                                            const int32_t n0, const int32_t n1) {
  auto vcoeff = _mm256_set1_epi16(Bcoeff_simd_avx2);
  auto vfour  = _mm256_set1_epi16(4);
  for (int32_t n = init_pos, i = 0; i < simdlen; i += 8, n += 16) {
    auto xin0  = _mm256_loadu_si256((__m256i *)(X + n + n0));
    auto xtmp0 = _mm256_mulhrs_epi16(xin0, vcoeff);
    auto xin2  = _mm256_loadu_si256((__m256i *)(X + n + n1));
    auto xtmp1 = _mm256_mulhrs_epi16(xin2, vcoeff);
    auto xsum  = _mm256_add_epi16(xtmp0, xtmp1);
    xsum       = _mm256_add_epi16(xsum, vfour);
    xsum       = _mm256_blend_epi16(xsum, _mm256_setzero_si256(), 0xAA);
    xsum       = _mm256_srai_epi16(xsum, 3);
    xsum       = _mm256_slli_si256(xsum, 2);
    xin0       = _mm256_sub_epi16(xin0, xsum);
    _mm256_storeu_si256((__m256i *)(X + n + n0), xin0);
  }
};

auto idwt_irrev97_fixed_avx2_hor_step3 = [](const int32_t init_pos, const int32_t simdlen, int16_t *const X,
                                            const int32_t n0, const int32_t n1) {
  auto vcoeff = _mm256_set1_epi16(Acoeff_simd);
  for (int32_t n = init_pos, i = 0; i < simdlen; i += 8, n += 16) {
    auto xin0 = _mm256_loadu_si256((__m256i *)(X + n + n0));
    auto xin2 = _mm256_loadu_si256((__m256i *)(X + n + n1));
    auto xsum = _mm256_add_epi16(xin0, xin2);
    auto xtmp = _mm256_blend_epi16(xsum, _mm256_setzero_si256(), 0xAA);
    xsum      = _mm256_mulhrs_epi16(xtmp, vcoeff);
    xsum      = _mm256_sub_epi16(xsum, xtmp);
    xsum      = _mm256_slli_si256(xsum, 2);
    xin0      = _mm256_sub_epi16(xin0, xsum);
    _mm256_storeu_si256((__m256i *)(X + n + n0), xin0);
  }
};

[[maybe_unused]] auto idwt_irrev97_fixed_avx2_hor_step = [](const int32_t init_pos, const int32_t simdlen,
                                                            int16_t *const X, const int32_t n0,
                                                            const int32_t n1, const int32_t coeff,
                                                            const int32_t offset, const int32_t shift) {
  auto vcoeff  = _mm256_set1_epi32(coeff);
  auto voffset = _mm256_set1_epi32(offset);
  for (int32_t n = init_pos, i = 0; i < simdlen; i += 8, n += 16) {
    auto xin0 = _mm256_loadu_si256((__m256i *)(X + n + n0));
    auto xin2 = _mm256_loadu_si256((__m256i *)(X + n + n1));
    auto xin_tmp =
        _mm256_permutevar8x32_epi32(_mm256_shufflelo_epi16(_mm256_shufflehi_epi16(xin0, 0xD8), 0xD8),
                                    _mm256_setr_epi32(0, 2, 4, 6, 1, 3, 5, 7));
    auto xin00 = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(xin_tmp, 0));
    auto xin01 = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(xin_tmp, 1));
    xin_tmp = _mm256_permutevar8x32_epi32(_mm256_shufflelo_epi16(_mm256_shufflehi_epi16(xin2, 0xD8), 0xD8),
                                          _mm256_setr_epi32(0, 2, 4, 6, 1, 3, 5, 7));
    auto xin20 = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(xin_tmp, 0));
    auto vsum  = _mm256_add_epi32(xin00, xin20);
    xin01      = _mm256_sub_epi32(
             xin01, _mm256_srai_epi32(_mm256_add_epi32(_mm256_mullo_epi32(vsum, vcoeff), voffset), shift));
    auto xout32           = _mm256_shuffle_epi32(_mm256_packs_epi32(xin00, xin01), 0xD8);
    auto xout_interleaved = _mm256_shufflelo_epi16(_mm256_shufflehi_epi16(xout32, 0xD8), 0xD8);
    _mm256_storeu_si256((__m256i *)(X + n + n0), xout_interleaved);
  }
};

void idwt_1d_filtr_irrev97_fixed_avx2(sprec_t *X, const int32_t left, const int32_t u_i0,
                                      const int32_t u_i1) {
  const auto i0        = static_cast<int32_t>(u_i0);
  const auto i1        = static_cast<int32_t>(u_i1);
  const int32_t start  = i0 / 2;
  const int32_t stop   = i1 / 2;
  const int32_t offset = left - i0 % 2;

  // step 1
  int32_t simdlen = stop + 2 - (start - 1);
  idwt_irrev97_fixed_avx2_hor_step0(offset - 2, simdlen, X, -1, 1);

  // step 2
  simdlen = stop + 1 - (start - 1);
  idwt_irrev97_fixed_avx2_hor_step1(offset - 2, simdlen, X, 0, 2);

  // step 3
  simdlen = stop + 1 - start;
  idwt_irrev97_fixed_avx2_hor_step2(offset, simdlen, X, -1, 1);

  // step 4
  simdlen = stop - start;
  idwt_irrev97_fixed_avx2_hor_step3(offset, simdlen, X, 0, 2);
}

// reversible IDWT
void idwt_1d_filtr_rev53_fixed_avx2(sprec_t *X, const int32_t left, const int32_t u_i0,
                                    const int32_t u_i1) {
  const auto i0        = static_cast<int32_t>(u_i0);
  const auto i1        = static_cast<int32_t>(u_i1);
  const int32_t start  = i0 / 2;
  const int32_t stop   = i1 / 2;
  const int32_t offset = left - i0 % 2;

  // step 1
  int32_t simdlen = stop + 1 - start;
  sprec_t *sp     = X + offset;
  for (; simdlen > 0; simdlen -= 8) {
    auto xin0 = _mm256_loadu_si256((__m256i *)(sp - 1));
    auto xin2 = _mm256_loadu_si256((__m256i *)(sp + 1));
    auto xsum = _mm256_add_epi16(xin0, xin2);
    xsum      = _mm256_add_epi16(xsum, _mm256_set1_epi16(2));
    xsum      = _mm256_srai_epi16(xsum, 2);
    xsum      = _mm256_blend_epi16(xsum, _mm256_setzero_si256(), 0xAA);
    xsum      = _mm256_slli_si256(xsum, 2);
    xin0      = _mm256_sub_epi16(xin0, xsum);
    _mm256_storeu_si256((__m256i *)(sp - 1), xin0);
    sp += 16;

    // auto xin02 =
    //     _mm256_permutevar8x32_epi32(_mm256_shufflelo_epi16(_mm256_shufflehi_epi16(xin0, 0xD8), 0xD8),
    //                                 _mm256_setr_epi32(0, 2, 4, 6, 1, 3, 5, 7));
    // auto xodd0  = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(xin02, 0));
    // auto xeven0 = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(xin02, 1));
    // auto xin22 =
    //     _mm256_permutevar8x32_epi32(_mm256_shufflelo_epi16(_mm256_shufflehi_epi16(xin2, 0xD8), 0xD8),
    //                                 _mm256_setr_epi32(0, 2, 4, 6, 1, 3, 5, 7));
    // auto xodd1 = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(xin22, 0));
    // auto vsum  = _mm256_add_epi32(xodd0, xodd1);
    // auto xout =
    //     _mm256_sub_epi32(xeven0, _mm256_srai_epi32(_mm256_add_epi32(vsum, _mm256_set1_epi32(2)), 2));
    // auto xout_odd_even    = _mm256_shuffle_epi32(_mm256_packs_epi32(xodd0, xout), 0xD8);
    // auto xout_interleaved = _mm256_shufflelo_epi16(_mm256_shufflehi_epi16(xout_odd_even, 0xD8), 0xD8);
    // _mm256_storeu_si256((__m256i *)(X + n - 1), xout_interleaved);
  }

  // step 2
  simdlen = stop - start;
  sp      = X + offset;
  for (; simdlen > 0; simdlen -= 8) {
    auto xin0 = _mm256_loadu_si256((__m256i *)sp);
    auto xin2 = _mm256_loadu_si256((__m256i *)(sp + 2));
    auto xsum = _mm256_add_epi16(xin0, xin2);
    xsum      = _mm256_srai_epi16(xsum, 1);
    xsum      = _mm256_blend_epi16(xsum, _mm256_setzero_si256(), 0xAA);
    xsum      = _mm256_slli_si256(xsum, 2);
    xin0      = _mm256_add_epi16(xin0, xsum);
    _mm256_storeu_si256((__m256i *)sp, xin0);
    sp += 16;

    // auto xin02 =
    //     _mm256_permutevar8x32_epi32(_mm256_shufflelo_epi16(_mm256_shufflehi_epi16(xin0, 0xD8), 0xD8),
    //                                 _mm256_setr_epi32(0, 2, 4, 6, 1, 3, 5, 7));
    // auto xeven0 = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(xin02, 0));
    // auto xodd0  = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(xin02, 1));
    // auto xin22 =
    //     _mm256_permutevar8x32_epi32(_mm256_shufflelo_epi16(_mm256_shufflehi_epi16(xin2, 0xD8), 0xD8),
    //                                 _mm256_setr_epi32(0, 2, 4, 6, 1, 3, 5, 7));
    // auto xeven1           = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(xin22, 0));
    // auto vsum             = _mm256_add_epi32(xeven0, xeven1);
    // auto xout             = _mm256_add_epi32(xodd0, _mm256_srai_epi32(vsum, 1));
    // auto xout_even_odd    = _mm256_shuffle_epi32(_mm256_packs_epi32(xeven0, xout), 0xD8);
    // auto xout_interleaved = _mm256_shufflelo_epi16(_mm256_shufflehi_epi16(xout_even_odd, 0xD8), 0xD8);
    // _mm256_storeu_si256((__m256i *)(X + n), xout_interleaved);
  }
}

/********************************************************************************
 * vertical transform
 *******************************************************************************/
// irreversible IDWT
auto idwt_irrev97_fixed_avx2_ver_step = [](const int32_t simdlen, int16_t *const Xin0, int16_t *const Xin1,
                                           int16_t *const Xout, const int32_t coeff, const int32_t offset,
                                           const int32_t shift) {
  auto vcoeff  = _mm256_set1_epi32(coeff);
  auto voffset = _mm256_set1_epi32(offset);
  for (int32_t n = 0; n < simdlen; n += 16) {
    auto xin0_16 = _mm256_loadu_si256((__m256i *)(Xin0 + n));
    auto xin2_16 = _mm256_loadu_si256((__m256i *)(Xin1 + n));
    auto xout16  = _mm256_loadu_si256((__m256i *)(Xout + n));
    // low
    auto xin0_32 = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(xin0_16, 0));
    auto xin2_32 = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(xin2_16, 0));
    auto xout32  = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(xout16, 0));
    auto vsum32  = _mm256_add_epi32(xin0_32, xin2_32);
    auto xout32l = _mm256_sub_epi32(
        xout32, _mm256_srai_epi32(_mm256_add_epi32(_mm256_mullo_epi32(vsum32, vcoeff), voffset), shift));

    // high
    xin0_32      = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(xin0_16, 1));
    xin2_32      = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(xin2_16, 1));
    xout32       = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(xout16, 1));
    vsum32       = _mm256_add_epi32(xin0_32, xin2_32);
    auto xout32h = _mm256_sub_epi32(
        xout32, _mm256_srai_epi32(_mm256_add_epi32(_mm256_mullo_epi32(vsum32, vcoeff), voffset), shift));

    // pack and store
    _mm256_storeu_si256((__m256i *)(Xout + n),
                        _mm256_permute4x64_epi64(_mm256_packs_epi32(xout32l, xout32h), 0xD8));
  }
};

void idwt_irrev_ver_sr_fixed_avx2(sprec_t *in, const int32_t u0, const int32_t u1, const int32_t v0,
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

    const int32_t simdlen = (u1 - u0) - (u1 - u0) % 16;
    for (int32_t n = -2 + offset, i = start - 1; i < stop + 2; i++, n += 2) {
      idwt_irrev97_fixed_avx2_ver_step(simdlen, buf[n - 1], buf[n + 1], buf[n], Dcoeff, Doffset, Dshift);
      for (int32_t col = simdlen; col < u1 - u0; ++col) {
        int32_t sum = buf[n - 1][col];
        sum += buf[n + 1][col];
        buf[n][col] = static_cast<sprec_t>(buf[n][col] - ((Dcoeff * sum + Doffset) >> Dshift));
      }
    }
    for (int32_t n = -2 + offset, i = start - 1; i < stop + 1; i++, n += 2) {
      idwt_irrev97_fixed_avx2_ver_step(simdlen, buf[n], buf[n + 2], buf[n + 1], Ccoeff, Coffset, Cshift);
      for (int32_t col = simdlen; col < u1 - u0; ++col) {
        int32_t sum = buf[n][col];
        sum += buf[n + 2][col];
        buf[n + 1][col] = static_cast<sprec_t>(buf[n + 1][col] - ((Ccoeff * sum + Coffset) >> Cshift));
      }
    }
    for (int32_t n = 0 + offset, i = start; i < stop + 1; i++, n += 2) {
      idwt_irrev97_fixed_avx2_ver_step(simdlen, buf[n - 1], buf[n + 1], buf[n], Bcoeff, Boffset, Bshift);
      for (int32_t col = simdlen; col < u1 - u0; ++col) {
        int32_t sum = buf[n - 1][col];
        sum += buf[n + 1][col];
        buf[n][col] = static_cast<sprec_t>(buf[n][col] - ((Bcoeff * sum + Boffset) >> Bshift));
      }
    }
    for (int32_t n = 0 + offset, i = start; i < stop; i++, n += 2) {
      idwt_irrev97_fixed_avx2_ver_step(simdlen, buf[n], buf[n + 2], buf[n + 1], Acoeff, Aoffset, Ashift);
      for (int32_t col = simdlen; col < u1 - u0; ++col) {
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

// reversible IDWT
void idwt_rev_ver_sr_fixed_avx2(sprec_t *in, const int32_t u0, const int32_t u1, const int32_t v0,
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

    const int32_t simdlen = (u1 - u0) - (u1 - u0) % 16;
    const __m256i vone    = _mm256_set1_epi16(1);
    for (int32_t n = 0 + offset, i = start; i < stop + 1; ++i, n += 2) {
      int16_t *xp0 = buf[n - 1];
      int16_t *xp1 = buf[n];
      int16_t *xp2 = buf[n + 1];
      __m256i x0   = _mm256_loadu_si256((__m256i *)xp0);
      __m256i x2   = _mm256_loadu_si256((__m256i *)xp2);
      __m256i x1   = _mm256_loadu_si256((__m256i *)xp1);
      for (int32_t col = 0; col < simdlen; col += 16) {
        __m256i vout = _mm256_add_epi16(vone, _mm256_srai_epi16(_mm256_add_epi16(x0, x2), 1));
        vout         = _mm256_srai_epi16(vout, 1);
        x1           = _mm256_sub_epi16(x1, vout);
        _mm256_storeu_si256((__m256i *)xp1, x1);
        xp0 += 16;
        xp1 += 16;
        xp2 += 16;
        x0 = _mm256_loadu_si256((__m256i *)xp0);
        x2 = _mm256_loadu_si256((__m256i *)xp2);
        x1 = _mm256_loadu_si256((__m256i *)xp1);
      }
      for (int32_t col = simdlen; col < u1 - u0; ++col) {
        int32_t sum = *xp0++;
        sum += *xp2++;
        *xp1 = static_cast<sprec_t>(*xp1 - ((sum + 2) >> 2));
        xp1++;
      }
    }
    for (int32_t n = 0 + offset, i = start; i < stop; ++i, n += 2) {
      int16_t *xp0 = buf[n];
      int16_t *xp1 = buf[n + 1];
      int16_t *xp2 = buf[n + 2];
      __m256i x0   = _mm256_loadu_si256((__m256i *)xp0);
      __m256i x2   = _mm256_loadu_si256((__m256i *)xp2);
      __m256i x1   = _mm256_loadu_si256((__m256i *)xp1);
      for (int32_t col = 0; col < simdlen; col += 16) {
        x1 = _mm256_add_epi16(x1, _mm256_srai_epi16(_mm256_add_epi16(x0, x2), 1));
        _mm256_storeu_si256((__m256i *)xp1, x1);
        xp0 += 16;
        xp1 += 16;
        xp2 += 16;
        x0 = _mm256_loadu_si256((__m256i *)xp0);
        x2 = _mm256_loadu_si256((__m256i *)xp2);
        x1 = _mm256_loadu_si256((__m256i *)xp1);
      }
      for (int32_t col = simdlen; col < u1 - u0; ++col) {
        int32_t sum = *xp0++;
        sum += *xp2++;
        *xp1 = static_cast<sprec_t>(*xp1 + (sum >> 1));
        xp1++;
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
#endif
