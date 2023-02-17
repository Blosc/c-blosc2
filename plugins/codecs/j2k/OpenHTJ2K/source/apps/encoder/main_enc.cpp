// Copyright (c) 2019 - 2022, Osamu Watanabe
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

// open_htj2k_enc: An encoder implementation of ITU-T Rec. 814 | ISO/IEC 15444-15
// (a.k.a HTJ2K)
//
// This software is currently compliant to limited part of the standard.
// Supported markers: SIZ, CAP, COD, QCD, QCC, COM. Other features are undone and future work.
// (c) 2021 Osamu Watanabe, Takushoku University
// (c) 2022 Osamu Watanabe, Takushoku University

#include <chrono>
#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
  #include <filesystem>
#else
  #include <sys/stat.h>
#endif
#include <vector>
#include <exception>
#include "encoder.hpp"
#include "enc_utils.hpp"

int main(int argc, char *argv[]) {
  j2k_argset args(argc, argv);  // parsed command line
  std::vector<std::string> fnames = args.ifnames;
  for (const auto &fname : fnames) {
#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
    try {
      if (!std::filesystem::exists(fname)) {
        throw std::exception();
      }
    }
#else
    try {
      struct stat st;
      if (stat(fname.c_str(), &st)) {
        throw std::exception();
      }
    }
#endif
    catch (std::exception &exc) {
      printf("ERROR: File %s is not found.\n", fname.c_str());
      return EXIT_FAILURE;
    }
  }
  auto fstart = std::chrono::high_resolution_clock::now();
  open_htj2k::image img(fnames);  // input image
  auto fduration = std::chrono::high_resolution_clock::now() - fstart;
  auto fcount    = std::chrono::duration_cast<std::chrono::microseconds>(fduration).count();
  double ftime   = static_cast<double>(fcount) / 1000.0;
  printf("elapsed time for reading inputs %-15.3lf[ms]\n", ftime);
  auto fbytes = img.get_width() * img.get_height() * img.get_num_components() * 2;
  printf("%f [MB/s]\n", (double)fbytes / ftime / 1000);
  element_siz_local image_origin = args.get_origin();
  element_siz_local image_size(img.get_width(), img.get_height());

  uint16_t num_components = img.get_num_components();
  std::vector<int32_t *> input_buf;
  for (uint16_t c = 0; c < num_components; ++c) {
    input_buf.push_back(img.get_buf(c));
  }
  bool isJPH               = false;
  std::string out_filename = args.ofname;
  bool toFile              = true;
  if (out_filename.empty()) {
    toFile = false;
  } else {
    std::string::size_type pos = out_filename.find_last_of('.');
    std::string fext           = out_filename.substr(pos, 4);
    if (fext == ".jph" || fext == ".JPH") {
      isJPH = true;
    } else if (fext.compare(".j2c") && fext.compare(".j2k") && fext.compare(".jphc") && fext.compare(".J2C")
               && fext.compare(".J2K") && fext.compare(".JPHC")) {
      printf("ERROR: invalid extension for output file\n");
      exit(EXIT_FAILURE);
    }
  }
  element_siz_local tile_size   = args.get_tile_size();
  element_siz_local tile_origin = args.get_tile_origin();
  if (image_origin.x != 0 && tile_origin.x == 0) {
    tile_origin.x = image_origin.x;
  }
  if (image_origin.y != 0 && tile_origin.y == 0) {
    tile_origin.y = image_origin.y;
  }
  open_htj2k::siz_params siz;  // information of input image
  siz.Rsiz   = 0;
  siz.Xsiz   = image_size.x + image_origin.x;
  siz.Ysiz   = image_size.y + image_origin.y;
  siz.XOsiz  = image_origin.x;
  siz.YOsiz  = image_origin.y;
  siz.XTsiz  = tile_size.x;
  siz.YTsiz  = tile_size.y;
  siz.XTOsiz = tile_origin.x;
  siz.YTOsiz = tile_origin.y;
  siz.Csiz   = num_components;
  for (uint16_t c = 0; c < siz.Csiz; ++c) {
    siz.Ssiz.push_back(img.get_Ssiz_value(c));
    auto compw = img.get_component_width(c);
    auto comph = img.get_component_height(c);
    siz.XRsiz.push_back(static_cast<unsigned char>(((siz.Xsiz - siz.XOsiz) + compw - 1) / compw));
    siz.YRsiz.push_back(static_cast<unsigned char>(((siz.Ysiz - siz.YOsiz) + comph - 1) / comph));
  }
  // siz.bpp    = img_depth;

  open_htj2k::cod_params cod;  // parameters related to COD marker
  element_siz_local cblk_size       = args.get_cblk_size();
  cod.blkwidth                      = static_cast<uint16_t>(cblk_size.x);
  cod.blkheight                     = static_cast<uint16_t>(cblk_size.y);
  cod.is_max_precincts              = args.is_max_precincts();
  cod.use_SOP                       = args.is_use_sop();
  cod.use_EPH                       = args.is_use_eph();
  cod.progression_order             = args.get_progression();
  cod.number_of_layers              = 1;
  cod.use_color_trafo               = args.get_ycc();
  cod.dwt_levels                    = args.get_dwt_levels();
  cod.codeblock_style               = 0x040;
  cod.transformation                = args.get_transformation();
  std::vector<element_siz_local> PP = args.get_prct_size();
  for (auto &i : PP) {
    cod.PPx.push_back(static_cast<unsigned char>(i.x));
    cod.PPy.push_back(static_cast<unsigned char>(i.y));
  }

  open_htj2k::qcd_params qcd{};  // parameters related to QCD marker
  qcd.is_derived          = args.is_derived();
  qcd.number_of_guardbits = args.get_num_guard();
  qcd.base_step           = args.get_basestep_size();
  if (qcd.base_step == 0.0) {
    qcd.base_step = 1.0f / static_cast<float>(1 << img.get_max_bpp());
  }
  uint8_t color_space = args.jph_color_space;

  size_t total_size      = 0;
  int32_t num_iterations = args.num_iteration;
  // memory buffer for output codestream/file
  std::vector<uint8_t> outbuf;
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < num_iterations; ++i) {
    // create encoder
    open_htj2k::openhtj2k_encoder encoder(out_filename.c_str(), input_buf, siz, cod, qcd,
                                          args.get_qfactor(), isJPH, color_space, args.num_threads);
    if (!toFile) {
      encoder.set_output_buffer(outbuf);
    }
    // invoke encoding
    try {
      total_size = encoder.invoke();
    } catch (std::exception &exc) {
      return EXIT_FAILURE;
    }
  }

  auto duration = std::chrono::high_resolution_clock::now() - start;
  auto count    = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
  double time   = static_cast<double>(count) / 1000.0 / static_cast<double>(num_iterations);
  double bpp    = (double)total_size * 8 / (img.get_width() * img.get_height());

  // show stats
  printf("Codestream bytes  = %zu = %f [bits/pixel]\n", total_size, bpp);
  printf("elapsed time %-15.3lf[ms]\n", time);
  return EXIT_SUCCESS;
}
