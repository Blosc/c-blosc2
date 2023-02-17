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

#include <fstream>
#include "codestream.hpp"

// MARK: j2c_dst_memory -
int32_t j2c_dst_memory::put_byte(uint8_t byte) {
  buf.push_back(byte);
  pos++;
  return EXIT_SUCCESS;
}

int32_t j2c_dst_memory::put_word(uint16_t word) {
  auto upper_byte = static_cast<uint8_t>(word >> 8);
  auto lower_byte = static_cast<uint8_t>(word & 0xFF);
  put_byte(upper_byte);
  put_byte(lower_byte);
  return EXIT_SUCCESS;
}

int32_t j2c_dst_memory::put_dword(uint32_t dword) {
  auto upper_word = static_cast<uint16_t>(dword >> 16);
  auto lower_word = static_cast<uint16_t>(dword & 0xFFFF);
  put_word(upper_word);
  put_word(lower_word);
  return EXIT_SUCCESS;
}

int32_t j2c_dst_memory::put_N_bytes(uint8_t *src, uint32_t length) {
  buf.resize(pos + length);
  memcpy(buf.data() + pos, src, length);
  pos += length;
  // for (unsigned long i = 0; i < length; i++) {
  //   buf.push_back(src[i]);
  // }
  return EXIT_SUCCESS;
}

int32_t j2c_dst_memory::flush(std::ofstream &dst) {
  dst.write((char *)&buf[0], static_cast<long>(buf.size() * sizeof(buf[0])));
  return EXIT_SUCCESS;
}

int32_t j2c_dst_memory::flush(std::vector<uint8_t> *obuf) {
  if (is_flushed) {
    return EXIT_FAILURE;
  }
  obuf->resize(buf.size());
  memcpy(obuf->data(), buf.data(), buf.size());
  //  for (size_t i = 0; i < buf.size(); ++i) {
  //    *(obuf->data() + i) = buf[i];
  //  }
  is_flushed = true;
  return EXIT_SUCCESS;
}

size_t j2c_dst_memory::get_length() const { return buf.size(); }

[[maybe_unused]] [[deprecated]] void j2c_dst_memory::print_bytes() {
  for (uint32_t i = 0; i < pos; i++) {
    if (i % 32 == 0) {
      printf("\n");
    }
    printf("%02x ", buf[i]);
  }
  printf("\n");
}
