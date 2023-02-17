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
  #include "coding_units.hpp"
  #include "ht_block_encoding_neon.hpp"
  #include "coding_local.hpp"
  #include "enc_CxtVLC_tables.hpp"
  #include "utils.hpp"

// Uncomment for experimental use of HT SigProp and MagRef encoding (does not work)
//#define ENABLE_SP_MR

// Quantize DWT coefficients and transfer them to codeblock buffer in a form of MagSgn value
void j2k_codeblock::quantize(uint32_t &or_val) {
  // TODO: check the way to quantize in terms of precision and reconstruction quality
  float fscale = 1.0f / this->stepsize;
  fscale /= (1 << (FRACBITS));
  // Set fscale = 1.0 in lossless coding instead of skipping quantization
  // to avoid if-branch in the following SIMD processing
  if (transformation) fscale = 1.0f;

  const uint32_t height = this->size.y;
  const uint32_t stride = this->band_stride;
  #if defined(ENABLE_SP_MR)
  const int32_t pshift = (refsegment) ? 1 : 0;
  const int32_t pLSB   = (refsegment) ? 1 : 1;
  #endif
  float32x4_t vscale = vdupq_n_f32(fscale);
  int32x4_t vorval   = vdupq_n_s32(0);
  for (uint16_t i = 0; i < static_cast<uint16_t>(height); ++i) {
    sprec_t *sp        = this->i_samples + i * stride;
    int32_t *dp        = this->sample_buf + i * blksampl_stride;
    size_t block_index = (i + 1U) * (blkstate_stride) + 1U;
    uint8_t *dstblk    = block_states + block_index;
  #if defined(ENABLE_SP_MR)
    int32x4_t vpLSB = vdupq_n_s32(pLSB);
  #endif
    int16_t len = static_cast<int16_t>(this->size.x);
    for (; len >= 8; len -= 8) {
      int16x8_t coeff16 = vld1q_s16(sp);
      int32x4_t v0      = vmovl_s16(vget_low_s16(coeff16));
      int32x4_t v1      = vmovl_high_s16(coeff16);
      // Quantization
      v0 = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(v0), vscale));
      v1 = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(v1), vscale));
      // Take sign bit
      int32x4_t s0 = vshrq_n_u32(v0, 31);
      int32x4_t s1 = vshrq_n_u32(v1, 31);
      // Absolute value
      v0 = vabsq_s32(v0);
      v1 = vabsq_s32(v1);
  #if defined(ENABLE_SP_MR)
      int32x4_t z0 = vandq_s32(v0, vpLSB);  // only for SigProp and MagRef
      int32x4_t z1 = vandq_s32(v1, vpLSB);  // only for SigProp and MagRef
      // Down-shift if other than HT Cleanup pass exists
      v0 = v0 >> pshift;
      v1 = v1 >> pshift;
  #endif
      // ------------- Block states related begin
      uint8x8_t vblkstate = vdup_n_u8(0);
      // Signed saturating extract Narrow (qmovn) is important for very finer quantization stepsize
      vblkstate |=
          vmovn_s16(vandq_s16(vcgtzq_s16(vcombine_s16(vqmovn_s32(v0), vqmovn_s32(v1))), vdupq_n_s16(1)));
      //      vblkstate |=
      //          vmovn_s16(vbicq_s16(vdupq_n_s16(1), vceqzq_s16(vcombine_s16(vqmovn_s32(v0),
      //          vqmovn_s32(v1)))));
  #if defined(ENABLE_SP_MR)
      // bits in lowest bitplane, only for SigProp and MagRef TODO: test this line
      vblkstate |= vmovn_s16(vshlq_n_s16(vcombine_s16(vmovn_s32(z0), vmovn_s32(z1)), SHIFT_SMAG));
      // sign-bits, only for SigProp and MagRef  TODO: test this line
      vblkstate |= vmovn_s16(vshlq_n_s16(vcombine_s16(vmovn_s32(s0), vmovn_s32(s1)), SHIFT_SSGN));
  #endif
      vst1_u8(dstblk, vblkstate);
      dstblk += 8;
      // ------------- Block states related end
  #if defined(ENABLE_SP_MR)
      // Modify sign if other than HT Cleanup pass exists
      s0 = vandq_s32(vcgtzq_s32(v0), s0);
      s1 = vandq_s32(vcgtzq_s32(v1), s1);
  #endif
      // Check emptiness of a block
      vorval = vorrq_s32(vorval, v0);
      vorval = vorrq_s32(vorval, v1);
      // Convert two's compliment to MagSgn form
      v0 = vqsubq_u32(v0, vdupq_n_u32(1));
      v1 = vqsubq_u32(v1, vdupq_n_u32(1));
      v0 = vshlq_n_s32(v0, 1);
      v1 = vshlq_n_s32(v1, 1);
      v0 = vaddq_s32(v0, s0);
      v1 = vaddq_s32(v1, s1);
      // Store
      vst1q_s32(dp, v0);
      vst1q_s32(dp + 4, v1);
      sp += 8;
      dp += 8;
    }
    // Check emptiness of a block
    or_val |= static_cast<unsigned int>(vmaxvq_s32(vorval));
    // process leftover
    for (; len > 0; --len) {
      int32_t temp;
      temp = static_cast<int32_t>(static_cast<float>(sp[0]) * fscale);  // needs to be rounded towards zero
      uint32_t sign = static_cast<uint32_t>(temp) & 0x80000000;
  #if defined(ENABLE_SP_MR)
      dstblk[0] |= static_cast<uint8_t>(((temp & pLSB) & 1) << SHIFT_SMAG);
      dstblk[0] |= static_cast<uint8_t>((sign >> 31) << SHIFT_SSGN);
  #endif
      temp = (temp < 0) ? -temp : temp;
      temp &= 0x7FFFFFFF;
  #if defined(ENABLE_SP_MR)
      temp >>= pshift;
      sign = (temp > 0) ? sign : 0;
  #endif
      if (temp) {
        or_val |= 1;
        dstblk[0] |= 1;
        temp--;
        temp <<= 1;
        temp += static_cast<uint8_t>(sign >> 31);
        dp[0] = temp;
      }
      ++sp;
      ++dp;
      ++dstblk;
    }
  }
}

/********************************************************************************
 * state_MEL_enc: member functions
 *******************************************************************************/
void state_MEL_enc::emitMELbit(uint8_t bit) {
  tmp = static_cast<uint8_t>((tmp << 1) + bit);
  rem--;
  if (rem == 0) {
    buf[pos] = tmp;
    pos++;
    rem = (tmp == 0xFF) ? 7 : 8;
    tmp = 0;
  }
}

void state_MEL_enc::encodeMEL(uint8_t smel) {
  uint8_t eval;
  switch (smel) {
    case 0:
      MEL_run++;
      if (MEL_run >= MEL_t) {
        emitMELbit(1);
        MEL_run = 0;
        MEL_k   = (int8_t)std::min(12, MEL_k + 1);
        eval    = MEL_E[MEL_k];
        MEL_t   = static_cast<uint8_t>(1 << eval);
      }
      break;

    default:
      emitMELbit(0);
      eval = MEL_E[MEL_k];
      while (eval > 0) {
        eval--;
        // (MEL_run >> eval) & 1 = msb
        emitMELbit((MEL_run >> eval) & 1);
      }
      MEL_run = 0;
      MEL_k   = (int8_t)std::max(0, MEL_k - 1);
      eval    = MEL_E[MEL_k];
      MEL_t   = static_cast<uint8_t>(1 << eval);
      break;
  }
}

void state_MEL_enc::termMEL() {
  if (MEL_run > 0) {
    emitMELbit(1);
  }
}

/********************************************************************************
 * HT cleanup encoding: helper functions
 *******************************************************************************/
auto make_storage = [](uint8_t *ssp0, uint8_t *ssp1, int32_t *sp0, int32_t *sp1, int32x4_t &sig0,
                       int32x4_t &sig1, int32x4_t &v0, int32x4_t &v1, int32x4_t &E0, int32x4_t &E1,
                       int32_t &rho0, int32_t &rho1) {
  // This function shall be called on the assumption that there are two quads
  int32x4_t t0 = vld1q_s32(sp0);
  int32x4_t t1 = vld1q_s32(sp1);
  v0           = vzip1q_s32(t0, t1);
  v1           = vzip2q_s32(t0, t1);
  //  int32x4x2_t v = vzipq_s32(vld1q_s32(sp0), vld1q_s32(sp1));
  //  v0            = v.val[0];
  //  v1            = v.val[1];

  uint8x8_t sig01 = vand_u8(vzip1_u8(vld1_u8(ssp0), vld1_u8(ssp1)), vdup_n_u8(1));
  sig0            = vcgtzq_s32(vmovl_u16(vget_low_u16(vmovl_u8(sig01))));
  sig1            = vcgtzq_s32(vmovl_u16(vget_high_u16(vmovl_u8(sig01))));
  int8x8_t shift  = {0, 1, 2, 3, 4, 5, 6, 7};
  uint8_t rho01   = vaddv_u8(vshl_u8(sig01, shift));
  rho0            = rho01 & 0xF;
  rho1            = rho01 >> 4;

  E0 = vandq_s32(vsubq_u32(vdupq_n_s32(32), vclzq_u32(v0)), sig0);
  E1 = vandq_s32(vsubq_u32(vdupq_n_s32(32), vclzq_u32(v1)), sig1);
};

auto make_storage_one = [](uint8_t *ssp0, uint8_t *ssp1, int32_t *sp0, int32_t *sp1, int32x4_t &sig0,
                           int32x4_t &v0, int32x4_t &E0, int32_t &rho0) {
  //  v0 = {sp0[0], sp1[0], sp0[1], sp1[1]};
  v0 = vsetq_lane_s32(sp0[0], v0, 0);
  v0 = vsetq_lane_s32(sp1[0], v0, 1);
  v0 = vsetq_lane_s32(sp0[1], v0, 2);
  v0 = vsetq_lane_s32(sp1[1], v0, 3);

  int32x4_t sig   = {ssp0[0] & 1, ssp1[0] & 1, ssp0[1] & 1, ssp1[1] & 1};
  int32x4_t shift = {0, 1, 2, 3};
  uint32x4_t vtmp = vshlq_s32(sig, shift);
  rho0            = vaddvq_u32(vtmp) & 0xF;
  sig0            = vcgtzq_s32(sig);

  E0 = vandq_s32(vsubq_u32(vdupq_n_s32(32), vclzq_u32(v0)), sig0);
};

// joint termination of MEL and VLC
int32_t termMELandVLC(state_VLC_enc &VLC, state_MEL_enc &MEL) {
  VLC.termVLC();
  uint8_t MEL_mask, VLC_mask, fuse;
  MEL.tmp  = static_cast<uint8_t>(MEL.tmp << MEL.rem);
  MEL_mask = static_cast<uint8_t>((0xFF << MEL.rem) & 0xFF);
  VLC_mask = static_cast<uint8_t>(0xFF >> (8 - VLC.bits));
  if ((MEL_mask | VLC_mask) != 0) {
    fuse = MEL.tmp | VLC.tmp;
    if (((((fuse ^ MEL.tmp) & MEL_mask) | ((fuse ^ VLC.tmp) & VLC_mask)) == 0) && (fuse != 0xFF)) {
      MEL.buf[MEL.pos] = fuse;
    } else {
      MEL.buf[MEL.pos] = MEL.tmp;
      VLC.buf[VLC.pos] = VLC.tmp;
      VLC.pos--;  // reverse order
    }
    MEL.pos++;
  }
  // concatenate MEL and VLC buffers
  memmove(&MEL.buf[MEL.pos], &VLC.buf[VLC.pos + 1], static_cast<size_t>(MAX_Scup - VLC.pos - 1));
  // return Scup
  return (MEL.pos + MAX_Scup - VLC.pos - 1);
}

// joint termination of SP and MR
int32_t termSPandMR(SP_enc &SP, MR_enc &MR) {
  uint8_t SP_mask = static_cast<uint8_t>(0xFF >> (8 - SP.bits));  // if SP_bits is 0, SP_mask = 0
  SP_mask =
      static_cast<uint8_t>(SP_mask | ((1 << SP.max) & 0x80));  // Auguments SP_mask to cover any stuff bit
  uint8_t MR_mask = static_cast<uint8_t>(0xFF >> (8 - MR.bits));  // if MR_bits is 0, MR_mask = 0
  if ((SP_mask | MR_mask) == 0) {
    // last SP byte cannot be 0xFF, since then SP_max would be 7
    memmove(&SP.buf[SP.pos], &MR.buf[MR.pos + 1], MAX_Lref - MR.pos);
    return static_cast<int32_t>(SP.pos + MAX_Lref - MR.pos);
  }
  uint8_t fuse = SP.tmp | MR.tmp;
  if ((((fuse ^ SP.tmp) & SP_mask) | ((fuse ^ MR.tmp) & MR_mask)) == 0) {
    SP.buf[SP.pos] = fuse;  // fuse always < 0x80 here; no false marker risk
  } else {
    SP.buf[SP.pos] = SP.tmp;  // SP_tmp cannot be 0xFF
    MR.buf[MR.pos] = MR.tmp;
    MR.pos--;  // MR buf gorws reverse order
  }
  SP.pos++;
  memmove(&SP.buf[SP.pos], &MR.buf[MR.pos + 1], MAX_Lref - MR.pos);
  return static_cast<int32_t>(SP.pos + MAX_Lref - MR.pos);
}

/*********************************************************************************************************************/
// HT Cleanup encoding
/*********************************************************************************************************************/
int32_t htj2k_cleanup_encode(j2k_codeblock *const block, const uint8_t ROIshift) noexcept {
  // length of HT cleanup pass
  int32_t Lcup;
  // length of MagSgn buffer
  int32_t Pcup;
  // length of MEL buffer + VLC buffer
  int32_t Scup;
  // used as a flag to invoke HT Cleanup encoding
  uint32_t or_val = 0;
  if (ROIshift) {
    printf("WARNING: Encoding with ROI is not supported.\n");
  }

  const uint32_t QW = ceil_int(block->size.x, 2U);
  const uint32_t QH = ceil_int(block->size.y, 2U);

  block->quantize(or_val);

  if (!or_val) {
    // nothing to do here because this codeblock is empty
    // set length of coding passes
    block->length         = 0;
    block->pass_length[0] = 0;
    // set number of coding passes
    block->num_passes      = 0;
    block->layer_passes[0] = 0;
    block->layer_start[0]  = 0;
    // set number of zero-bitplanes (=Zblk)
    block->num_ZBP = static_cast<uint8_t>(block->get_Mb() - 1);
    return static_cast<int32_t>(block->length);
  }

  // buffers shall be zeroed.
  std::unique_ptr<uint8_t[]> fwd_buf = MAKE_UNIQUE<uint8_t[]>(MAX_Lcup);
  std::unique_ptr<uint8_t[]> rev_buf = MAKE_UNIQUE<uint8_t[]>(MAX_Scup);
  memset(fwd_buf.get(), 0, sizeof(uint8_t) * (MAX_Lcup));
  memset(rev_buf.get(), 0, sizeof(uint8_t) * MAX_Scup);

  state_MS_enc MagSgn_encoder(fwd_buf.get());
  state_MEL_enc MEL_encoder(rev_buf.get());
  state_VLC_enc VLC_encoder(rev_buf.get());

  alignas(32) auto Eline   = MAKE_UNIQUE<int32_t[]>(2U * QW + 6U);
  Eline[0]                 = 0;
  auto E_p                 = Eline.get() + 1;
  alignas(32) auto rholine = MAKE_UNIQUE<int32_t[]>(QW + 3U);
  rholine[0]               = 0;
  auto rho_p               = rholine.get() + 1;

  int32_t gamma;
  int32_t context = 0, n_q;
  uint32_t CxtVLC, lw, cwd;
  int32_t Emax_q;
  int32_t rho0, rho1, U0, U1;
  int32_t u_q, uoff, u_min, uvlc_idx, kappa = 1;
  int32_t emb_pattern, embk_0, embk_1, emb1_0, emb1_1;

  const int32x4_t lshift = {0, 1, 2, 3};
  const int32x4_t rshift = {0, -1, -2, -3};  // negative value with vshlq() does right shift
  const int32x4_t vone   = vdupq_n_s32(1);
  int32x4_t v0, v1, E0, E1, sig0, sig1, Etmp, vuoff, mask, m0, m1, known1_0, known1_1;

  /*******************************************************************************************************************/
  // Initial line-pair
  /*******************************************************************************************************************/
  uint8_t *ssp0 = block->block_states + 1U * (block->blkstate_stride) + 1U;
  uint8_t *ssp1 = ssp0 + block->blkstate_stride;
  int32_t *sp0  = block->sample_buf;
  int32_t *sp1  = sp0 + block->blksampl_stride;
  uint32_t qx;
  for (qx = QW; qx >= 2; qx -= 2) {
    bool uoff_flag = true;

    // MAKE_STORAGE()
    make_storage(ssp0, ssp1, sp0, sp1, sig0, sig1, v0, v1, E0, E1, rho0, rho1);
    // update Eline
    vst1q_s32(E_p, vuzp2q_s32(E0, E1));  // vzip2q_s32(vzip1q_s32(E0, E1), vzip2q_s32(E0, E1)));
    E_p += 4;
    // MEL encoding for the first quad
    if (context == 0) {
      MEL_encoder.encodeMEL((rho0 != 0));
    }
    // calculate u_off values
    Emax_q   = vmaxvq_s32(E0);
    U0       = std::max(Emax_q, kappa);
    u_q      = U0 - kappa;
    u_min    = u_q;
    uvlc_idx = u_q;
    uoff     = (u_q) ? 1 : 0;
    uoff_flag &= uoff;
    Etmp        = vdupq_n_s32(Emax_q);
    vuoff       = vdupq_n_s32(uoff);
    mask        = vceqq_s32(E0, Etmp);
    emb_pattern = vaddvq_s32(vshlq_s32(vuoff, lshift) & mask);
    n_q         = emb_pattern + (rho0 << 4) + (context << 8);
    // prepare VLC encoding of quad 0
    CxtVLC = enc_CxtVLC_table0[n_q];
    embk_0 = CxtVLC & 0xF;
    emb1_0 = emb_pattern & embk_0;
    lw     = (CxtVLC >> 4) & 0x07;
    cwd    = CxtVLC >> 7;

    // context for the next quad
    context = (rho0 >> 1) | (rho0 & 0x1);

    Emax_q = vmaxvq_s32(E1);
    U1     = std::max(Emax_q, kappa);
    u_q    = U1 - kappa;
    u_min  = (u_min < u_q) ? u_min : u_q;
    uvlc_idx += u_q << 5;
    uoff = (u_q) ? 1 : 0;
    uoff_flag &= uoff;
    Etmp        = vdupq_n_s32(Emax_q);
    vuoff       = vdupq_n_s32(uoff);
    mask        = vceqq_s32(E1, Etmp);
    emb_pattern = vaddvq_s32(vshlq_s32(vuoff, lshift) & mask);
    n_q         = emb_pattern + (rho1 << 4) + (context << 8);
    // VLC encoding of quads 0 and 1
    VLC_encoder.emitVLCBits(cwd, lw);  // quad 0
    CxtVLC = enc_CxtVLC_table0[n_q];
    embk_1 = CxtVLC & 0xF;
    emb1_1 = emb_pattern & embk_1;
    lw     = (CxtVLC >> 4) & 0x07;
    cwd    = CxtVLC >> 7;
    VLC_encoder.emitVLCBits(cwd, lw);  // quad 1
    // UVLC encoding
    uint32_t tmp = enc_UVLC_table0[uvlc_idx];
    lw           = tmp & 0xFF;
    cwd          = tmp >> 8;
    VLC_encoder.emitVLCBits(cwd, lw);

    // MEL encoding of the second quad
    if (context == 0) {
      if (rho1 != 0) {
        MEL_encoder.encodeMEL(1);
      } else {
        if (u_min > 2) {
          MEL_encoder.encodeMEL(1);
        } else {
          MEL_encoder.encodeMEL(0);
        }
      }
    } else if (uoff_flag) {
      if (u_min > 2) {
        MEL_encoder.encodeMEL(1);
      } else {
        MEL_encoder.encodeMEL(0);
      }
    }

    // MagSgn encoding
    m0       = vsubq_s32(vandq_s32(sig0, vdupq_n_s32(U0)),
                         vandq_s32(vshlq_s32(vdupq_n_s32(embk_0), rshift), vone));
    m1       = vsubq_s32(vandq_s32(sig1, vdupq_n_s32(U1)),
                         vandq_s32(vshlq_s32(vdupq_n_s32(embk_1), rshift), vone));
    known1_0 = vandq_s32(vshlq_s32(vdupq_n_s32(emb1_0), rshift), vone);
    known1_1 = vandq_s32(vshlq_s32(vdupq_n_s32(emb1_1), rshift), vone);
    MagSgn_encoder.emitBits(v0, m0, known1_0);
    MagSgn_encoder.emitBits(v1, m1, known1_1);

    // context for the next quad
    context = (rho1 >> 1) | (rho1 & 0x1);
    // update rho_line
    *rho_p++ = rho0;
    *rho_p++ = rho1;
    // update pointer to line buffer
    ssp0 += 4;
    ssp1 += 4;
    sp0 += 4;
    sp1 += 4;
  }
  if (qx) {
    make_storage_one(ssp0, ssp1, sp0, sp1, sig0, v0, E0, rho0);
    *E_p++ = E0[1];
    *E_p++ = E0[3];

    // MEL encoding
    if (context == 0) {
      MEL_encoder.encodeMEL((rho0 != 0));
    }

    Emax_q   = vmaxvq_s32(E0);
    U0       = std::max(Emax_q, kappa);
    u_q      = U0 - kappa;
    uvlc_idx = u_q;
    uoff     = (u_q) ? 1 : 0;

    Etmp        = vdupq_n_s32(Emax_q);
    vuoff       = vdupq_n_s32(uoff);
    mask        = vceqq_s32(E0, Etmp);
    emb_pattern = vaddvq_s32(vshlq_s32(vuoff, lshift) & mask);
    n_q         = emb_pattern + (rho0 << 4) + (context << 8);
    // VLC encoding
    CxtVLC = enc_CxtVLC_table0[n_q];
    embk_0 = CxtVLC & 0xF;
    emb1_0 = emb_pattern & embk_0;
    lw     = (CxtVLC >> 4) & 0x07;
    cwd    = CxtVLC >> 7;
    VLC_encoder.emitVLCBits(cwd, lw);
    // UVLC encoding
    uint32_t tmp = enc_UVLC_table0[uvlc_idx];
    lw           = tmp & 0xFF;
    cwd          = tmp >> 8;
    VLC_encoder.emitVLCBits(cwd, lw);

    // MagSgn encoding
    m0       = vsubq_s32(vandq_s32(sig0, vdupq_n_s32(U0)),
                         vandq_s32(vshlq_s32(vdupq_n_s32(embk_0), rshift), vone));
    known1_0 = vandq_s32(vshlq_s32(vdupq_n_s32(emb1_0), rshift), vone);
    MagSgn_encoder.emitBits(v0, m0, known1_0);

    // update rho_line
    *rho_p++ = rho0;
  }

  /*******************************************************************************************************************/
  // Non-initial line-pair
  /*******************************************************************************************************************/
  int32_t Emax0, Emax1;
  for (uint32_t qy = 1; qy < QH; ++qy) {
    E_p   = Eline.get() + 1;
    rho_p = rholine.get() + 1;
    rho1  = 0;

    Emax0 = find_max(E_p[-1], E_p[0], E_p[1], E_p[2]);
    Emax1 = find_max(E_p[1], E_p[2], E_p[3], E_p[4]);

    // calculate context for the next quad
    context = ((rho1 & 0x4) << 7) | ((rho1 & 0x8) << 6);            // (w | sw) << 9
    context |= ((rho_p[-1] & 0x8) << 5) | ((rho_p[0] & 0x2) << 7);  // (nw | n) << 8
    context |= ((rho_p[0] & 0x8) << 7) | ((rho_p[1] & 0x2) << 9);   // (ne | nf) << 10

    ssp0 = block->block_states + (2U * qy + 1U) * (block->blkstate_stride) + 1U;
    ssp1 = ssp0 + block->blkstate_stride;
    sp0  = block->sample_buf + 2U * (qy * block->blksampl_stride);
    sp1  = sp0 + block->blksampl_stride;
    for (qx = QW; qx >= 2; qx -= 2) {
      make_storage(ssp0, ssp1, sp0, sp1, sig0, sig1, v0, v1, E0, E1, rho0, rho1);
      // MEL encoding of the first quad
      if (context == 0) {
        MEL_encoder.encodeMEL((rho0 != 0));
      }
      gamma       = ((rho0 & (rho0 - 1)) == 0) ? 0 : 1;
      kappa       = std::max((Emax0 - 1) * gamma, 1);
      Emax_q      = vmaxvq_s32(E0);
      U0          = std::max(Emax_q, kappa);
      u_q         = U0 - kappa;
      uvlc_idx    = u_q;
      uoff        = (u_q) ? 1 : 0;
      Etmp        = vdupq_n_s32(Emax_q);
      vuoff       = vdupq_n_s32(uoff);
      mask        = vceqq_s32(E0, Etmp);
      emb_pattern = vaddvq_s32(vshlq_s32(vuoff, lshift) & mask);
      n_q         = emb_pattern + (rho0 << 4) + (context << 0);
      // prepare VLC encoding of quad 0
      CxtVLC = enc_CxtVLC_table1[n_q];
      embk_0 = CxtVLC & 0xF;
      emb1_0 = emb_pattern & embk_0;
      lw     = (CxtVLC >> 4) & 0x07;
      cwd    = CxtVLC >> 7;

      // calculate context for the next quad
      context = ((rho0 & 0x4) << 7) | ((rho0 & 0x8) << 6);           // (w | sw) << 9
      context |= ((rho_p[0] & 0x8) << 5) | ((rho_p[1] & 0x2) << 7);  // (nw | n) << 8
      context |= ((rho_p[1] & 0x8) << 7) | ((rho_p[2] & 0x2) << 9);  // (ne | nf) << 10
      // MEL encoding of the second quad
      if (context == 0) {
        MEL_encoder.encodeMEL((rho1 != 0));
      }
      gamma  = ((rho1 & (rho1 - 1)) == 0) ? 0 : 1;
      kappa  = std::max((Emax1 - 1) * gamma, 1);
      Emax_q = vmaxvq_s32(E1);
      U1     = std::max(Emax_q, kappa);
      u_q    = U1 - kappa;
      uvlc_idx += u_q << 5;
      uoff        = (u_q) ? 1 : 0;
      Etmp        = vdupq_n_s32(Emax_q);
      vuoff       = vdupq_n_s32(uoff);
      mask        = vceqq_s32(E1, Etmp);
      emb_pattern = vaddvq_s32(vshlq_s32(vuoff, lshift) & mask);
      n_q         = emb_pattern + (rho1 << 4) + (context << 0);
      // VLC encoding of quands 0 and 1
      VLC_encoder.emitVLCBits(cwd, lw);  // quad 0
      CxtVLC = enc_CxtVLC_table1[n_q];
      embk_1 = CxtVLC & 0xF;
      emb1_1 = emb_pattern & embk_1;
      lw     = (CxtVLC >> 4) & 0x07;
      cwd    = CxtVLC >> 7;
      VLC_encoder.emitVLCBits(cwd, lw);  // quad 1
      // UVLC encoding
      uint32_t tmp = enc_UVLC_table1[uvlc_idx];
      lw           = tmp & 0xFF;
      cwd          = tmp >> 8;
      VLC_encoder.emitVLCBits(cwd, lw);

      // MagSgn encoding
      m0       = vsubq_s32(vandq_s32(sig0, vdupq_n_s32(U0)),
                           vandq_s32(vshlq_s32(vdupq_n_s32(embk_0), rshift), vone));
      m1       = vsubq_s32(vandq_s32(sig1, vdupq_n_s32(U1)),
                           vandq_s32(vshlq_s32(vdupq_n_s32(embk_1), rshift), vone));
      known1_0 = vandq_s32(vshlq_s32(vdupq_n_s32(emb1_0), rshift), vone);
      known1_1 = vandq_s32(vshlq_s32(vdupq_n_s32(emb1_1), rshift), vone);
      MagSgn_encoder.emitBits(v0, m0, known1_0);
      MagSgn_encoder.emitBits(v1, m1, known1_1);

      Emax0 = vmaxvq_s32(vld1q_s32(E_p + 3));  // find_max(E_p[3], E_p[4], E_p[5], E_p[6]);
      Emax1 = vmaxvq_s32(vld1q_s32(E_p + 5));  // find_max(E_p[5], E_p[6], E_p[7], E_p[8]);
      vst1q_s32(E_p, vuzp2q_s32(E0, E1));      // vzip2q_s32(vzip1q_s32(E0, E1), vzip2q_s32(E0, E1)));
      E_p += 4;

      // calculate context for the next quad
      context = ((rho1 & 0x4) << 7) | ((rho1 & 0x8) << 6);           // (w | sw) << 9
      context |= ((rho_p[1] & 0x8) << 5) | ((rho_p[2] & 0x2) << 7);  // (nw | n) << 8
      context |= ((rho_p[2] & 0x8) << 7) | ((rho_p[3] & 0x2) << 9);  // (ne | nf) << 10

      // update rho_line
      *rho_p++ = rho0;
      *rho_p++ = rho1;
      // update pointer to line buffer
      ssp0 += 4;
      ssp1 += 4;
      sp0 += 4;
      sp1 += 4;
    }
    if (qx) {
      make_storage_one(ssp0, ssp1, sp0, sp1, sig0, v0, E0, rho0);
      *E_p++ = E0[1];
      *E_p++ = E0[3];

      // MEL encoding of the first quad
      if (context == 0) {
        MEL_encoder.encodeMEL((rho0 != 0));
      }

      gamma    = (popcount32((uint32_t)rho0) > 1) ? 1 : 0;
      kappa    = std::max((Emax0 - 1) * gamma, 1);
      Emax_q   = vmaxvq_s32(E0);
      U0       = std::max(Emax_q, kappa);
      u_q      = U0 - kappa;
      uvlc_idx = u_q;
      uoff     = (u_q) ? 1 : 0;

      Etmp        = vdupq_n_s32(Emax_q);
      vuoff       = vdupq_n_s32(uoff);
      mask        = vceqq_s32(E0, Etmp);
      emb_pattern = vaddvq_s32(vshlq_s32(vuoff, lshift) & mask);
      n_q         = emb_pattern + (rho0 << 4) + (context << 0);
      // VLC encoding
      CxtVLC = enc_CxtVLC_table1[n_q];
      embk_0 = CxtVLC & 0xF;
      emb1_0 = emb_pattern & embk_0;
      lw     = (CxtVLC >> 4) & 0x07;
      cwd    = CxtVLC >> 7;
      VLC_encoder.emitVLCBits(cwd, lw);
      // UVLC encoding
      uint32_t tmp = enc_UVLC_table1[uvlc_idx];
      lw           = tmp & 0xFF;
      cwd          = tmp >> 8;
      VLC_encoder.emitVLCBits(cwd, lw);

      // MagSgn encoding
      m0       = vsubq_s32(vandq_s32(sig0, vdupq_n_s32(U0)),
                           vandq_s32(vshlq_s32(vdupq_n_s32(embk_0), rshift), vone));
      known1_0 = vandq_s32(vshlq_s32(vdupq_n_s32(emb1_0), rshift), vone);
      MagSgn_encoder.emitBits(v0, m0, known1_0);

      // update rho_line
      *rho_p++ = rho0;
    }
  }

  Pcup = MagSgn_encoder.termMS();
  MEL_encoder.termMEL();
  Scup = termMELandVLC(VLC_encoder, MEL_encoder);
  memcpy(&fwd_buf[static_cast<size_t>(Pcup)], &rev_buf[0], static_cast<size_t>(Scup));
  Lcup = Pcup + Scup;

  fwd_buf[static_cast<size_t>(Lcup - 1)] = static_cast<uint8_t>(Scup >> 4);
  fwd_buf[static_cast<size_t>(Lcup - 2)] =
      (fwd_buf[static_cast<size_t>(Lcup - 2)] & 0xF0) | static_cast<uint8_t>(Scup & 0x0f);

  // transfer Dcup[] to block->compressed_data
  block->set_compressed_data(fwd_buf.get(), static_cast<uint16_t>(Lcup), MAX_Lref);
  // set length of compressed data
  block->length         = static_cast<uint32_t>(Lcup);
  block->pass_length[0] = static_cast<unsigned int>(Lcup);
  // set number of coding passes
  block->num_passes      = 1;
  block->layer_passes[0] = 1;
  block->layer_start[0]  = 0;
  // set number of zero-bit planes (=Zblk)
  block->num_ZBP = static_cast<uint8_t>(block->get_Mb() - 1);
  return static_cast<int32_t>(block->length);
}
/********************************************************************************
 * HT sigprop encoding
 *******************************************************************************/
auto process_stripes_block_enc = [](SP_enc &SigProp, j2k_codeblock *block, const uint32_t i_start,
                                    const uint32_t j_start, const uint32_t width, const uint32_t height) {
  uint8_t *sp;
  uint8_t causal_cond = 0;
  uint8_t bit;
  uint8_t mbr;
  // uint32_t mbr_info;  // NOT USED
  const auto block_width  = j_start + width;
  const auto block_height = i_start + height;
  for (uint32_t j = j_start; j < block_width; j++) {
    // mbr_info = 0;
    for (uint32_t i = i_start; i < block_height; i++) {
      sp          = block->block_states + (i + 1) * block->blkstate_stride + (j + 1);
      causal_cond = (((block->Cmodes & CAUSAL) == 0) || (i != i_start + height - 1));
      mbr         = 0;
      //      if (block->get_state(Sigma, i, j) == 0) {
      if ((sp[0] >> SHIFT_SIGMA & 1) == 0) {
        mbr = block->calc_mbr(i, j, causal_cond);
      }
      // mbr_info >>= 3;
      if (mbr != 0) {
        bit = (*sp >> SHIFT_SMAG) & 1;
        SigProp.emitSPBit(bit);
        //        block->modify_state(refinement_indicator, 1, i, j);
        sp[0] |= 1 << SHIFT_PI_;
        //        block->modify_state(refinement_value, bit, i, j);
        sp[0] |= bit << SHIFT_REF;
      }
      //      block->modify_state(scan, 1, i, j);
      sp[0] |= 1 << SHIFT_SCAN;
    }
  }
  for (uint32_t j = j_start; j < block_width; j++) {
    for (uint32_t i = i_start; i < block_height; i++) {
      sp = block->block_states + (i + 1) * block->blkstate_stride + (j + 1);
      // encode sign
      //      if (block->get_state(Refinement_value, i, j)) {
      if ((sp[0] >> SHIFT_REF) & 1) {
        bit = (sp[0] >> SHIFT_SSGN) & 1;
        SigProp.emitSPBit(bit);
      }
    }
  }
};

void ht_sigprop_encode(j2k_codeblock *block, SP_enc &SigProp) {
  const uint32_t num_v_stripe = block->size.y / 4;
  const uint32_t num_h_stripe = block->size.x / 4;
  uint32_t i_start            = 0, j_start;
  uint32_t width              = 4;
  uint32_t width_last;
  uint32_t height = 4;

  // encode full-height (=4) stripes
  for (uint32_t n1 = 0; n1 < num_v_stripe; n1++) {
    j_start = 0;
    for (uint32_t n2 = 0; n2 < num_h_stripe; n2++) {
      process_stripes_block_enc(SigProp, block, i_start, j_start, width, height);
      j_start += 4;
    }
    width_last = block->size.x % 4;
    if (width_last) {
      process_stripes_block_enc(SigProp, block, i_start, j_start, width_last, height);
    }
    i_start += 4;
  }
  // encode remaining height stripes
  height  = block->size.y % 4;
  j_start = 0;
  for (uint32_t n2 = 0; n2 < num_h_stripe; n2++) {
    process_stripes_block_enc(SigProp, block, i_start, j_start, width, height);
    j_start += 4;
  }
  width_last = block->size.x % 4;
  if (width_last) {
    process_stripes_block_enc(SigProp, block, i_start, j_start, width_last, height);
  }
}
/********************************************************************************
 * HT magref encoding
 *******************************************************************************/
void ht_magref_encode(j2k_codeblock *block, MR_enc &MagRef) {
  const uint32_t blk_height   = block->size.y;
  const uint32_t blk_width    = block->size.x;
  const uint32_t num_v_stripe = block->size.y / 4;
  uint32_t i_start            = 0;
  uint32_t height             = 4;
  uint8_t *sp;
  uint8_t bit;

  for (uint32_t n1 = 0; n1 < num_v_stripe; n1++) {
    for (uint32_t j = 0; j < blk_width; j++) {
      for (uint32_t i = i_start; i < i_start + height; i++) {
        sp = block->block_states + (i + 1) * block->blkstate_stride + (j + 1);
        //        sp               = &block->block_states[j + 1 + (i + 1) * (block->size.x + 2)];
        if ((sp[0] >> SHIFT_SIGMA & 1) != 0) {
          bit = (sp[0] >> SHIFT_SMAG) & 1;
          MagRef.emitMRBit(bit);
          //          block->modify_state(refinement_indicator, 1, i, j);
          sp[0] |= 1 << SHIFT_PI_;
        }
      }
    }
    i_start += 4;
  }
  height = blk_height % 4;
  for (uint32_t j = 0; j < blk_width; j++) {
    for (uint32_t i = i_start; i < i_start + height; i++) {
      sp = block->block_states + (i + 1) * block->blkstate_stride + (j + 1);
      if ((sp[0] >> SHIFT_SIGMA & 1) != 0) {
        bit = (sp[0] >> SHIFT_SMAG) & 1;
        MagRef.emitMRBit(bit);
        //        block->modify_state(refinement_indicator, 1, i, j);
        sp[0] |= 1 << SHIFT_PI_;
      }
    }
  }
}

/********************************************************************************
 * HT encoding
 *******************************************************************************/
int32_t htj2k_encode(j2k_codeblock *block, uint8_t ROIshift) noexcept {
  #ifdef ENABLE_SP_MR
  block->refsegment = true;
  #endif
  int32_t Lcup = htj2k_cleanup_encode(block, ROIshift);
  if (Lcup && block->refsegment) {
    uint8_t Dref[2047] = {0};
    SP_enc SigProp(Dref);
    MR_enc MagRef(Dref);
    int32_t HTMagRefLength = 0;
    // SigProp encoding
    ht_sigprop_encode(block, SigProp);
    // MagRef encoding
    ht_magref_encode(block, MagRef);
    if (MagRef.get_length()) {
      HTMagRefLength         = termSPandMR(SigProp, MagRef);
      block->num_passes      = static_cast<uint8_t>(block->num_passes + 2);
      block->layer_passes[0] = static_cast<uint8_t>(block->layer_passes[0] + 2);
      block->pass_length.push_back(SigProp.get_length());
      block->pass_length.push_back(MagRef.get_length());
    } else {
      SigProp.termSP();
      HTMagRefLength         = static_cast<int32_t>(SigProp.get_length());
      block->num_passes      = static_cast<uint8_t>(block->num_passes + 1);
      block->layer_passes[0] = static_cast<uint8_t>(block->layer_passes[0] + 1);
      block->pass_length.push_back(SigProp.get_length());
    }
    if (HTMagRefLength) {
      block->length += static_cast<unsigned int>(HTMagRefLength);
      block->num_ZBP = static_cast<uint8_t>(block->num_ZBP - (block->refsegment));
      block->set_compressed_data(Dref, static_cast<uint16_t>(HTMagRefLength));
    }
    //    // debugging
    //    printf("SP length = %d\n", SigProp.get_length());
    //    printf("MR length = %d\n", MagRef.get_length());
    //    printf("HT MAgRef length = %d\n", HTMagRefLength);
    //    for (int i = 0; i < HTMagRefLength; ++i) {
    //      printf("%02X ", Dref[i]);
    //    }
    //    printf("\n");
  }
  return EXIT_SUCCESS;
}
#endif