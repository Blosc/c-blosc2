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

#if not defined(OPENHTJ2K_TRY_AVX2) || not defined(__AVX2__) || not defined(OPENHTJ2K_ENABLE_ARM_NEON)
  #include "color.hpp"

void cvt_rgb_to_ycbcr_rev(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width, uint32_t height) {
  int32_t R, G, B;
  int32_t Y, Cb, Cr;
  for (uint32_t y = 0; y < height; ++y) {
    int32_t *p0 = sp0 + y * round_up(width, 32U);
    int32_t *p1 = sp1 + y * round_up(width, 32U);
    int32_t *p2 = sp2 + y * round_up(width, 32U);
    for (uint32_t n = 0; n < width; n++) {
      R     = p0[n];
      G     = p1[n];
      B     = p2[n];
      Y     = (R + 2 * G + B) >> 2;
      Cb    = B - G;
      Cr    = R - G;
      p0[n] = Y;
      p1[n] = Cb;
      p2[n] = Cr;
    }
  }
}

void cvt_rgb_to_ycbcr_irrev(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width, uint32_t height) {
  double fR, fG, fB;
  double fY, fCb, fCr;
  for (uint32_t y = 0; y < height; ++y) {
    int32_t *p0 = sp0 + y * round_up(width, 32U);
    int32_t *p1 = sp1 + y * round_up(width, 32U);
    int32_t *p2 = sp2 + y * round_up(width, 32U);
    for (uint32_t n = 0; n < width; n++) {
      fR    = static_cast<double>(p0[n]);
      fG    = static_cast<double>(p1[n]);
      fB    = static_cast<double>(p2[n]);
      fY    = ALPHA_R * fR + ALPHA_G * fG + ALPHA_B * fB;
      fCb   = (1.0 / CB_FACT_B) * (fB - fY);
      fCr   = (1.0 / CR_FACT_R) * (fR - fY);
      p0[n] = round_d(fY);
      p1[n] = round_d(fCb);
      p2[n] = round_d(fCr);
    }
  }
}

void cvt_ycbcr_to_rgb_rev(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width, uint32_t height) {
  int32_t R, G, B;
  int32_t Y, Cb, Cr;
  for (uint32_t y = 0; y < height; ++y) {
    int32_t *p0 = sp0 + y * round_up(width, 32U);
    int32_t *p1 = sp1 + y * round_up(width, 32U);
    int32_t *p2 = sp2 + y * round_up(width, 32U);
    for (uint32_t n = 0; n < width; n++) {
      Y     = p0[n];
      Cb    = p1[n];
      Cr    = p2[n];
      G     = Y - ((Cb + Cr) >> 2);
      R     = Cr + G;
      B     = Cb + G;
      p0[n] = R;
      p1[n] = G;
      p2[n] = B;
    }
  }
}

void cvt_ycbcr_to_rgb_irrev(int32_t *sp0, int32_t *sp1, int32_t *sp2, uint32_t width, uint32_t height) {
  int32_t R, G, B;
  double fY, fCb, fCr;
  for (uint32_t y = 0; y < height; ++y) {
    int32_t *p0 = sp0 + y * round_up(width, 32U);
    int32_t *p1 = sp1 + y * round_up(width, 32U);
    int32_t *p2 = sp2 + y * round_up(width, 32U);
    for (uint32_t n = 0; n < width; n++) {
      fY    = static_cast<double>(p0[n]);
      fCb   = static_cast<double>(p1[n]);
      fCr   = static_cast<double>(p2[n]);
      R     = static_cast<int32_t>(round_d(fY + CR_FACT_R * fCr));
      B     = static_cast<int32_t>(round_d(fY + CB_FACT_B * fCb));
      G     = static_cast<int32_t>(round_d(fY - CR_FACT_G * fCr - CB_FACT_G * fCb));
      p0[n] = R;
      p1[n] = G;
      p2[n] = B;
    }
  }
}
#endif