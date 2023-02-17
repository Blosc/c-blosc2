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
#include <cfloat>
#include <cmath>
#include "utils.hpp"

#define ALPHA_R 0.299
#define ALPHA_B 0.114
#define ALPHA_RB (ALPHA_R + ALPHA_B)
#define ALPHA_G (1 - ALPHA_RB)
#define CR_FACT_R (2 * (1 - ALPHA_R))
#define CB_FACT_B (2 * (1 - ALPHA_B))
#define CR_FACT_G (2 * ALPHA_R * (1 - ALPHA_R) / ALPHA_G)
#define CB_FACT_G (2 * ALPHA_B * (1 - ALPHA_B) / ALPHA_G)

typedef void (*cvt_color_func)(int32_t *, int32_t *, int32_t *, uint32_t, uint32_t);
#if defined(OPENHTJ2K_TRY_AVX2) && defined(__AVX2__)
/**
 * @brief Forward reversible color transform (RCT) with AVX2 intrinsics
 * @param sp0 pointer to Red samples (shall be aligned and multiple of 8 samples)
 * @param sp1 pointer to Green samples (shall be aligned and multiple of 8 samples)
 * @param sp2 pointer to Blue samples (shall be aligned and multiple of 8 samples)
 * @param width original width (may not be multiple of 8)
 * @param height original height
 */
void cvt_rgb_to_ycbcr_rev_avx2(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width, uint32_t height);
/**
 * @brief Forward irreversible color transform (ICT) with AVX2 intrinsics
 * @param sp0 pointer to Red samples (shall be aligned and multiple of 8 samples)
 * @param sp1 pointer to Green samples (shall be aligned and multiple of 8 samples)
 * @param sp2 pointer to Blue samples (shall be aligned and multiple of 8 samples)
 * @param width original width (may not be multiple of 8)
 * @param height original height
 */
void cvt_rgb_to_ycbcr_irrev_avx2(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width, uint32_t height);
/**
 * @brief Inverse reversible color transform (RCT) with AVX2 intrinsics
 * @param sp0 pointer to Y samples (shall be aligned and multiple of 8 samples)
 * @param sp1 pointer to Cb samples (shall be aligned and multiple of 8 samples)
 * @param sp2 pointer to Cr samples (shall be aligned and multiple of 8 samples)
 * @param width original width (may not be multiple of 8)
 * @param height original height
 */
void cvt_ycbcr_to_rgb_rev_avx2(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width, uint32_t height);
/**
 * @brief Inverse irreversible color transform (ICT) with AVX2 intrinsics
 * @param sp0 pointer to Y samples (shall be aligned and multiple of 8 samples)
 * @param sp1 pointer to Cb samples (shall be aligned and multiple of 8 samples)
 * @param sp2 pointer to Cr samples (shall be aligned and multiple of 8 samples)
 * @param width original width (may not be multiple of 8)
 * @param height original height
 */
void cvt_ycbcr_to_rgb_irrev_avx2(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width, uint32_t height);
#elif defined(OPENHTJ2K_ENABLE_ARM_NEON)
/**
 * @brief Forward reversible color transform (RCT) with NEON intrinsics
 * @param sp0 pointer to Red samples (shall be aligned and multiple of 8 samples)
 * @param sp1 pointer to Green samples (shall be aligned and multiple of 8 samples)
 * @param sp2 pointer to Blue samples (shall be aligned and multiple of 8 samples)
 * @param width original width (may not be multiple of 8)
 * @param height original height
 */
void cvt_rgb_to_ycbcr_rev_neon(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width, uint32_t height);
/**
 * @brief Forward irreversible color transform (ICT) with NEON intrinsics
 * @param sp0 pointer to Red samples (shall be aligned and multiple of 8 samples)
 * @param sp1 pointer to Green samples (shall be aligned and multiple of 8 samples)
 * @param sp2 pointer to Blue samples (shall be aligned and multiple of 8 samples)
 * @param width original width (may not be multiple of 8)
 * @param height original height
 */
void cvt_rgb_to_ycbcr_irrev_neon(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width, uint32_t height);
/**
 * @brief Inverse reversible color transform (RCT) with NEON intrinsics
 * @param sp0 pointer to Y samples (shall be aligned and multiple of 8 samples)
 * @param sp1 pointer to Cb samples (shall be aligned and multiple of 8 samples)
 * @param sp2 pointer to Cr samples (shall be aligned and multiple of 8 samples)
 * @param width original width (may not be multiple of 8)
 * @param height original height
 */
void cvt_ycbcr_to_rgb_rev_neon(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width, uint32_t height);
/**
 * @brief Inverse irreversible color transform (ICT) with NEON intrinsics
 * @param sp0 pointer to Y samples (shall be aligned and multiple of 8 samples)
 * @param sp1 pointer to Cb samples (shall be aligned and multiple of 8 samples)
 * @param sp2 pointer to Cr samples (shall be aligned and multiple of 8 samples)
 * @param width original width (may not be multiple of 8)
 * @param height original height
 */
void cvt_ycbcr_to_rgb_irrev_neon(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width, uint32_t height);
#else
void cvt_rgb_to_ycbcr_rev(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width, uint32_t height);
void cvt_rgb_to_ycbcr_irrev(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width, uint32_t height);
void cvt_ycbcr_to_rgb_rev(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width, uint32_t height);
void cvt_ycbcr_to_rgb_irrev(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width, uint32_t height);
#endif

inline int32_t round_d(double val) {
  if (fabs(val) < DBL_EPSILON) {
    return 0;
  } else {
    return static_cast<int32_t>(val + ((val > 0) ? 0.5 : -0.5));
  }
}
