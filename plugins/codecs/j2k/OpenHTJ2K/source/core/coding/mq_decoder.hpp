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

//#define MQNAIVE
//#define CDP

class mq_decoder {
 public:
  int32_t A;  // was uint16_t
  int32_t t;  // was uint8_t
  // Lower-bound interval
  int32_t C;  // was uint32_t
#if defined(CDP)
  int32_t D;  // only for CDP implementation
#endif
  // Temporary byte register
  int32_t T;  // was uint8_t
  // position in byte-stream
  uint32_t L;
  // start position in byte-stream
  [[maybe_unused]] uint32_t L_start;
  // position of current codeword segment boundary
  uint32_t Lmax;
  // dynamic table for context
  uint16_t dynamic_table[2][19];
  // Byte-stream buffer
  uint8_t const *byte_buffer;
  explicit mq_decoder(const uint8_t *buf);
  void fill_LSBs();
  void init(uint32_t buf_pos, uint32_t segment_length, bool is_bypass);
  void init_states_for_all_contexts();
  void renormalize_once();
  uint8_t decode(uint8_t label);
  uint8_t get_raw_symbol();
  void finish();
};
