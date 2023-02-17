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

#include "j2kmarkers.hpp"

#include <cstdio>
#include <cmath>
#include <string>

constexpr uint8_t YCC444 = 0;
constexpr uint8_t YCC420 = 1;
constexpr uint8_t YCC422 = 2;

/********************************************************************************
 * j2k_marker_io_base
 *******************************************************************************/
void j2k_marker_io_base::set_buf(uint8_t *p) { buf = p; }

[[maybe_unused]] uint16_t j2k_marker_io_base::get_marker() const { return this->code; }

uint16_t j2k_marker_io_base::get_length() const { return this->Lmar; }

uint8_t *j2k_marker_io_base::get_buf() { return buf + pos; }

uint8_t j2k_marker_io_base::get_byte() {
  assert(pos < Lmar);
  uint8_t out = buf[pos];
  pos++;
  return out;
}

uint16_t j2k_marker_io_base::get_word() {
  assert(pos < Lmar - 1);
  auto out = static_cast<uint16_t>((get_byte() << 8) + get_byte());
  return out;
}

uint32_t j2k_marker_io_base::get_dword() {
  assert(pos < Lmar - 3);
  uint32_t out = (static_cast<uint32_t>(get_word()) << 16) + static_cast<uint32_t>(get_word());
  return out;
}

/********************************************************************************
 * SIZ_marker
 *******************************************************************************/

SIZ_marker::SIZ_marker(j2c_src_memory &in) : j2k_marker_io_base(_SIZ) {
  Lmar = in.get_word();
  this->set_buf(in.get_buf_pos());
  in.get_N_byte(this->get_buf(), Lmar - 2U);
  Rsiz   = get_word();
  Xsiz   = get_dword();
  Ysiz   = get_dword();
  XOsiz  = get_dword();
  YOsiz  = get_dword();
  XTsiz  = get_dword();
  YTsiz  = get_dword();
  XTOsiz = get_dword();
  YTOsiz = get_dword();
  Csiz   = get_word();
  for (unsigned long i = 0; i < Csiz; ++i) {
    Ssiz.push_back(get_byte());
    XRsiz.push_back(get_byte());
    YRsiz.push_back(get_byte());
  }
  is_set = true;
}

SIZ_marker::SIZ_marker(uint16_t R, uint32_t X, uint32_t Y, uint32_t XO, uint32_t YO, uint32_t XT,
                       uint32_t YT, uint32_t XTO, uint32_t YTO, uint16_t C, std::vector<uint8_t> &S,
                       std::vector<uint8_t> &XR, std::vector<uint8_t> &YR, bool needCAP)
    : j2k_marker_io_base(_SIZ),
      Rsiz(R | static_cast<uint16_t>(needCAP ? 1 << 14 : 0)),
      Xsiz(X),
      Ysiz(Y),
      XOsiz(XO),
      YOsiz(YO),
      XTsiz(XT),
      YTsiz(YT),
      XTOsiz(XTO),
      YTOsiz(YTO),
      Csiz(C) {
  Lmar = static_cast<uint16_t>(38 + 3 * C);
  for (unsigned long i = 0; i < Csiz; ++i) {
    Ssiz.push_back(S[i]);
    XRsiz.push_back(XR[i]);
    YRsiz.push_back(YR[i]);
  }
  is_set = true;
}

int SIZ_marker::write(j2c_dst_memory &dst) {
  if (!is_set) {
    printf("ERROR: illegal attempt to call write() for SIZ_marker not yet set.\n");
    throw std::exception();
  }
  dst.put_word(code);
  dst.put_word(Lmar);
  dst.put_word(Rsiz);
  dst.put_dword(Xsiz);
  dst.put_dword(Ysiz);
  dst.put_dword(XOsiz);
  dst.put_dword(YOsiz);
  dst.put_dword(XTsiz);
  dst.put_dword(YTsiz);
  dst.put_dword(XTOsiz);
  dst.put_dword(YTOsiz);
  dst.put_word(Csiz);
  for (unsigned long i = 0; i < Csiz; ++i) {
    dst.put_byte(Ssiz[i]);
    dst.put_byte(XRsiz[i]);
    dst.put_byte(YRsiz[i]);
  }
  return EXIT_SUCCESS;
}

bool SIZ_marker::is_signed(uint16_t c) {
  assert(c < Csiz);
  if (Ssiz[c] & 0x80) {
    return true;
  } else {
    return false;
  }
}

uint8_t SIZ_marker::get_bitdepth(uint16_t c) {
  assert(c < Csiz);
  return static_cast<uint8_t>((Ssiz[c] & 0x7F) + 1);
}

void SIZ_marker::get_image_size(element_siz &siz) const {
  siz.x = Xsiz;
  siz.y = Ysiz;
}

uint32_t SIZ_marker::get_component_stride(uint16_t c) const {
  if (c >= Csiz) {
    printf("ERROR: invalid component index\n");
    throw std::exception();
  }
  return Xsiz / XRsiz[c] - XOsiz;
}

void SIZ_marker::get_image_origin(element_siz &siz) const {
  siz.x = XOsiz;
  siz.y = YOsiz;
}

void SIZ_marker::get_tile_size(element_siz &siz) const {
  siz.x = XTsiz;
  siz.y = YTsiz;
}

void SIZ_marker::get_tile_origin(element_siz &siz) const {
  siz.x = XTOsiz;
  siz.y = YTOsiz;
}

void SIZ_marker::get_subsampling_factor(element_siz &siz, uint16_t c) {
  siz.x = XRsiz[c];
  siz.y = YRsiz[c];
}

uint16_t SIZ_marker::get_num_components() const { return Csiz; }

uint8_t SIZ_marker::get_chroma_format() const {
  uint8_t chroma_format = YCC444;
  // determine type of chroma subsampling
  if (Csiz != 3) {
    return chroma_format;
  } else {
    if (XRsiz[1] == 2 && XRsiz[2] == 2) {
      if (YRsiz[1] == 2 && YRsiz[2] == 2) {
        chroma_format = YCC420;
      }
      if (YRsiz[1] == 1 && YRsiz[2] == 1) {
        chroma_format = YCC422;
      }
    }
  }
  return chroma_format;
}

/********************************************************************************
 * CAP_marker
 *******************************************************************************/
CAP_marker::CAP_marker() : j2k_marker_io_base(_CAP), Pcap(0), Ccap{0} { Lmar = 6; }

CAP_marker::CAP_marker(j2c_src_memory &in) : j2k_marker_io_base(_CAP), Ccap{0} {
  Lmar = in.get_word();
  this->set_buf(in.get_buf_pos());
  in.get_N_byte(this->get_buf(), Lmar - 2U);
  unsigned long n = (Lmar - 6U) / 2U;
  Pcap            = get_dword();

  for (int i = 0; i < 32; ++i) {
    if (Pcap & static_cast<uint32_t>(1 << (31 - i))) {
      Ccap[i] = get_word();
      n--;
    }
    //    } else {
    //      Ccap[i] = 0;
    //    }
    //    if (Ccap[i]) {
    //      n--;
    //    }
  }
  if (n != 0) {
    printf("ERROR: Lcap and number of Ccap does not match\n");
    throw std::exception();
  }
  is_set = true;
}

[[maybe_unused]] uint32_t CAP_marker::get_Pcap() const { return Pcap; }

uint16_t CAP_marker::get_Ccap(uint8_t n) {
  assert(n < 32);
  return Ccap[n - 1];
}

void CAP_marker::set_Ccap(uint16_t val, uint8_t part) {
  assert(part > 0 && part < 33);
  Ccap[part - 1] = val;
  set_Pcap(part);
}
void CAP_marker::set_Pcap(uint8_t part) {
  // implemented only for Part 15
  Pcap |= static_cast<uint32_t>(1 << (32 - part));
  Lmar++;
  Lmar++;
  is_set = true;
}

int CAP_marker::write(j2c_dst_memory &dst) {
  assert(is_set == true);
  dst.put_word(code);
  dst.put_word(Lmar);
  dst.put_dword(Pcap);
  for (uint8_t n = 0; n < 32; ++n) {
    if (Pcap & static_cast<uint32_t>(1 << (32 - n - 1))) {
      dst.put_word(Ccap[n]);
    }
  }
  return EXIT_SUCCESS;
}

/********************************************************************************
 * CPF_marker
 *******************************************************************************/
CPF_marker::CPF_marker() : j2k_marker_io_base(_CPF) { Pcpf = {0}; }

CPF_marker::CPF_marker(j2c_src_memory &in) : j2k_marker_io_base(_CPF) {
  Lmar = in.get_word();
  this->set_buf(in.get_buf_pos());
  in.get_N_byte(this->get_buf(), Lmar - 2U);
  uint16_t len = 2;  // Lcpf
  size_t n     = static_cast<size_t>(Lmar - len) / 2U;
  for (size_t i = 0; i < n; ++i) {
    if (i < Pcpf.size()) {
      Pcpf[i] = get_word();
    } else {
      Pcpf.push_back(get_word());
    }
  }
  is_set = true;
}

/********************************************************************************
 * COD_marker
 *******************************************************************************/
COD_marker::COD_marker(j2c_src_memory &in)
    : j2k_marker_io_base(_COD), Scod(0), SGcod(0), SPcod({0, 0, 0, 0, 0}) {
  Lmar = in.get_word();
  this->set_buf(in.get_buf_pos());
  in.get_N_byte(this->get_buf(), Lmar - 2U);
  uint16_t len = 2;  // tmp length including Lcod
  Scod         = get_byte();
  len++;
  SGcod = get_dword();
  len   = static_cast<uint16_t>(len + 4);
  for (size_t i = 0; i < static_cast<size_t>(Lmar - len); ++i) {
    if (i < SPcod.size()) {
      SPcod[i] = get_byte();
    } else {
      SPcod.push_back(get_byte());
    }
  }
  is_set = true;
}

COD_marker::COD_marker(bool is_max_precincts, bool use_SOP, bool use_EPH, uint8_t progression_order,
                       uint16_t number_of_layers, uint8_t use_color_trafo, uint8_t dwt_levels,
                       uint8_t cblksizx_log2, uint8_t cblksizy_log2, uint8_t codeblock_style,
                       uint8_t reversible_flag, std::vector<uint8_t> PPx, std::vector<uint8_t> PPy)
    : j2k_marker_io_base(_COD), Scod(0), SGcod(0), SPcod({0, 0, 0, 0, 0}) {
  Lmar = static_cast<uint16_t>((is_max_precincts) ? 12 : 13 + dwt_levels);
  if (!is_max_precincts) {
    Scod |= 0x01;
  }
  if (use_SOP) {
    Scod |= 0x02;
  }
  if (use_EPH) {
    Scod |= 0x04;
  }
  // Scod += (is_max_precincts) ? 0 : 1;
  // Scod += (use_SOP) ? 2 : 0;
  // Scod += (use_EPH) ? 4 : 0;
  SGcod += static_cast<uint32_t>(progression_order) << 24;
  SGcod += static_cast<uint32_t>(number_of_layers) << 8;
  SGcod += use_color_trafo;

  SPcod[0] = dwt_levels;
  SPcod[1] = cblksizx_log2;
  SPcod[2] = cblksizy_log2;
  SPcod[3] = codeblock_style;
  SPcod[4] = reversible_flag;

  if (PPx.size() != PPy.size()) {
    printf(
        "ERROR: Length of parameters to specify horizontal and vertical precinct size shall be the "
        "same.\n");
    throw std::exception();
  }
  size_t PPlength  = PPx.size();
  uint8_t last_PPx = '\0', last_PPy = '\0';
  if (!is_max_precincts) {
    std::vector<uint8_t> tmpPP;
    for (size_t i = 0; i <= dwt_levels; ++i) {
      if (i < PPlength) {
        last_PPx = PPx[i];
        last_PPy = PPy[i];
      }
      tmpPP.push_back(static_cast<unsigned char>(last_PPx + (last_PPy << 4)));
    }
    for (size_t i = 0; i <= dwt_levels; ++i) {
      SPcod.push_back(tmpPP[dwt_levels - i]);
    }
  }
  is_set = true;
}

int COD_marker::write(j2c_dst_memory &dst) {
  assert(is_set == true);
  dst.put_word(code);
  dst.put_word(Lmar);
  dst.put_byte(Scod);
  dst.put_dword(SGcod);
  for (unsigned char &i : SPcod) {
    dst.put_byte(i);
  }
  return EXIT_SUCCESS;
}

bool COD_marker::is_maximum_precincts() const { return (Scod & 1) == 0; }

bool COD_marker::is_use_SOP() const { return (Scod & 2) != 0; }

bool COD_marker::is_use_EPH() const { return (Scod & 4) != 0; }

uint8_t COD_marker::get_progression_order() const { return static_cast<uint8_t>(SGcod >> 24); }

uint16_t COD_marker::get_number_of_layers() const { return static_cast<uint16_t>((SGcod >> 8) & 0xFFFF); }

uint8_t COD_marker::use_color_trafo() const { return static_cast<uint8_t>(SGcod & 0xFF); }

uint8_t COD_marker::get_dwt_levels() { return SPcod[0]; }

void COD_marker::get_codeblock_size(element_siz &out) {
  out.x = static_cast<uint32_t>(1 << (SPcod[1] + 2));
  out.y = static_cast<uint32_t>(1 << (SPcod[2] + 2));
}

void COD_marker::get_precinct_size(element_siz &out, uint8_t resolution) {
  if (is_maximum_precincts()) {
    out.x = 15;
    out.y = 15;
  } else {
    out.x = (SPcod[5U + resolution] & 0x0F);
    out.y = (SPcod[5U + resolution] & 0xF0) >> 4;
  }
}

uint8_t COD_marker::get_Cmodes() { return SPcod[3]; }

uint8_t COD_marker::get_transformation() { return SPcod[4]; }

/********************************************************************************
 * COC_marker
 *******************************************************************************/
COC_marker::COC_marker() : j2k_marker_io_base(_COC) {
  Ccoc  = 0;
  Scoc  = 0;
  SPcoc = {0, 0, 0, 0, 0};
}

COC_marker::COC_marker(j2c_src_memory &in, uint16_t Csiz) : j2k_marker_io_base(_COC) {
  Lmar = in.get_word();
  this->set_buf(in.get_buf_pos());
  in.get_N_byte(this->get_buf(), Lmar - 2U);
  uint16_t len = 2;  // tmp length including Lcoc
  if (Csiz < 257) {
    Ccoc = get_byte();
    len++;
  } else {
    Ccoc = get_word();
    len++;
    len++;
  }
  Scoc = get_byte();
  len++;

  for (size_t i = 0; i < static_cast<size_t>(Lmar - len); ++i) {
    if (i < SPcoc.size()) {
      SPcoc[i] = get_byte();
    } else {
      SPcoc.push_back(get_byte());
    }
  }
  is_set = true;
}

uint16_t COC_marker::get_component_index() const { return Ccoc; }

bool COC_marker::is_maximum_precincts() const { return (Scoc & 1) == 0; }

uint8_t COC_marker::get_dwt_levels() { return SPcoc[0]; }

void COC_marker::get_codeblock_size(element_siz &out) {
  out.x = static_cast<uint32_t>(1 << (SPcoc[1] + 2));
  out.y = static_cast<uint32_t>(1 << (SPcoc[2] + 2));
}

void COC_marker::get_precinct_size(element_siz &out, uint8_t resolution) {
  if (is_maximum_precincts()) {
    out.x = 15;
    out.y = 15;
  } else {
    out.x = (SPcoc[5U + resolution] & 0x0F);
    out.y = (SPcoc[5U + resolution] & 0xF0) >> 4;
  }
}

uint8_t COC_marker::get_Cmodes() { return SPcoc[3]; }

uint8_t COC_marker::get_transformation() { return SPcoc[4]; }

/********************************************************************************
 * RGN_marker
 *******************************************************************************/
RGN_marker::RGN_marker() : j2k_marker_io_base(_RGN) {
  Crgn  = 0;
  Srgn  = 0;
  SPrgn = 0;
}

RGN_marker::RGN_marker(j2c_src_memory &in, uint16_t Csiz) : j2k_marker_io_base(_RGN) {
  Lmar = in.get_word();
  this->set_buf(in.get_buf_pos());
  in.get_N_byte(this->get_buf(), Lmar - 2U);
  uint16_t len = 2;  // tmp length including Lrgn
  if (Csiz < 257) {
    Crgn = get_byte();
    len++;
  } else {
    Crgn = get_word();
    len++;
    len++;
  }
  Srgn = get_byte();
  assert(Srgn == 0);
  SPrgn  = get_byte();
  is_set = true;
}

uint16_t RGN_marker::get_component_index() const { return Crgn; }

uint8_t RGN_marker::get_ROIshift() const { return SPrgn; }

/********************************************************************************
 * QCD_marker
 *******************************************************************************/
QCD_marker::QCD_marker(j2c_src_memory &in) : j2k_marker_io_base(_QCD), Sqcd(0) {
  Lmar = in.get_word();
  this->set_buf(in.get_buf_pos());
  in.get_N_byte(this->get_buf(), Lmar - 2U);
  uint16_t len = 2;  // tmp length including Lqcd
  Sqcd         = get_byte();
  len++;
  if ((Sqcd & 0x1F) == 0) {
    // reversible transform
    for (size_t i = 0; i < static_cast<size_t>(Lmar - len); ++i) {
      if (i < SPqcd.size()) {
        SPqcd[i] = get_byte();
      } else {
        SPqcd.push_back(get_byte());
      }
    }
  } else {
    // irreversible transformation
    assert((Lmar - len) % 2 == 0);
    for (size_t i = 0; i < static_cast<size_t>(Lmar - len) / 2U; ++i) {
      if (i < SPqcd.size()) {
        SPqcd[i] = get_word();
      } else {
        SPqcd.push_back(get_word());
      }
    }
  }
  is_set = true;
}

QCD_marker::QCD_marker(uint8_t number_of_guardbits, uint8_t dwt_levels, uint8_t transformation,
                       bool is_derived, uint8_t RI, uint8_t use_ycc, double basestep, uint8_t qfactor)
    : j2k_marker_io_base(_QCD), Sqcd(0), is_reversible(transformation == 1) {
  const size_t num_bands = static_cast<size_t>(3 * dwt_levels + 1);
  std::vector<double> wmse_or_BIBO;
  wmse_or_BIBO.reserve(num_bands);
  std::vector<uint8_t> epsilon(num_bands, 0);
  std::vector<uint16_t> mu(num_bands, 0);

  if (is_derived && qfactor != 0xFF) {
    is_derived = false;
    // TODO: show warning??
  }
  if (is_reversible) {
    Lmar = static_cast<uint16_t>(4 + 3 * dwt_levels);
  } else if (is_derived) {
    Lmar = 5;
    Sqcd = 0x01;
  } else {
    Lmar = static_cast<uint16_t>(5 + 6 * dwt_levels);
    Sqcd = 0x02;
  }

  assert(number_of_guardbits < 8 && number_of_guardbits >= 0);
  Sqcd = static_cast<uint8_t>(Sqcd + (number_of_guardbits << 5U));

  const std::vector<double> CDF53L = {-0.125, 0.25, 0.75, 0.25, -0.125};
  const std::vector<double> CDF53H = {-0.5, 1, -0.5};  // gain is doubled(x2)
  const std::vector<double> D97SL  = {-0.091271763114250, -0.057543526228500, 0.591271763114250,
                                      1.115087052457000,  0.5912717631142500, -0.05754352622850,
                                      -0.091271763114250};
  const std::vector<double> D97SH  = {0.053497514821622,  0.033728236885750,
                                      -0.156446533057980, -0.533728236885750,
                                      1.205898036472720,  -0.533728236885750,
                                      -0.156446533057980, 0.033728236885750,
                                      0.053497514821622};  // gain is doubled(x2)

  // Square roots of the visual weighting factors for Y content
  const std::vector<double> W_b_Y = {0.0901, 0.2758, 0.2758, 0.7018, 0.8378, 0.8378, 1.0000, 1.0000,
                                     1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000};
  // The squared Euclidean norm of the multi-component synthesis operator that represents the contribution
  // of component 𝑐 (e.g., Y, Cb or Cr) to reconstructed image samples (usually R, G and B)
  const double G_c_sqrt[3] = {1.7321, 1.8051, 1.5734};

  double gain_low = 0.0, gain_high = 0.0;

  std::vector<double> L, H;
  L = (is_reversible) ? CDF53L : D97SL;
  H = (is_reversible) ? CDF53H : D97SH;
  std::vector<double> outL(L);
  std::vector<double> outH(H);

  // derive BIBO gain for lossless, or
  // derive weighted mse for lossy
  if (dwt_levels == 0) {
    wmse_or_BIBO.push_back(1.0);
  } else {
    for (uint8_t level = 0; level < dwt_levels; ++level) {
      gain_low  = 0.0;
      gain_high = 0.0;
      for (const auto &e : outL) {
        gain_low += (is_reversible) ? fabs(e) : e * e;
      }
      for (const auto &e : outH) {
        gain_high += (is_reversible) ? fabs(e) : e * e;
      }

      wmse_or_BIBO.push_back(gain_high * gain_high);  // HH
      wmse_or_BIBO.push_back(gain_low * gain_high);   // LH
      wmse_or_BIBO.push_back(gain_high * gain_low);   // HL

      std::vector<double> L2, H2;
      // upsampling
      for (auto &i : outL) {
        L2.push_back(i);
        L2.push_back(0.0);
      }
      for (auto &i : outH) {
        H2.push_back(i);
        H2.push_back(0.0);
      }
      std::vector<double> tmpL(L.size() + L2.size() - 1, 0.0);
      for (size_t i = 0; i < L.size(); ++i) {
        for (size_t j = 0; j < L2.size(); ++j) {
          tmpL[i + j] += L[i] * L2[j];
        }
      }
      std::vector<double> tmpH(L.size() + H2.size() - 1, 0.0);
      for (size_t i = 0; i < L.size(); ++i) {
        for (size_t j = 0; j < H2.size(); ++j) {
          tmpH[i + j] += L[i] * H2[j];
        }
      }
      outL = tmpL;
      outH = tmpH;
    }
    wmse_or_BIBO.push_back(gain_low * gain_low);
  }

  // construct epsilon and mu
  if (is_reversible) {
    // lossless
    for (size_t i = 0; i < epsilon.size(); ++i) {
      epsilon[epsilon.size() - i - 1] = static_cast<uint8_t>(RI - number_of_guardbits + use_ycc);
      while (wmse_or_BIBO[i] > 0.9) {
        epsilon[epsilon.size() - i - 1]++;
        wmse_or_BIBO[i] *= 0.5;
      }
    }
  } else {
    // lossy
    if (qfactor == 0xFF) {
      for (size_t i = 0; i < epsilon.size(); ++i) {
        int32_t exponent, mantissa;
        double fval = basestep / sqrt(wmse_or_BIBO[i]);
        for (exponent = 0; fval < 1.0; exponent++) {
          fval *= 2.0;
        }
        mantissa = static_cast<int32_t>(floor((fval - 1.0) * static_cast<double>(1 << 11) + 0.5));
        if (mantissa >= (1 << 11)) {
          mantissa = 0;
          exponent--;
        }
        if (exponent > 31) {
          exponent = 31;
          mantissa = 0;
        }
        if (exponent < 0) {
          exponent = 0;
          mantissa = (1 << 11) - 1;
        }
        epsilon[epsilon.size() - i - 1] = static_cast<uint8_t>(exponent);
        mu[epsilon.size() - i - 1]      = static_cast<uint16_t>(mantissa);
      }
    } else {
      // lossy with qfactor: The detail of Qfactor feature is described in HTJ2K white paper at
      // https://htj2k.com/wp-content/uploads/white-paper.pdf
      double M_Q;
      uint8_t t0 = 65, t1 = 97;
      const double alpha_T0 = 0.04;
      const double alpha_T1 = 0.10;
      const double M_T0     = 2.0 * (1.0 - t0 / 100.0);
      const double M_T1     = 2.0 * (1.0 - t1 / 100.0);
      double alpha_Q        = alpha_T0;
      double qfactor_power  = 1.0;

      if (qfactor < 50) {
        M_Q = 50.0 / qfactor;
      } else {
        M_Q = 2.0 * (1.0 - qfactor / 100.0);
      }
      // adjust the scaling
      if (qfactor >= t1) {
        qfactor_power = 0.0;
        alpha_Q       = alpha_T1;
      } else if (qfactor > t0) {
        qfactor_power = (log(M_T1) - log(M_Q)) / (log(M_T1) - log(M_T0));
        alpha_Q       = alpha_T1 * pow(alpha_T0 / alpha_T1, qfactor_power);
      }

      const double eps0 = sqrt(0.5) / static_cast<double>(1 << RI);
      double delta_Q    = alpha_Q * M_Q;
      double delta_ref  = delta_Q * G_c_sqrt[0] + eps0;
      double G_c        = G_c_sqrt[0];  // gain of color transform
      for (size_t i = 0; i < epsilon.size(); ++i) {
        int32_t exponent, mantissa;
        double w_b;
        // w_b for LL band shall be 1.0
        w_b = (i >= W_b_Y.size()) ? 1.0 : pow(W_b_Y[i], qfactor_power);

        double fval = (qfactor != 0xFF) ? delta_ref / (sqrt(wmse_or_BIBO[i]) * w_b * G_c)
                                        : basestep / sqrt(wmse_or_BIBO[i]);
        for (exponent = 0; fval < 1.0; exponent++) {
          fval *= 2.0;
        }
        mantissa = static_cast<int32_t>(floor((fval - 1.0) * static_cast<double>(1 << 11) + 0.5));
        if (mantissa >= (1 << 11)) {
          mantissa = 0;
          exponent--;
        }
        if (exponent > 31) {
          exponent = 31;
          mantissa = 0;
        }
        if (exponent < 0) {
          exponent = 0;
          mantissa = (1 << 11) - 1;
        }
        epsilon[epsilon.size() - i - 1] = static_cast<uint8_t>(exponent);
        mu[epsilon.size() - i - 1]      = static_cast<uint16_t>(mantissa);
      }
    }
  }

  // set SPqcd from epsilon and mu
  if (is_derived) {
    if (is_reversible) {
      printf("ERROR: Derived quantization stepsize is not valid for reversible transform.\n");
      throw std::exception();
    }
    // Quantization style -> Scalar derived (values signalled for LL subband only)
    SPqcd.push_back(static_cast<uint16_t>((epsilon[0] << 11) + mu[0]));
  } else {
    for (size_t i = 0; i < num_bands; ++i) {
      if (is_reversible) {
        SPqcd.push_back(static_cast<uint16_t>(epsilon[i] << 3));
      } else {
        // Quantization style -> Scalar expounded (values signalled for each sub-band)
        SPqcd.push_back(static_cast<uint16_t>((epsilon[i] << 11) + mu[i]));
      }
    }
  }

  is_set = true;
}

int QCD_marker::write(j2c_dst_memory &dst) {
  assert(is_set == true);
  dst.put_word(code);
  dst.put_word(Lmar);
  dst.put_byte(Sqcd);

  if (is_reversible) {
    for (unsigned short &i : SPqcd) {
      dst.put_byte(static_cast<uint8_t>(i));
    }
  } else {
    for (unsigned short &i : SPqcd) {
      dst.put_word(i);
    }
  }
  return EXIT_SUCCESS;
}

uint8_t QCD_marker::get_quantization_style() const { return (Sqcd & 0x1F); }

uint8_t QCD_marker::get_exponents(uint8_t nb) {
  uint8_t qstyle = get_quantization_style();
  if (qstyle == 0) {
    // lossless
    return static_cast<uint8_t>(SPqcd[nb] >> 3);
  } else if (qstyle == 1) {
    // lossy derived
    return static_cast<uint8_t>(SPqcd[0] >> 11);
  } else {
    // lossy expounded
    assert(qstyle == 2);
    return static_cast<uint8_t>(SPqcd[nb] >> 11);
  }
}

uint16_t QCD_marker::get_mantissas(uint8_t nb) {
  uint8_t qstyle = get_quantization_style();
  if (qstyle == 1) {
    // lossy derived
    return (SPqcd[0] & 0x7FF);
  } else {
    // lossy expounded
    assert(qstyle == 2);
    return (SPqcd[nb] & 0x7FF);
  }
}

uint8_t QCD_marker::get_number_of_guardbits() const { return static_cast<uint8_t>(Sqcd >> 5); }

uint8_t QCD_marker::get_MAGB() {
  uint8_t qstyle = get_quantization_style();
  uint8_t tmp    = (qstyle > 0) ? 0xFF : 0;
  for (uint16_t &val : SPqcd) {
    if (qstyle == 0) {
      tmp = (tmp < (val >> 3)) ? static_cast<uint8_t>(val >> 3) : tmp;
    } else {
      tmp = (tmp > (val >> 11)) ? static_cast<uint8_t>(val >> 11) : tmp;
    }
  }
  return tmp;
}

/********************************************************************************
 * QCC_marker
 *******************************************************************************/
QCC_marker::QCC_marker(uint16_t Csiz, uint16_t c, uint8_t number_of_guardbits, uint8_t dwt_levels,
                       uint8_t transformation, bool is_derived, uint8_t RI, uint8_t use_ycc,
                       uint8_t qfactor, uint8_t chroma_format)
    : j2k_marker_io_base(_QCC), max_components(Csiz), Cqcc(c), Sqcc(0), is_reversible(transformation == 1) {
  size_t num_bands = static_cast<size_t>(3 * dwt_levels + 1);
  std::vector<double> wmse_or_BIBO;
  wmse_or_BIBO.reserve(num_bands);
  std::vector<uint8_t> epsilon(num_bands, 0);
  std::vector<uint16_t> mu(num_bands, 0);

  if (is_derived && qfactor != 0xFF) {
    is_derived = false;
    // TODO: show warning??
  }
  if (is_reversible) {
    Lmar = static_cast<uint16_t>(5 + 3 * dwt_levels);
  } else if (is_derived) {
    Lmar = 6;
    Sqcc = 0x01;
  } else {
    Lmar = static_cast<uint16_t>(6 + 6 * dwt_levels);
    Sqcc = 0x02;
  }
  if (max_components >= 257) {
    Lmar++;
  }

  assert(number_of_guardbits < 8 && number_of_guardbits >= 0);
  Sqcc = static_cast<uint8_t>(Sqcc + (number_of_guardbits << 5));

  const std::vector<double> CDF53L = {-0.125, 0.25, 0.75, 0.25, -0.125};
  const std::vector<double> CDF53H = {-0.5, 1, -0.5};  // gain is doubled(x2)
  const std::vector<double> D97SL  = {-0.091271763114250, -0.057543526228500, 0.591271763114250,
                                      1.115087052457000,  0.5912717631142500, -0.05754352622850,
                                      -0.091271763114250};
  const std::vector<double> D97SH  = {0.053497514821622,  0.033728236885750,
                                      -0.156446533057980, -0.533728236885750,
                                      1.205898036472720,  -0.533728236885750,
                                      -0.156446533057980, 0.033728236885750,
                                      0.053497514821622};  // gain is doubled(x2)

  // Square roots of the visual weighting factors for 4:4:4 YCbCr content
  const std::vector<std::vector<double>> W_b_444 = {
      {0.0901, 0.2758, 0.2758, 0.7018, 0.8378, 0.8378, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
       1.0000, 1.0000, 1.0000},
      {0.0263, 0.0863, 0.0863, 0.1362, 0.2564, 0.2564, 0.3346, 0.4691, 0.4691, 0.5444, 0.6523, 0.6523,
       0.7078, 0.7797, 0.7797},
      {0.0773, 0.1835, 0.1835, 0.2598, 0.4130, 0.4130, 0.5040, 0.6464, 0.6464, 0.7220, 0.8254, 0.8254,
       0.8769, 0.9424, 0.9424}};
  // Square roots of the visual weighting factors for 4:2:0 YCbCr content
  const std::vector<std::vector<double>> W_b_420 = {
      {0.0901, 0.2758, 0.2758, 0.7018, 0.8378, 0.8378, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
       1.0000, 1.0000, 1.0000},
      {0.1362, 0.2564, 0.2564, 0.3346, 0.4691, 0.4691, 0.5444, 0.6523, 0.6523, 0.7078, 0.7797, 0.7797,
       1.0000, 1.0000, 1.0000},
      {0.2598, 0.4130, 0.4130, 0.5040, 0.6464, 0.6464, 0.7220, 0.8254, 0.8254, 0.8769, 0.9424, 0.9424,
       1.0000, 1.0000, 1.0000}};
  // Square roots of the visual weighting factors for 4:2:2 YCbCr content
  const std::vector<std::vector<double>> W_b_422 = {
      {0.0901, 0.2758, 0.2758, 0.7018, 0.8378, 0.8378, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000,
       1.0000, 1.0000, 1.0000},
      {0.0863, 0.0863, 0.2564, 0.2564, 0.2564, 0.4691, 0.4691, 0.4691, 0.6523, 0.6523, 0.6523, 0.7797,
       0.7797, 0.7797, 1.0000},
      {0.1835, 0.1835, 0.4130, 0.4130, 0.4130, 0.6464, 0.6464, 0.6464, 0.8254, 0.8254, 0.8254, 0.9424,
       0.9424, 0.9424, 1.0000}};
  std::vector<std::vector<double>> W_b_sqrt;

  switch (chroma_format) {
    case YCC444:
      W_b_sqrt.emplace_back(W_b_444[0]);
      W_b_sqrt.emplace_back(W_b_444[1]);
      W_b_sqrt.emplace_back(W_b_444[2]);
      break;
    case YCC420:
      W_b_sqrt.emplace_back(W_b_420[0]);
      W_b_sqrt.emplace_back(W_b_420[1]);
      W_b_sqrt.emplace_back(W_b_420[2]);
      break;
    case YCC422:
      W_b_sqrt.emplace_back(W_b_422[0]);
      W_b_sqrt.emplace_back(W_b_422[1]);
      W_b_sqrt.emplace_back(W_b_422[2]);
      break;
    default:
      printf("ERROR: chroma format for QCC_marker is invalid.\n");
      throw std::exception();
  }

  // The squared Euclidean norm of the multi-component synthesis operator that represents the
  // contribution of component 𝑐 (e.g., Y, Cb or Cr) to reconstructed image samples (usually R, G and B)
  const double G_c_sqrt[3] = {1.7321, 1.8051, 1.5734};

  double gain_low = 0.0, gain_high = 0.0;

  std::vector<double> L, H;
  L = (is_reversible) ? CDF53L : D97SL;
  H = (is_reversible) ? CDF53H : D97SH;
  std::vector<double> outL(L);
  std::vector<double> outH(H);

  // derive BIBO gain for lossless, or
  // derive weighted mse for lossy
  if (dwt_levels == 0) {
    wmse_or_BIBO.push_back(1.0);
  } else {
    for (uint8_t level = 0; level < dwt_levels; ++level) {
      gain_low  = 0.0;
      gain_high = 0.0;
      for (const auto &e : outL) {
        gain_low += (is_reversible) ? fabs(e) : e * e;
      }
      for (const auto &e : outH) {
        gain_high += (is_reversible) ? fabs(e) : e * e;
      }

      wmse_or_BIBO.push_back(gain_high * gain_high);  // HH
      wmse_or_BIBO.push_back(gain_low * gain_high);   // LH
      wmse_or_BIBO.push_back(gain_high * gain_low);   // HL

      std::vector<double> L2, H2;
      // upsampling
      for (auto &i : outL) {
        L2.push_back(i);
        L2.push_back(0.0);
      }
      for (auto &i : outH) {
        H2.push_back(i);
        H2.push_back(0.0);
      }
      std::vector<double> tmpL(L.size() + L2.size() - 1, 0.0);
      for (size_t i = 0; i < L.size(); ++i) {
        for (size_t j = 0; j < L2.size(); ++j) {
          tmpL[i + j] += L[i] * L2[j];
        }
      }
      std::vector<double> tmpH(L.size() + H2.size() - 1, 0.0);
      for (size_t i = 0; i < L.size(); ++i) {
        for (size_t j = 0; j < H2.size(); ++j) {
          tmpH[i + j] += L[i] * H2[j];
        }
      }
      outL = tmpL;
      outH = tmpH;
    }
    wmse_or_BIBO.push_back(gain_low * gain_low);
  }

  // construct epsilon and mu
  if (is_reversible) {
    // lossless
    for (size_t i = 0; i < epsilon.size(); ++i) {
      epsilon[epsilon.size() - i - 1] = static_cast<uint8_t>(RI - number_of_guardbits + use_ycc);
      while (wmse_or_BIBO[i] > 0.9) {
        epsilon[epsilon.size() - i - 1]++;
        wmse_or_BIBO[i] *= 0.5;
      }
    }
  } else {
    // lossy with qfactor: The detail of Qfactor feature is described in HTJ2K white paper at
    // https://htj2k.com/wp-content/uploads/white-paper.pdf
    double M_Q;
    uint8_t t0 = 65, t1 = 97;
    const double alpha_T0 = 0.04;
    const double alpha_T1 = 0.10;
    const double M_T0     = 2.0 * (1.0 - t0 / 100.0);
    const double M_T1     = 2.0 * (1.0 - t1 / 100.0);
    double alpha_Q        = alpha_T0;
    double qfactor_power  = 1.0;

    if (qfactor < 50) {
      M_Q = 50.0 / qfactor;
    } else {
      M_Q = 2.0 * (1.0 - qfactor / 100.0);
    }
    // adjust the scaling
    if (qfactor >= t1) {
      qfactor_power = 0.0;
      alpha_Q       = alpha_T1;
    } else if (qfactor > t0) {
      qfactor_power = (log(M_T1) - log(M_Q)) / (log(M_T1) - log(M_T0));
      alpha_Q       = alpha_T1 * pow(alpha_T0 / alpha_T1, qfactor_power);
    }

    const double eps0 = sqrt(0.5) / static_cast<double>(1 << RI);
    double delta_Q    = alpha_Q * M_Q;
    double delta_ref  = delta_Q * G_c_sqrt[0] + eps0;
    double G_c        = G_c_sqrt[Cqcc];  // gain of color transform

    for (size_t i = 0; i < epsilon.size(); ++i) {
      int32_t exponent, mantissa;
      double w_b;
      // w_b for LL band shall be 1.0
      w_b = (i >= W_b_sqrt[Cqcc].size()) ? 1.0 : pow(W_b_sqrt[Cqcc][i], qfactor_power);

      double fval = delta_ref / (sqrt(wmse_or_BIBO[i]) * w_b * G_c);
      for (exponent = 0; fval < 1.0; exponent++) {
        fval *= 2.0;
      }
      mantissa = static_cast<int32_t>(floor((fval - 1.0) * static_cast<double>(1 << 11) + 0.5));
      if (mantissa >= (1 << 11)) {
        mantissa = 0;
        exponent--;
      }
      if (exponent > 31) {
        exponent = 31;
        mantissa = 0;
      }
      if (exponent < 0) {
        exponent = 0;
        mantissa = (1 << 11) - 1;
      }
      epsilon[epsilon.size() - i - 1] = static_cast<uint8_t>(exponent);
      mu[epsilon.size() - i - 1]      = static_cast<uint16_t>(mantissa);
    }
  }

  // set SPqcc from epsilon and mu
  if (is_derived) {
    if (is_reversible) {
      printf("ERROR: Derived quantization stepsize is not valid for reversible transform.\n");
      throw std::exception();
    }
    // Quantization style -> Scalar derived (values signalled for LL subband only)
    SPqcc.push_back(static_cast<unsigned short>((epsilon[0] << 11) + mu[0]));
  } else {
    for (size_t i = 0; i < num_bands; ++i) {
      if (is_reversible) {
        SPqcc.push_back(static_cast<unsigned short>(epsilon[i] << 3));
      } else {
        // Quantization style -> Scalar expounded (values signalled for each sub-band)
        SPqcc.push_back(static_cast<unsigned short>((epsilon[i] << 11) + mu[i]));
      }
    }
  }

  is_set = true;
}

QCC_marker::QCC_marker(j2c_src_memory &in, uint16_t Csiz) : j2k_marker_io_base(_QCC), max_components(Csiz) {
  Lmar = in.get_word();
  this->set_buf(in.get_buf_pos());
  in.get_N_byte(this->get_buf(), Lmar - 2U);
  uint16_t len = 2;  // tmp length including Lqcc
  if (max_components < 257) {
    Cqcc = get_byte();
    len++;
  } else {
    Cqcc = get_word();
    len++;
    len++;
  }
  Sqcc = get_byte();
  len++;
  if ((Sqcc & 0x1F) == 0) {
    // reversible transform
    for (size_t i = 0; i < static_cast<size_t>(Lmar - len); ++i) {
      if (i < SPqcc.size()) {
        SPqcc[i] = get_byte();
      } else {
        SPqcc.push_back(get_byte());
      }
    }
  } else {
    // irreversible transformation
    assert((Lmar - len) % 2 == 0);
    for (size_t i = 0; i < static_cast<size_t>((Lmar - len) / 2); ++i) {
      if (i < SPqcc.size()) {
        SPqcc[i] = get_word();
      } else {
        SPqcc.push_back(get_word());
      }
    }
  }
  is_set = true;
}

int QCC_marker::write(j2c_dst_memory &dst) {
  assert(is_set == true);
  dst.put_word(code);
  dst.put_word(Lmar);
  if (max_components < 257) {
    dst.put_byte(static_cast<uint8_t>(Cqcc));
  } else {
    dst.put_word(Cqcc);
  }
  dst.put_byte(Sqcc);

  if (is_reversible) {
    for (unsigned short &i : SPqcc) {
      dst.put_byte(static_cast<uint8_t>(i));
    }
  } else {
    for (unsigned short &i : SPqcc) {
      dst.put_word(i);
    }
  }
  return EXIT_SUCCESS;
}
uint16_t QCC_marker::get_component_index() const { return Cqcc; }

uint8_t QCC_marker::get_quantization_style() const { return (Sqcc & 0x1F); }

uint8_t QCC_marker::get_exponents(uint8_t nb) {
  uint8_t qstyle = get_quantization_style();
  if (qstyle == 0) {
    // lossless
    return static_cast<uint8_t>(SPqcc[nb] >> 3);
  } else if (qstyle == 1) {
    // lossy derived
    return static_cast<uint8_t>(SPqcc[0] >> 11);
  } else {
    // lossy expounded
    assert(qstyle == 2);
    return static_cast<uint8_t>(SPqcc[nb] >> 11);
  }
}

uint16_t QCC_marker::get_mantissas(uint8_t nb) {
  uint8_t qstyle = get_quantization_style();
  if (qstyle == 1) {
    // lossy derived
    return (SPqcc[0] & 0x7FF);
  } else {
    // lossy expounded
    assert(qstyle == 2);
    return (SPqcc[nb] & 0x7FF);
  }
}

uint8_t QCC_marker::get_number_of_guardbits() const { return static_cast<uint8_t>(Sqcc >> 5); }

/********************************************************************************
 * POC_marker
 *******************************************************************************/
POC_marker::POC_marker() : j2k_marker_io_base(_POC) { nPOC = 0; }

POC_marker::POC_marker(uint8_t RS, uint16_t CS, uint16_t LYE, uint8_t RE, uint16_t CE, uint8_t P)
    : j2k_marker_io_base(_POC) {
  Lmar = 0;
  RSpoc.push_back(RS);
  CSpoc.push_back(CS);
  LYEpoc.push_back(LYE);
  REpoc.push_back(RE);
  CEpoc.push_back(CE);
  Ppoc.push_back(P);
  nPOC = 1;
}

POC_marker::POC_marker(j2c_src_memory &in, uint16_t Csiz) : j2k_marker_io_base(_POC) {
  Lmar = in.get_word();
  this->set_buf(in.get_buf_pos());
  in.get_N_byte(this->get_buf(), Lmar - 2U);
  uint16_t len = 2;  // tmp length including Lpoc
  if (Csiz < 257) {
    nPOC = static_cast<unsigned long>((Lmar - len) / 7);
  } else {
    nPOC = static_cast<unsigned long>((Lmar - len) / 9);
  }

  for (unsigned long i = 0; i < nPOC; ++i) {
    if (i < RSpoc.size()) {
      RSpoc[i] = get_byte();
    } else {
      RSpoc.push_back(get_byte());
    }
    if (Csiz < 257) {
      if (i < CSpoc.size()) {
        CSpoc[i] = get_byte();
      } else {
        CSpoc.push_back(get_byte());
      }
    } else {
      if (i < CSpoc.size()) {
        CSpoc[i] = get_word();
      } else {
        CSpoc.push_back(get_word());
      }
    }
    if (i < LYEpoc.size()) {
      LYEpoc[i] = get_word();
    } else {
      LYEpoc.push_back(get_word());
    }
    if (i < REpoc.size()) {
      REpoc[i] = get_byte();
    } else {
      REpoc.push_back(get_byte());
    }
    if (Csiz < 257) {
      if (i < CEpoc.size()) {
        CEpoc[i] = get_byte();
      } else {
        CEpoc.push_back(get_byte());
      }
    } else {
      if (i < CEpoc.size()) {
        CEpoc[i] = get_word();
      } else {
        CEpoc.push_back(get_word());
      }
    }
    if (i < Ppoc.size()) {
      Ppoc[i] = get_byte();
    } else {
      Ppoc.push_back(get_byte());
    }
  }
  is_set = true;
}

void POC_marker::add(uint8_t RS, uint16_t CS, uint16_t LYE, uint8_t RE, uint16_t CE, uint8_t P) {
  RSpoc.push_back(RS);
  CSpoc.push_back(CS);
  LYEpoc.push_back(LYE);
  REpoc.push_back(RE);
  CEpoc.push_back(CE);
  Ppoc.push_back(P);
  nPOC++;
}

[[maybe_unused]] unsigned long POC_marker::get_num_poc() const { return nPOC; }

/********************************************************************************
 * TLM_marker
 *******************************************************************************/
TLM_marker::TLM_marker() : j2k_marker_io_base(_TLM) {
  Ztlm = 0;
  Stlm = 0;
  Ttlm = {0};
  Ptlm = {0};
}

TLM_marker::TLM_marker(j2c_src_memory &in) : j2k_marker_io_base(_TLM) {
  Lmar = in.get_word();
  this->set_buf(in.get_buf_pos());
  in.get_N_byte(this->get_buf(), Lmar - 2U);
  // Ltlm = length;
  // uint16_t len = 2;  // Ltlm
  Ztlm = get_byte();
  Stlm = get_byte();
  uint8_t ST, SP;
  size_t n;
  ST = ((Stlm >> 4) & 0x03);
  SP = ((Stlm >> 4) & 0x0C) >> 2;
  if (ST == 0) {
    if (SP == 0) {
      n = static_cast<size_t>((Lmar - 4) / 2);
    } else {
      n = static_cast<size_t>((Lmar - 4) / 4);
    }
  } else if (ST == 1) {
    if (SP == 0) {
      n = static_cast<size_t>((Lmar - 4) / 3);
    } else {
      n = static_cast<size_t>((Lmar - 4) / 5);
    }
  } else {
    if (SP == 0) {
      n = static_cast<size_t>((Lmar - 4) / 4);
    } else {
      n = static_cast<size_t>((Lmar - 4) / 6);
    }
  }
  for (unsigned long i = 0; i < n; ++i) {
    if (ST == 1) {
      if (i < Ttlm.size()) {
        Ttlm[i] = get_byte();
      } else {
        Ttlm.push_back(get_byte());
      }
    } else if (ST == 2) {
      if (i < Ttlm.size()) {
        Ttlm[i] = get_word();
      } else {
        Ttlm.push_back(get_word());
      }
    }
    if (SP == 0) {
      if (i < Ptlm.size()) {
        Ptlm[i] = get_word();
      } else {
        Ptlm.push_back(get_word());
      }
    } else {
      if (i < Ptlm.size()) {
        Ptlm[i] = get_dword();
      } else {
        Ptlm.push_back(get_dword());
      }
    }
  }
  is_set = true;
}

/********************************************************************************
 * PLM_marker
 *******************************************************************************/
PLM_marker::PLM_marker() : j2k_marker_io_base(_PLM), Zplm(0), plmbuf(nullptr), plmlen(0) {}

PLM_marker::PLM_marker(j2c_src_memory &in) : j2k_marker_io_base(_PLM) {
  Lmar = in.get_word();
  this->set_buf(in.get_buf_pos());
  in.get_N_byte(this->get_buf(), Lmar - 2U);
  // Lplm         = length;
  uint16_t len = 2;  // tmp length including Ltlm
  Zplm         = get_byte();
  len++;
  plmlen = static_cast<uint16_t>(Lmar - len);
  plmbuf = get_buf();
  is_set = true;
}

/********************************************************************************
 * PPM_marker
 *******************************************************************************/
PPM_marker::PPM_marker() : j2k_marker_io_base(_PPM), Zppm(0), ppmbuf(nullptr), ppmlen(0) {}

PPM_marker::PPM_marker(j2c_src_memory &in) : j2k_marker_io_base(_PPM) {
  Lmar = in.get_word();
  this->set_buf(in.get_buf_pos());
  in.get_N_byte(this->get_buf(), Lmar - 2U);
  uint16_t len = 2;  // tmp length including Lppm
  Zppm         = get_byte();
  len++;
  ppmlen = static_cast<uint16_t>(Lmar - len);
  ppmbuf = get_buf();
  is_set = true;
}

/********************************************************************************
 * CRG_marker
 *******************************************************************************/
CRG_marker::CRG_marker() : j2k_marker_io_base(_CRG) {
  Xcrg = {0};
  Ycrg = {0};
}

CRG_marker::CRG_marker(j2c_src_memory &in) : j2k_marker_io_base(_CRG) {
  Lmar = in.get_word();
  this->set_buf(in.get_buf_pos());
  in.get_N_byte(this->get_buf(), Lmar - 2U);
  uint16_t len = 2;  // tmp length including Lcrg
  size_t n     = static_cast<size_t>((Lmar - len) / 4);
  for (size_t i = 0; i < n; ++i) {
    if (i < Xcrg.size()) {
      Xcrg[i] = get_word();
      Ycrg[i] = get_word();
    } else {
      Xcrg.push_back(get_word());
      Ycrg.push_back(get_word());
    }
  }
  is_set = true;
}

/********************************************************************************
 * COM_marker
 *******************************************************************************/
COM_marker::COM_marker(j2c_src_memory &in) : j2k_marker_io_base(_COM), Rcom(0) {
  Lmar = in.get_word();
  this->set_buf(in.get_buf_pos());
  in.get_N_byte(this->get_buf(), Lmar - 2U);
  uint16_t len = 2;  // tmp length including Lcom
  Rcom         = get_word();
  len++;
  len++;
  for (size_t i = 0; i < static_cast<size_t>(Lmar - len); ++i) {
    if (i < Ccom.size()) {
      Ccom[i] = get_byte();
    } else {
      Ccom.push_back(get_byte());
    }
  }
  is_set = true;
}

COM_marker::COM_marker(std::string com, bool is_text) : j2k_marker_io_base(_COM), Rcom(0) {
  Lmar = static_cast<uint16_t>(4 + com.size());
  if (is_text) {
    Rcom = 1;
  }
  for (unsigned long i = 0; i < com.size(); ++i) {
    if (i < Ccom.size()) {
      Ccom[i] = static_cast<uint8_t>(com[i]);
    } else {
      Ccom.push_back(static_cast<uint8_t>(com[i]));
    }
  }
  is_set = true;
}

int COM_marker::write(j2c_dst_memory &dst) {
  assert(is_set == true);
  dst.put_word(code);
  dst.put_word(Lmar);
  dst.put_word(Rcom);
  for (unsigned char &i : Ccom) {
    dst.put_byte(static_cast<uint8_t>(i));
  }
  return EXIT_SUCCESS;
}

/********************************************************************************
 * SOT_marker
 *******************************************************************************/
SOT_marker::SOT_marker() : j2k_marker_io_base(_SOT) {
  Isot  = 0;
  Psot  = 0;
  TPsot = 0;
  TNsot = 0;
}

SOT_marker::SOT_marker(j2c_src_memory &in) : j2k_marker_io_base(_SOT) {
  Lmar = in.get_word();
  if (Lmar != 10) {
    printf("ERROR: Lsot value is invalid.\n");
    throw std::exception();
  }
  this->set_buf(in.get_buf_pos());
  in.get_N_byte(this->get_buf(), Lmar - 2U);
  Isot   = this->get_word();
  Psot   = this->get_dword();
  TPsot  = this->get_byte();
  TNsot  = this->get_byte();
  is_set = true;
}

int SOT_marker::set_SOT_marker(uint16_t tile_index, uint8_t tile_part_index, uint8_t num_tile_parts) {
  Lmar  = 10;  // fixed value
  Isot  = tile_index;
  TPsot = tile_part_index;
  TNsot = num_tile_parts;
  return EXIT_SUCCESS;
}

int SOT_marker::set_tile_part_length(uint32_t length) {
  if (is_set) {
    printf("WARNING: illegal attempt to call SOT_marker::set_tile_part_length() twice.\n");
  }
  // `length` includes parcket header + packet body
  Psot   = length + Lmar + 2 + 2;  // 2 + 2 = length of SOT + SOD
  is_set = true;
  return EXIT_SUCCESS;
}

int SOT_marker::write(j2c_dst_memory &dst) {
  assert(is_set == true);
  dst.put_word(code);
  dst.put_word(Lmar);
  dst.put_word(Isot);
  dst.put_dword(Psot);
  dst.put_byte(TPsot);
  dst.put_byte(TNsot);
  dst.put_word(_SOD);  // SOT marker segment shall be end with SOD marker
  return EXIT_SUCCESS;
}

uint16_t SOT_marker::get_tile_index() const { return Isot; }

uint32_t SOT_marker::get_tile_part_length() const { return Psot; }

uint8_t SOT_marker::get_tile_part_index() const { return TPsot; }

[[maybe_unused]] uint8_t SOT_marker::get_number_of_tile_parts() const { return TNsot; }

/********************************************************************************
 * PLT_marker
 *******************************************************************************/
PLT_marker::PLT_marker() : j2k_marker_io_base(_PLT), Zplt(0), pltbuf(nullptr), pltlen(0) {}

PLT_marker::PLT_marker(j2c_src_memory &in) : j2k_marker_io_base(_PLT) {
  Lmar = in.get_word();
  this->set_buf(in.get_buf_pos());
  in.get_N_byte(this->get_buf(), Lmar - 2U);
  uint16_t len = 2;  // tmp length including Lplt
  Zplt         = get_byte();
  len++;
  pltlen = static_cast<uint16_t>(Lmar - len);
  pltbuf = get_buf();
  is_set = true;
}

/********************************************************************************
 * PPT_marker
 *******************************************************************************/
PPT_marker::PPT_marker() : j2k_marker_io_base(_PPT), Zppt(0), pptbuf(nullptr), pptlen(0) {}

PPT_marker::PPT_marker(j2c_src_memory &in) : j2k_marker_io_base(_PPT) {
  Lmar = in.get_word();
  this->set_buf(in.get_buf_pos());
  in.get_N_byte(this->get_buf(), Lmar - 2U);
  uint16_t len = 2;  // tmp length including Lppt
  Zppt         = get_byte();
  len++;
  pptlen = static_cast<uint16_t>(Lmar - len);
  pptbuf = get_buf();
}

/********************************************************************************
 * j2k_main_header
 *******************************************************************************/
j2k_main_header::j2k_main_header() {
  SIZ        = nullptr;
  CAP        = nullptr;
  COD        = nullptr;
  CPF        = nullptr;
  QCD        = nullptr;
  POC        = nullptr;
  CRG        = nullptr;
  ppm_header = nullptr;
  ppm_buf    = nullptr;
}

j2k_main_header::j2k_main_header(SIZ_marker *siz, COD_marker *cod, QCD_marker *qcd, CAP_marker *cap,
                                 uint8_t qfactor, CPF_marker *cpf, POC_marker *poc, CRG_marker *crg) {
  SIZ = MAKE_UNIQUE<SIZ_marker>(*siz);
  COD = MAKE_UNIQUE<COD_marker>(*cod);
  QCD = MAKE_UNIQUE<QCD_marker>(*qcd);
  // Qfactor, if any
  if (qfactor != 0xFF) {
    if (siz->get_num_components() != 3 && siz->get_num_components() != 1) {
      printf("feature Qfactor is only available for gray-scale or color images.\n");
      throw std::exception();
    }
    for (uint16_t c = 1; c < siz->get_num_components(); ++c) {
      QCC.push_back(MAKE_UNIQUE<QCC_marker>(siz->get_num_components(), c, qcd->get_number_of_guardbits(),
                                            cod->get_dwt_levels(), cod->get_transformation(), false,
                                            siz->get_bitdepth(c), cod->use_color_trafo(), qfactor,
                                            SIZ->get_chroma_format()));
    }
  }

  if (cap != nullptr) {
    CAP = MAKE_UNIQUE<CAP_marker>(*cap);
  }
  if (cpf != nullptr) {
    CPF = MAKE_UNIQUE<CPF_marker>(*cpf);
  }
  if (poc != nullptr) {
    POC = MAKE_UNIQUE<POC_marker>(*poc);
  }
  if (crg != nullptr) {
    CRG = MAKE_UNIQUE<CRG_marker>(*crg);
  }
}

void j2k_main_header::add_COM_marker(const COM_marker &com) { COM.push_back(MAKE_UNIQUE<COM_marker>(com)); }

void j2k_main_header::flush(j2c_dst_memory &buf) {
  SIZ->write(buf);
  if (CAP != nullptr) {
    CAP->write(buf);
  }
  COD->write(buf);
  if (!COC.empty()) {
    for (size_t i = 0; i < COC.size(); ++i) {
      // COC[i]->write(buf);
    }
  }
  QCD->write(buf);
  if (!QCC.empty()) {
    for (size_t i = 0; i < QCC.size(); ++i) {
      QCC[i]->write(buf);
    }
  }
  if (!RGN.empty()) {
    for (size_t i = 0; i < RGN.size(); ++i) {
      // RGN[i]->write(buf);
    }
  }
  // POC->write(buf);
  if (!PPM.empty()) {
    for (size_t i = 0; i < PPM.size(); ++i) {
      // PPM[i]->write(buf);
    }
  }
  if (!TLM.empty()) {
    for (size_t i = 0; i < TLM.size(); ++i) {
      // TLM[i]->write(buf);
    }
  }
  if (!PLM.empty()) {
    for (size_t i = 0; i < PLM.size(); ++i) {
      // PLM[i]->write(buf);
    }
  }
  // CRG->write(buf);
  if (!COM.empty()) {
    for (auto &i : COM) {
      i->write(buf);
    }
  }
}

int j2k_main_header::read(j2c_src_memory &in) {
  uint16_t word = in.get_word();
  assert(word == _SOC);  // check SOC

  while ((word = in.get_word()) != _SOT) {
    switch (word) {
      case _SIZ:
        SIZ = MAKE_UNIQUE<SIZ_marker>(in);
        break;
      case _CAP:
        CAP = MAKE_UNIQUE<CAP_marker>(in);
        break;
      case _COD:
        COD = MAKE_UNIQUE<COD_marker>(in);
        break;
      case _COC:
        COC.push_back(MAKE_UNIQUE<COC_marker>(in, SIZ->get_num_components()));
        break;
      case _TLM:
        TLM.push_back(MAKE_UNIQUE<TLM_marker>(in));
        break;
      case _PLM:
        PLM.push_back(MAKE_UNIQUE<PLM_marker>(in));
        break;
      case _CPF:
        CPF = MAKE_UNIQUE<CPF_marker>(in);
        break;
      case _QCD:
        QCD = MAKE_UNIQUE<QCD_marker>(in);
        break;
      case _QCC:
        QCC.push_back(MAKE_UNIQUE<QCC_marker>(in, SIZ->get_num_components()));
        break;
      case _RGN:
        RGN.push_back(MAKE_UNIQUE<RGN_marker>(in, SIZ->get_num_components()));
        break;
      case _POC:
        POC = MAKE_UNIQUE<POC_marker>(in, SIZ->get_num_components());
        break;
      case _PPM:
        PPM.push_back(MAKE_UNIQUE<PPM_marker>(in));
        break;
      case _CRG:
        CRG = MAKE_UNIQUE<CRG_marker>(in);
        break;
      case _COM:
        COM.push_back(MAKE_UNIQUE<COM_marker>(in));
        break;
      default:
        printf("WARNING: unknown marker %04X is found in main header\n", word);
        break;
    }
  }
  if (!PPM.empty()) {
    uint32_t len = 0;
    for (auto &i : PPM) {
      len += i->ppmlen;
    }
    ppm_buf    = MAKE_UNIQUE<uint8_t[]>(len);
    uint8_t *p = ppm_buf.get();
    for (auto &i : PPM) {
      for (int j = 0; j < i->ppmlen; j++) {
        *p++ = *(i->ppmbuf + j);
      }
    }
    p             = ppm_buf.get();
    uint32_t Nppm = 0;
    ppm_header    = MAKE_UNIQUE<buf_chain>();
    while (len > 0) {
      for (int i = 0; i < 4; ++i, len--) {
        Nppm <<= 8;
        Nppm += *p++;
      }
      ppm_header->add_buf_node(p, Nppm);
      len -= Nppm;
      p += Nppm;
      Nppm = 0;
    }
    ppm_header->activate();
  }
  assert(word == _SOT);
  return EXIT_SUCCESS;
}

void j2k_main_header::get_number_of_tiles(uint32_t &xsize, uint32_t &ysize) const {
  element_siz imsiz, osiz, tsiz, tosiz;
  SIZ->get_image_size(imsiz);
  SIZ->get_image_origin(osiz);
  SIZ->get_tile_size(tsiz);
  SIZ->get_tile_origin(tosiz);
  xsize = ceil_int(imsiz.x - tosiz.x, tsiz.x);
  ysize = ceil_int(imsiz.y - tosiz.y, tsiz.y);
}

/********************************************************************************
 * j2k_tilepart_header
 *******************************************************************************/
j2k_tilepart_header::j2k_tilepart_header(uint16_t nc) {
  num_components = nc;
  // SOT            = nullptr;
  COD = nullptr;
  QCD = nullptr;
  POC = nullptr;
}

uint32_t j2k_tilepart_header::read(j2c_src_memory &in) {
  uint16_t word;
  uint32_t length_of_tilepart_markers = 2U + this->SOT.get_length() + 2U;  // SOT + Lsot + SOD;
  while ((word = in.get_word()) != _SOD) {
    switch (word) {
      case _COD:
        this->COD = MAKE_UNIQUE<COD_marker>(in);
        length_of_tilepart_markers += this->COD->get_length() + 2U;
        break;
      case _COC:
        this->COC.push_back(MAKE_UNIQUE<COC_marker>(in, num_components));
        length_of_tilepart_markers += this->COC[this->COC.size() - 1]->get_length() + 2U;
        break;
      case _PLT:
        this->PLT.push_back(MAKE_UNIQUE<PLT_marker>(in));
        length_of_tilepart_markers += this->PLT[this->PLT.size() - 1]->get_length() + 2U;
        break;
      case _QCD:
        this->QCD = MAKE_UNIQUE<QCD_marker>(in);
        length_of_tilepart_markers += this->QCD->get_length() + 2U;
        break;
      case _QCC:
        this->QCC.push_back(MAKE_UNIQUE<QCC_marker>(in, num_components));
        length_of_tilepart_markers += this->QCC[this->QCC.size() - 1]->get_length() + 2U;
        break;
      case _RGN:
        this->RGN.push_back(MAKE_UNIQUE<RGN_marker>(in, num_components));
        length_of_tilepart_markers += this->RGN[this->RGN.size() - 1]->get_length() + 2U;
        break;
      case _POC:
        this->POC = MAKE_UNIQUE<POC_marker>(in, num_components);
        length_of_tilepart_markers += this->POC->get_length() + 2U;
        break;
      case _PPT:
        this->PPT.push_back(MAKE_UNIQUE<PPT_marker>(in));
        length_of_tilepart_markers += this->PPT[this->PPT.size() - 1]->get_length() + 2U;
        break;
      case _COM:
        this->COM.push_back(MAKE_UNIQUE<COM_marker>(in));
        length_of_tilepart_markers += this->COM[this->COM.size() - 1]->get_length() + 2U;
        break;
      default:
        printf("WARNING: unknown marker %04X is found in tile-part header of tile %d and tile-part %d.\n",
               word, this->SOT.get_tile_index(), this->SOT.get_tile_part_index());
        break;
    }
  }
  return length_of_tilepart_markers;
}
