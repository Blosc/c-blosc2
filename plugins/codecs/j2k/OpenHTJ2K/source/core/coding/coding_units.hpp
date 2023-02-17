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

#include "j2kmarkers.hpp"
#include <cstring>
#include <functional>

/********************************************************************************
 * j2k_region
 *******************************************************************************/
class j2k_region {
 public:
  // top-left coordinate (inclusive) of a region in the reference grid
  element_siz pos0;
  // bottom-right coordinate (exclusive) of a region in the reference grid
  element_siz pos1;
  // return top-left coordinate (inclusive)
  [[nodiscard]] element_siz get_pos0() const { return pos0; }
  // return bottom-right coordinate (exclusive)
  [[nodiscard]] element_siz get_pos1() const { return pos1; }
  // get size of a region
  void get_size(element_siz &out) const {
    out.x = pos1.x - pos0.x;
    out.y = pos1.y - pos0.y;
  }
  // set top-left coordinate (inclusive)
  void set_pos0(element_siz in) { pos0 = in; }
  // set bottom-right coordinate (exclusive)
  void set_pos1(element_siz in) { pos1 = in; }
  j2k_region() = default;
  j2k_region(element_siz p0, element_siz p1) : pos0(p0), pos1(p1) {}
};

/********************************************************************************
 * j2k_codeblock
 *******************************************************************************/
class j2k_codeblock : public j2k_region {
 public:
  const element_siz size;

 private:
  uint8_t *compressed_data;
  uint8_t *current_address;
  const uint8_t band;
  const uint8_t M_b;
  [[maybe_unused]] const uint32_t index;

 public:
  int32_t *sample_buf;
  size_t blksampl_stride;
  uint8_t *block_states;
  size_t blkstate_stride;
  sprec_t *const i_samples;
  const uint32_t band_stride;
  [[maybe_unused]] const uint8_t R_b;
  const uint8_t transformation;
  const float stepsize;

  const uint16_t num_layers;

  uint32_t length;
  uint16_t Cmodes;
  uint8_t num_passes;
  uint8_t num_ZBP;
  uint8_t fast_skip_passes;
  uint8_t Lblock;
  // length of a coding pass in byte
  std::vector<uint32_t> pass_length;
  // index of the coding-pass from which layer starts
  std::unique_ptr<uint8_t[]> layer_start;
  // number of coding-passes included in a layer
  std::unique_ptr<uint8_t[]> layer_passes;
  bool already_included;
  bool refsegment;

  j2k_codeblock(const uint32_t &idx, uint8_t orientation, uint8_t M_b, uint8_t R_b, uint8_t transformation,
                float stepsize, uint32_t band_stride, sprec_t *ibuf, uint32_t offset,
                const uint16_t &numlayers, const uint8_t &codeblock_style, const element_siz &p0,
                const element_siz &p1, const element_siz &s);
  ~j2k_codeblock() {
    if (compressed_data != nullptr) {
      free(compressed_data);
    }
  }
  //  void modify_state(const std::function<void(uint8_t &, uint8_t)> &callback, uint8_t val, int16_t j1,
  //                    int16_t j2) {
  //    callback(
  //        block_states[static_cast<uint32_t>(j1 + 1) * (blkstate_stride) + static_cast<uint32_t>(j2 + 1)],
  //        val);
  //  }
  //  uint8_t get_state(const std::function<uint8_t(uint8_t &)> &callback, int16_t j1, int16_t j2) const {
  //    return (uint8_t)callback(
  //        block_states[static_cast<uint32_t>(j1 + 1) * (blkstate_stride) + static_cast<uint32_t>(j2 +
  //        1)]);
  //  }
  [[nodiscard]] uint8_t get_orientation() const { return band; }

  //  [[nodiscard]] uint8_t get_context_label_sig(const uint32_t &j1, const uint32_t &j2) const;
  //  [[nodiscard]] uint8_t get_signLUT_index(const uint32_t &j1, const uint32_t &j2) const;
  [[nodiscard]] uint8_t get_Mb() const;
  uint8_t *get_compressed_data();
  void set_compressed_data(uint8_t *buf, uint16_t size, uint16_t Lref = 0);
  void create_compressed_buffer(buf_chain *tile_buf, int32_t buf_limit, const uint16_t &layer);
  //  void update_sample(const uint8_t &symbol, const uint8_t &p, const int16_t &j1, const int16_t &j2)
  //  const; void update_sign(const int8_t &val, const uint32_t &j1, const uint32_t &j2) const;
  //  [[nodiscard]] uint8_t get_sign(const uint32_t &j1, const uint32_t &j2) const;
  void quantize(uint32_t &or_val);
  uint8_t calc_mbr(uint32_t i, uint32_t j, uint8_t causal_cond) const;
  void dequantize(uint8_t ROIshift) const;
};

/********************************************************************************
 * j2k_subband
 *******************************************************************************/
class j2k_subband : public j2k_region {
 public:
  uint8_t orientation;
  uint8_t transformation;
  uint8_t R_b;
  [[maybe_unused]] uint8_t epsilon_b;
  [[maybe_unused]] uint16_t mantissa_b;
  uint8_t M_b;
  float delta;
  [[maybe_unused]] float nominal_range;
  sprec_t *i_samples;

  // j2k_subband();
  j2k_subband(element_siz p0, element_siz p1, uint8_t orientation, uint8_t transformation, uint8_t R_b,
              uint8_t epsilon_b, uint16_t mantissa_b, uint8_t M_b, float delta, float nominal_range,
              sprec_t *ibuf);
  ~j2k_subband();
};

/********************************************************************************
 * j2k_precinct_subband
 *******************************************************************************/
class j2k_precinct_subband : public j2k_region {
 private:
  [[maybe_unused]] const uint8_t orientation;
  //  std::unique_ptr<tagtree> inclusion_info;
  //  std::unique_ptr<tagtree> ZBP_info;
  //  std::unique_ptr<std::unique_ptr<j2k_codeblock>[]> codeblocks;
  tagtree *inclusion_info;
  tagtree *ZBP_info;
  j2k_codeblock **codeblocks;

 public:
  uint32_t num_codeblock_x;
  uint32_t num_codeblock_y;
  j2k_precinct_subband(uint8_t orientation, uint8_t M_b, uint8_t R_b, uint8_t transformation,
                       float stepsize, sprec_t *ibuf, const element_siz &bp0, const element_siz &bp1,
                       const element_siz &p0, const element_siz &p1, const uint16_t &num_layers,
                       const element_siz &codeblock_size, const uint8_t &Cmodes);
  ~j2k_precinct_subband() {
    delete inclusion_info;
    delete ZBP_info;
    for (uint32_t i = 0; i < num_codeblock_x * num_codeblock_y; ++i) {
      delete codeblocks[i];
    }
    delete[] codeblocks;
  }
  //  void destroy_codeblocks() {
  //    for (uint32_t i = 0; i < num_codeblock_x * num_codeblock_y; ++i) {
  //      delete codeblocks[i];
  //    }
  //    delete[] codeblocks;
  //  }
  tagtree_node *get_inclusion_node(uint32_t i);
  tagtree_node *get_ZBP_node(uint32_t i);
  j2k_codeblock *access_codeblock(uint32_t i);
  void parse_packet_header(buf_chain *packet_header, uint16_t layer_idx, uint16_t Ccap15);
  void generate_packet_header(packet_header_writer &header, uint16_t layer_idx);
};

/********************************************************************************
 * j2k_precinct
 *******************************************************************************/
class j2k_precinct : public j2k_region {
 private:
  // index of this precinct
  [[maybe_unused]] const uint32_t index;
  // index of resolution level to which this precinct belongs
  const uint8_t resolution;
  // number of subbands in this precinct
  const uint8_t num_bands;
  // length which includes packet header and body, used only for encoder
  uint32_t length;
  // container for a subband within this precinct which includes codeblocks
  std::unique_ptr<std::unique_ptr<j2k_precinct_subband>[]> pband;

 public:
  // buffer for generated packet header: only for encoding
  std::unique_ptr<uint8_t[]> packet_header;
  // length of packet header
  uint32_t packet_header_length;

 public:
  j2k_precinct(const uint8_t &r, const uint32_t &idx, const element_siz &p0, const element_siz &p1,
               const std::unique_ptr<std::unique_ptr<j2k_subband>[]> &subband, const uint16_t &num_layers,
               const element_siz &codeblock_size, const uint8_t &Cmodes);
  //  ~j2k_precinct() {
  //    for (size_t i = 0; i < num_bands; ++i) {
  //      pband[i]->destroy_codeblocks();
  //    }
  //  }

  j2k_precinct_subband *access_pband(uint8_t b);
  void set_length(uint32_t len) { length = len; }
  [[nodiscard]] uint32_t get_length() const { return length; }
};

/********************************************************************************
 * j2c_packet
 *******************************************************************************/
class j2c_packet {
 public:
  [[maybe_unused]] uint16_t layer;
  [[maybe_unused]] uint8_t resolution;
  [[maybe_unused]] uint16_t component;
  [[maybe_unused]] uint32_t precinct;
  [[maybe_unused]] buf_chain *header;
  [[maybe_unused]] buf_chain *body;
  // only for encoder
  std::unique_ptr<uint8_t[]> buf;
  uint32_t length;

  j2c_packet()
      : layer(0), resolution(0), component(0), precinct(0), header(nullptr), body(nullptr), length(0){};
  // constructor for decoding
  j2c_packet(const uint16_t l, const uint8_t r, const uint16_t c, const uint32_t p,
             buf_chain *const h = nullptr, buf_chain *const bo = nullptr)
      : layer(l), resolution(r), component(c), precinct(p), header(h), body(bo), length(0) {}
  // constructor for encoding
  j2c_packet(uint16_t l, uint8_t r, uint16_t c, uint32_t p, j2k_precinct *cp, uint8_t num_bands);
};

/********************************************************************************
 * j2k_resolution
 *******************************************************************************/
class j2k_resolution : public j2k_region {
 private:
  // resolution level
  const uint8_t index;
  // array of unique pointer to precincts
  std::unique_ptr<std::unique_ptr<j2k_precinct>[]> precincts;
  // unique pointer to subbands
  std::unique_ptr<std::unique_ptr<j2k_subband>[]> subbands;
  // nominal ranges of subbands
  float child_ranges[4]{};

 public:
  // number of subbands
  const uint8_t num_bands;
  // number of precincts wide
  const uint32_t npw;
  // number of precincts height
  const uint32_t nph;
  // a resolution is empty if it has no precincts
  const bool is_empty;
  // post-shift value for inverse DWT
  uint8_t normalizing_upshift;
  // pre-shift value for forward DWT
  uint8_t normalizing_downshift;
  sprec_t *i_samples;
  //  float *f_samples;
  j2k_resolution(const uint8_t &r, const element_siz &p0, const element_siz &p1, const uint32_t &npw,
                 const uint32_t &nph);
  ~j2k_resolution();
  [[maybe_unused]] uint8_t get_index() const { return index; }
  void create_subbands(element_siz &p0, element_siz &p1, uint8_t NL, uint8_t transformation,
                       std::vector<uint8_t> &exponents, std::vector<uint16_t> &mantissas,
                       uint8_t num_guard_bits, uint8_t qstyle, uint8_t bitdepth);
  void create_precincts(element_siz PP, uint16_t num_layers, element_siz codeblock_size, uint8_t Cmodes);

  // void create_precinct_bands(uint16_t num_layers, element_siz codeblock_size, uint8_t Cmodes);
  j2k_precinct *access_precinct(uint32_t p);
  j2k_subband *access_subband(uint8_t b);
  void set_nominal_ranges(const float *ranges) {
    child_ranges[0] = ranges[0];
    child_ranges[1] = ranges[1];
    child_ranges[2] = ranges[2];
    child_ranges[3] = ranges[3];
  }
  void scale();
};

/********************************************************************************
 * j2k_tile_part
 *******************************************************************************/
class j2k_tile_part {
 private:
  // tile index to which this tile-part belongs
  uint16_t tile_index;
  // tile-part index
  uint8_t tile_part_index;
  // pointer to tile-part buffer
  uint8_t *body;
  // length of tile-part
  uint32_t length;

 public:
  // pointer to tile-part header
  std::unique_ptr<j2k_tilepart_header> header;
  explicit j2k_tile_part(uint16_t num_components);
  void set_SOT(SOT_marker &tmpSOT);
  int read(j2c_src_memory &);
  [[maybe_unused]] [[nodiscard]] uint16_t get_tile_index() const;
  [[maybe_unused]] [[nodiscard]] uint8_t get_tile_part_index() const;
  [[nodiscard]] uint32_t get_length() const;
  uint8_t *get_buf();
  void set_tile_index(uint16_t t);
  void set_tile_part_index(uint8_t tp);
};

/********************************************************************************
 * j2k_tile_base
 *******************************************************************************/
class j2k_tile_base : public j2k_region {
 public:
  // number of DWT decomposition levels
  uint8_t NL;
  // resolution reduction
  uint8_t reduce_NL;
  // code-block width and height
  element_siz codeblock_size;
  // codeblock style (Table A.19)
  uint8_t Cmodes;
  // DWT type (Table A.20), 0:9x7, 1:5x3
  uint8_t transformation;
  // precinct width and height as exponents of the power of 2
  std::vector<element_siz> precinct_size;
  // quantization style (Table A.28)
  uint8_t quantization_style;
  // exponents of step sizes
  std::vector<uint8_t> exponents;
  // mantissas of step sizes
  std::vector<uint16_t> mantissas;
  // number of guard bits
  uint8_t num_guard_bits;
  j2k_tile_base() : reduce_NL(0) {}
};

/********************************************************************************
 * j2k_tile_component
 *******************************************************************************/
class j2k_tile_component : public j2k_tile_base {
 private:
  // component index
  uint16_t index;
  // pointer to sample buffer (integer)
  int32_t *samples;
  // shift value for ROI
  uint8_t ROIshift;
  // pointer to instances of resolution class
  std::unique_ptr<std::unique_ptr<j2k_resolution>[]> resolution;
  // set members related to COC marker
  void setCOCparams(COC_marker *COC);
  // set members related to QCC marker
  void setQCCparams(QCC_marker *QCC);
  // set ROIshift from RGN marker
  void setRGNparams(RGN_marker *RGN);

 public:
  // component bit-depth
  uint8_t bitdepth;
  // default constructor
  j2k_tile_component();
  // destructor
  ~j2k_tile_component();
  // initialization of coordinates and parameters defined in tile-part markers
  void init(j2k_main_header *hdr, j2k_tilepart_header *tphdr, j2k_tile_base *tile, uint16_t c,
            std::vector<int32_t *> img = {});
  int32_t *get_sample_address(uint32_t x, uint32_t y);
  uint8_t get_dwt_levels();
  uint8_t get_transformation();
  [[maybe_unused]] [[nodiscard]] uint8_t get_Cmodes() const;
  [[maybe_unused]] [[nodiscard]] uint8_t get_bitdepth() const;
  element_siz get_precinct_size(uint8_t r);
  [[maybe_unused]] element_siz get_codeblock_size();
  [[maybe_unused]] [[nodiscard]] uint8_t get_ROIshift() const;
  j2k_resolution *access_resolution(uint8_t r);
  void create_resolutions(uint16_t numlayers);

  void perform_dc_offset(uint8_t transformation, bool is_signed);
};

/********************************************************************************
 * j2k_tile
 *******************************************************************************/
class j2k_tile : public j2k_tile_base {
 private:
  // vector array of tile-parts
  std::vector<std::unique_ptr<j2k_tile_part>> tile_part;
  // index of this tile
  uint16_t index;
  // number of components
  uint16_t num_components;
  // SOP is used or not (Table A.13)
  bool use_SOP;
  // EPH is used or not (Table A.13)
  bool use_EPH;
  // progression order (Table A.16)
  uint8_t progression_order;
  // number of layers (Table A.14)
  uint16_t numlayers;
  // multiple component transform (Table A.17)
  uint8_t MCT;

  // length of tile (in bytes)
  uint32_t length;
  // pointer to tile buffer
  std::unique_ptr<buf_chain> tile_buf;
  // pointer to packet header
  buf_chain *packet_header;
  // buffer for PPM marker segments
  buf_chain sbst_packet_header;
  // number of tile-parts
  uint8_t num_tile_part;
  // position of current tile-part
  int current_tile_part_pos;
  // unique pointer to tile-components
  std::unique_ptr<j2k_tile_component[]> tcomp;
  // pointer to packet header in PPT marker segments
  std::unique_ptr<buf_chain> ppt_header;
  // number_of_packets (for encoder only)
  uint32_t num_packets;
  // unique pointer to packets
  std::unique_ptr<j2c_packet[]> packet;
  // value of Ccap15 parameter in CAP marker segment
  uint16_t Ccap15;
  // progression order information for both COD and POC
  POC_marker porder_info;
  // return SOP is used or not
  [[nodiscard]] bool is_use_SOP() const { return this->use_SOP; }
  // return EPH is used or not
  [[maybe_unused]] [[nodiscard]] bool is_use_EPH() const { return this->use_EPH; }
  // set members related to COD marker
  void setCODparams(COD_marker *COD);
  // set members related to QCD marker
  void setQCDparams(QCD_marker *QCD);
  // read packets
  void read_packet(j2k_precinct *current_precint, uint16_t layer, uint8_t num_band);
  // function to retrieve greatest common divisor of precinct size among resolution levels
  void find_gcd_of_precinct_size(element_siz &out);

 public:
  j2k_tile();
  // Decoding
  // Initialization with tile-index
  void dec_init(uint16_t idx, j2k_main_header &main_header, uint8_t reduce_levels);
  // read and add a tile_part into a tile
  void add_tile_part(SOT_marker &tmpSOT, j2c_src_memory &in, j2k_main_header &main_header);
  // create buffer to store compressed data for decoding
  void create_tile_buf(j2k_main_header &main_header);
  // decoding (does block decoding and IDWT) function for a tile
  void decode();
  // inverse color transform
  void ycbcr_to_rgb();
  // inverse DC offset and clipping
  void finalize(j2k_main_header &main_header, uint8_t reduce_NL, std::vector<int32_t *> &dst);

  // Encoding
  // Initialization with tile-index
  void enc_init(uint16_t idx, j2k_main_header &main_header, std::vector<int32_t *> img);
  // DC offsetting
  int perform_dc_offset(j2k_main_header &main_header);
  // forward color transform
  void rgb_to_ycbcr();
  // encoding (does block encoding and FDWT) function for a tile
  uint8_t *encode();
  // create packets in encoding
  void construct_packets(j2k_main_header &main_header);
  // write packets into destination
  void write_packets(j2c_dst_memory &outbuf);

  // getters
  [[maybe_unused]] [[nodiscard]] uint16_t get_numlayers() const { return this->numlayers; }
  j2k_tile_component *get_tile_component(uint16_t c);

  [[maybe_unused]] [[maybe_unused]] uint8_t get_byte_from_tile_buf();
  [[maybe_unused]] uint8_t get_bit_from_tile_buf();
  [[nodiscard]] uint32_t get_length() const;
  [[maybe_unused]] uint32_t get_buf_length();
};

int32_t htj2k_encode(j2k_codeblock *block, uint8_t ROIshift) noexcept;
