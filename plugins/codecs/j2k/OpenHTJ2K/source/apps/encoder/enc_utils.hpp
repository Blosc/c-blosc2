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

#pragma once
#include <algorithm>
#include <iterator>
#include <string>

#define NO_QFACTOR 0xFF

void print_help(char *cmd) {
  printf("%s: JPEG 2000 Part 15 encoder\n", cmd);
  printf("USAGE: %s -i input-image(s) -o output-codestream [options...]\n\n", cmd);
  printf("-i: Input-image(s)\n  PGM, PPM, and TIFF (optional, 8 or 16 bpp only) are supported.\n");
  printf("-o: Output codestream\n  `.jhc` or `.j2c` are recommended as the extension.\n");
  printf("  Note: If this option is unspecified, encoding result is placed on a memory buffer.\n\n");
  printf("OPTIONS:\n");
  printf(
      "Stiles=Size:\n  Size of tile. `Size` should be in the format "
      "{height, width}."
      "\n  Default is equal to the image size.\n");
  printf(
      "Sorigin=Size:\n  Offset from the origin of the reference grid to the image area.\n  Default is "
      "{0,0}\n");
  printf(
      "Stile_origin=Size\n  Offset from the origin of the reference grid to the first tile.\n  Default is "
      "{0,0}\n");
  printf(
      "Clevels=Int:\n  Number of DWT decomposition.\n  Valid range for number of DWT levels is from 0 to "
      "32 (Default is 5.)\n");
  printf("Creversible=yes or no:\n  yes for lossless mode, no for lossy mode. Default is no.\n");
  printf("Cblk=Size:\n  Code-block size.\n  Default is {64,64}]");
  printf("Cprecincts=Size:\n  Precinct size. Shall be power of two.\n");
  printf("Cycc=yes or no:\n  yes to use RGB->YCbCr color space conversion.\n");
  printf("Corder:\n  Progression order. Valid entry is one of LRCP, RLCP, RPCL, PCRL, CPRL.\n");
  printf("Cuse_sop=yes or no:\n  yes to use SOP (Start Of Packet) marker segment.\n  Default is no.\n");
  printf("Cuse_eph=yes or no:\n  yes to use EPH (End of Packet Header) marker.\n  Default is no.\n");
  printf("Qstep=Float:\n  Base step size for quantization.\n  0.0 < base step size <= 2.0.\n");
  printf("Qguard=Int:\n  Number of guard bits. Valid range is from 0 to 8 (Default is 1.)\n");
  printf("Qfactor=Int:\n  Quality factor. Valid range is from 0 to 100 (100 is for the best quality)\n");
  printf("  Note: If this option is present, Qstep is ignored and Cycc is set to `yes`.\n");
  printf(
      "-jph_color_space\n"
      "  Color space of input components: Valid entry is one of RGB, YCC.\n  If inputs are represented in "
      "YCbCr, use YCC.\n");
  printf(
      "-num_threads Int\n"
      "  number of threads to use in encode or decode\n"
      "  0, which is the default, indicates usage of all threads.\n");
}

class element_siz_local {
 public:
  uint32_t x;
  uint32_t y;
  element_siz_local() : x(0), y(0) {}
  element_siz_local(uint32_t x0, uint32_t y0) {
    x = x0;
    y = y0;
  }
};

size_t popcount_local(uintmax_t num) {
  size_t precision = 0;
  while (num != 0) {
    if (1 == (num & 1)) {
      precision++;
    }
    num >>= 1;
  }
  return precision;
}

int32_t log2i32(int32_t x) {
  if (x <= 0) {
    printf("ERROR: cannot compute log2 of negative value.\n");
    exit(EXIT_FAILURE);
  }
  int32_t y = 0;
  while (x > 1) {
    y++;
    x >>= 1;
  }
  return y;
}

class j2k_argset {
 private:
  std::vector<std::string> args;
  element_siz_local origin;
  element_siz_local tile_origin;
  uint8_t transformation;
  uint8_t use_ycc;
  uint8_t dwt_levels;
  element_siz_local cblksize;
  bool max_precincts;
  std::vector<element_siz_local> prctsize;
  element_siz_local tilesize;
  uint8_t Porder;
  bool use_sop;
  bool use_eph;
  double base_step_size;
  uint8_t num_guard;
  bool qderived;
  uint8_t qfactor;

  static void get_coordinate(const std::string &param_name, std::string &arg, element_siz_local &dims) {
    size_t pos0, pos1;
    std::string param, val;
    std::string subparam;

    pos0 = arg.find_first_of('=');

    if (pos0 == std::string::npos) {
      printf("ERROR: S%s needs a coordinate for the %s {y,x}\n", param_name.c_str(), param_name.c_str());
      exit(EXIT_FAILURE);
    }
    pos0 = arg.find_first_of('{');
    if (pos0 == std::string::npos) {
      printf("ERROR: S%s needs a coordinate for the %s {y,x}\n", param_name.c_str(), param_name.c_str());
      exit(EXIT_FAILURE);
    }
    pos1 = arg.find_first_of('}');
    if (pos1 == std::string::npos) {
      printf("ERROR: S%s needs a coordinate for the %s {y,x}\n", param_name.c_str(), param_name.c_str());
      exit(EXIT_FAILURE);
    }
    subparam = arg.substr(pos0 + 1, pos1 - pos0 - 1);
    pos0     = subparam.find_first_of(',');
    dims.y   = static_cast<uint32_t>(std::stoi(subparam.substr(0, pos0)));
    dims.x   = static_cast<uint32_t>(std::stoi(subparam.substr(pos0 + 1, subparam.length())));
  };

  template <class T>
  void get_yn(const std::string &param_name, std::string &arg, T &val) {
    size_t pos0;
    std::string param, subparam;
    pos0  = arg.find_first_of('=');
    param = arg.substr(1, pos0 - 1);
    if (param == param_name) {
      pos0 = arg.find_first_of('=');
      if (pos0 == std::string::npos) {
        printf("ERROR: C%s needs =yes or =no\n", param_name.c_str());
        exit(EXIT_FAILURE);
      }
      subparam = arg.substr(pos0 + 1);
      if (subparam == "yes") {
        val = 1;
      } else if (subparam == "no") {
        val = 0;
      } else {
        printf("ERROR: C%s needs =yes or =no\n", param_name.c_str());
        exit(EXIT_FAILURE);
      }
    }
  }

  static void get_bool(const std::string &param_name, std::string &arg, bool &val) {
    size_t pos0;
    std::string param, subparam;
    pos0  = arg.find_first_of('=');
    param = arg.substr(1, pos0 - 1);
    if (param == param_name) {
      pos0 = arg.find_first_of('=');
      if (pos0 == std::string::npos) {
        printf("ERROR: C%s needs =yes or =no\n", param_name.c_str());
        exit(EXIT_FAILURE);
      }
      subparam = arg.substr(pos0 + 1);
      if (subparam == "yes") {
        val = true;
      } else if (subparam == "no") {
        val = false;
      } else {
        printf("ERROR: C%s needs =yes or =no\n", param_name.c_str());
        exit(EXIT_FAILURE);
      }
    }
  }

  template <class T>
  T get_numerical_param(const char &c, const std::string &param_name, std::string &arg, T minval,
                        T maxval) {
    size_t pos0;
    std::string param, subparam;
    pos0 = arg.find_first_of('=');
    if (pos0 == std::string::npos) {
      printf("ERROR: %c%s needs =Int\n", c, param_name.c_str());
      exit(EXIT_FAILURE);
    }
    subparam = arg.substr(pos0 + 1);
    if (subparam.empty()) {
      printf("ERROR: %c%s needs =Int\n", c, param_name.c_str());
      exit(EXIT_FAILURE);
    }
    int tmp = std::stoi(subparam);
    if (tmp < minval || tmp > maxval) {
      printf("ERROR: %c%s shall be in the range of [%d, %d]\n", c, param_name.c_str(), minval, maxval);
      exit(EXIT_FAILURE);
    }
    return static_cast<T>(tmp);
  }

  static double get_numerical_param(const char &c, const std::string &param_name, std::string &arg,
                                    double minval, double maxval) {
    size_t pos0;
    std::string param, subparam;
    pos0 = arg.find_first_of('=');
    if (pos0 == std::string::npos) {
      printf("ERROR: %c%s needs =Int\n", c, param_name.c_str());
      exit(EXIT_FAILURE);
    }
    subparam = arg.substr(pos0 + 1);
    if (subparam.empty()) {
      printf("ERROR: %c%s needs =Int\n", c, param_name.c_str());
      exit(EXIT_FAILURE);
    }
    double tmp = std::stod(subparam);
    if (tmp <= minval || tmp > maxval) {
      printf("ERROR: %c%s shall be in the range of (%f, %f]\n", c, param_name.c_str(), minval, maxval);
      exit(EXIT_FAILURE);
    }
    return static_cast<double>(tmp);
  }

  std::vector<std::string> get_infile() {
    auto p = std::find(args.begin(), args.end(), "-i");
    if (p == args.end()) {
      printf("ERROR: input file (\"-i\") is missing!\n");
      exit(EXIT_FAILURE);
    }
    auto idx = static_cast<size_t>(std::distance(args.begin(), p));
    if (idx + 1 > args.size() - 1) {
      printf("ERROR: file name for input is missing!\n");
      exit(EXIT_FAILURE);
    }
    const std::string buf = args[idx + 1];
    const std::string comma(",");
    std::string::size_type pos = 0;
    std::string::size_type newpos;
    std::vector<std::string> fnames;

    while (true) {
      newpos = buf.find(comma, pos + comma.length());
      fnames.push_back(buf.substr(pos, newpos - pos));
      pos = newpos;
      if (pos != std::string::npos) {
        pos += 1;
      } else {
        break;
      }
    }
    args.erase(args.begin() + static_cast<long>(idx) + 1);
    return fnames;
    // return args[idx + 1].c_str();
  }

  std::string get_outfile() {
    auto p = std::find(args.begin(), args.end(), "-o");
    if (p == args.end()) {
      printf("INFO: no output file is specified. Compressed output is placed on a memory buffer.\n");
      return "";
    }
    auto idx = static_cast<size_t>(std::distance(args.begin(), p));
    if (idx + 1 > args.size() - 1) {
      printf("ERROR: file name for output is missing!\n");
      exit(EXIT_FAILURE);
    }
    std::string out = args[idx + 1];
    args.erase(args.begin() + static_cast<long>(idx) + 1);
    return out;
  }

  int32_t get_num_iteration() {
    auto p = std::find(args.begin(), args.end(), "-iter");
    if (p == args.end()) {
      return 1;
    }
    auto idx = static_cast<size_t>(std::distance(args.begin(), p));
    if (idx + 1 > args.size() - 1) {
      printf("ERROR: -iter requires number of iteration\n");
      exit(EXIT_FAILURE);
    }
    long tmp = std::stol(args[idx + 1]);
    if (tmp < 1 || tmp > INT32_MAX) {
      printf("ERROR: -iter requires positive integer within int32_t range.\n");
      exit(EXIT_FAILURE);
    }
    return static_cast<int32_t>(tmp);
  }

  uint32_t get_num_threads() {
    // zero implies all threads
    auto p = std::find(args.begin(), args.end(), "-num_threads");
    if (p == args.end()) {
      return 0;
    }
    auto idx = static_cast<size_t>(std::distance(args.begin(), p));
    if (idx + 1 > args.size() - 1) {
      printf("ERROR: -num_threads requires number of threads\n");
      exit(EXIT_FAILURE);
    }
    long tmp = std::stol(args[idx + 1]);
    if (tmp < 0 || tmp > UINT32_MAX) {
      printf("ERROR: -num_threads requires non-negative integer within int32_t range.\n");
      exit(EXIT_FAILURE);
    }
    return static_cast<uint32_t>(tmp);
  }

  uint8_t get_jph_color_space() {
    uint8_t val = 0;
    auto p      = std::find(args.begin(), args.end(), "-jph_color_space");
    if (p == args.end()) {
      return val;
    }
    auto idx = static_cast<size_t>(std::distance(args.begin(), p));
    if (idx + 1 > args.size() - 1) {
      printf("ERROR: -jph_color_space requires name of color-space\n");
      exit(EXIT_FAILURE);
    }
    if (args[idx + 1] != "YCC" && args[idx + 1] != "RGB") {
      printf("ERROR: invalid name for color-space\n");
      exit(EXIT_FAILURE);
    } else if (args[idx + 1] == "YCC") {
      val = 1;
    }
    return val;
  }

 public:
  std::vector<std::string> ifnames;
  std::string ofname;
  int32_t num_iteration;
  uint32_t num_threads;
  uint8_t jph_color_space;

  j2k_argset(int argc, char *argv[])
      : origin(0, 0),
        tile_origin(0, 0),
        transformation(0),
        use_ycc(1),
        dwt_levels(5),
        cblksize(4, 4),
        max_precincts(true),
        tilesize(0, 0),
        Porder(0),
        use_sop(false),
        use_eph(false),
        base_step_size(0.0),
        num_guard(1),
        qderived(false),
        qfactor(NO_QFACTOR),
        ifnames{},
        num_iteration(1),
        num_threads(0),
        jph_color_space(0) {
    args.reserve(static_cast<unsigned long>(argc));
    // skip command itself
    for (int i = 1; i < argc; ++i) {
      args.emplace_back(argv[i]);
    }
    get_help(argc, argv);
    ifnames         = get_infile();
    ofname          = get_outfile();
    num_threads     = get_num_threads();
    num_iteration   = get_num_iteration();
    jph_color_space = get_jph_color_space();

    for (auto &arg : args) {
      char &c = arg.front();
      if (c == '-') {
        std::string optname = arg.substr(1);
        if (optname != "i" && optname != "o" && optname != "num_threads" && optname != "jph_color_space"
            && optname != "iter") {
          printf("ERROR: unknown option %s\n", arg.c_str());
          exit(EXIT_FAILURE);
        }
      }
      size_t pos0, pos1;
      std::string param, val;
      std::string subparam;
      element_siz_local tmpsiz;
      if (c == 'S') {
        pos0  = arg.find_first_of('=');
        param = arg.substr(1, pos0 - 1);
        if (param == "tiles") {
          get_coordinate(param, arg, tilesize);
        } else if (param == "origin") {
          get_coordinate(param, arg, origin);
        } else if (param == "tile_origin") {
          get_coordinate(param, arg, tile_origin);
        } else {
          printf("ERROR: unknown parameter S%s\n", param.c_str());
          exit(EXIT_FAILURE);
        }
      } else if (c == 'C') {
        pos0  = arg.find_first_of('=');
        param = arg.substr(1, pos0 - 1);
        if (param == "reversible") {
          get_yn(param, arg, transformation);
        } else if (param == "ycc") {
          get_yn(param, arg, use_ycc);
        } else if (param == "levels") {
          dwt_levels = static_cast<uint8_t>(get_numerical_param(c, param, arg, 0, 32));
        } else if (param == "blk") {
          get_coordinate(param, arg, cblksize);
          if ((popcount_local(cblksize.y) > 1) || (popcount_local(cblksize.x) > 1)) {
            printf("ERROR: code block size must be power of two.\n");
            exit(EXIT_FAILURE);
          }
          if (cblksize.x < 4 || cblksize.y < 4) {
            printf("ERROR: code block size must be greater than four\n");
            exit(EXIT_FAILURE);
          }
          if (cblksize.x * cblksize.y > 4096) {
            printf("ERROR: code block area must be less than or equal to 4096.\n");
            exit(EXIT_FAILURE);
          }
          cblksize.x = static_cast<uint32_t>(log2i32(static_cast<int32_t>(cblksize.x)) - 2);
          cblksize.y = static_cast<uint32_t>(log2i32(static_cast<int32_t>(cblksize.y)) - 2);
        } else if (param == "precincts") {
          max_precincts = false;
          pos0          = arg.find_first_of('=');
          if (pos0 == std::string::npos) {
            printf("ERROR: Cprecincts needs at least one precinct size {height,width}\n");
            exit(EXIT_FAILURE);
          }
          pos0 = arg.find_first_of('{');
          if (pos0 == std::string::npos) {
            printf("ERROR: Cprecincts needs at least one precinct size {height,width}\n");
            exit(EXIT_FAILURE);
          }
          while (pos0 != std::string::npos) {
            pos1 = arg.find(std::string("}"), pos0);
            if (pos1 == std::string::npos) {
              printf("ERROR: Cprecincts needs at least one precinct size {height,width}\n");
              exit(EXIT_FAILURE);
            }
            subparam = arg.substr(pos0 + 1, pos1 - pos0 - 1);
            pos0     = subparam.find_first_of(',');
            tmpsiz.y = static_cast<uint32_t>(std::stoi(subparam.substr(0, pos0)));
            tmpsiz.x = static_cast<uint32_t>(std::stoi(subparam.substr(pos0 + 1, 5)));
            if ((popcount_local(tmpsiz.y) > 1) || (popcount_local(tmpsiz.x) > 1)) {
              printf("ERROR: precinct size must be power of two.\n");
              exit(EXIT_FAILURE);
            }
            prctsize.emplace_back(log2i32(static_cast<int32_t>(tmpsiz.x)),
                                  log2i32(static_cast<int32_t>(tmpsiz.y)));
            pos0 = arg.find(std::string("{"), pos1);
          }
        } else if (param == "order") {
          pos0 = arg.find_first_of('=');
          if (pos0 == std::string::npos) {
            printf("ERROR: Corder needs progression order =(LRCP, RLCP, RPCL, PCRL, CPRL)\n");
            exit(EXIT_FAILURE);
          }
          val = arg.substr(pos0 + 1, 4);
          if (val == "LRCP") {
            Porder = 0;
          } else if (val == "RLCP") {
            Porder = 1;
          } else if (val == "RPCL") {
            Porder = 2;
          } else if (val == "PCRL") {
            Porder = 3;
          } else if (val == "CPRL") {
            Porder = 4;
          } else {
            printf("ERROR: unknown progression order %s\n", val.c_str());
            exit(EXIT_FAILURE);
          }
        } else if (param == "use_sop") {
          get_bool(param, arg, use_sop);
        } else if (param == "use_eph") {
          get_bool(param, arg, use_eph);
        } else {
          printf("ERROR: unknown parameter C%s\n", param.c_str());
          exit(EXIT_FAILURE);
        }
      } else if (c == 'Q') {
        pos0  = arg.find_first_of('=');
        param = arg.substr(1, pos0 - 1);
        if (param == "step") {
          base_step_size = get_numerical_param(c, param, arg, 0.0, 2.0);
        } else if (param == "guard") {
          num_guard = static_cast<uint8_t>(get_numerical_param(c, param, arg, 0, 7));
        } else if (param == "derived") {
          get_bool(param, arg, qderived);
        } else if (param == "factor") {
          qfactor = static_cast<uint8_t>(get_numerical_param(c, param, arg, 0, 100));
        } else {
          printf("ERROR: unknown parameter Q%s\n", param.c_str());
          exit(EXIT_FAILURE);
        }
      } else {
      }
    }
  }

  void get_help(int argc, char *argv[]) {
    auto p = std::find(args.begin(), args.end(), "-h");
    if (p == args.end() && argc > 1) {
      return;
    }
    print_help(argv[0]);
    exit(EXIT_SUCCESS);
  }

  [[nodiscard]] element_siz_local get_origin() const { return origin; }
  [[nodiscard]] element_siz_local get_tile_origin() const { return tile_origin; }
  [[nodiscard]] uint8_t get_transformation() const { return transformation; }
  [[nodiscard]] uint8_t get_ycc() const { return use_ycc; }
  [[nodiscard]] uint8_t get_dwt_levels() const { return dwt_levels; }
  element_siz_local get_cblk_size() { return cblksize; }
  [[nodiscard]] bool is_max_precincts() const { return max_precincts; }
  std::vector<element_siz_local> get_prct_size() { return prctsize; }
  element_siz_local get_tile_size() { return tilesize; }
  [[nodiscard]] uint8_t get_progression() const { return Porder; }
  [[nodiscard]] bool is_use_sop() const { return use_sop; }
  [[nodiscard]] bool is_use_eph() const { return use_eph; }
  [[nodiscard]] double get_basestep_size() const { return base_step_size; }
  [[nodiscard]] uint8_t get_num_guard() const { return num_guard; }
  [[nodiscard]] bool is_derived() const { return qderived; }
  [[nodiscard]] uint8_t get_qfactor() const { return qfactor; }
};
