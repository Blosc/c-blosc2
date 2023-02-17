// Copyright (c) 2019 - 2022, Osamu Watanabe
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
  #include "coding_units.hpp"
  #include "dec_CxtVLC_tables.hpp"
  #include "ht_block_decoding.hpp"
  #include "coding_local.hpp"
  #include "utils.hpp"

  #include <arm_neon.h>

uint8_t j2k_codeblock::calc_mbr(const uint32_t i, const uint32_t j, const uint8_t causal_cond) const {
  uint8_t *state_p0 = block_states + i * blkstate_stride + j;
  uint8_t *state_p1 = block_states + (i + 1) * blkstate_stride + j;
  uint8_t *state_p2 = block_states + (i + 2) * blkstate_stride + j;

  uint8_t mbr0 = state_p0[0] | state_p0[1] | state_p0[2];
  uint8_t mbr1 = state_p1[0] | state_p1[2];
  uint8_t mbr2 = state_p2[0] | state_p2[1] | state_p2[2];
  uint8_t mbr  = mbr0 | mbr1 | (mbr2 & causal_cond);
  mbr |= (mbr0 >> SHIFT_REF) & (mbr0 >> SHIFT_SCAN);
  mbr |= (mbr1 >> SHIFT_REF) & (mbr1 >> SHIFT_SCAN);
  mbr |= (mbr2 >> SHIFT_REF) & (mbr2 >> SHIFT_SCAN) & causal_cond;

  //  uint8_t mbr = state_p0[0];
  //  mbr |= state_p0[1];
  //  mbr |= state_p0[2];
  //  mbr |= state_p1[0];
  //  mbr |= state_p1[2];
  //  mbr |= (state_p2[0]) & causal_cond;
  //  mbr |= (state_p2[1]) & causal_cond;
  //  mbr |= (state_p2[2]) & causal_cond;
  //
  //  mbr |= ((state_p0[0] >> SHIFT_REF)) & ((state_p0[0] >> SHIFT_SCAN));
  //  mbr |= ((state_p0[1] >> SHIFT_REF)) & ((state_p0[1] >> SHIFT_SCAN));
  //  mbr |= ((state_p0[2] >> SHIFT_REF)) & ((state_p0[2] >> SHIFT_SCAN));
  //
  //  mbr |= ((state_p1[0] >> SHIFT_REF)) & ((state_p1[0] >> SHIFT_SCAN));
  //  mbr |= ((state_p1[2] >> SHIFT_REF)) & ((state_p1[2] >> SHIFT_SCAN));
  //
  //  mbr |= ((state_p2[0] >> SHIFT_REF)) & ((state_p2[0] >> SHIFT_SCAN)) & causal_cond;
  //  mbr |= ((state_p2[1] >> SHIFT_REF)) & ((state_p2[1] >> SHIFT_SCAN)) & causal_cond;
  //  mbr |= ((state_p2[2] >> SHIFT_REF)) & ((state_p2[2] >> SHIFT_SCAN)) & causal_cond;

  //  const int16_t im1 = static_cast<int16_t>(i - 1);
  //  const int16_t jm1 = static_cast<int16_t>(j - 1);
  //  const int16_t ip1 = static_cast<int16_t>(i + 1);
  //  const int16_t jp1 = static_cast<int16_t>(j + 1);
  //  uint8_t mbr       = get_state(Sigma, im1, jm1);
  //  mbr         = mbr | get_state(Sigma, im1, j);
  //  mbr = mbr | get_state(Sigma, im1, jp1);
  //  mbr = mbr | get_state(Sigma, i, jm1);
  //  mbr = mbr | get_state(Sigma, i, jp1);
  //  mbr = mbr | static_cast<uint8_t>(get_state(Sigma, ip1, jm1) * causal_cond);
  //  mbr = mbr | static_cast<uint8_t>(get_state(Sigma, ip1, j) * causal_cond);
  //  mbr = mbr | static_cast<uint8_t>(get_state(Sigma, ip1, jp1) * causal_cond);
  //  mbr = mbr | static_cast<uint8_t>(get_state(Refinement_value, im1, jm1) * get_state(Scan, im1, jm1));
  //  mbr = mbr | static_cast<uint8_t>(get_state(Refinement_value, im1, j) * get_state(Scan, im1, j));
  //  mbr = mbr | static_cast<uint8_t>(get_state(Refinement_value, im1, jp1) * get_state(Scan, im1, jp1));
  //  mbr = mbr | static_cast<uint8_t>(get_state(Refinement_value, i, jm1) * get_state(Scan, i, jm1));
  //  mbr = mbr | static_cast<uint8_t>(get_state(Refinement_value, i, jp1) * get_state(Scan, i, jp1));
  //  mbr = mbr
  //        | static_cast<uint8_t>(get_state(Refinement_value, ip1, jm1) * get_state(Scan, ip1, jm1)
  //                               * causal_cond);
  //  mbr = mbr
  //        | static_cast<uint8_t>(get_state(Refinement_value, ip1, j) * get_state(Scan, ip1, j) *
  //        causal_cond);
  //  mbr = mbr
  //        | static_cast<uint8_t>(get_state(Refinement_value, ip1, jp1) * get_state(Scan, ip1, jp1)
  //                               * causal_cond);
  return mbr & 1;
}

void ht_cleanup_decode(j2k_codeblock *block, const uint8_t &pLSB, const int32_t Lcup, const int32_t Pcup,
                       const int32_t Scup) {
  fwd_buf<0xFF> MagSgn(block->get_compressed_data(), Pcup);
  MEL_dec MEL(block->get_compressed_data(), Lcup, Scup);
  rev_buf VLC_dec(block->get_compressed_data(), Lcup, Scup);

  const uint16_t QW = static_cast<uint16_t>(ceil_int(static_cast<int16_t>(block->size.x), 2));
  const uint16_t QH = static_cast<uint16_t>(ceil_int(static_cast<int16_t>(block->size.y), 2));

  int32x4_t vExp;
  const int32_t mask[4]  = {1, 2, 4, 8};
  const int32x4_t vm     = vld1q_s32(mask);
  const int32x4_t vone   = vdupq_n_s32(1);
  const int32x4_t vtwo   = vdupq_n_s32(2);
  const int32x4_t vshift = vdupq_n_s32(pLSB - 1);

  auto mp0 = block->sample_buf;
  auto mp1 = block->sample_buf + block->blksampl_stride;
  auto sp0 = block->block_states + 1 + block->blkstate_stride;
  auto sp1 = block->block_states + 1 + 2 * block->blkstate_stride;

  uint32_t rho0, rho1;
  uint32_t u_off0, u_off1;
  uint32_t emb_k_0, emb_k_1;
  uint32_t emb_1_0, emb_1_1;
  uint32_t u0, u1;
  uint32_t U0, U1;
  uint8_t gamma0, gamma1;
  uint32_t kappa0 = 1, kappa1 = 1;  // kappa is always 1 for initial line-pair

  const uint16_t *dec_table0, *dec_table1;
  dec_table0 = dec_CxtVLC_table0_fast_16;
  dec_table1 = dec_CxtVLC_table1_fast_16;

  alignas(32) auto rholine = MAKE_UNIQUE<uint32_t[]>(QW + 4U);
  rholine[0]               = 0;
  auto rho_p               = rholine.get() + 1;
  alignas(32) auto Eline   = MAKE_UNIQUE<int32_t[]>(2U * QW + 8U);
  Eline[0]                 = 0;
  auto E_p                 = Eline.get() + 1;

  uint32_t context = 0;
  uint32_t vlcval;
  int32_t mel_run = MEL.get_run();

  int32_t qx;
  // Initial line-pair
  for (qx = QW; qx > 0; qx -= 2) {
    // Decoding of significance and EMB patterns and unsigned residual offsets
    vlcval       = VLC_dec.fetch();
    uint16_t tv0 = dec_table0[(vlcval & 0x7F) + context];
    if (context == 0) {
      mel_run -= 2;
      tv0 = (mel_run == -1) ? tv0 : 0;
      if (mel_run < 0) {
        mel_run = MEL.get_run();
      }
    }

    rho0    = (tv0 & 0x00F0) >> 4;
    emb_k_0 = (tv0 & 0xF000) >> 12;
    emb_1_0 = (tv0 & 0x0F00) >> 8;

    // calculate context for the next quad
    context = ((tv0 & 0xE0U) << 2) | ((tv0 & 0x10U) << 3);

    // Decoding of significance and EMB patterns and unsigned residual offsets
    vlcval       = VLC_dec.advance((tv0 & 0x000F) >> 1);
    uint16_t tv1 = dec_table0[(vlcval & 0x7F) + context];
    if (context == 0 && qx > 1) {
      mel_run -= 2;
      tv1 = (mel_run == -1) ? tv1 : 0;
      if (mel_run < 0) {
        mel_run = MEL.get_run();
      }
    }
    tv1     = (qx > 1) ? tv1 : 0;
    rho1    = (tv1 & 0x00F0) >> 4;
    emb_k_1 = (tv1 & 0xF000) >> 12;
    emb_1_1 = (tv1 & 0x0F00) >> 8;

    // store sigma
    *sp0++   = (rho0 >> 0) & 1;
    *sp0++   = (rho0 >> 2) & 1;
    *sp0++   = (rho1 >> 0) & 1;
    *sp0++   = (rho1 >> 2) & 1;
    *sp1++   = (rho0 >> 1) & 1;
    *sp1++   = (rho0 >> 3) & 1;
    *sp1++   = (rho1 >> 1) & 1;
    *sp1++   = (rho1 >> 3) & 1;
    *rho_p++ = rho0;
    *rho_p++ = rho1;

    // calculate context for the next quad
    context = ((tv1 & 0xE0U) << 2) | ((tv1 & 0x10U) << 3);

    // UVLC decoding
    vlcval = VLC_dec.advance((tv1 & 0x000F) >> 1);
    u_off0 = tv0 & 1;
    u_off1 = tv1 & 1;

    uint32_t mel_offset = 0;
    if (u_off0 == 1 && u_off1 == 1) {
      mel_run -= 2;
      mel_offset = (mel_run == -1) ? 0x40 : 0;
      if (mel_run < 0) {
        mel_run = MEL.get_run();
      }
    }
    uint32_t idx         = (vlcval & 0x3F) + (u_off0 << 6U) + (u_off1 << 7U) + mel_offset;
    uint32_t uvlc_result = uvlc_dec_0[idx];
    // remove total prefix length
    vlcval = VLC_dec.advance(uvlc_result & 0x7);
    uvlc_result >>= 3;
    // extract suffixes for quad 0 and 1
    uint32_t len = uvlc_result & 0xF;            // suffix length for 2 quads (up to 10 = 5 + 5)
    uint32_t tmp = vlcval & ((1U << len) - 1U);  // suffix value for 2 quads
    VLC_dec.advance(len);
    uvlc_result >>= 4;
    // quad 0 length
    len = uvlc_result & 0x7;  // quad 0 suffix length
    uvlc_result >>= 3;
    u0 = (uvlc_result & 7) + (tmp & ~(0xFFU << len));
    u1 = (uvlc_result >> 3) + (tmp >> len);

    U0 = kappa0 + u0;
    U1 = kappa1 + u1;

    // NEON section
    int32x4_t vmask1, sig0, sig1, vtmp, m_n_0, m_n_1, msvec, v_n_0, v_n_1, mu0, mu1;

    sig0 = vdupq_n_u32(rho0);
    sig0 = vtstq_s32(sig0, vm);
    // k_n in the spec can be derived from emb_k
    vtmp  = vandq_s32(vtstq_s32(vdupq_n_u32(emb_k_0), vm), vone);
    m_n_0 = vsubq_s32(vandq_s32(sig0, vdupq_n_u32(U0)), vtmp);
    sig1  = vdupq_n_u32(rho1);
    sig1  = vtstq_s32(sig1, vm);
    // k_n in the spec can be derived from emb_k
    vtmp  = vandq_s32(vtstq_s32(vdupq_n_u32(emb_k_1), vm), vone);
    m_n_1 = vsubq_s32(vandq_s32(sig1, vdupq_n_u32(U1)), vtmp);

    /******************************** RecoverMagSgnValue step ****************************************/

    vmask1 = vsubq_u32(vshlq_u32(vone, m_n_0), vone);
    // retrieve MagSgn codewords
    msvec = MagSgn.fetch(m_n_0);
    //      MagSgn.advance(vaddvq_u32(m_n_0));
    v_n_0 = vandq_u32(msvec, vmask1);
    // i_n in the spec can be derived from emb_^{-1}
    vtmp  = vandq_s32(vtstq_s32(vdupq_n_u32(emb_1_0), vm), vone);
    v_n_0 = vorrq_u32(v_n_0, vshlq_u32(vtmp, m_n_0));  // v = 2(mu-1) + sign (0 or 1)
    mu0   = vaddq_u32(v_n_0, vtwo);                    // 2(mu-1) + sign + 2 = 2mu + sign
    // Add center bin (would be used for lossy and truncated lossless codestreams)
    mu0 = vorrq_s32(mu0, vone);  // This cancels the effect of a sign bit in LSB
    mu0 = vshlq_u32(mu0, vshift);
    mu0 = vorrq_u32(mu0, vshlq_n_u32(v_n_0, 31));
    mu0 = vandq_u32(mu0, sig0);

    vmask1 = vsubq_u32(vshlq_u32(vone, m_n_1), vone);
    // retrieve MagSgn codewords
    msvec = MagSgn.fetch(m_n_1);
    //      MagSgn.advance(vaddvq_u32(m_n_1));
    v_n_1 = vandq_u32(msvec, vmask1);
    // i_n in the spec can be derived from emb_^{-1}
    vtmp  = vandq_s32(vtstq_s32(vdupq_n_u32(emb_1_1), vm), vone);
    v_n_1 = vorrq_u32(v_n_1, vshlq_u32(vtmp, m_n_1));  // v = 2(mu-1) + sign (0 or 1)
    mu1   = vaddq_u32(v_n_1, vtwo);                    // 2(mu-1) + sign + 2 = 2mu + sign
    // Add center bin (would be used for lossy and truncated lossless codestreams)
    mu1 = vorrq_s32(mu1, vone);  // This cancels the effect of a sign bit in LSB
    mu1 = vshlq_u32(mu1, vshift);
    mu1 = vorrq_u32(mu1, vshlq_n_u32(v_n_1, 31));
    mu1 = vandq_u32(mu1, sig1);

    // store mu
    int32x4x2_t t = vzipq_s32(mu0, mu1);
    vst1q_s32(mp0, vzip1q_s32(t.val[0], t.val[1]));
    vst1q_s32(mp1, vzip2q_s32(t.val[0], t.val[1]));
    mp0 += 4;
    mp1 += 4;

    // update Exponent
    t    = vzipq_s32(v_n_0, v_n_1);
    vExp = vsubq_s32(vdupq_n_s32(32), vclzq_s32(vzip2q_s32(t.val[0], t.val[1])));
    vst1q_s32(E_p, vExp);
    E_p += 4;
  }

  // Initial line-pair end

  /*******************************************************************************************************************/
  // Non-initial line-pair
  /*******************************************************************************************************************/
  for (uint16_t row = 1; row < QH; row++) {
    rho_p = rholine.get() + 1;
    E_p   = Eline.get() + 1;
    mp0   = block->sample_buf + (row * 2U) * block->blksampl_stride;
    mp1   = block->sample_buf + (row * 2U + 1U) * block->blksampl_stride;
    sp0   = block->block_states + (row * 2U + 1U) * block->blkstate_stride + 1U;
    sp1   = block->block_states + (row * 2U + 2U) * block->blkstate_stride + 1U;
    rho1  = 0;

    int32_t Emax0, Emax1;
    // calculate Emax for the next two quads
    Emax0 = vmaxvq_s32(vld1q_s32(E_p - 1));
    Emax1 = vmaxvq_s32(vld1q_s32(E_p + 1));

    // calculate context for the next quad
    context = ((rho1 & 0x4) << 6) | ((rho1 & 0x8) << 5);            // (w | sw) << 8
    context |= ((rho_p[-1] & 0x8) << 4) | ((rho_p[0] & 0x2) << 6);  // (nw | n) << 7
    context |= ((rho_p[0] & 0x8) << 6) | ((rho_p[1] & 0x2) << 8);   // (ne | nf) << 9

    for (qx = QW; qx > 0; qx -= 2) {
      // Decoding of significance and EMB patterns and unsigned residual offsets
      vlcval       = VLC_dec.fetch();
      uint16_t tv0 = dec_table1[(vlcval & 0x7F) + context];
      if (context == 0) {
        mel_run -= 2;
        tv0 = (mel_run == -1) ? tv0 : 0;
        if (mel_run < 0) {
          mel_run = MEL.get_run();
        }
      }

      rho0    = (tv0 & 0x00F0) >> 4;
      emb_k_0 = (tv0 & 0xF000) >> 12;
      emb_1_0 = (tv0 & 0x0F00) >> 8;

      vlcval = VLC_dec.advance((tv0 & 0x000F) >> 1);

      // calculate context for the next quad
      context = ((rho0 & 0x4) << 6) | ((rho0 & 0x8) << 5);           // (w | sw) << 8
      context |= ((rho_p[0] & 0x8) << 4) | ((rho_p[1] & 0x2) << 6);  // (nw | n) << 7
      context |= ((rho_p[1] & 0x8) << 6) | ((rho_p[2] & 0x2) << 8);  // (ne | nf) << 9

      // Decoding of significance and EMB patterns and unsigned residual offsets
      uint16_t tv1 = dec_table1[(vlcval & 0x7F) + context];
      if (context == 0 && qx > 1) {
        mel_run -= 2;
        tv1 = (mel_run == -1) ? tv1 : 0;
        if (mel_run < 0) {
          mel_run = MEL.get_run();
        }
      }
      tv1     = (qx > 1) ? tv1 : 0;
      rho1    = (tv1 & 0x00F0) >> 4;
      emb_k_1 = (tv1 & 0xF000) >> 12;
      emb_1_1 = (tv1 & 0x0F00) >> 8;

      // calculate context for the next quad
      context = ((rho1 & 0x4) << 6) | ((rho1 & 0x8) << 5);           // (w | sw) << 8
      context |= ((rho_p[1] & 0x8) << 4) | ((rho_p[2] & 0x2) << 6);  // (nw | n) << 7
      context |= ((rho_p[2] & 0x8) << 6) | ((rho_p[3] & 0x2) << 8);  // (ne | nf) << 9

      // store sigma
      *sp0++ = (rho0 >> 0) & 1;
      *sp0++ = (rho0 >> 2) & 1;
      *sp0++ = (rho1 >> 0) & 1;
      *sp0++ = (rho1 >> 2) & 1;
      *sp1++ = (rho0 >> 1) & 1;
      *sp1++ = (rho0 >> 3) & 1;
      *sp1++ = (rho1 >> 1) & 1;
      *sp1++ = (rho1 >> 3) & 1;
      // Update rho_p
      *rho_p++ = rho0;
      *rho_p++ = rho1;

      vlcval = VLC_dec.advance((tv1 & 0x000F) >> 1);

      // UVLC decoding
      u_off0       = tv0 & 1;
      u_off1       = tv1 & 1;
      uint32_t idx = (vlcval & 0x3F) + (u_off0 << 6U) + (u_off1 << 7U);

      uint32_t uvlc_result = uvlc_dec_1[idx];
      // remove total prefix length
      vlcval = VLC_dec.advance(uvlc_result & 0x7);
      uvlc_result >>= 3;
      // extract suffixes for quad 0 and 1
      uint32_t len = uvlc_result & 0xF;            // suffix length for 2 quads (up to 10 = 5 + 5)
      uint32_t tmp = vlcval & ((1U << len) - 1U);  // suffix value for 2 quads
      VLC_dec.advance(len);

      uvlc_result >>= 4;
      // quad 0 length
      len = uvlc_result & 0x7;  // quad 0 suffix length
      uvlc_result >>= 3;
      u0 = (uvlc_result & 7) + (tmp & ~(0xFFU << len));
      u1 = (uvlc_result >> 3) + (tmp >> len);

      gamma0 = ((rho0 & (rho0 - 1)) == 0) ? 0 : 1;  // (popcount32(rho0) < 2) ? 0 : 1;
      gamma1 = ((rho1 & (rho1 - 1)) == 0) ? 0 : 1;  // (popcount32(rho1) < 2) ? 0 : 1;
      kappa0 = (1 > gamma0 * (Emax0 - 1)) ? 1U : static_cast<uint32_t>(Emax0 - 1);
      kappa1 = (1 > gamma1 * (Emax1 - 1)) ? 1U : static_cast<uint32_t>(Emax1 - 1);
      U0     = kappa0 + u0;
      U1     = kappa1 + u1;

      // NEON section
      int32x4_t vmask1, sig0, sig1, vtmp, m_n_0, m_n_1, msvec, v_n_0, v_n_1, mu0, mu1;

      sig0 = vdupq_n_u32(rho0);
      sig0 = vtstq_s32(sig0, vm);
      // k_n in the spec can be derived from emb_k
      vtmp  = vandq_s32(vtstq_s32(vdupq_n_u32(emb_k_0), vm), vone);
      m_n_0 = vsubq_s32(vandq_s32(sig0, vdupq_n_u32(U0)), vtmp);
      sig1  = vdupq_n_u32(rho1);
      sig1  = vtstq_s32(sig1, vm);
      // k_n in the spec can be derived from emb_k
      vtmp  = vandq_s32(vtstq_s32(vdupq_n_u32(emb_k_1), vm), vone);
      m_n_1 = vsubq_s32(vandq_s32(sig1, vdupq_n_u32(U1)), vtmp);

      /******************************** RecoverMagSgnValue step ****************************************/

      vmask1 = vsubq_u32(vshlq_u32(vone, m_n_0), vone);
      // retrieve MagSgn codewords
      msvec = MagSgn.fetch(m_n_0);
      //      MagSgn.advance(vaddvq_u32(m_n_0));
      v_n_0 = vandq_u32(msvec, vmask1);
      // i_n in the spec can be derived from emb_^{-1}
      vtmp  = vandq_s32(vtstq_s32(vdupq_n_u32(emb_1_0), vm), vone);
      v_n_0 = vorrq_u32(v_n_0, vshlq_u32(vtmp, m_n_0));  // v = 2(mu-1) + sign (0 or 1)
      mu0   = vaddq_u32(v_n_0, vtwo);                    // 2(mu-1) + sign + 2 = 2mu + sign
      // Add center bin (would be used for lossy and truncated lossless codestreams)
      mu0 = vorrq_s32(mu0, vone);  // This cancels the effect of a sign bit in LSB
      mu0 = vshlq_u32(mu0, vshift);
      mu0 = vorrq_u32(mu0, vshlq_n_u32(v_n_0, 31));
      mu0 = vandq_u32(mu0, sig0);

      vmask1 = vsubq_u32(vshlq_u32(vone, m_n_1), vone);
      // retrieve MagSgn codewords
      msvec = MagSgn.fetch(m_n_1);
      //      MagSgn.advance(vaddvq_u32(m_n_1));
      v_n_1 = vandq_u32(msvec, vmask1);
      // i_n in the spec can be derived from emb_^{-1}
      vtmp  = vandq_s32(vtstq_s32(vdupq_n_u32(emb_1_1), vm), vone);
      v_n_1 = vorrq_u32(v_n_1, vshlq_u32(vtmp, m_n_1));  // v = 2(mu-1) + sign (0 or 1)
      mu1   = vaddq_u32(v_n_1, vtwo);                    // 2(mu-1) + sign + 2 = 2mu + sign
      // Add center bin (would be used for lossy and truncated lossless codestreams)
      mu1 = vorrq_s32(mu1, vone);  // This cancels the effect of a sign bit in LSB
      mu1 = vshlq_u32(mu1, vshift);
      mu1 = vorrq_u32(mu1, vshlq_n_u32(v_n_1, 31));
      mu1 = vandq_u32(mu1, sig1);

      // store mu
      int32x4x2_t t = vzipq_s32(mu0, mu1);
      vst1q_s32(mp0, vzip1q_s32(t.val[0], t.val[1]));
      vst1q_s32(mp1, vzip2q_s32(t.val[0], t.val[1]));
      mp0 += 4;
      mp1 += 4;

      // calculate Emax for the next two quads
      Emax0 = vmaxvq_s32(vld1q_s32(E_p + 3));
      Emax1 = vmaxvq_s32(vld1q_s32(E_p + 5));

      // Update Exponent
      t    = vzipq_s32(v_n_0, v_n_1);
      vExp = vsubq_s32(vdupq_n_s32(32), vclzq_s32(vzip2q_s32(t.val[0], t.val[1])));
      vst1q_s32(E_p, vExp);
      E_p += 4;
    }
  }  // Non-Initial line-pair end
}  // Cleanup decoding end

auto process_stripes_block_dec = [](SP_dec &SigProp, j2k_codeblock *block, const uint32_t i_start,
                                    const uint32_t j_start, const uint32_t width, const uint32_t height,
                                    const uint8_t &pLSB) {
  int32_t *sp;
  uint8_t causal_cond = 0;
  uint8_t bit;
  uint8_t mbr;
  const auto block_width  = j_start + width;
  const auto block_height = i_start + height;

  // Decode magnitude
  for (uint32_t j = j_start; j < block_width; j++) {
    for (uint32_t i = i_start; i < block_height; i++) {
      sp               = &block->sample_buf[j + i * block->blksampl_stride];
      causal_cond      = (((block->Cmodes & CAUSAL) == 0) || (i != block_height - 1));
      mbr              = 0;
      uint8_t *state_p = block->block_states + (i + 1) * block->blkstate_stride + (j + 1);
      if ((state_p[0] >> SHIFT_SIGMA & 1) == 0) {
        mbr = block->calc_mbr(i, j, causal_cond);
      }
      if (mbr != 0) {
        //        block->modify_state(refinement_indicator, 1, i, j);
        state_p[0] |= 1 << SHIFT_PI_;
        bit = SigProp.importSigPropBit();
        //        block->modify_state(refinement_value, bit, i, j);
        state_p[0] |= bit << SHIFT_REF;
        *sp |= bit << pLSB;
        *sp |= bit << (pLSB - 1);  // new bin center ( = 0.5)
      }
      //      block->modify_state(scan, 1, i, j);
      state_p[0] |= 1 << SHIFT_SCAN;
    }
  }
  // Decode sign
  for (uint32_t j = j_start; j < block_width; j++) {
    for (uint32_t i = i_start; i < block_height; i++) {
      sp               = &block->sample_buf[j + i * block->blksampl_stride];
      uint8_t *state_p = block->block_states + (i + 1) * block->blkstate_stride + (j + 1);
      //      if ((*sp & (1 << pLSB)) != 0) {
      if ((state_p[0] >> SHIFT_REF) & 1) {
        *sp |= static_cast<int32_t>(SigProp.importSigPropBit()) << 31;
      }
    }
  }
};

void ht_sigprop_decode(j2k_codeblock *block, uint8_t *HT_magref_segment, uint32_t magref_length,
                       const uint8_t &pLSB) {
  SP_dec SigProp(HT_magref_segment, magref_length);
  const uint32_t num_v_stripe = block->size.y / 4;
  const uint32_t num_h_stripe = block->size.x / 4;
  uint32_t i_start            = 0, j_start;
  uint32_t width              = 4;
  uint32_t width_last;
  uint32_t height = 4;

  // decode full-height (=4) stripes
  for (uint16_t n1 = 0; n1 < num_v_stripe; n1++) {
    j_start = 0;
    for (uint16_t n2 = 0; n2 < num_h_stripe; n2++) {
      process_stripes_block_dec(SigProp, block, i_start, j_start, width, height, pLSB);
      j_start += 4;
    }
    width_last = block->size.x % 4;
    if (width_last) {
      process_stripes_block_dec(SigProp, block, i_start, j_start, width_last, height, pLSB);
    }
    i_start += 4;
  }
  // decode remaining height stripes
  height  = block->size.y % 4;
  j_start = 0;
  for (uint16_t n2 = 0; n2 < num_h_stripe; n2++) {
    process_stripes_block_dec(SigProp, block, i_start, j_start, width, height, pLSB);
    j_start += 4;
  }
  width_last = block->size.x % 4;
  if (width_last) {
    process_stripes_block_dec(SigProp, block, i_start, j_start, width_last, height, pLSB);
  }
}

void ht_magref_decode(j2k_codeblock *block, uint8_t *HT_magref_segment, uint32_t magref_length,
                      const uint8_t &pLSB) {
  MR_dec MagRef(HT_magref_segment, magref_length);
  const uint32_t blk_height   = block->size.y;
  const uint32_t blk_width    = block->size.x;
  const uint32_t num_v_stripe = block->size.y / 4;
  uint32_t i_start            = 0;
  uint32_t height             = 4;
  int32_t *sp;
  int32_t bit;
  int32_t tmp;
  for (uint32_t n1 = 0; n1 < num_v_stripe; n1++) {
    for (uint32_t j = 0; j < blk_width; j++) {
      for (uint32_t i = i_start; i < i_start + height; i++) {
        sp               = &block->sample_buf[j + i * block->blksampl_stride];
        uint8_t *state_p = block->block_states + (i + 1) * block->blkstate_stride + (j + 1);
        //        if (block->get_state(Sigma, i, j) != 0) {
        if ((state_p[0] >> SHIFT_SIGMA & 1) != 0) {
          //          block->modify_state(refinement_indicator, 1, i, j);
          state_p[0] |= 1 << SHIFT_PI_;
          bit = MagRef.importMagRefBit();
          tmp = static_cast<int32_t>(0xFFFFFFFE | static_cast<unsigned int>(bit));
          tmp <<= pLSB;
          sp[0] &= tmp;
          sp[0] |= 1 << (pLSB - 1);  // new bin center ( = 0.5)
        }
      }
    }
    i_start += 4;
  }
  height = blk_height % 4;
  for (uint32_t j = 0; j < blk_width; j++) {
    for (uint32_t i = i_start; i < i_start + height; i++) {
      sp               = &block->sample_buf[j + i * block->blksampl_stride];
      uint8_t *state_p = block->block_states + (i + 1) * block->blkstate_stride + (j + 1);
      //        if (block->get_state(Sigma, i, j) != 0) {
      if ((state_p[0] >> SHIFT_SIGMA & 1) != 0) {
        //          block->modify_state(refinement_indicator, 1, i, j);
        state_p[0] |= 1 << SHIFT_PI_;
        bit = MagRef.importMagRefBit();
        tmp = static_cast<int32_t>(0xFFFFFFFE | static_cast<unsigned int>(bit));
        tmp <<= pLSB;
        sp[0] &= tmp;
        sp[0] |= 1 << (pLSB - 1);  // new bin center ( = 0.5)
      }
    }
  }
}

void j2k_codeblock::dequantize(uint8_t ROIshift) const {
  // number of decoded magnitude bit‐planes
  const int32_t pLSB = 31 - M_b;  // indicates binary point;

  // bit mask for ROI detection
  const uint32_t mask  = UINT32_MAX >> (M_b + 1);
  const auto vmask     = vdupq_n_s32(static_cast<int32_t>(~mask));
  const auto vROIshift = vdupq_n_s32(ROIshift);

  // vdst0, vdst1 cannot be auto for gcc
  int32x4_t v0, v1, s0, s1, vROImask, vmagmask, vdst0, vdst1;
  vmagmask = vdupq_n_s32(INT32_MAX);
  if (this->transformation) {
    // lossless path
    for (size_t i = 0; i < static_cast<size_t>(this->size.y); i++) {
      int32_t *val = this->sample_buf + i * this->blksampl_stride;
      sprec_t *dst = this->i_samples + i * this->band_stride;
      size_t len   = this->size.x;
      v0           = vld1q_s32(val);
      v1           = vld1q_s32(val + 4);
      for (; len >= 8; len -= 8) {  // dequantize two vectors at a time
        s0 = vshrq_n_s32(v0, 31);   // generate a mask for negative values
        s1 = vshrq_n_s32(v1, 31);   // generate a mask for negative values
        v0 = vandq_s32(v0, vmagmask);
        v1 = vandq_s32(v1, vmagmask);
        // upshift background region, if necessary
        vROImask = vandq_s32(v0, vmask);
        vROImask = vceqzq_s32(vROImask);
        vROImask &= vROIshift;
        v0       = vshlq_s32(v0, vROImask - pLSB);
        vROImask = vandq_s32(v1, vmask);
        vROImask = vceqzq_s32(vROImask);
        vROImask &= vROIshift;
        v1 = vshlq_s32(v1, vROImask - pLSB);
        // convert values from sign-magnitude form to two's complement one
        vdst0 = vbslq_s32(vreinterpretq_u32_s32(s0), vnegq_s32(v0), v0);
        vdst1 = vbslq_s32(vreinterpretq_u32_s32(s1), vnegq_s32(v1), v1);
        vst1q_s16(dst, vcombine_s16(vmovn_s32(vdst0), vmovn_s32(vdst1)));
        val += 8;
        v0 = vld1q_s32(val);
        v1 = vld1q_s32(val + 4);
        dst += 8;
      }
      for (; len > 0; --len) {
        int32_t sign = *val & INT32_MIN;
        *val &= INT32_MAX;
        // upshift background region, if necessary
        if (ROIshift && (((uint32_t)*val & ~mask) == 0)) {
          *val <<= ROIshift;
        }
        *val >>= pLSB;
        // convert sign-magnitude to two's complement form
        if (sign) {
          *val = -(*val & INT32_MAX);
        }
        assert(pLSB >= 0);  // assure downshift is not negative
        *dst = static_cast<int16_t>(*val);
        val++;
        dst++;
      }
    }
  } else {
    // lossy path
    float fscale = this->stepsize;
    fscale *= (1 << FRACBITS);
    if (M_b <= 31) {
      fscale /= (static_cast<float>(1 << (31 - M_b)));
    } else {
      fscale *= (static_cast<float>(1 << (M_b - 31)));
    }
    constexpr int32_t downshift = 15;
    fscale *= (float)(1 << 16) * (float)(1 << downshift);
    const auto scale = (int32_t)(fscale + 0.5);

    for (size_t i = 0; i < static_cast<size_t>(this->size.y); i++) {
      int32_t *val = this->sample_buf + i * this->blksampl_stride;
      sprec_t *dst = this->i_samples + i * this->band_stride;
      size_t len   = this->size.x;
      v0           = vld1q_s32(val);
      v1           = vld1q_s32(val + 4);
      for (; len >= 8; len -= 8) {  // dequantize two vectors at a time
        s0 = vshrq_n_s32(v0, 31);   // generate a mask for negative values
        s1 = vshrq_n_s32(v1, 31);   // generate a mask for negative values
        v0 = vandq_s32(v0, vmagmask);
        v1 = vandq_s32(v1, vmagmask);
        // upshift background region, if necessary
        vROImask = vandq_s32(v0, vmask);
        vROImask = vceqzq_s32(vROImask);
        vROImask &= vROIshift;
        v0       = vshlq_s32(v0, vROImask);
        vROImask = vandq_s32(v1, vmask);
        vROImask = vceqzq_s32(vROImask);
        vROImask &= vROIshift;
        v1 = vshlq_s32(v1, vROImask);
        // to prevent overflow, truncate to int16_t range
        v0 = vrshrq_n_s32(v0, 16);  // (v0 + (1 << 15)) >> 16;
        v1 = vrshrq_n_s32(v1, 16);  // (v1 + (1 << 15)) >> 16;
        // dequantization
        v0 = vmulq_s32(v0, vdupq_n_s32(scale));
        v1 = vmulq_s32(v1, vdupq_n_s32(scale));
        // downshift and convert values from sign-magnitude form to two's complement one
        v0    = (v0 + (1 << (downshift - 1))) >> downshift;
        v1    = (v1 + (1 << (downshift - 1))) >> downshift;
        vdst0 = vbslq_s32(vreinterpretq_u32_s32(s0), vnegq_s32(v0), v0);
        vdst1 = vbslq_s32(vreinterpretq_u32_s32(s1), vnegq_s32(v1), v1);
        vst1q_s16(dst, vcombine_s16(vmovn_s32(vdst0), vmovn_s32(vdst1)));
        val += 8;
        v0 = vld1q_s32(val);
        v1 = vld1q_s32(val + 4);
        dst += 8;
      }
      for (; len > 0; --len) {
        int32_t sign = *val & INT32_MIN;
        *val &= INT32_MAX;
        // upshift background region, if necessary
        if (ROIshift && (((uint32_t)*val & ~mask) == 0)) {
          *val <<= ROIshift;
        }
        // to prevent overflow, truncate to int16_t
        *val = (*val + (1 << 15)) >> 16;
        //  dequantization
        *val *= scale;
        // downshift
        *val = (int16_t)((*val + (1 << (downshift - 1))) >> downshift);
        // convert sign-magnitude to two's complement form
        if (sign) {
          *val = -(*val & INT32_MAX);
        }
        *dst = static_cast<int16_t>(*val);
        val++;
        dst++;
      }
    }
  }
}

bool htj2k_decode(j2k_codeblock *block, const uint8_t ROIshift) {
  // number of placeholder pass
  uint8_t P0 = 0;
  // length of HT Cleanup segment
  int32_t Lcup = 0;
  // length of HT Refinement segment
  uint32_t Lref = 0;
  // number of HT Sets preceding the given(this) HT Set
  const uint8_t S_skip = 0;

  if (block->num_passes > 3) {
    for (uint32_t i = 0; i < block->pass_length.size(); i++) {
      if (block->pass_length[i] != 0) {
        break;
      }
      P0++;
    }
    P0 /= 3;
  } else if (block->length == 0 && block->num_passes != 0) {
    P0 = 1;
  } else {
    P0 = 0;
  }
  // number of (skipped) magnitude bitplanes
  const auto S_blk = static_cast<uint8_t>(P0 + block->num_ZBP + S_skip);
  if (S_blk >= 30) {
    printf("WARNING: Number of skipped mag bitplanes %d is too large.\n", S_blk);
    return false;
  }

  const auto empty_passes = static_cast<uint8_t>(P0 * 3);
  if (block->num_passes < empty_passes) {
    printf("WARNING: number of passes %d exceeds number of empty passes %d", block->num_passes,
           empty_passes);
    return false;
  }
  // number of ht coding pass (Z_blk in the spec)
  const auto num_ht_passes = static_cast<uint8_t>(block->num_passes - empty_passes);
  // pointer to buffer for HT Cleanup segment
  uint8_t *Dcup;
  // pointer to buffer for HT Refinement segment
  uint8_t *Dref;

  if (num_ht_passes > 0) {
    std::vector<uint8_t> all_segments;
    all_segments.reserve(3);
    for (uint32_t i = 0; i < block->pass_length.size(); i++) {
      if (block->pass_length[i] != 0) {
        all_segments.push_back(static_cast<uint8_t>(i));
      }
    }
    Lcup += static_cast<int32_t>(block->pass_length[all_segments[0]]);
    if (Lcup < 2) {
      printf("WARNING: Cleanup pass length must be at least 2 bytes in length.\n");
      return false;
    }
    Dcup = block->get_compressed_data();
    // Suffix length (=MEL + VLC) of HT Cleanup pass
    const auto Scup = static_cast<int32_t>((Dcup[Lcup - 1] << 4) + (Dcup[Lcup - 2] & 0x0F));
    // modDcup (shall be done before the creation of state_VLC instance)
    Dcup[Lcup - 1] = 0xFF;
    Dcup[Lcup - 2] |= 0x0F;

    if (Scup < 2 || Scup > Lcup || Scup > 4079) {
      printf("WARNING: cleanup pass suffix length %d is invalid.\n", Scup);
      return false;
    }
    // Prefix length (=MagSgn) of HT Cleanup pass
    const auto Pcup = static_cast<int32_t>(Lcup - Scup);

    for (uint32_t i = 1; i < all_segments.size(); i++) {
      Lref += block->pass_length[all_segments[i]];
    }
    if (block->num_passes > 1 && all_segments.size() > 1) {
      Dref = block->get_compressed_data() + Lcup;
    } else {
      Dref = nullptr;
    }

    ht_cleanup_decode(block, static_cast<uint8_t>(30 - S_blk), Lcup, Pcup, Scup);
    if (num_ht_passes > 1) {
      ht_sigprop_decode(block, Dref, Lref, static_cast<uint8_t>(30 - (S_blk + 1)));
    }
    if (num_ht_passes > 2) {
      ht_magref_decode(block, Dref, Lref, static_cast<uint8_t>(30 - (S_blk + 1)));
    }

    // dequantization
    block->dequantize(ROIshift);

  }  // end

  return true;
}
#endif
