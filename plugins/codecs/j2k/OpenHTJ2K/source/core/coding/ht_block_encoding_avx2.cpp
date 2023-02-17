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

#if defined(OPENHTJ2K_TRY_AVX2) && defined(__AVX2__)
  #include <algorithm>
  #include <cmath>
  #include "coding_units.hpp"
  #include "ht_block_encoding_avx2.hpp"
  #include "coding_local.hpp"
  #include "enc_CxtVLC_tables.hpp"
  #include "utils.hpp"

  #define Q0 0
  #define Q1 1

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
  for (uint16_t i = 0; i < static_cast<uint16_t>(height); ++i) {
    sprec_t *sp        = this->i_samples + i * stride;
    int32_t *dp        = this->sample_buf + i * blksampl_stride;
    size_t block_index = (i + 1U) * (blkstate_stride) + 1U;
    uint8_t *dstblk    = block_states + block_index;
  #if defined(ENABLE_SP_MR)
    const __m256i vpLSB = _mm256_set1_epi32(pLSB);
  #endif
    const __m256i vone  = _mm256_set1_epi32(1);
    const __m256 vscale = _mm256_set1_ps(fscale);
    // simd
    int32_t len = static_cast<int32_t>(this->size.x);
    for (; len >= 16; len -= 16) {
      __m256i coeff16 = _mm256_loadu_si256((__m256i *)sp);
      __m256i v0      = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(coeff16, 0));
      __m256i v1      = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(coeff16, 1));
      // Quantization with cvt't'ps (truncates inexact values by rounding towards zero)
      v0 = _mm256_cvttps_epi32(_mm256_mul_ps(_mm256_cvtepi32_ps(v0), vscale));
      v1 = _mm256_cvttps_epi32(_mm256_mul_ps(_mm256_cvtepi32_ps(v1), vscale));
      // Take sign bit
      __m256i s0 = _mm256_srli_epi32(v0, 31);
      __m256i s1 = _mm256_srli_epi32(v1, 31);
      v0         = _mm256_abs_epi32(v0);
      v1         = _mm256_abs_epi32(v1);
  #if defined(ENABLE_SP_MR)
      __m256i z0 = _mm256_and_si256(v0, vpLSB);  // only for SigProp and MagRef
      __m256i z1 = _mm256_and_si256(v1, vpLSB);  // only for SigProp and MagRef

      // Down-shift if other than HT Cleanup pass exists
      v0 = _mm256_srai_epi32(v0, pshift);
      v1 = _mm256_srai_epi32(v1, pshift);
  #endif
      // Generate masks for sigma
      __m256i mask0 = _mm256_cmpgt_epi32(v0, _mm256_setzero_si256());
      __m256i mask1 = _mm256_cmpgt_epi32(v1, _mm256_setzero_si256());
      // Check emptiness of a block
      or_val |= static_cast<uint32_t>(_mm256_movemask_epi8(mask0));
      or_val |= static_cast<uint32_t>(_mm256_movemask_epi8(mask1));

      // Convert two's compliment to MagSgn form
      __m256i vone0 = _mm256_and_si256(mask0, vone);
      __m256i vone1 = _mm256_and_si256(mask1, vone);
      v0            = _mm256_sub_epi32(v0, vone0);
      v1            = _mm256_sub_epi32(v1, vone1);
      v0            = _mm256_slli_epi32(v0, 1);
      v1            = _mm256_slli_epi32(v1, 1);
      v0            = _mm256_add_epi32(v0, _mm256_and_si256(s0, mask0));
      v1            = _mm256_add_epi32(v1, _mm256_and_si256(s1, mask1));
      // Store
      _mm256_storeu_si256((__m256i *)dp, v0);
      _mm256_storeu_si256((__m256i *)(dp + 8), v1);
      sp += 16;
      dp += 16;
      // for Block states
      v0 = _mm256_packs_epi32(vone0, vone1);  // re-use v0 as sigma
      v0 = _mm256_permute4x64_epi64(v0, 0xD8);
  #if defined(ENABLE_SP_MR)
      vone0 = _mm256_packs_epi32(z0, z1);  // re-use vone0 as z
      vone0 = _mm256_permute4x64_epi64(vone0, 0xD8);
      vone1 = _mm256_packs_epi32(s0, s1);  // re-use vone1 as sign
      vone1 = _mm256_permute4x64_epi64(vone1, 0xD8);
      v0    = _mm256_or_si256(v0, _mm256_slli_epi16(vone0, SHIFT_SMAG));
      v0    = _mm256_or_si256(v0, _mm256_slli_epi16(vone1, SHIFT_SSGN));
  #endif
      v0        = _mm256_packs_epi16(v0, v0);  // re-use vone0
      v0        = _mm256_permute4x64_epi64(v0, 0xD8);
      __m128i v = _mm256_extracti128_si256(v0, 0);
      // _mm256_zeroupper(); // does not work on GCC, TODO: find a solution with __m128i v
      _mm_storeu_si128((__m128i *)dstblk, v);
      dstblk += 16;
    }
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
 * HT cleanup encoding: helper functions
 *******************************************************************************/

// https://stackoverflow.com/a/58827596
inline __m128i sse_lzcnt_epi32(__m128i v) {
  // prevent value from being rounded up to the next power of two
  v = _mm_andnot_si128(_mm_srli_epi32(v, 8), v);  // keep 8 MSB

  v = _mm_castps_si128(_mm_cvtepi32_ps(v));    // convert an integer to float
  v = _mm_srli_epi32(v, 23);                   // shift down the exponent
  v = _mm_subs_epu16(_mm_set1_epi32(158), v);  // undo bias
  v = _mm_min_epi16(v, _mm_set1_epi32(32));    // clamp at 32

  return v;
}

auto make_storage = [](const uint8_t *ssp0, const uint8_t *ssp1, const int32_t *sp0, const int32_t *sp1,
                       __m128i &sig0, __m128i &sig1, __m128i &v0, __m128i &v1, __m128i &E0, __m128i &E1,
                       int32_t &rho0, int32_t &rho1) {
  // This function shall be called on the assumption that there are two quads
  const __m128i zero = _mm_setzero_si128();
  __m128i t0         = _mm_set1_epi64x(*((int64_t *)ssp0));
  __m128i t1         = _mm_set1_epi64x(*((int64_t *)ssp1));
  __m128i t          = _mm_unpacklo_epi8(t0, t1);
  __m128i v_u8_out   = _mm_and_si128(t, _mm_set1_epi8(1));
  v_u8_out           = _mm_cmpgt_epi8(v_u8_out, zero);
  sig0               = _mm_cvtepu8_epi32(v_u8_out);
  sig1               = _mm_cvtepu8_epi32(_mm_srli_si128(v_u8_out, 4));
  rho0               = _mm_movemask_epi8(_mm_packus_epi16(_mm_packus_epi32(sig0, zero), zero));
  rho1               = _mm_movemask_epi8(_mm_packus_epi16(_mm_packus_epi32(sig1, zero), zero));

  sig0 = _mm_cmpgt_epi32(sig0, zero);
  sig1 = _mm_cmpgt_epi32(sig1, zero);

  t0 = _mm_loadu_si128((__m128i *)sp0);
  t1 = _mm_loadu_si128((__m128i *)sp1);
  v0 = _mm_unpacklo_epi32(t0, t1);
  v1 = _mm_unpackhi_epi32(t0, t1);

  t0 = _mm_sub_epi32(_mm_set1_epi32(32), sse_lzcnt_epi32(v0));
  E0 = _mm_and_si128(t0, sig0);
  t1 = _mm_sub_epi32(_mm_set1_epi32(32), sse_lzcnt_epi32(v1));
  E1 = _mm_and_si128(t1, sig1);
};

auto make_storage_one = [](const uint8_t *ssp0, const uint8_t *ssp1, const int32_t *sp0, const int32_t *sp1,
                           __m128i &sig0, __m128i &v0, __m128i &E0, int32_t &rho0) {
  sig0 = _mm_setr_epi32(ssp0[0] & 1, ssp1[0] & 1, ssp0[1] & 1, ssp1[1] & 1);

  __m128i shift = _mm_setr_epi32(7, 7, 7, 7);
  __m128i t0    = _mm_sllv_epi32(sig0, shift);
  __m128i zero  = _mm_setzero_si128();
  rho0          = _mm_movemask_epi8(_mm_packus_epi16(_mm_packus_epi32(t0, zero), zero));

  v0 = _mm_setr_epi32(sp0[0], sp1[0], sp0[1], sp1[1]);

  sig0 = _mm_cmpgt_epi32(sig0, zero);
  t0   = _mm_sub_epi32(_mm_set1_epi32(32), sse_lzcnt_epi32(v0));
  E0   = _mm_and_si128(t0, sig0);
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

/********************************************************************************
 * HT cleanup encoding
 *******************************************************************************/
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

  const uint16_t QW = static_cast<uint16_t>(ceil_int(static_cast<int16_t>(block->size.x), 2));
  const uint16_t QH = static_cast<uint16_t>(ceil_int(static_cast<int16_t>(block->size.y), 2));

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

  // buffers shall be zeroed. unique_pre shoul be initialized by 0
  std::unique_ptr<uint8_t[]> fwd_buf = MAKE_UNIQUE<uint8_t[]>(MAX_Lcup);
  std::unique_ptr<uint8_t[]> rev_buf = MAKE_UNIQUE<uint8_t[]>(MAX_Scup);
  // memset(fwd_buf.get(), 0, sizeof(uint8_t) * (MAX_Lcup));
  // memset(rev_buf.get(), 0, sizeof(uint8_t) * MAX_Scup);

  state_MS_enc MagSgn_encoder(fwd_buf.get());
  state_MEL_enc MEL_encoder(rev_buf.get());
  state_VLC_enc VLC_encoder(rev_buf.get());

  int32_t rho0, rho1, U0, U1;

  /*******************************************************************************************************************/
  // Initial line-pair
  /*******************************************************************************************************************/
  uint8_t *ssp0 = block->block_states + 1U * (block->blkstate_stride) + 1U;
  uint8_t *ssp1 = ssp0 + block->blkstate_stride;
  int32_t *sp0  = block->sample_buf;
  int32_t *sp1  = sp0 + block->blksampl_stride;

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
  int32_t u_q, uoff, u_min, uvlc_idx, kappa = 1;
  const __m128i vshift = _mm_setr_epi32(0, 1, 2, 3);
  const __m128i vone   = _mm_set1_epi32(1);
  int32_t emb_pattern, embk_0, embk_1, emb1_0, emb1_1;
  __m128i sig0, sig1, v0, v1, E0, E1, m0, m1, known1_0, known1_1;
  __m128i Etmp, vuoff, mask, vtmp;

  int32_t qx = QW;
  for (; qx >= 2; qx -= 2) {
    bool uoff_flag = true;
    make_storage(ssp0, ssp1, sp0, sp1, sig0, sig1, v0, v1, E0, E1, rho0, rho1);
    // MEL encoding for the first quad
    if (context == 0) {
      MEL_encoder.encodeMEL((rho0 != 0));
    }

    Emax_q   = find_max(_mm_extract_epi32(E0, 0), _mm_extract_epi32(E0, 1), _mm_extract_epi32(E0, 2),
                        _mm_extract_epi32(E0, 3));
    U0       = std::max((int32_t)Emax_q, kappa);
    u_q      = U0 - kappa;
    u_min    = u_q;
    uvlc_idx = u_q;
    uoff     = (u_q) ? 1 : 0;
    uoff_flag &= uoff;
    Etmp        = _mm_set1_epi32(Emax_q);
    vuoff       = _mm_set1_epi32(uoff << 7);
    mask        = _mm_cmpeq_epi32(E0, Etmp);
    vtmp        = _mm_and_si128(vuoff, mask);
    emb_pattern = _mm_movemask_epi8(
        _mm_packus_epi16(_mm_packus_epi32(vtmp, _mm_setzero_si128()), _mm_setzero_si128()));
    n_q = emb_pattern + (rho0 << 4) + (context << 8);
    // prepare VLC encoding of quad 0
    CxtVLC = enc_CxtVLC_table0[n_q];
    embk_0 = CxtVLC & 0xF;
    emb1_0 = emb_pattern & embk_0;
    lw     = (CxtVLC >> 4) & 0x07;
    cwd    = CxtVLC >> 7;

    // context for the next quad
    context = (rho0 >> 1) | (rho0 & 0x1);

    Emax_q = find_max(_mm_extract_epi32(E1, 0), _mm_extract_epi32(E1, 1), _mm_extract_epi32(E1, 2),
                      _mm_extract_epi32(E1, 3));
    U1     = std::max((int32_t)Emax_q, kappa);
    u_q    = U1 - kappa;
    u_min  = (u_min < u_q) ? u_min : u_q;
    uvlc_idx += u_q << 5;
    uoff = (u_q) ? 1 : 0;
    uoff_flag &= uoff;
    Etmp        = _mm_set1_epi32(Emax_q);
    vuoff       = _mm_set1_epi32(uoff << 7);
    mask        = _mm_cmpeq_epi32(E1, Etmp);
    vtmp        = _mm_and_si128(vuoff, mask);
    emb_pattern = _mm_movemask_epi8(
        _mm_packus_epi16(_mm_packus_epi32(vtmp, _mm_setzero_si128()), _mm_setzero_si128()));
    n_q = emb_pattern + (rho1 << 4) + (context << 8);
    // VLC encoding of quads 0 and 1
    VLC_encoder.emitVLCBits(cwd, lw);
    CxtVLC = enc_CxtVLC_table0[n_q];
    embk_1 = CxtVLC & 0xF;
    emb1_1 = emb_pattern & embk_1;
    lw     = (CxtVLC >> 4) & 0x07;
    cwd    = CxtVLC >> 7;
    VLC_encoder.emitVLCBits(cwd, lw);
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
    m0       = _mm_sub_epi32(_mm_and_si128(sig0, _mm_set1_epi32(U0)),
                             _mm_and_si128(_mm_srlv_epi32(_mm_set1_epi32(embk_0), vshift), vone));
    m1       = _mm_sub_epi32(_mm_and_si128(sig1, _mm_set1_epi32(U1)),
                             _mm_and_si128(_mm_srlv_epi32(_mm_set1_epi32(embk_1), vshift), vone));
    known1_0 = _mm_and_si128(_mm_srlv_epi32(_mm_set1_epi32(emb1_0), vshift), vone);
    known1_1 = _mm_and_si128(_mm_srlv_epi32(_mm_set1_epi32(emb1_1), vshift), vone);
    MagSgn_encoder.emitBits(v0, m0, known1_0);
    MagSgn_encoder.emitBits(v1, m1, known1_1);

    // context for the next quad
    context = (rho1 >> 1) | (rho1 & 0x1);
    // update rho_line
    *rho_p++ = rho0;
    *rho_p++ = rho1;
    // update Eline
    E0 = _mm_shuffle_epi32(E0, 0xD8);
    E1 = _mm_shuffle_epi32(E1, 0xD8);
    _mm_storeu_si128((__m128i *)E_p, _mm_unpackhi_epi32(E0, E1));
    E_p += 4;
    // update pointer to line buffer
    ssp0 += 4;
    ssp1 += 4;
    sp0 += 4;
    sp1 += 4;
  }
  if (qx) {
    make_storage_one(ssp0, ssp1, sp0, sp1, sig0, v0, E0, rho0);
    // MEL encoding for the first quad
    if (context == 0) {
      MEL_encoder.encodeMEL((rho0 != 0));
    }
    Emax_q      = find_max(_mm_extract_epi32(E0, 0), _mm_extract_epi32(E0, 1), _mm_extract_epi32(E0, 2),
                           _mm_extract_epi32(E0, 3));
    U0          = std::max((int32_t)Emax_q, kappa);
    u_q         = U0 - kappa;
    uvlc_idx    = u_q;
    uoff        = (u_q) ? 1 : 0;
    Etmp        = _mm_set1_epi32(Emax_q);
    vuoff       = _mm_set1_epi32(uoff << 7);
    mask        = _mm_cmpeq_epi32(E0, Etmp);
    vtmp        = _mm_and_si128(vuoff, mask);
    emb_pattern = _mm_movemask_epi8(
        _mm_packus_epi16(_mm_packus_epi32(vtmp, _mm_setzero_si128()), _mm_setzero_si128()));
    n_q = emb_pattern + (rho0 << 4) + (context << 8);
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
    m0       = _mm_sub_epi32(_mm_and_si128(sig0, _mm_set1_epi32(U0)),
                             _mm_and_si128(_mm_srlv_epi32(_mm_set1_epi32(embk_0), vshift), vone));
    known1_0 = _mm_and_si128(_mm_srlv_epi32(_mm_set1_epi32(emb1_0), vshift), vone);
    MagSgn_encoder.emitBits(v0, m0, known1_0);

    *E_p++ = _mm_extract_epi32(E0, 1);
    *E_p++ = _mm_extract_epi32(E0, 3);
    // update rho_line
    *rho_p++ = rho0;
  }

  /*******************************************************************************************************************/
  // Non-initial line-pair
  /*******************************************************************************************************************/
  int32_t Emax0, Emax1;
  for (uint16_t qy = 1; qy < QH; qy++) {
    E_p   = Eline.get() + 1;
    rho_p = rholine.get() + 1;
    rho1  = 0;

    Emax0 = find_max(E_p[-1], E_p[0], E_p[1], E_p[2]);
    Emax1 = find_max(E_p[1], E_p[2], E_p[3], E_p[4]);

    // calculate context for the next quad
    context = ((rho1 & 0x4) << 7) | ((rho1 & 0x8) << 6);            // (w | sw) << 9
    context |= ((rho_p[-1] & 0x8) << 5) | ((rho_p[0] & 0xa) << 7);  // ((nw | n) << 8) | (ne << 10)
    context |= (rho_p[1] & 0x2) << 9;                               // (nf) << 10

    ssp0 = block->block_states + (2U * qy + 1U) * (block->blkstate_stride) + 1U;
    ssp1 = ssp0 + block->blkstate_stride;
    sp0  = block->sample_buf + 2U * (qy * block->blksampl_stride);
    sp1  = sp0 + block->blksampl_stride;

    qx = QW;
    for (; qx >= 2; qx -= 2) {
      make_storage(ssp0, ssp1, sp0, sp1, sig0, sig1, v0, v1, E0, E1, rho0, rho1);
      // MEL encoding of the first quad
      if (context == 0) {
        MEL_encoder.encodeMEL((rho0 != 0));
      }

      gamma       = ((rho0 & (rho0 - 1)) == 0) ? 0 : (int32_t)0xFFFFFFFF;
      kappa       = std::max((Emax0 - 1) & gamma, 1);
      Emax_q      = find_max(_mm_extract_epi32(E0, 0), _mm_extract_epi32(E0, 1), _mm_extract_epi32(E0, 2),
                             _mm_extract_epi32(E0, 3));
      U0          = std::max((int32_t)Emax_q, kappa);
      u_q         = U0 - kappa;
      uvlc_idx    = u_q;
      uoff        = (u_q) ? 1 : 0;
      Etmp        = _mm_set1_epi32(Emax_q);
      vuoff       = _mm_set1_epi32(uoff << 7);
      mask        = _mm_cmpeq_epi32(E0, Etmp);
      vtmp        = _mm_and_si128(vuoff, mask);
      emb_pattern = _mm_movemask_epi8(
          _mm_packus_epi16(_mm_packus_epi32(vtmp, _mm_setzero_si128()), _mm_setzero_si128()));
      n_q = emb_pattern + (rho0 << 4) + (context << 0);
      // prepare VLC encoding of quad 0
      CxtVLC = enc_CxtVLC_table1[n_q];
      embk_0 = CxtVLC & 0xF;
      emb1_0 = emb_pattern & embk_0;
      lw     = (CxtVLC >> 4) & 0x07;
      // lw  = _pext_u32(CxtVLC, 0x70);
      cwd = CxtVLC >> 7;

      // calculate context for the next quad
      context = ((rho0 & 0x4) << 7) | ((rho0 & 0x8) << 6);           // (w | sw) << 9
      context |= ((rho_p[0] & 0x8) << 5) | ((rho_p[1] & 0xa) << 7);  // ((nw | n) << 8) | (ne << 10)
      context |= (rho_p[2] & 0x2) << 9;                              // (nf) << 10
      // MEL encoding of the second quad
      if (context == 0) {
        MEL_encoder.encodeMEL((rho1 != 0));
      }
      gamma  = ((rho1 & (rho1 - 1)) == 0) ? 0 : 1;
      kappa  = std::max((Emax1 - 1) * gamma, 1);
      Emax_q = find_max(_mm_extract_epi32(E1, 0), _mm_extract_epi32(E1, 1), _mm_extract_epi32(E1, 2),
                        _mm_extract_epi32(E1, 3));
      U1     = std::max((int32_t)Emax_q, kappa);
      u_q    = U1 - kappa;
      uvlc_idx += u_q << 5;
      uoff        = (u_q) ? 1 : 0;
      Etmp        = _mm_set1_epi32(Emax_q);
      vuoff       = _mm_set1_epi32(uoff << 7);
      mask        = _mm_cmpeq_epi32(E1, Etmp);
      vtmp        = _mm_and_si128(vuoff, mask);
      emb_pattern = _mm_movemask_epi8(
          _mm_packus_epi16(_mm_packus_epi32(vtmp, _mm_setzero_si128()), _mm_setzero_si128()));
      n_q = emb_pattern + (rho1 << 4) + (context << 0);
      // VLC encoding of quads 0 and 1
      VLC_encoder.emitVLCBits(cwd, lw);
      CxtVLC = enc_CxtVLC_table1[n_q];
      embk_1 = CxtVLC & 0xF;
      emb1_1 = emb_pattern & embk_1;
      lw     = (CxtVLC >> 4) & 0x07;
      // lw  = _pext_u32(CxtVLC, 0x70);
      cwd = CxtVLC >> 7;
      VLC_encoder.emitVLCBits(cwd, lw);
      // UVLC encoding
      uint32_t tmp = enc_UVLC_table1[uvlc_idx];
      lw           = tmp & 0xFF;
      cwd          = tmp >> 8;
      VLC_encoder.emitVLCBits(cwd, lw);

      // MagSgn encoding
      m0       = _mm_sub_epi32(_mm_and_si128(sig0, _mm_set1_epi32(U0)),
                               _mm_and_si128(_mm_srlv_epi32(_mm_set1_epi32(embk_0), vshift), vone));
      m1       = _mm_sub_epi32(_mm_and_si128(sig1, _mm_set1_epi32(U1)),
                               _mm_and_si128(_mm_srlv_epi32(_mm_set1_epi32(embk_1), vshift), vone));
      known1_0 = _mm_and_si128(_mm_srlv_epi32(_mm_set1_epi32(emb1_0), vshift), vone);
      known1_1 = _mm_and_si128(_mm_srlv_epi32(_mm_set1_epi32(emb1_1), vshift), vone);
      MagSgn_encoder.emitBits(v0, m0, known1_0);
      MagSgn_encoder.emitBits(v1, m1, known1_1);

      Emax0 = find_max(E_p[3], E_p[4], E_p[5], E_p[6]);
      Emax1 = find_max(E_p[5], E_p[6], E_p[7], E_p[8]);

      // update Eline
      E0 = _mm_shuffle_epi32(E0, 0xD8);
      E1 = _mm_shuffle_epi32(E1, 0xD8);
      _mm_storeu_si128((__m128i *)E_p, _mm_unpackhi_epi32(E0, E1));
      E_p += 4;

      // calculate context for the next quad
      context = ((rho1 & 0x4) << 7) | ((rho1 & 0x8) << 6);           // (w | sw) << 9
      context |= ((rho_p[1] & 0x8) << 5) | ((rho_p[2] & 0xa) << 7);  // ((nw | n) << 8) | (ne << 10)
      context |= (rho_p[3] & 0x2) << 9;                              // (nf) << 10

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
      // MEL encoding of the first quad
      if (context == 0) {
        MEL_encoder.encodeMEL((rho0 != 0));
      }
      //(popcount32((uint32_t)rho0) > 1) ? 0xFFFFFFFF : 0;
      gamma    = ((rho0 & (rho0 - 1)) == 0) ? 0 : (int32_t)0xFFFFFFFF;
      kappa    = std::max((Emax0 - 1) & gamma, 1);
      Emax_q   = find_max(_mm_extract_epi32(E0, 0), _mm_extract_epi32(E0, 1), _mm_extract_epi32(E0, 2),
                          _mm_extract_epi32(E0, 3));
      U0       = std::max((int32_t)Emax_q, kappa);
      u_q      = U0 - kappa;
      uvlc_idx = u_q;
      uoff     = (u_q) ? 1 : 0;

      Etmp        = _mm_set1_epi32(Emax_q);
      vuoff       = _mm_set1_epi32(uoff << 7);
      mask        = _mm_cmpeq_epi32(E0, Etmp);
      vtmp        = _mm_and_si128(vuoff, mask);
      emb_pattern = _mm_movemask_epi8(
          _mm_packus_epi16(_mm_packus_epi32(vtmp, _mm_setzero_si128()), _mm_setzero_si128()));
      n_q = emb_pattern + (rho0 << 4) + (context << 0);
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
      m0       = _mm_sub_epi32(_mm_and_si128(sig0, _mm_set1_epi32(U0)),
                               _mm_and_si128(_mm_srlv_epi32(_mm_set1_epi32(embk_0), vshift), vone));
      known1_0 = _mm_and_si128(_mm_srlv_epi32(_mm_set1_epi32(emb1_0), vshift), vone);
      MagSgn_encoder.emitBits(v0, m0, known1_0);

      *E_p++ = _mm_extract_epi32(E0, 1);
      *E_p++ = _mm_extract_epi32(E0, 3);
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
        sp[0] |= static_cast<uint8_t>(bit << SHIFT_REF);
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