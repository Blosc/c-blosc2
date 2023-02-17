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

#include "mq_decoder.hpp"

#include <cstdio>
#include <stdexcept>

#if defined(MQNAIVE)
  #define MQMINOFF 0
#else
  #define MQMINOFF 8
#endif
static constexpr int32_t Areg_min = 1 << (15 + MQMINOFF);

// static table for state transition (32bits)
// Xs (1bit), Sigma_mps (6bit in 7bit),Sigma_lps (6bit in 8bit), Qe (16bit)
static constexpr uint32_t static_table[47] = {
    0x81015601, 0x02063401, 0x03091801, 0x040c0ac1, 0x051d0521, 0x26210221, 0x87065601, 0x080e5401,
    0x090e4801, 0x0a0e3801, 0x0b113001, 0x0c122401, 0x0d141c01, 0x1d151601, 0x8f0e5601, 0x100e5401,
    0x110f5101, 0x12104801, 0x13113801, 0x14123401, 0x15133001, 0x16132801, 0x17142401, 0x18152201,
    0x19161c01, 0x1a171801, 0x1b181601, 0x1c191401, 0x1d1a1201, 0x1e1b1101, 0x1f1c0ac1, 0x201d09c1,
    0x211e08a1, 0x221f0521, 0x23200441, 0x242102a1, 0x25220221, 0x26230141, 0x27240111, 0x28250085,
    0x29260049, 0x2a270025, 0x2b280015, 0x2c290009, 0x2d2a0005, 0x2d2b0001, 0x2e2e5601};

mq_decoder::mq_decoder(const uint8_t *const buf)
    : A(0), t(0), C(0), T(0), L(0), L_start(0), Lmax(0), dynamic_table{{}}, byte_buffer(buf) {}

void mq_decoder::init(uint32_t buf_pos, uint32_t segment_length, bool is_bypass) {
  L_start = buf_pos;
  Lmax    = buf_pos + segment_length;
  L       = buf_pos;  // this means L points the beginning of a codeword segment (L=0)
  T       = 0;
  if (is_bypass) {
    t = 0;
  } else {
    A = Areg_min;
    C = 0;
    fill_LSBs();
    C <<= t;
    fill_LSBs();
    C <<= 7;
    t -= 7;
#if defined(CDP)
    // only for CDP implementation
    D = A - Areg_min;
    D = (C < D) ? C : D;
    A -= D;
    C -= D;
#endif
  }
}

void mq_decoder::init_states_for_all_contexts() {
  for (uint8_t i = 0; i < 19; i++) {
    dynamic_table[0][i] = 0;
    dynamic_table[1][i] = 0;
  }
  dynamic_table[0][0]  = 4;
  dynamic_table[0][17] = 3;
  dynamic_table[0][18] = 46;
}

void mq_decoder::renormalize_once() {
  if (t == 0) {
    fill_LSBs();
  }
  A <<= 1;
  C <<= 1;
  t--;
}

void mq_decoder::fill_LSBs() {
  t = 8;
  if (L == Lmax || (T == 0xFF && byte_buffer[L] > 0x8F)) {
    // Codeword exhausted; fill C with 1's from now on
    C += 0xFF;
  } else {
    if (T == 0xFF) {
      t = 7;
    }
    T = byte_buffer[L];
    L++;
    C += T << (8 - t);
  }
}

#if defined(MQNAIVE)
// Naive implementation
uint8_t mq_decoder::decode(uint8_t label) {
  uint8_t x;
  uint16_t tmp;
  constexpr uint8_t min_C_active   = 8;
  constexpr uint32_t C_active_mask = 0xFFFF00;
  uint16_t *sk                     = &dynamic_table[1][label];
  uint16_t *Sigma_k                = &dynamic_table[0][label];
  uint32_t val                     = static_table[*Sigma_k];
  const uint16_t Sigma_mps         = (val >> 24) & 0x3F;  // static_table[0][Sigma_k];
  const uint16_t Sigma_lps         = (val >> 16) & 0x3F;  // static_table[1][Sigma_k];
  const uint16_t Xs                = val >> 31;           // static_table[2][Sigma_k];
  uint16_t p                       = static_cast<uint16_t>(val & 0xFFFF);
  uint16_t s                       = dynamic_table[1][label];

  if (s > 1) {
    printf("ERROR: mq_dec error in function decode()\n");
    throw std::exception();
  }

  A = static_cast<uint16_t>(A - p);
  if (A < p) {
    // Conditional exchange of MPS and LPS
    s = static_cast<uint16_t>(1 - s);
  }

  // Compare active region of C
  if (((C & C_active_mask) >> 8) < p) {
    x = static_cast<uint8_t>(1 - s);
    A = p;
  } else {
    x   = static_cast<uint8_t>(s);
    tmp = static_cast<uint16_t>(((C & C_active_mask) >> min_C_active) - static_cast<uint32_t>(p));
    C &= ~C_active_mask;
    C += static_cast<uint32_t>((tmp << min_C_active)) & C_active_mask;
  }
  if (A < 0x8000) {
    if (x == *sk) {
      // The x was a real MPS
      *Sigma_k = Sigma_mps;
    } else {
      // The x was a real LPS
      *sk      = *sk ^ Xs;
      *Sigma_k = Sigma_lps;
    }
  }

  while (A < 0x8000) {
    renormalize_once();
  }
  return x;
}
#endif

#if !defined(CDP) && !defined(MQNAIVE)
/**
 * @brief MQ Decode Procedure (from JPEG 2000 book P.646 - 647)
 * @param[in] label context label
 * @return uint8_t output-symbol
 */
uint8_t mq_decoder::decode(uint8_t label) {
  int32_t x;
  uint16_t Sigma_k         = dynamic_table[0][label];
  uint32_t val             = static_table[Sigma_k];
  const uint16_t Sigma_mps = (val >> 24) & 0x3F;                // static_table[0][Sigma_k];
  const uint16_t Sigma_lps = (val >> 16) & 0x3F;                // static_table[1][Sigma_k];
  const uint16_t Xs        = static_cast<uint16_t>(val >> 31);  // static_table[2][Sigma_k];
  // = p_bar (from the static table) << 8
  const int32_t p_shifted = (int32_t)((val & 0xFFFF) << 8);
  uint16_t sk             = dynamic_table[1][label];

  x = sk;  // set to MPS for now, since this is most likely
  A = A - p_shifted;
  if (C >= p_shifted) {     // identical to C_active >= p_bar (upper sub-interval selected),
    C -= p_shifted;         // identical to C_active -= p_bar, C_active is equal to (C & 0xFFFF00)
    if (A < Areg_min) {     // need renormalization and perhaps conditional exchange
      if (A < p_shifted) {  // conditional exchange, LPS decoded
        x       = 1 - sk;
        sk      = sk ^ Xs;
        Sigma_k = Sigma_lps;
      } else {  // MPS decoded
        Sigma_k = Sigma_mps;
      }
      while (A < Areg_min) {
        renormalize_once();
      }
    }
  } else {                // lower sub-interval selected; renormalization is inevitable
    if (A < p_shifted) {  // conditional exchange, MPS decoded
      Sigma_k = Sigma_mps;
    } else {  // LPS decoded
      x       = 1 - sk;
      sk      = sk ^ Xs;
      Sigma_k = Sigma_lps;
    }
    A = p_shifted;
    while (A < Areg_min) {
      renormalize_once();
    }
  }
  dynamic_table[0][label] = Sigma_k;
  dynamic_table[1][label] = sk;
  return static_cast<uint8_t>(x);
}
#elif defined(CDP)
// Common Decoding Path (CDP) implementation, require a new state value D
uint8_t mq_decoder::decode(uint8_t label) {
  int32_t x;
  uint16_t Sigma_k         = dynamic_table[0][label];
  uint32_t val             = static_table[Sigma_k];
  const uint16_t Sigma_mps = (val >> 24) & 0x3F;  // static_table[0][Sigma_k];
  const uint16_t Sigma_lps = (val >> 16) & 0x3F;  // static_table[1][Sigma_k];
  const uint16_t Xs        = val >> 31;           // static_table[2][Sigma_k];
  // = p_bar (from the static table) << 8
  const int32_t p_shifted = (int32_t)((val & 0xFFFF) << 8);
  uint16_t sk             = dynamic_table[1][label];

  x = sk;  // set to MPS for now, since this is most likely
  D -= p_shifted;
  if (D < 0) {
    A += D;
    C += D;
    if (C >= 0) {
      if (A < p_shifted) {  // conditional exchange, LPS decoded
        x       = 1 - sk;
        sk      = sk ^ Xs;
        Sigma_k = Sigma_lps;
      } else {  // MPS decoded
        Sigma_k = Sigma_mps;
      }
      while (A < Areg_min) {
        renormalize_once();
      }
    } else {
      C += p_shifted;
      if (A < p_shifted) {  // conditional exchange, MPS decoded
        Sigma_k = Sigma_mps;
      } else {  // LPS decoded
        x       = 1 - sk;
        sk      = sk ^ Xs;
        Sigma_k = Sigma_lps;
      }
      A = p_shifted;
      while (A < Areg_min) {
        renormalize_once();
      }
    }
    D = A - Areg_min;
    D = (C < D) ? C : D;
    A -= D;
    C -= D;
  }
  dynamic_table[0][label] = Sigma_k;
  dynamic_table[1][label] = sk;
  return static_cast<uint8_t>(x);
}
#endif

uint8_t mq_decoder::get_raw_symbol() {
  if (t == 0) {
    t = 8;
    if (L == Lmax) {
      T = 0xFF;
    } else {
      if (T == 0xFF) {
        t = 7;
      }
      T = byte_buffer[L];
      L++;
    }
  }
  t--;
  return ((T >> t) & 1);
}

void mq_decoder::finish() {
  // TODO: ERTERM
}
