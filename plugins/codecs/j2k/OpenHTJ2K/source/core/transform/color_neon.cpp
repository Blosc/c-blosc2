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

#if defined(OPENHTJ2K_ENABLE_ARM_NEON)
  #include <arm_neon.h>
  #include "color.hpp"

  #define neon_alphaR (static_cast<int32_t>(0.5 + ALPHA_R * (1 << 15)))
  #define neon_alphaB (static_cast<int32_t>(0.5 + ALPHA_B * (1 << 15)))
  #define neon_alphaG (static_cast<int32_t>(0.5 + ALPHA_G * (1 << 15)))
  #define neon_CBfact (static_cast<int32_t>(0.5 + CB_FACT * (1 << 15)))
  #define neon_CRfact (static_cast<int32_t>(0.5 + CR_FACT * (1 << 15)))
  #define neon_CRfactR (static_cast<int32_t>(0.5 + (CR_FACT_R - 1) * (1 << 15)))
  #define neon_CBfactB (static_cast<int32_t>(0.5 + (CB_FACT_B - 1) * (1 << 15)))
  #define neon_neg_CRfactG (static_cast<int32_t>(0.5 - CR_FACT_G * (1 << 15)))
  #define neon_neg_CBfactG (static_cast<int32_t>(0.5 - CB_FACT_G * (1 << 15)))

// lossless: forward RCT
void cvt_rgb_to_ycbcr_rev_neon(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width, uint32_t height) {
  // process two vectors at a time
  for (uint32_t y = 0; y < height; ++y) {
    int32_t *p0 = sp0 + y * round_up(width, 32U);
    int32_t *p1 = sp1 + y * round_up(width, 32U);
    int32_t *p2 = sp2 + y * round_up(width, 32U);
    int32_t len = static_cast<int32_t>(width);
    for (; len > 0; len -= 8) {
      auto vR0  = vld1q_s32(p0);
      auto vR1  = vld1q_s32(p0 + 4);
      auto vG0  = vld1q_s32(p1);
      auto vG1  = vld1q_s32(p1 + 4);
      auto vB0  = vld1q_s32(p2);
      auto vB1  = vld1q_s32(p2 + 4);
      auto vY0  = (vR0 + 2 * vG0 + vB0) >> 2;
      auto vY1  = (vR1 + 2 * vG1 + vB1) >> 2;
      auto vCb0 = vB0 - vG0;
      auto vCb1 = vB1 - vG1;
      auto vCr0 = vR0 - vG0;
      auto vCr1 = vR1 - vG1;
      vst1q_s32(p0, vY0);
      vst1q_s32(p0 + 4, vY1);
      vst1q_s32(p1, vCb0);
      vst1q_s32(p1 + 4, vCb1);
      vst1q_s32(p2, vCr0);
      vst1q_s32(p2 + 4, vCr1);
      p0 += 8;
      p1 += 8;
      p2 += 8;
    }
  }
}

// lossy: forward ICT
void cvt_rgb_to_ycbcr_irrev_neon(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width,
                                 uint32_t height) {
  int32x4_t R0, G0, B0, R1, G1, B1;
  float32x4_t Y0, Cb0, Cr0, fR0, fG0, fB0, Y1, Cb1, Cr1, fR1, fG1, fB1;
  const float32x4_t a0 = vdupq_n_f32(static_cast<float32_t>(ALPHA_R));
  const float32x4_t a1 = vdupq_n_f32(static_cast<float32_t>(ALPHA_G));
  const float32x4_t a2 = vdupq_n_f32(static_cast<float32_t>(ALPHA_B));
  const float32x4_t a3 = vdupq_n_f32(static_cast<float32_t>(1.0 / CB_FACT_B));
  const float32x4_t a4 = vdupq_n_f32(static_cast<float32_t>(1.0 / CR_FACT_R));
  // process two vectors at a time

  for (uint32_t y = 0; y < height; ++y) {
    int32_t *p0 = sp0 + y * round_up(width, 32U);
    int32_t *p1 = sp1 + y * round_up(width, 32U);
    int32_t *p2 = sp2 + y * round_up(width, 32U);
    int32_t len = static_cast<int32_t>(width);
    for (; len > 0; len -= 8) {
      R0  = vld1q_s32(p0);
      R1  = vld1q_s32(p0 + 4);
      G0  = vld1q_s32(p1);
      G1  = vld1q_s32(p1 + 4);
      B0  = vld1q_s32(p2);
      B1  = vld1q_s32(p2 + 4);
      fR0 = vcvtq_f32_s32(R0);
      fR1 = vcvtq_f32_s32(R1);
      fG0 = vcvtq_f32_s32(G0);
      fG1 = vcvtq_f32_s32(G1);
      fB0 = vcvtq_f32_s32(B0);
      fB1 = vcvtq_f32_s32(B1);
      Y0  = vmulq_f32(fR0, a0);
      Y0  = vfmaq_f32(Y0, fG0, a1);
      Y0  = vfmaq_f32(Y0, fB0, a2);
      Y1  = vmulq_f32(fR1, a0);
      Y1  = vfmaq_f32(Y1, fG1, a1);
      Y1  = vfmaq_f32(Y1, fB1, a2);
      // Y0  = fR0 * a0 + fG0 * a1 + fB0 * a2;
      Cb0 = vmulq_f32(vsubq_f32(fB0, Y0), a3);
      Cb1 = vmulq_f32(vsubq_f32(fB1, Y1), a3);
      Cr0 = vmulq_f32(vsubq_f32(fR0, Y0), a4);
      Cr1 = vmulq_f32(vsubq_f32(fR1, Y1), a4);

      // TODO: need to consider precision and setting FPSCR register value
      vst1q_s32(p0, vcvtnq_s32_f32(Y0));
      vst1q_s32(p0 + 4, vcvtnq_s32_f32(Y1));
      vst1q_s32(p1, vcvtnq_s32_f32(Cb0));
      vst1q_s32(p1 + 4, vcvtnq_s32_f32(Cb1));
      vst1q_s32(p2, vcvtnq_s32_f32(Cr0));
      vst1q_s32(p2 + 4, vcvtnq_s32_f32(Cr1));
      p0 += 8;
      p1 += 8;
      p2 += 8;
    }
  }
}

// lossless: inverse RCT
void cvt_ycbcr_to_rgb_rev_neon(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width, uint32_t height) {
  int32x4_t vY0, vCb0, vCr0, vG0, vR0, vB0;
  int32x4_t vY1, vCb1, vCr1, vG1, vR1, vB1;

  // process two vectors at a time
  for (uint32_t y = 0; y < height; ++y) {
    int32_t *p0 = sp0 + y * round_up(width, 32U);
    int32_t *p1 = sp1 + y * round_up(width, 32U);
    int32_t *p2 = sp2 + y * round_up(width, 32U);
    int32_t len = static_cast<int32_t>(width);

    for (; len > 0; len -= 8) {
      vY0  = vld1q_s32(p0);
      vCb0 = vld1q_s32(p1);
      vCr0 = vld1q_s32(p2);
      vY1  = vld1q_s32(p0 + 4);
      vCb1 = vld1q_s32(p1 + 4);
      vCr1 = vld1q_s32(p2 + 4);
      vG0  = vsubq_s32(vY0, vshrq_n_s32(vaddq_s32(vCb0, vCr0), 2));
      vG1  = vsubq_s32(vY1, vshrq_n_s32(vaddq_s32(vCb1, vCr1), 2));
      vR0  = vaddq_s32(vCr0, vG0);
      vR1  = vaddq_s32(vCr1, vG1);
      vB0  = vaddq_s32(vCb0, vG0);
      vB1  = vaddq_s32(vCb1, vG1);

      vst1q_s32(p0, vR0);
      vst1q_s32(p0 + 4, vR1);
      vst1q_s32(p1, vG0);
      vst1q_s32(p1 + 4, vG1);
      vst1q_s32(p2, vB0);
      vst1q_s32(p2 + 4, vB1);
      p0 += 8;
      p1 += 8;
      p2 += 8;
      vY0  = vld1q_s32(sp0);
      vCb0 = vld1q_s32(sp1);
      vCr0 = vld1q_s32(sp2);
      vY1  = vld1q_s32(sp0 + 4);
      vCb1 = vld1q_s32(sp1 + 4);
      vCr1 = vld1q_s32(sp2 + 4);
    }
  }
}

// lossy: inverse ICT
void cvt_ycbcr_to_rgb_irrev_neon(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width,
                                 uint32_t height) {
  const float32x4_t fCR_FACT_R = vdupq_n_f32(static_cast<float32_t>(CR_FACT_R));
  const float32x4_t fCB_FACT_B = vdupq_n_f32(static_cast<float32_t>(CB_FACT_B));
  const float32x4_t fCR_FACT_G = vdupq_n_f32(static_cast<float32_t>(CR_FACT_G));
  const float32x4_t fCB_FACT_G = vdupq_n_f32(static_cast<float32_t>(CB_FACT_G));
  for (uint32_t y = 0; y < height; ++y) {
    int32_t *p0 = sp0 + y * round_up(width, 32U);
    int32_t *p1 = sp1 + y * round_up(width, 32U);
    int32_t *p2 = sp2 + y * round_up(width, 32U);
    int32_t len = static_cast<int32_t>(width);
    for (; len > 0; len -= 8) {
      auto Y0  = vcvtq_f32_s32(vld1q_s32(p0));
      auto Y1  = vcvtq_f32_s32(vld1q_s32(p0 + 4));
      auto Cb0 = vcvtq_f32_s32(vld1q_s32(p1));
      auto Cb1 = vcvtq_f32_s32(vld1q_s32(p1 + 4));
      auto Cr0 = vcvtq_f32_s32(vld1q_s32(p2));
      auto Cr1 = vcvtq_f32_s32(vld1q_s32(p2 + 4));

      // TODO: need to consider precision and setting FPSCR register value
      vst1q_s32(p0, vcvtnq_s32_f32(vfmaq_f32(Y0, Cr0, fCR_FACT_R)));
      vst1q_s32(p0 + 4, vcvtnq_s32_f32(vfmaq_f32(Y1, Cr1, fCR_FACT_R)));
      vst1q_s32(p2, vcvtnq_s32_f32(vfmaq_f32(Y0, Cb0, fCB_FACT_B)));
      vst1q_s32(p2 + 4, vcvtnq_s32_f32(vfmaq_f32(Y1, Cb1, fCB_FACT_B)));
      Y0 = vfmsq_f32(Y0, Cr0, fCR_FACT_G);
      vst1q_s32(p1, vcvtnq_s32_f32(vfmsq_f32(Y0, Cb0, fCB_FACT_G)));
      Y1 = vfmsq_f32(Y1, Cr1, fCR_FACT_G);
      vst1q_s32(p1 + 4, vcvtnq_s32_f32(vfmsq_f32(Y1, Cb1, fCB_FACT_G)));
      p0 += 8;
      p1 += 8;
      p2 += 8;
    }
  }
}

  #if 0
// 16bit fixedpoint calculation
void cvt_ycbcr_to_rgb_irrev_neon2(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width,
                                  uint32_t height) {
  const int16x8_t cr_fact_r     = vdupq_n_s16(neon_CRfactR);
  const int16x8_t cb_fact_b     = vdupq_n_s16(neon_CBfactB);
  const int16x8_t cr_neg_fact_g = vdupq_n_s16(neon_neg_CRfactG);
  const int16x8_t cb_neg_fact_g = vdupq_n_s16(neon_neg_CBfactG);
  int16x8_t Y, Cb, Cr, tmp0, tmp1;
  int32x4_t Y32, Cb32, Cr32, tmp;
  for (uint32_t y = 0; y < height; ++y) {
    int32_t *src1 = sp0 + y * round_up(width, 32U);
    int32_t *src2 = sp1 + y * round_up(width, 32U);
    int32_t *src3 = sp2 + y * round_up(width, 32U);
    int32_t len   = static_cast<int32_t>(width);
    for (; len > 0; len -= 8) {
      Y32  = vld1q_s32(src1);
      Y    = vcombine_s16(vmovn_s32(Y32), vmovn_s32(vld1q_s32(src1 + 4)));
      Cr32 = vld1q_s32(src3);
      Cr   = vcombine_s16(vmovn_s32(Cr32), vmovn_s32(vld1q_s32(src3 + 4)));
      tmp0 = vqrdmulhq_s16(Cr, cr_fact_r);
      tmp0 = vaddq_s16(tmp0, Cr);
      tmp1 = vaddq_s16(tmp0, Y);
      vst1q_s32(src1, vmovl_s16(vget_low_s16(tmp1)));  // Save Red
      vst1q_s32(src1 + 4, vmovl_high_s16(tmp1));       // Save Red
      Cr   = vqrdmulhq_s16(Cr, cr_neg_fact_g);
      Cb32 = vld1q_s32(src2);  // Load CB
      Cb   = vcombine_s16(vmovn_s32(Cb32), vmovn_s32(vld1q_s32(src2 + 4)));
      tmp0 = vqrdmulhq_s16(Cb, cb_fact_b);
      tmp0 = vaddq_s16(tmp0, Cb);
      tmp1 = vaddq_s16(tmp0, Y);
      vst1q_s32(src3, vmovl_s16(vget_low_s16(tmp1)));  // Save Blue
      vst1q_s32(src3 + 4, vmovl_high_s16(tmp1));       // Save Blue
      Cb   = vqrdmulhq_s16(Cb, cb_neg_fact_g);
      Y    = vqaddq_s16(Y, Cr);
      tmp1 = vqaddq_s16(Y, Cb);
      vst1q_s32(src2, vmovl_s16(vget_low_s16(tmp1)));  // Save Green
      vst1q_s32(src2 + 4, vmovl_high_s16(tmp1));       // Save Green
      src1 += 8;
      src3 += 8;
      src2 += 8;
    }
  }
}
  #endif
#endif