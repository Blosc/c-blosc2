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

#include <cstdio>
#include <cstdint>
#include <vector>
#include "j2kmarkers.hpp"
#include "codestream.hpp"
#include "open_htj2k_typedef.hpp"

class box_base {
 public:
  uint32_t LBox;
  uint32_t TBox;
  uint64_t XLBox;

 public:
  box_base(uint32_t l, uint32_t t) : LBox(l), TBox(t), XLBox(0) {}
  virtual size_t write(j2c_dst_memory &dst) = 0;
  void base_write(j2c_dst_memory &dst) const {
    dst.put_dword(LBox);
    dst.put_dword(TBox);
  }
};

class signature_box : public box_base {
 private:
  uint32_t signature;

 public:
  signature_box();
  size_t write(j2c_dst_memory &dst) override;
};

class file_type_box : public box_base {
 private:
  uint32_t BR;
  const uint32_t MinV;
  std::vector<uint32_t> CLi;

 public:
  file_type_box(uint8_t type);
  size_t write(j2c_dst_memory &dst) override;
};

class image_header_box : public box_base {
 private:
  uint32_t HEIGHT;
  uint32_t WIDTH;
  uint16_t NC;
  uint8_t BPC;
  const uint8_t C;
  uint8_t UnkC;
  uint8_t IPR;

 public:
  image_header_box(j2k_main_header &hdr);
  bool needBPCC();
  size_t write(j2c_dst_memory &dst) override;
};

class bits_per_component_box : public box_base {
 private:
  std::vector<uint8_t> BPC;

 public:
  bits_per_component_box(j2k_main_header &hdr);
  size_t write(j2c_dst_memory &dst) override;
};

class colour_specification_box : public box_base {
 private:
  uint8_t METH;
  uint8_t PREC;
  uint8_t APPROX;
  uint32_t EnumCS;
  [[maybe_unused]] uint8_t PROFILE;
  [[maybe_unused]] uint16_t COLPRIMS;
  [[maybe_unused]] uint16_t TRANSFC;
  [[maybe_unused]] uint16_t MATCOEFFS;
  [[maybe_unused]] bool VIDFRNG;
  [[maybe_unused]] uint8_t VIDFRNG_RSVD;

 public:
  colour_specification_box(j2k_main_header &hdr, bool isSRGB);
  size_t write(j2c_dst_memory &dst) override;
};

class header_box : public box_base {
 private:
  image_header_box ihdr;
  bits_per_component_box bpcc;
  colour_specification_box colr;

 public:
  header_box(j2k_main_header &hdr, bool isSRGB);
  size_t write(j2c_dst_memory &dst) override;
};

class contiguous_codestream_box : public box_base {
 public:
  contiguous_codestream_box(size_t len);
  size_t write(j2c_dst_memory &dst) override;
};

class jph_boxes {
 private:
  signature_box sig;
  file_type_box ftyp;
  header_box jp2h;
  contiguous_codestream_box jp2c;

 public:
  jph_boxes(j2k_main_header &hdr, uint8_t type, bool isSRGB, size_t code_len);
  size_t write(j2c_dst_memory &dst);
};