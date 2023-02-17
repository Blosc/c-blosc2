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

#define SHIFT_SIGMA 0   // J2K and HTJ2K
#define SHIFT_SIGMA_ 1  // J2K only
#define SHIFT_PI_ 2     // J2K and HTJ2K; used as refinement indicator for HTJ2K
#define SHIFT_REF 3U    // HTJ2K only
#define SHIFT_SCAN 4    // HTJ2K only
#define SHIFT_P 3U      // J2K only
#define SHIFT_SMAG 5    // HTJ2K enc only; used for HT SigProp and MagRef
#define SHIFT_SSGN 6    // HTJ2K enc only; used for HT SigProp

//// getters
// inline uint8_t Sigma(uint8_t &data) { return static_cast<uint8_t>((data >> SHIFT_SIGMA) & 1); }
// inline uint8_t Sigma_(uint8_t &data) { return static_cast<uint8_t>((data >> SHIFT_SIGMA_) & 1); }
// inline uint8_t Pi_(uint8_t &data) { return static_cast<uint8_t>((data >> SHIFT_PI_) & 1); }
// inline uint8_t Scan(uint8_t &data) { return static_cast<uint8_t>((data >> SHIFT_SCAN) & 1); }
// inline uint8_t Refinement_value(uint8_t &data) { return static_cast<uint8_t>((data >> SHIFT_REF) & 1); }
// inline uint8_t Refinement_indicator(uint8_t &data) { return static_cast<uint8_t>((data >> SHIFT_PI_) &
// 1); } inline uint8_t Decoded_bitplane_index(uint8_t &data) { return static_cast<uint8_t>(data >>
// SHIFT_P); }
//
//// setters
// inline void sigma(uint8_t &data, const uint8_t &val) { data |= val; }
// inline void sigma_(uint8_t &data, const uint8_t &val) { data |= static_cast<uint8_t>(val <<
// SHIFT_SIGMA_); } inline void pi_(uint8_t &data, const uint8_t &val) {
//   if (val) {
//     data |= static_cast<uint8_t>(1 << SHIFT_PI_);
//   } else {
//     data &= static_cast<uint8_t>(~(1 << SHIFT_PI_));
//   }
// }
// inline void scan(uint8_t &data, const uint8_t &val) { data |= static_cast<uint8_t>(val << SHIFT_SCAN); }
// inline void refinement_value(uint8_t &data, const uint8_t &val) {
//   data |= static_cast<uint8_t>(val << SHIFT_REF);
// }
// inline void refinement_indicator(uint8_t &data, const uint8_t &val) {
//   if (val) {
//     data |= static_cast<uint8_t>(1 << SHIFT_PI_);
//   } else {
//     data &= static_cast<uint8_t>(~(1 << SHIFT_PI_));
//   }
// }
// inline void decoded_bitplane_index(uint8_t &data, const uint8_t &val) {
//   data &= 0x07;
//   data |= static_cast<uint8_t>(val << SHIFT_P);
// }
