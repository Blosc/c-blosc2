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
#include <vector>
#include "open_htj2k_typedef.hpp"
#include "codestream.hpp"
#include "marker_def.hpp"

/********************************************************************************
 * j2k_marker_io_base
 *******************************************************************************/
class j2k_marker_io_base {
 protected:
  // marker code
  uint16_t code;
  // length of marker segment in bytes
  uint16_t Lmar{};
  // some markers requires pointer to buffer
  uint8_t *buf;
  // position in buffer
  uint16_t pos;
  bool is_set;

 public:
  explicit j2k_marker_io_base(uint16_t mar) : code(mar), buf(nullptr), pos(0), is_set(false) {}
  ~j2k_marker_io_base() = default;
  void set_buf(uint8_t *p);

  [[maybe_unused]] uint16_t get_marker() const;
  uint16_t get_length() const;
  uint8_t *get_buf();
  uint8_t get_byte();
  uint16_t get_word();
  uint32_t get_dword();
};

/********************************************************************************
 * SIZ_marker
 *******************************************************************************/
class SIZ_marker : public j2k_marker_io_base {
 private:
  uint16_t Rsiz;
  uint32_t Xsiz;
  uint32_t Ysiz;
  uint32_t XOsiz;
  uint32_t YOsiz;
  uint32_t XTsiz;
  uint32_t YTsiz;
  uint32_t XTOsiz;
  uint32_t YTOsiz;
  uint16_t Csiz;
  std::vector<uint8_t> Ssiz;
  std::vector<uint8_t> XRsiz;
  std::vector<uint8_t> YRsiz;

 public:
  explicit SIZ_marker(j2c_src_memory &in);
  SIZ_marker(uint16_t R, uint32_t X, uint32_t Y, uint32_t XO, uint32_t YO, uint32_t XT, uint32_t YT,
             uint32_t XTO, uint32_t YTO, uint16_t C, std::vector<uint8_t> &S, std::vector<uint8_t> &XR,
             std::vector<uint8_t> &YR, bool needCAP);
  int write(j2c_dst_memory &dst);
  bool is_signed(uint16_t c);
  uint8_t get_bitdepth(uint16_t c);
  void get_image_size(element_siz &siz) const;
  uint32_t get_component_stride(uint16_t c) const;
  void get_image_origin(element_siz &siz) const;
  void get_tile_size(element_siz &siz) const;
  void get_tile_origin(element_siz &siz) const;
  void get_subsampling_factor(element_siz &siz, uint16_t c);
  uint16_t get_num_components() const;
  uint8_t get_chroma_format() const;
};

/********************************************************************************
 * CAP_marker
 *******************************************************************************/
class CAP_marker : public j2k_marker_io_base {
 private:
  uint32_t Pcap;
  uint16_t Ccap[32];
  void set_Pcap(uint8_t part);

 public:
  CAP_marker();
  explicit CAP_marker(j2c_src_memory &in);
  void set_Ccap(uint16_t val, uint8_t Ccap);

  [[maybe_unused]] uint32_t get_Pcap() const;
  uint16_t get_Ccap(uint8_t n);
  int write(j2c_dst_memory &dst);
};

/********************************************************************************
 * CPF_marker
 *******************************************************************************/
class CPF_marker : public j2k_marker_io_base {
 private:
  std::vector<uint16_t> Pcpf;

 public:
  CPF_marker();
  explicit CPF_marker(j2c_src_memory &in);
  int write(j2c_dst_memory &dst);
};

/********************************************************************************
 * COD_marker
 *******************************************************************************/
class COD_marker : public j2k_marker_io_base {
 private:
  uint8_t Scod;
  uint32_t SGcod;
  std::vector<uint8_t> SPcod;

 public:
  explicit COD_marker(j2c_src_memory &in);
  COD_marker(bool is_max_precincts, bool use_SOP, bool use_EPH, uint8_t progression_order,
             uint16_t number_of_layers, uint8_t use_color_trafo, uint8_t dwt_levels, uint8_t log2cblksizex,
             uint8_t log2cblksizey, uint8_t codeblock_style, uint8_t reversible_flag,
             std::vector<uint8_t> log2PPx, std::vector<uint8_t> log2PPy);
  int write(j2c_dst_memory &dst);
  bool is_maximum_precincts() const;
  bool is_use_SOP() const;
  bool is_use_EPH() const;
  uint8_t get_progression_order() const;
  uint16_t get_number_of_layers() const;
  uint8_t use_color_trafo() const;
  uint8_t get_dwt_levels();
  void get_codeblock_size(element_siz &out);
  void get_precinct_size(element_siz &out, uint8_t resolution);
  uint8_t get_Cmodes();
  uint8_t get_transformation();
};

/********************************************************************************
 * COC_marker
 *******************************************************************************/
class COC_marker : public j2k_marker_io_base {
 private:
  uint16_t Ccoc;
  uint8_t Scoc;
  std::vector<uint8_t> SPcoc;

 public:
  COC_marker();
  COC_marker(j2c_src_memory &in, uint16_t Csiz);
  uint16_t get_component_index() const;
  bool is_maximum_precincts() const;
  uint8_t get_dwt_levels();
  void get_codeblock_size(element_siz &out);
  void get_precinct_size(element_siz &out, uint8_t resolution);
  uint8_t get_Cmodes();
  uint8_t get_transformation();
};

/********************************************************************************
 * RGN_marker
 *******************************************************************************/
class RGN_marker : public j2k_marker_io_base {
 private:
  uint16_t Crgn;
  uint8_t Srgn;
  uint8_t SPrgn;

 public:
  RGN_marker();
  RGN_marker(j2c_src_memory &in, uint16_t Csiz);
  uint16_t get_component_index() const;
  uint8_t get_ROIshift() const;
};

/********************************************************************************
 * QCD_marker
 *******************************************************************************/
class QCD_marker : public j2k_marker_io_base {
 private:
  uint8_t Sqcd;
  std::vector<uint16_t> SPqcd;
  bool is_reversible{};

 public:
  explicit QCD_marker(j2c_src_memory &in);
  QCD_marker(uint8_t number_of_guardbits, uint8_t dwt_levels, uint8_t transformation, bool is_derived,
             uint8_t RI, uint8_t use_ycc, double basestep = 1.0 / 256.0, uint8_t qfactor = 0xFF);
  int write(j2c_dst_memory &dst);
  uint8_t get_quantization_style() const;
  uint8_t get_exponents(uint8_t nb);
  uint16_t get_mantissas(uint8_t nb);
  uint8_t get_number_of_guardbits() const;
  // return MAGB value for CAP
  uint8_t get_MAGB();
};

/********************************************************************************
 * QCC_marker
 *******************************************************************************/
class QCC_marker : public j2k_marker_io_base {
 private:
  uint16_t max_components;
  uint16_t Cqcc;
  uint8_t Sqcc;
  std::vector<uint16_t> SPqcc;
  bool is_reversible;

 public:
  QCC_marker(uint16_t Csiz, uint16_t c, uint8_t number_of_guardbits, uint8_t dwt_levels,
             uint8_t transformation, bool is_derived, uint8_t RI, uint8_t use_ycc, uint8_t qfactor,
             uint8_t chroma_format);
  QCC_marker(j2c_src_memory &in, uint16_t Csiz);
  int write(j2c_dst_memory &dst);
  uint16_t get_component_index() const;
  uint8_t get_quantization_style() const;
  uint8_t get_exponents(uint8_t nb);
  uint16_t get_mantissas(uint8_t nb);
  uint8_t get_number_of_guardbits() const;
};

/********************************************************************************
 * POC_marker
 *******************************************************************************/
class POC_marker : public j2k_marker_io_base {
 private:
 public:
  std::vector<uint8_t> RSpoc;
  std::vector<uint16_t> CSpoc;
  std::vector<uint16_t> LYEpoc;
  std::vector<uint8_t> REpoc;
  std::vector<uint16_t> CEpoc;
  std::vector<uint8_t> Ppoc;
  unsigned long nPOC;
  POC_marker();
  POC_marker(uint8_t RS, uint16_t CS, uint16_t LYE, uint8_t RE, uint16_t CE, uint8_t P);
  POC_marker(j2c_src_memory &in, uint16_t Csiz);
  void add(uint8_t RS, uint16_t CS, uint16_t LYE, uint8_t RE, uint16_t CE, uint8_t P);

  [[maybe_unused]] unsigned long get_num_poc() const;
};

/********************************************************************************
 * TLM_marker
 *******************************************************************************/
class TLM_marker : public j2k_marker_io_base {
 private:
  uint8_t Ztlm;
  uint8_t Stlm;
  std::vector<uint16_t> Ttlm;
  std::vector<uint32_t> Ptlm;

 public:
  TLM_marker();
  explicit TLM_marker(j2c_src_memory &in);
};

/********************************************************************************
 * PLM_marker
 *******************************************************************************/
class PLM_marker : public j2k_marker_io_base {
 private:
  uint8_t Zplm;
  uint8_t *plmbuf;
  uint16_t plmlen;

 public:
  PLM_marker();
  explicit PLM_marker(j2c_src_memory &in);
};

/********************************************************************************
 * PPM_marker
 *******************************************************************************/
class PPM_marker : public j2k_marker_io_base {
 private:
  uint8_t Zppm;

 public:
  uint8_t *ppmbuf;
  uint16_t ppmlen;
  PPM_marker();
  explicit PPM_marker(j2c_src_memory &in);
};

/********************************************************************************
 * CRG_marker
 *******************************************************************************/
class CRG_marker : public j2k_marker_io_base {
 private:
  std::vector<uint16_t> Xcrg;
  std::vector<uint16_t> Ycrg;

 public:
  CRG_marker();
  explicit CRG_marker(j2c_src_memory &in);
};

/********************************************************************************
 * COM_marker
 *******************************************************************************/
class COM_marker : public j2k_marker_io_base {
 private:
  uint16_t Rcom;
  std::vector<uint8_t> Ccom;

 public:
  explicit COM_marker(j2c_src_memory &in);
  COM_marker(std::string com, bool is_text);
  int write(j2c_dst_memory &dst);
};

/********************************************************************************
 * SOT_marker
 *******************************************************************************/
class SOT_marker : public j2k_marker_io_base {
 private:
  uint16_t Isot;
  uint32_t Psot;
  uint8_t TPsot;
  uint8_t TNsot;

 public:
  SOT_marker();
  explicit SOT_marker(j2c_src_memory &in);
  int set_SOT_marker(uint16_t tile_index, uint8_t tile_part_index, uint8_t num_tile_parts);
  int set_tile_part_length(uint32_t length);
  int write(j2c_dst_memory &dst);
  uint16_t get_tile_index() const;
  uint32_t get_tile_part_length() const;
  uint8_t get_tile_part_index() const;

  [[maybe_unused]] uint8_t get_number_of_tile_parts() const;
};

/********************************************************************************
 * PLT_marker
 *******************************************************************************/
class PLT_marker : public j2k_marker_io_base {
 private:
  uint8_t Zplt;
  uint8_t *pltbuf;
  uint16_t pltlen;

 public:
  PLT_marker();
  explicit PLT_marker(j2c_src_memory &in);
};

/********************************************************************************
 * PPT_marker
 *******************************************************************************/
class PPT_marker : public j2k_marker_io_base {
 private:
  uint8_t Zppt;

 public:
  uint8_t *pptbuf;
  uint16_t pptlen;
  PPT_marker();
  explicit PPT_marker(j2c_src_memory &in);
};

/********************************************************************************
 * j2k_main_header
 *******************************************************************************/
class j2k_main_header {
 public:
  std::unique_ptr<SIZ_marker> SIZ;
  std::unique_ptr<CAP_marker> CAP;
  std::unique_ptr<COD_marker> COD;
  std::vector<std::unique_ptr<COC_marker>> COC;
  std::unique_ptr<CPF_marker> CPF;
  std::unique_ptr<QCD_marker> QCD;
  std::vector<std::unique_ptr<QCC_marker>> QCC;
  std::vector<std::unique_ptr<RGN_marker>> RGN;
  std::unique_ptr<POC_marker> POC;
  std::vector<std::unique_ptr<PPM_marker>> PPM;
  std::vector<std::unique_ptr<TLM_marker>> TLM;
  std::vector<std::unique_ptr<PLM_marker>> PLM;
  std::unique_ptr<CRG_marker> CRG;
  std::vector<std::unique_ptr<COM_marker>> COM;
  std::unique_ptr<buf_chain> ppm_header;
  std::unique_ptr<uint8_t[]> ppm_buf;

 public:
  j2k_main_header();
  j2k_main_header(SIZ_marker *siz, COD_marker *cod, QCD_marker *qcd, CAP_marker *cap = nullptr,
                  uint8_t qfactor = 0xFF, CPF_marker *cpf = nullptr, POC_marker *poc = nullptr,
                  CRG_marker *crg = nullptr);
  void add_COM_marker(const COM_marker &com);
  void flush(j2c_dst_memory &buf);
  int read(j2c_src_memory &);
  void get_number_of_tiles(uint32_t &x, uint32_t &y) const;
  buf_chain *get_ppm_header() const { return ppm_header.get(); }
};

/********************************************************************************
 * j2k_tilepart_header
 *******************************************************************************/
class j2k_tilepart_header {
 public:
  uint16_t num_components;
  SOT_marker SOT;
  std::unique_ptr<COD_marker> COD;
  std::vector<std::unique_ptr<COC_marker>> COC;
  std::unique_ptr<QCD_marker> QCD;
  std::vector<std::unique_ptr<QCC_marker>> QCC;
  std::vector<std::unique_ptr<RGN_marker>> RGN;
  std::unique_ptr<POC_marker> POC;
  std::vector<std::unique_ptr<PPT_marker>> PPT;
  std::vector<std::unique_ptr<PLT_marker>> PLT;
  std::vector<std::unique_ptr<COM_marker>> COM;

 public:
  explicit j2k_tilepart_header(uint16_t nc);
  uint32_t read(j2c_src_memory &in);
};
