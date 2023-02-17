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

#if defined(OPENHTJ2K_TRY_AVX2) && defined(__AVX2__)
  #include "coding_units.hpp"
  #include "dec_CxtVLC_tables.hpp"
  #include "ht_block_decoding.hpp"
  #include "coding_local.hpp"
  #include "utils.hpp"

  #if defined(_MSC_VER) || defined(__MINGW64__)
    #include <intrin.h>
  #else
    #include <x86intrin.h>
  #endif

uint8_t j2k_codeblock::calc_mbr(const uint32_t i, const uint32_t j, const uint8_t causal_cond) const {
  uint8_t *state_p0 = block_states + static_cast<size_t>(i) * blkstate_stride + j;
  uint8_t *state_p1 = block_states + static_cast<size_t>(i + 1) * blkstate_stride + j;
  uint8_t *state_p2 = block_states + static_cast<size_t>(i + 2) * blkstate_stride + j;

  uint32_t mbr0 = state_p0[0] | state_p0[1] | state_p0[2];
  uint32_t mbr1 = state_p1[0] | state_p1[2];
  uint32_t mbr2 = state_p2[0] | state_p2[1] | state_p2[2];
  uint32_t mbr  = mbr0 | mbr1 | (mbr2 & causal_cond);
  mbr |= (mbr0 >> SHIFT_REF) & (mbr0 >> SHIFT_SCAN);
  mbr |= (mbr1 >> SHIFT_REF) & (mbr1 >> SHIFT_SCAN);
  mbr |= (mbr2 >> SHIFT_REF) & (mbr2 >> SHIFT_SCAN) & causal_cond;
  return mbr & 1;
}

// https://stackoverflow.com/a/58827596
inline __m128i sse_lzcnt_epi32(__m128i v) {
  // prevent value from being rounded up to the next power of two
  v = _mm_andnot_si128(_mm_srli_epi32(v, 8), v);  // keep 8 MSB

  v = _mm_castps_si128(_mm_cvtepi32_ps(v));    // convert an integer to float
  v = _mm_srli_epi32(v, 23);                   // shift down the exponent
  v = _mm_subs_epu16(_mm_set1_epi32(158), v);  // undo bias
  v = _mm_min_epi16(v, _mm_set1_epi32(32));    // clamp at 32

  // __m128i t;
  // const __m128i lut_lo        = _mm_set_epi8(4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 7, 31);
  // const __m128i lut_hi        = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 31);
  // const __m128i nibble_mask   = _mm_set1_epi8(0x0F);
  // const __m128i byte_offset8  = _mm_set1_epi16(8);
  // const __m128i byte_offset16 = _mm_set1_epi16(16);
  // t                           = _mm_and_si128(nibble_mask, v);
  // v                           = _mm_and_si128(_mm_srli_epi16(v, 4), nibble_mask);
  // t                           = _mm_shuffle_epi8(lut_lo, t);
  // v                           = _mm_shuffle_epi8(lut_hi, v);
  // v                           = _mm_min_epu8(v, t);

  // t = _mm_srli_epi16(v, 8);
  // v = _mm_or_si128(v, byte_offset8);
  // v = _mm_min_epu8(v, t);

  // t = _mm_srli_epi32(v, 16);
  // v = _mm_or_si128(v, byte_offset16);
  // v = _mm_min_epu8(v, t);
  return v;
}

void ht_cleanup_decode(j2k_codeblock *block, const uint8_t &pLSB, const int32_t Lcup, const int32_t Pcup,
                       const int32_t Scup) {
  uint8_t *compressed_data = block->get_compressed_data();
  const uint16_t QW        = static_cast<uint16_t>(ceil_int(static_cast<int16_t>(block->size.x), 2));
  const uint16_t QH        = static_cast<uint16_t>(ceil_int(static_cast<int16_t>(block->size.y), 2));

  uint16_t scratch[8 * 513] = {0};
  int32_t sstr              = static_cast<int32_t>(((block->size.x + 2) + 7u) & ~7u);  // multiples of 8
  uint16_t *sp;
  int32_t qx;
  /*******************************************************************************************************************/
  // VLC, UVLC and MEL decoding
  /*******************************************************************************************************************/
  MEL_dec MEL(compressed_data, Lcup, Scup);
  rev_buf VLC_dec(compressed_data, Lcup, Scup);
  auto sp0 = block->block_states + 1 + block->blkstate_stride;
  auto sp1 = block->block_states + 1 + 2 * block->blkstate_stride;
  uint32_t u_off0, u_off1;
  uint32_t u0, u1;
  uint32_t context = 0;
  uint32_t vlcval;

  const uint16_t *dec_table;
  // Initial line-pair
  dec_table       = dec_CxtVLC_table0_fast_16;
  sp              = scratch;
  int32_t mel_run = MEL.get_run();
  for (qx = QW; qx > 0; qx -= 2, sp += 4) {
    // Decoding of significance and EMB patterns and unsigned residual offsets
    vlcval       = VLC_dec.fetch();
    uint16_t tv0 = dec_table[(vlcval & 0x7F) + context];
    if (context == 0) {
      mel_run -= 2;
      tv0 = (mel_run == -1) ? tv0 : 0;
      if (mel_run < 0) {
        mel_run = MEL.get_run();
      }
    }
    sp[0] = tv0;

    // calculate context for the next quad, Eq. (1) in the spec
    context = ((tv0 & 0xE0U) << 2) | ((tv0 & 0x10U) << 3);  // = context << 7

    // Decoding of significance and EMB patterns and unsigned residual offsets
    vlcval       = VLC_dec.advance((tv0 & 0x000F) >> 1);
    uint16_t tv1 = dec_table[(vlcval & 0x7F) + context];
    if (context == 0 && qx > 1) {
      mel_run -= 2;
      tv1 = (mel_run == -1) ? tv1 : 0;
      if (mel_run < 0) {
        mel_run = MEL.get_run();
      }
    }
    tv1   = (qx > 1) ? tv1 : 0;
    sp[2] = tv1;

    // store sigma
    *sp0++ = ((tv0 >> 4) >> 0) & 1;
    *sp0++ = ((tv0 >> 4) >> 2) & 1;
    *sp0++ = ((tv1 >> 4) >> 0) & 1;
    *sp0++ = ((tv1 >> 4) >> 2) & 1;
    *sp1++ = ((tv0 >> 4) >> 1) & 1;
    *sp1++ = ((tv0 >> 4) >> 3) & 1;
    *sp1++ = ((tv1 >> 4) >> 1) & 1;
    *sp1++ = ((tv1 >> 4) >> 3) & 1;

    // calculate context for the next quad, Eq. (1) in the spec
    context = ((tv1 & 0xE0U) << 2) | ((tv1 & 0x10U) << 3);  // = context << 7

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

    // UVLC decoding
    uint32_t idx         = (vlcval & 0x3F) + (u_off0 << 6U) + (u_off1 << 7U) + mel_offset;
    uint32_t uvlc_result = uvlc_dec_0[idx];
    // remove total prefix length
    vlcval = VLC_dec.advance(uvlc_result & 0x7);
    uvlc_result >>= 3;
    // extract suffixes for quad 0 and 1
    uint32_t len = uvlc_result & 0xF;  // suffix length for 2 quads (up to 10 = 5 + 5)
    //  ((1U << len) - 1U) can be replaced with _bzhi_u32(UINT32_MAX, len); not fast
    uint32_t tmp = vlcval & ((1U << len) - 1U);  // suffix value for 2 quads
    vlcval       = VLC_dec.advance(len);
    uvlc_result >>= 4;
    // quad 0 length
    len = uvlc_result & 0x7;  // quad 0 suffix length
    uvlc_result >>= 3;
    // U = 1+ u
    u0 = 1 + (uvlc_result & 7) + (tmp & ~(0xFFU << len));  // always kappa = 1 in initial line pair
    u1 = 1 + (uvlc_result >> 3) + (tmp >> len);            // always kappa = 1 in initial line pair

    sp[1] = static_cast<uint16_t>(u0);
    sp[3] = static_cast<uint16_t>(u1);
  }
  // sp[0] = sp[1] = 0;

  // Non-initial line-pair
  dec_table = dec_CxtVLC_table1_fast_16;
  for (uint16_t row = 1; row < QH; row++) {
    sp0 = block->block_states + (row * 2U + 1U) * block->blkstate_stride + 1U;
    sp1 = sp0 + block->blkstate_stride;

    sp = scratch + row * sstr;
    // calculate context for the next quad: w, sw, nw are always 0 at the head of a row
    context = ((sp[0 - sstr] & 0xA0U) << 2) | ((sp[2 - sstr] & 0x20U) << 4);
    for (qx = QW; qx > 0; qx -= 2, sp += 4) {
      // Decoding of significance and EMB patterns and unsigned residual offsets
      vlcval       = VLC_dec.fetch();
      uint16_t tv0 = dec_table[(vlcval & 0x7F) + context];
      if (context == 0) {
        mel_run -= 2;
        tv0 = (mel_run == -1) ? tv0 : 0;
        if (mel_run < 0) {
          mel_run = MEL.get_run();
        }
      }
      // calculate context for the next quad, Eq. (2) in the spec
      context = ((tv0 & 0x40U) << 2) | ((tv0 & 0x80U) << 1);              // (w | sw) << 8
      context |= (sp[0 - sstr] & 0x80U) | ((sp[2 - sstr] & 0xA0U) << 2);  // ((nw | n) << 7) | (ne << 9)
      context |= (sp[4 - sstr] & 0x20U) << 4;                             // ( nf) << 9

      sp[0] = tv0;

      vlcval = VLC_dec.advance((tv0 & 0x000F) >> 1);

      // Decoding of significance and EMB patterns and unsigned residual offsets
      uint16_t tv1 = dec_table[(vlcval & 0x7F) + context];
      if (context == 0 && qx > 1) {
        mel_run -= 2;
        tv1 = (mel_run == -1) ? tv1 : 0;
        if (mel_run < 0) {
          mel_run = MEL.get_run();
        }
      }
      tv1 = (qx > 1) ? tv1 : 0;
      // calculate context for the next quad, Eq. (2) in the spec
      context = ((tv1 & 0x40U) << 2) | ((tv1 & 0x80U) << 1);              // (w | sw) << 8
      context |= (sp[2 - sstr] & 0x80U) | ((sp[4 - sstr] & 0xA0U) << 2);  // ((nw | n) << 7) | (ne << 9)
      context |= (sp[6 - sstr] & 0x20U) << 4;                             // ( nf) << 9

      sp[2] = tv1;

      // store sigma
      *sp0++ = ((tv0 >> 4) >> 0) & 1;
      *sp0++ = ((tv0 >> 4) >> 2) & 1;
      *sp0++ = ((tv1 >> 4) >> 0) & 1;
      *sp0++ = ((tv1 >> 4) >> 2) & 1;
      *sp1++ = ((tv0 >> 4) >> 1) & 1;
      *sp1++ = ((tv0 >> 4) >> 3) & 1;
      *sp1++ = ((tv1 >> 4) >> 1) & 1;
      *sp1++ = ((tv1 >> 4) >> 3) & 1;

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
      uint32_t len = uvlc_result & 0xF;  // suffix length for 2 quads (up to 10 = 5 + 5)
      //  ((1U << len) - 1U) can be replaced with _bzhi_u32(UINT32_MAX, len); not fast
      uint32_t tmp = vlcval & ((1U << len) - 1U);  // suffix value for 2 quads
      vlcval       = VLC_dec.advance(len);
      uvlc_result >>= 4;
      // quad 0 length
      len = uvlc_result & 0x7;  // quad 0 suffix length
      uvlc_result >>= 3;
      u0 = (uvlc_result & 7) + (tmp & ~(0xFFU << len));
      u1 = (uvlc_result >> 3) + (tmp >> len);

      sp[1] = static_cast<uint16_t>(u0);
      sp[3] = static_cast<uint16_t>(u1);
    }
    // sp[0] = sp[1] = 0;
  }

  /*******************************************************************************************************************/
  // MagSgn decoding
  /*******************************************************************************************************************/
  int32_t *const sample_buf = block->sample_buf;
  int32_t *mp0              = sample_buf;
  int32_t *mp1              = sample_buf + block->blksampl_stride;

  alignas(32) auto Eline = MAKE_UNIQUE<int32_t[]>(2U * QW + 8U);
  Eline[0]               = 0;
  int32_t *E_p           = Eline.get() + 1;

  __m128i v_n, qinf, U_q, mu0_n, mu1_n;
  fwd_buf<0xFF> MagSgn(compressed_data, Pcup);

  // Initial line-pair
  sp = scratch;
  for (qx = QW; qx > 0; qx -= 2, sp += 4) {
    v_n   = _mm_setzero_si128();
    qinf  = _mm_loadu_si128((__m128i *)sp);
    U_q   = _mm_srli_epi32(qinf, 16);
    mu0_n = MagSgn.decode_one_quad<0>(qinf, U_q, pLSB, v_n);  // 0, 1, 2, 3
    mu1_n = MagSgn.decode_one_quad<1>(qinf, U_q, pLSB, v_n);  // 4, 5, 6, 7

    // store mu
    auto t0 = _mm_unpacklo_epi32(mu0_n, mu1_n);  // 0, 4, 1, 5
    auto t1 = _mm_unpackhi_epi32(mu0_n, mu1_n);  // 2, 6, 3, 7
    mu0_n   = _mm_unpacklo_epi32(t0, t1);        // 0, 2, 4, 6
    mu1_n   = _mm_unpackhi_epi32(t0, t1);        // 1, 3, 5, 7
    _mm_storeu_si128((__m128i *)mp0, mu0_n);
    _mm_storeu_si128((__m128i *)mp1, mu1_n);
    mp0 += 4;
    mp1 += 4;

    // Update Exponent
    v_n = sse_lzcnt_epi32(v_n);
    v_n = _mm_sub_epi32(_mm_set1_epi32(32), v_n);
    _mm_storeu_si128((__m128i *)E_p, v_n);
    E_p += 4;
  }
  // Non-initial line-pair
  for (uint16_t row = 1; row < QH; row++) {
    E_p = Eline.get() + 1;
    mp0 = sample_buf + (row * 2U) * block->blksampl_stride;
    mp1 = mp0 + block->blksampl_stride;

    sp = scratch + row * sstr;

    // Calculate Emax for the next two quads
    int32_t Emax0, Emax1;
    Emax0 = find_max(E_p[-1], E_p[0], E_p[1], E_p[2]);
    Emax1 = find_max(E_p[1], E_p[2], E_p[3], E_p[4]);
    // Emax0 = hMax(_mm_loadu_si128((__m128i *)(E_p - 1)));
    // Emax1 = hMax(_mm_loadu_si128((__m128i *)(E_p + 1)));
    for (qx = QW; qx > 0; qx -= 2, sp += 4) {
      v_n  = _mm_setzero_si128();
      qinf = _mm_loadu_si128((__m128i *)sp);
      {
        // Compute gamma, kappa, U_q with Emax
        // The SIMD code below does the following
        //
        // gamma0 = (popcount32(rho0) < 2) ? 0 : 1;
        // gamma1 = (popcount32(rho1) < 2) ? 0 : 1;
        // kappa0 = (1 > gamma0 * (Emax0 - 1)) ? 1U : static_cast<uint8_t>(gamma0 * (Emax0 - 1));
        // kappa1 = (1 > gamma1 * (Emax1 - 1)) ? 1U : static_cast<uint8_t>(gamma1 * (Emax1 - 1));
        // U0     = kappa0 + u0;
        // U1     = kappa1 + u1;

        __m128i gamma, emax, kappa, u_q, w0;  // needed locally
        gamma = _mm_and_si128(qinf, _mm_set1_epi32(0xF0));
        w0    = _mm_sub_epi32(gamma, _mm_set1_epi32(1));
        gamma = _mm_and_si128(gamma, w0);
        gamma = _mm_cmpeq_epi32(gamma, _mm_setzero_si128());

        emax  = _mm_set_epi32(0, 0, Emax1 - 1, Emax0 - 1);
        emax  = _mm_andnot_si128(gamma, emax);
        kappa = _mm_set1_epi32(1);
        kappa = _mm_max_epi16(emax, kappa);  // no max_epi32 in ssse3

        u_q = _mm_srli_epi32(qinf, 16);
        U_q = _mm_add_epi32(u_q, kappa);
      }
      mu0_n = MagSgn.decode_one_quad<0>(qinf, U_q, pLSB, v_n);  // 0, 1, 2, 3
      mu1_n = MagSgn.decode_one_quad<1>(qinf, U_q, pLSB, v_n);  // 4, 5, 6, 7

      // store mu
      auto t0 = _mm_unpacklo_epi32(mu0_n, mu1_n);  // 0, 4, 1, 5
      auto t1 = _mm_unpackhi_epi32(mu0_n, mu1_n);  // 2, 6, 3, 7
      mu0_n   = _mm_unpacklo_epi32(t0, t1);        // 0, 2, 4, 6
      mu1_n   = _mm_unpackhi_epi32(t0, t1);        // 1, 3, 5, 7
      _mm_storeu_si128((__m128i *)mp0, mu0_n);
      _mm_storeu_si128((__m128i *)mp1, mu1_n);
      mp0 += 4;
      mp1 += 4;

      // Update Exponent
      Emax0 = find_max(E_p[3], E_p[4], E_p[5], E_p[6]);
      Emax1 = find_max(E_p[5], E_p[6], E_p[7], E_p[8]);
      // Emax0 = hMax(_mm_loadu_si128((__m128i *)(E_p + 3)));
      // Emax1 = hMax(_mm_loadu_si128((__m128i *)(E_p + 5)));
      v_n = sse_lzcnt_epi32(v_n);
      v_n = _mm_sub_epi32(_mm_set1_epi32(32), v_n);
      _mm_storeu_si128((__m128i *)E_p, v_n);
      E_p += 4;
    }
  }  // Non-Initial line-pair end
}

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
        state_p[0] |= static_cast<uint8_t>(bit << SHIFT_REF);
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
  for (uint32_t n1 = 0; n1 < num_v_stripe; n1++) {
    j_start = 0;
    for (uint32_t n2 = 0; n2 < num_h_stripe; n2++) {
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
  for (uint32_t n2 = 0; n2 < num_h_stripe; n2++) {
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
  const uint32_t mask = UINT32_MAX >> (M_b + 1);

  const __m256i magmask = _mm256_set1_epi32(0x7FFFFFFF);
  const __m256i vmask   = _mm256_set1_epi32(static_cast<int32_t>(~mask));
  const __m256i zero    = _mm256_setzero_si256();
  const __m256i shift   = _mm256_set1_epi32(ROIshift);
  __m256i v0, v1, s0, s1, vdst0, vdst1, vROImask;
  if (this->transformation) {
    // lossless path
    for (size_t i = 0; i < static_cast<size_t>(this->size.y); i++) {
      int32_t *val = this->sample_buf + i * this->blksampl_stride;
      sprec_t *dst = this->i_samples + i * this->band_stride;
      size_t len   = this->size.x;
      for (; len >= 16; len -= 16) {
        v0 = _mm256_loadu_si256((__m256i *)val);
        v1 = _mm256_loadu_si256((__m256i *)(val + 8));
        s0 = v0;  //_mm256_or_si256(_mm256_and_si256(v0, signmask), one);
        s1 = v1;  //_mm256_or_si256(_mm256_and_si256(v1, signmask), one);
        v0 = _mm256_and_si256(v0, magmask);
        v1 = _mm256_and_si256(v1, magmask);
        // upshift background region, if necessary
        vROImask = _mm256_and_si256(v0, vmask);
        vROImask = _mm256_cmpeq_epi32(vROImask, zero);
        vROImask = _mm256_and_si256(vROImask, shift);
        v0       = _mm256_sllv_epi32(v0, vROImask);
        vROImask = _mm256_and_si256(v1, vmask);
        vROImask = _mm256_cmpeq_epi32(vROImask, zero);
        vROImask = _mm256_and_si256(vROImask, shift);
        v1       = _mm256_sllv_epi32(v1, vROImask);

        // convert values from sign-magnitude form to two's complement one
        vdst0 = _mm256_sign_epi32(_mm256_srai_epi32(v0, pLSB), s0);
        vdst1 = _mm256_sign_epi32(_mm256_srai_epi32(v1, pLSB), s1);
        v0    = _mm256_permute4x64_epi64(_mm256_packs_epi32(vdst0, vdst1), 0xD8);
        _mm256_storeu_si256((__m256i *)dst, v0);
        val += 16;
        dst += 16;
      }
      for (; len > 0; --len) {
        int32_t sign = *val & INT32_MIN;
        *val &= INT32_MAX;
        // detect background region and upshift it
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
      for (; len >= 16; len -= 16) {
        v0 = _mm256_loadu_si256((__m256i *)val);
        v1 = _mm256_loadu_si256((__m256i *)(val + 8));
        s0 = v0;  //_mm256_or_si256(_mm256_and_si256(v0, signmask), one);
        s1 = v1;  //_mm256_or_si256(_mm256_and_si256(v1, signmask), one);
        v0 = _mm256_and_si256(v0, magmask);
        v1 = _mm256_and_si256(v1, magmask);
        // upshift background region, if necessary
        vROImask = _mm256_and_si256(v0, vmask);
        vROImask = _mm256_cmpeq_epi32(vROImask, zero);
        vROImask = _mm256_and_si256(vROImask, shift);
        v0       = _mm256_sllv_epi32(v0, vROImask);
        vROImask = _mm256_and_si256(v1, vmask);
        vROImask = _mm256_cmpeq_epi32(vROImask, zero);
        vROImask = _mm256_and_si256(vROImask, shift);
        v1       = _mm256_sllv_epi32(v1, vROImask);

        // to prevent overflow, truncate to int16_t range
        v0 = _mm256_srai_epi32(_mm256_add_epi32(v0, _mm256_set1_epi32(1 << 15)), 16);
        v1 = _mm256_srai_epi32(_mm256_add_epi32(v1, _mm256_set1_epi32(1 << 15)), 16);

        // dequantization
        v0 = _mm256_mullo_epi32(v0, _mm256_set1_epi32(scale));
        v1 = _mm256_mullo_epi32(v1, _mm256_set1_epi32(scale));

        // downshift and convert values from sign-magnitude form to two's complement one
        v0 = _mm256_srai_epi32(_mm256_add_epi32(v0, _mm256_set1_epi32(1 << (downshift - 1))), downshift);
        v1 = _mm256_srai_epi32(_mm256_add_epi32(v1, _mm256_set1_epi32(1 << (downshift - 1))), downshift);

        v0 = _mm256_sign_epi32(v0, s0);
        v1 = _mm256_sign_epi32(v1, s1);

        _mm256_storeu_si256((__m256i *)dst, _mm256_permute4x64_epi64(_mm256_packs_epi32(v0, v1), 0xD8));

        val += 16;
        dst += 16;
      }
      for (; len > 0; --len) {
        int32_t sign = *val & INT32_MIN;
        *val &= INT32_MAX;
        // detect background region and upshift it
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
  const uint8_t empty_passes = static_cast<uint8_t>(P0 * 3);
  if (block->num_passes < empty_passes) {
    printf("WARNING: number of passes %d exceeds number of empty passes %d", block->num_passes,
           empty_passes);
    return false;
  }
  // number of ht coding pass (Z_blk in the spec)
  const uint8_t num_ht_passes = static_cast<uint8_t>(block->num_passes - empty_passes);
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
    for (uint32_t i = 1; i < all_segments.size(); i++) {
      Lref += block->pass_length[all_segments[i]];
    }
    Dcup = block->get_compressed_data();

    if (block->num_passes > 1 && all_segments.size() > 1) {
      Dref = block->get_compressed_data() + Lcup;
    } else {
      Dref = nullptr;
    }
    // number of (skipped) magnitude bitplanes
    const uint8_t S_blk = static_cast<uint8_t>(P0 + block->num_ZBP + S_skip);
    if (S_blk >= 30) {
      printf("WARNING: Number of skipped mag bitplanes %d is too large.\n", S_blk);
      return false;
    }
    // Suffix length (=MEL + VLC) of HT Cleanup pass
    const int32_t Scup = static_cast<int32_t>((Dcup[Lcup - 1] << 4) + (Dcup[Lcup - 2] & 0x0F));
    if (Scup < 2 || Scup > Lcup || Scup > 4079) {
      printf("WARNING: cleanup pass suffix length %d is invalid.\n", Scup);
      return false;
    }
    // modDcup (shall be done before the creation of state_VLC instance)
    Dcup[Lcup - 1] = 0xFF;
    Dcup[Lcup - 2] |= 0x0F;
    const int32_t Pcup = static_cast<int32_t>(Lcup - Scup);

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