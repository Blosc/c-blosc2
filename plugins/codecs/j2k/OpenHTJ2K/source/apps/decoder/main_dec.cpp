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

// open_htj2k_dec: A decoder implementation for JPEG 2000 Part 1 and 15
// (ITU-T Rec. 814 | ISO/IEC 15444-15 and ITU-T Rec. 814 | ISO/IEC 15444-15)
//
// (c) 2019 - 2021 Osamu Watanabe, Takushoku University, Vrije Universiteit Brussels

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#ifdef _OPENMP
  #include <omp.h>
#endif

#include "decoder.hpp"
#include "dec_utils.hpp"

void print_help(char *cmd) {
  printf("JPEG 2000 Part 1 and Part 15 decoder\n");
  printf("USAGE: %s [options]\n\n", cmd);
  printf("OPTIONS:\n");
  printf("-i: Input file. .j2k, .j2c, .jhc, and .jphc are supported.\n");
  printf("    .jp2 and .jph (box based file-format) are not supported.\n");
  printf("-o: Output file. Supported formats are PPM, PGM, PGX and RAW.\n");
  printf("-reduce n: Number of DWT resolution reduction.\n");
}

int main(int argc, char *argv[]) {
  // parse input args
  char *infile_name, *infile_ext_name;
  char *outfile_name, *outfile_ext_name;
  if (command_option_exists(argc, argv, "-h") || argc < 2) {
    print_help(argv[0]);
    exit(EXIT_SUCCESS);
  }
  if (nullptr == (infile_name = get_command_option(argc, argv, "-i"))) {
    printf("ERROR: Input file is missing. Use -i to specify input file.\n");
    exit(EXIT_FAILURE);
  }
  infile_ext_name = strrchr(infile_name, '.');
  if (strcmp(infile_ext_name, ".j2k") != 0 && strcmp(infile_ext_name, ".j2c") != 0
      && strcmp(infile_ext_name, ".jhc") != 0 && strcmp(infile_ext_name, ".jphc") != 0) {
    printf("ERROR: Supported extensions are .j2k, .j2c, .jhc, and .jphc\n");
    exit(EXIT_FAILURE);
  }
  if (nullptr == (outfile_name = get_command_option(argc, argv, "-o"))) {
    printf(
        "ERROR: Output files are missing. Use -o to specify output file "
        "names.\n");
    exit(EXIT_FAILURE);
  }
  outfile_ext_name = strrchr(outfile_name, '.');
  if (strcmp(outfile_ext_name, ".pgm") != 0 && strcmp(outfile_ext_name, ".ppm") != 0
      && strcmp(outfile_ext_name, ".raw") != 0 && strcmp(outfile_ext_name, ".pgx") != 0) {
    printf("ERROR: Unsupported output file type.\n");
    exit(EXIT_FAILURE);
  }
  char *tmp_param, *endptr;
  long tmp_val;
  uint8_t reduce_NL;
  if (nullptr == (tmp_param = get_command_option(argc, argv, "-reduce"))) {
    reduce_NL = 0;
  } else {
    tmp_val = strtol(tmp_param, &endptr, 10);
    if (tmp_val >= 0 && tmp_val <= 32 && tmp_param != endptr) {
      reduce_NL = static_cast<uint8_t>(tmp_val);
    } else {
      printf("ERROR: -reduce takes non-negative integer in the range from 0 to 32.\n");
      exit(EXIT_FAILURE);
    }
  }
  int32_t num_iterations;
  if (nullptr == (tmp_param = get_command_option(argc, argv, "-iter"))) {
    num_iterations = 1;
  } else {
    tmp_val = strtol(tmp_param, &endptr, 10);
    if (tmp_param == endptr) {
      printf("ERROR: -iter takes positive integer.\n");
      exit(EXIT_FAILURE);
    }
    if (tmp_val < 1 || tmp_val > INT32_MAX) {
      printf("ERROR: -iter takes positive integer ( < INT32_MAX).\n");
      exit(EXIT_FAILURE);
    }
    num_iterations = static_cast<int32_t>(tmp_val);
  }

  uint32_t num_threads;
  if (nullptr == (tmp_param = get_command_option(argc, argv, "-num_threads"))) {
    num_threads = 0;
  } else {
    tmp_val = strtol(tmp_param, &endptr, 10);
    if (tmp_param == endptr) {
      printf("ERROR: -num_threads takes non-negative integer.\n");
      exit(EXIT_FAILURE);
    }
    if (tmp_val < 0 || tmp_val > UINT32_MAX) {
      printf("ERROR: -num_threads takes non-negative integer ( < UINT32_MAX).\n");
      exit(EXIT_FAILURE);
    }
    //    num_iterations = static_cast<int32_t>(tmp_val);
    num_threads = static_cast<uint32_t>(tmp_val);  // strtoul(tmp_param, nullptr, 10);
  }

  std::vector<int32_t *> buf;
  std::vector<uint32_t> img_width;
  std::vector<uint32_t> img_height;
  std::vector<uint8_t> img_depth;
  std::vector<bool> img_signed;
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < num_iterations; ++i) {
    // create decoder
    open_htj2k::openhtj2k_decoder decoder(infile_name, reduce_NL, num_threads);
    for (auto &j : buf) {
      delete[] j;
    }
    buf.clear();
    img_width.clear();
    img_height.clear();
    img_depth.clear();
    img_signed.clear();
    // invoke decoding
    try {
      decoder.invoke(buf, img_width, img_height, img_depth, img_signed);
    } catch (std::exception &exc) {
      return EXIT_FAILURE;
    }
  }
  auto duration = std::chrono::high_resolution_clock::now() - start;

  // write decoded components
  bool compositable   = false;
  auto num_components = static_cast<uint16_t>(img_depth.size());
  if (num_components == 3 && strcmp(outfile_ext_name, ".ppm") == 0) {
    compositable = true;
    for (uint16_t c = 0; c < num_components - 1; c++) {
      if (img_width[c] != img_width[c + 1U] || img_height[c] != img_height[c + 1U]) {
        compositable = false;
        break;
      }
    }
  }
  if (strcmp(outfile_ext_name, ".ppm") == 0) {
    // PPM
    if (!compositable) {
      printf("ERROR: the number of components of the input is not three.");
      exit(EXIT_FAILURE);
    }
    write_ppm(outfile_name, outfile_ext_name, buf, img_width, img_height, img_depth, img_signed);

  } else {
    // PGM or RAW
    write_components(outfile_name, outfile_ext_name, buf, img_width, img_height, img_depth, img_signed);
  }

  uint32_t total_samples = 0;
  for (uint16_t c = 0; c < num_components; ++c) {
    total_samples += img_width[c] * img_height[c];
    delete[] buf[c];
  }

  // show stats
  auto count  = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
  double time = static_cast<double>(count) / 1000.0 / static_cast<double>(num_iterations);
  printf("elapsed time %-15.3lf[ms]\n", time);
  printf("throughput %lf [Msamples/s]\n",
         total_samples * static_cast<double>(num_iterations) / (double)count);
  printf("throughput %lf [usec/sample]\n",
         (double)count / static_cast<double>(num_iterations) / total_samples);
  return EXIT_SUCCESS;
}
