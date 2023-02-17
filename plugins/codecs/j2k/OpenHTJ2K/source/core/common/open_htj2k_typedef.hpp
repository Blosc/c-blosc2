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

// calc BIBO gain? if not, LUT is used
//#define BIBO

// number of fractional bits for fixed-point representation
constexpr int32_t FRACBITS = 13;

// for 12 bit or higher sample precision, sprec_t == int16_t will lead overflow at quantization
typedef int16_t sprec_t;
typedef uint16_t usprec_t;

#define SIMD_LEN_F32 8
#define SIMD_LEN_I32 8

#define BAND_LL 0
#define BAND_HL 1
#define BAND_LH 2
#define BAND_HH 3

#define BYPASS 0x001
#define RESET 0x002
#define RESTART 0x004
#define CAUSAL 0x008
// TODO: implementation of MQ decoding with ERTERM
#define ERTERM 0x010
#define SEGMARK 0x020
#define HT 0x040
#define HT_MIXED 0x080
#define HT_PHLD 0x100

class element_siz {
 public:
  uint32_t x;
  uint32_t y;
  element_siz() : x(0), y(0) {}
  element_siz(uint32_t x0, uint32_t y0) {
    x = x0;
    y = y0;
  }
};