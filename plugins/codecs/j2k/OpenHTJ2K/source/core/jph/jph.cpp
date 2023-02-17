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

#include "jph.hpp"

signature_box::signature_box() : box_base(12, 0x6A502020), signature(0x0D0A870A) {}

size_t signature_box::write(j2c_dst_memory &dst) {
  base_write(dst);
  dst.put_dword(signature);
  return LBox;
}

file_type_box::file_type_box(uint8_t type) : box_base(16, 0x66747970), MinV(0) {
  if (type == 0) {
    // jp2
    BR = 0x6A703220;
    CLi.push_back(0x6A703220);
  } else if (type == 1) {
    // jph
    BR = 0x6A706820;
    CLi.push_back(0x6A706820);
  } else {
    printf("ERROR: unsupported type for file_type_box\n");
    throw std::exception();
  }
  for (size_t i = 0; i < CLi.size(); ++i) {
    LBox += 4;
  }
}

size_t file_type_box::write(j2c_dst_memory &dst) {
  base_write(dst);
  dst.put_dword(BR);
  dst.put_dword(MinV);
  for (auto &c : CLi) {
    dst.put_dword(c);
  }
  return LBox;
}

image_header_box::image_header_box(j2k_main_header &hdr) : box_base(22, 0x69686472), C(7), UnkC(0), IPR(0) {
  element_siz siz, Osiz;
  hdr.SIZ->get_image_size(siz);
  hdr.SIZ->get_image_origin(Osiz);
  HEIGHT      = siz.y - Osiz.y;
  WIDTH       = siz.x - Osiz.x;
  NC          = hdr.SIZ->get_num_components();
  uint8_t val = hdr.SIZ->get_bitdepth(0);
  for (uint16_t c = 1; c < NC; ++c) {
    if (val != hdr.SIZ->get_bitdepth(c)) {
      val = 0xFF;
      break;
    }
  }
  BPC = val;
}
bool image_header_box::needBPCC() {
  bool val = false;
  if (BPC == 0xFF) {
    val = true;
  }
  return val;
}
size_t image_header_box::write(j2c_dst_memory &dst) {
  base_write(dst);
  dst.put_dword(HEIGHT);
  dst.put_dword(WIDTH);
  dst.put_word(NC);
  dst.put_byte(BPC);
  dst.put_byte(C);
  dst.put_byte(UnkC);
  dst.put_byte(IPR);
  return LBox;
}

bits_per_component_box::bits_per_component_box(j2k_main_header &hdr) : box_base(8, 0x62706363) {
  for (uint16_t c = 0; c < hdr.SIZ->get_num_components(); ++c) {
    uint8_t val = static_cast<uint8_t>(hdr.SIZ->get_bitdepth(c) - 1);
    val         = val | static_cast<uint8_t>((hdr.SIZ->is_signed(c)) ? 0x80 : 0);
    BPC.push_back(val);
    LBox++;
  }
}
size_t bits_per_component_box::write(j2c_dst_memory &dst) {
  base_write(dst);
  for (auto &b : BPC) {
    dst.put_byte(b);
  }
  return LBox;
}

colour_specification_box::colour_specification_box(j2k_main_header &hdr, bool isSRGB)
    : box_base(15, 0x636F6C72), METH(1), PREC(0), APPROX(0) {
  if (hdr.SIZ->get_num_components() == 3) {
    if (isSRGB) {
      EnumCS = 16;
    } else {
      EnumCS = 18;
    }
  } else if (hdr.SIZ->get_num_components() == 1) {
    EnumCS = 17;
  } else {
    printf("ERROR: invalid color space specification.\n");
    throw std::exception();
  }
}
size_t colour_specification_box::write(j2c_dst_memory &dst) {
  base_write(dst);
  dst.put_byte(METH);
  dst.put_byte(PREC);
  dst.put_byte(APPROX);
  dst.put_dword(EnumCS);
  return LBox;
}

header_box::header_box(j2k_main_header &hdr, bool isSRGB)
    : box_base(8, 0x6A703268), ihdr(hdr), bpcc(hdr), colr(hdr, isSRGB) {
  LBox += ihdr.LBox + colr.LBox;
  if (ihdr.needBPCC()) {
    LBox += bpcc.LBox;
  }
}
size_t header_box::write(j2c_dst_memory &dst) {
  base_write(dst);
  ihdr.write(dst);
  if (ihdr.needBPCC()) {
    bpcc.write(dst);
  }
  colr.write(dst);
  return LBox;
}

contiguous_codestream_box::contiguous_codestream_box(size_t len)
    : box_base(static_cast<uint32_t>(len) + 8, 0x6A703263){};

size_t contiguous_codestream_box::write(j2c_dst_memory &dst) {
  // LBox = 0;
  base_write(dst);
  return LBox;
}

jph_boxes::jph_boxes(j2k_main_header &hdr, uint8_t type, bool isSRGB, size_t code_len)
    : sig(), ftyp(type), jp2h(hdr, isSRGB), jp2c(code_len) {}

size_t jph_boxes::write(j2c_dst_memory &dst) {
  size_t len = 0;
  len += sig.write(dst);
  len += ftyp.write(dst);
  len += jp2h.write(dst);
  len += jp2c.write(dst);
  return len;
}
