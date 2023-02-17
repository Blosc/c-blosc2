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

#include "codestream.hpp"

// MARK: j2c_src_memory -
void j2c_src_memory::alloc_memory(uint32_t length) {
  buf = static_cast<uint8_t *>(malloc(sizeof(uint8_t) * length));  // MAKE_UNIQUE<uint8_t[]>(length);
  pos = 0;
  len = length;
}

uint8_t j2c_src_memory::get_byte() {
  if (pos > len - 1) {
    printf("Codestream is shorter than the expected length\n");
    throw std::exception();
  }
  uint8_t out = buf[pos];
  pos++;
  return out;
}

int j2c_src_memory::get_N_byte(uint8_t *out, uint32_t length) {
  memcpy(out, buf + pos, length);
  pos += length;
  //  for (unsigned long i = 0; i < length; i++) {
  //    out[i] = get_byte();
  //  }
  return EXIT_SUCCESS;
}

uint16_t j2c_src_memory::get_word() {
  if (pos > len - 2) {
    printf("Codestream is shorter than the expected length\n");
    throw std::exception();
  }
  auto out = static_cast<uint16_t>((get_byte() << 8) + get_byte());
  return out;
}

int j2c_src_memory::rewind_2bytes() {
  if (pos < 2) {
    printf("Cannot rewind 2 bytes because the current position is less than 2\n");
    throw std::exception();
  }
  pos -= 2;
  return EXIT_SUCCESS;
}

int j2c_src_memory::forward_Nbytes(uint32_t N) {
  if (pos + N <= len) {
    pos += N;
    return EXIT_SUCCESS;
  } else {
    throw std::exception();
  }
}
