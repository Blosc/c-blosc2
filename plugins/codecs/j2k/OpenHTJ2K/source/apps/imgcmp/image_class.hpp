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
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cmath>
#include <memory>

#define READ_WIDTH 0
#define READ_HEIGHT 1
#define READ_MAXVAL 2
#define DONE 3

class image {
 private:
  // number of components
  uint_fast16_t num_components;
  // width
  uint_fast32_t width;
  // height
  uint_fast32_t height;
  // maximum value of pixel
  uint_fast32_t maxval;

 private:
  // bit-depth
  uint_fast8_t bitDepth;
  // is pixel signed value ?
  bool isSigned;
  // byte endian
  bool isBigendian;
  // pointer to pixel values
  int_fast32_t *data;

 public:
  // default constructor
  image()
      : num_components(0),
        width(0),
        height(0),
        maxval(0),
        bitDepth(0),
        isSigned(false),
        isBigendian(false),
        data(nullptr){};
  // // destructor
  ~image() { delete[] data; }

  [[nodiscard]] uint_fast32_t get_width() const { return width; }

  [[nodiscard]] uint_fast32_t get_height() const { return height; }

  [[nodiscard]] uint_fast32_t get_maxval() const { return static_cast<uint_fast32_t>((1 << bitDepth) - 1); }

  [[maybe_unused]] [[nodiscard]] uint8_t get_bpp() const { return bitDepth; }

  [[nodiscard]] uint_fast16_t get_num_components() const { return num_components; }

  int_fast32_t *access_pixels() { return data; }

  [[maybe_unused]] void cvt_to_planner() {
    auto *tmp = new int_fast32_t[this->width * this->height * num_components];
    for (uint_fast16_t c = 0; c < num_components; ++c) {
      for (uint_fast32_t i = 0; i < height; ++i) {
        for (uint_fast32_t j = 0; j < width; ++j) {
          tmp[i * width + j + c * width * height] =
              this->data[i * num_components * width + j * num_components + c];
        }
      }
    }
    delete[] this->data;
    this->data = tmp;
  }

  [[maybe_unused]] int_fast32_t *access_components(uint_fast16_t c) {
    if (c > num_components) {
      printf("ERROR: input argument c exceeds the number of components of the image.\n");
      exit(EXIT_FAILURE);
    }
    return data + (width * height) * c;
  }

  // parsing PNM/PGX header
  int read_pnmpgx(const char *name) {
    constexpr char SP = ' ';
    constexpr char LF = '\n';
    constexpr char CR = 0x0d;

    FILE *fp = fopen(name, "rb");
    if (fp == nullptr) {
      printf("File %s is not found.\n", name);
      exit(EXIT_FAILURE);
    }
    int status = 0;
    int c;
    int_fast32_t val = 0;
    char comment[256];
    c = fgetc(fp);
    if (c != 'P') {
      printf("%s is not a PNM/PGX file.\n", name);
      exit(EXIT_FAILURE);
    }

    bool isASCII = false;
    bool isPGX   = false;

    c = fgetc(fp);
    switch (c) {
      // PGM
      case '2':
        isASCII = true;
        /* FALLTHRU */
      case '5':
        num_components = 1;
        isBigendian    = true;
        break;
      // PPM
      case '3':
        isASCII = true;
        /* FALLTHRU */
      case '6':
        num_components = 3;
        isBigendian    = true;
        break;
      // PGX
      case 'G':
        num_components = 1;
        isPGX          = true;
        // read endian
        do {
          c = fgetc(fp);
        } while (c != 'M' && c != 'L');
        switch (c) {
          case 'M':
            isBigendian = true;
            c           = fgetc(fp);
            if (c != 'L') {
              printf("input PGX file %s is broken.\n", name);
            }
            break;
          case 'L':
            c = fgetc(fp);
            if (c != 'M') {
              printf("input PGX file %s is broken.\n", name);
            }
            break;
          default:
            printf("ERROR: input file does not conform to PGX format.\n");
            exit(EXIT_FAILURE);
        }
        // check signed or not
        do {
          c = fgetc(fp);
        } while (c != '+' && c != '-' && isdigit(c) == false);
        if (c == '+' || c == '-') {
          if (c == '-') {
            isSigned = true;
          }
          do {
            c = fgetc(fp);
          } while (isdigit(c) == false);
        }
        do {
          val *= 10;
          val += c - '0';
          c = fgetc(fp);
        } while (c != SP && c != LF && c != CR);
        bitDepth = static_cast<uint_fast8_t>(val);
        val      = 0;
        // fpos_t pos;
        // fgetpos(fp, &pos);
        // pos--;
        // fsetpos(fp, &pos);
        break;
      // PBM (not supported)
      case '1':
      case '4':
        printf("PBM file is not supported.\n");
        exit(EXIT_FAILURE);
        break;
      // error
      default:
        printf("%s is not a PNM/PGX file.\n", name);
        exit(EXIT_FAILURE);
        break;
    }
    while (status != DONE) {
      c = fgetc(fp);
      // eat white/LF/CR and comments
      while (c == SP || c == LF || c == CR) {
        c = fgetc(fp);
        if (c == '#') {
          char *nouse = fgets(comment, sizeof(comment), fp);
          if (nouse == nullptr) {
            printf("comment read error\n");
            exit(EXIT_FAILURE);
          }
          c = fgetc(fp);
        }
      }
      // read numerical value
      while (c != SP && c != LF && c != CR) {
        val *= 10;
        val += c - '0';
        c = fgetc(fp);
      }
      // update status
      switch (status) {
        case READ_WIDTH:
          this->width = static_cast<uint_fast32_t>(val);
          val         = 0;
          status      = READ_HEIGHT;
          break;
        case READ_HEIGHT:
          this->height = static_cast<uint_fast32_t>(val);
          val          = 0;
          if (isPGX) {
            status = DONE;
          } else {
            status = READ_MAXVAL;
          }
          break;
        case READ_MAXVAL:
          maxval   = static_cast<uint_fast32_t>(val);
          bitDepth = (uint_fast8_t)ceil(log2((double)maxval));
          val      = 0;
          status   = DONE;
          break;
        default:
          break;
      }
    }
    // easting trailing spaces/LF/CR or comments
    c = fgetc(fp);
    while (c == SP || c == LF || c == CR) {
      c = fgetc(fp);
      if (c == '#') {
        char *nouse = fgets(comment, sizeof(comment), fp);
        if (nouse == nullptr) {
          printf("comment read error\n");
          exit(EXIT_FAILURE);
        }
        c = fgetc(fp);
      }
    }
    fseek(fp, -1, SEEK_CUR);

    const uint_fast8_t nbytes = static_cast<uint_fast8_t>((bitDepth + 7) / 8);  // ceil bitDepth to byte
    const size_t num_samples  = this->width * this->height * num_components;
    const size_t num_bytes    = this->width * this->height * nbytes * num_components;

    // this->data = MAKE_UNIQUE<int_fast32_t[]>(this->width * this->height);
    this->data = new int_fast32_t[num_samples];

    if (!isASCII) {
#if ((defined(_MSVC_LANG) && _MSVC_LANG < 201402L) || __cplusplus < 201402L)
      std::unique_ptr<uint8_t[]> buf(new uint_fast8_t[num_bytes]);
#else
      auto buf = std::make_unique<uint_fast8_t[]>(num_bytes);
#endif
      if (fread(buf.get(), sizeof(uint8_t), num_bytes, fp) < num_bytes) {
        printf("ERROR: not enough samples in the given pnm file.\n");
        exit(EXIT_FAILURE);
      }
      switch (nbytes) {
        case 1:
          for (uint_fast32_t i = 0; i < num_samples; ++i) {
            this->data[i] = (isSigned) ? (int_fast8_t)buf[i] : buf[i];
          }
          break;
        case 2:
          for (uint_fast32_t i_in = 0, i_out = 0; i_out < num_samples; i_in += nbytes, ++i_out) {
            if (isSigned) {
              if (isBigendian) {
                this->data[i_out] =
                    static_cast<int_least16_t>((static_cast<uint_least16_t>(buf[i_in]) << 8)
                                               | static_cast<uint_least16_t>(buf[i_in + 1]));
              } else {
                this->data[i_out] =
                    static_cast<int_least16_t>(static_cast<uint_least16_t>(buf[i_in])
                                               | (static_cast<uint_least16_t>(buf[i_in + 1]) << 8));
              }
            } else {
              if (isBigendian) {
                this->data[i_out] = (buf[i_in] << 8) | buf[i_in + 1];
              } else {
                this->data[i_out] = buf[i_in] | (buf[i_in + 1] << 8);
              }
            }
          }
          break;
        default:
          printf("bit-depth over 16 is not supported.\n");
          exit(EXIT_FAILURE);
          break;
      }
    } else {
      for (uint_fast32_t i = 0; i < this->width * this->height; ++i) {
        val = 0;
        c   = fgetc(fp);
        while (c != ' ' && c != '\n' && c != EOF) {
          val *= 10;
          val += c - '0';
          c = fgetc(fp);
        }
        this->data[i] = val;
      }
    }
    fclose(fp);
    return EXIT_SUCCESS;
  }
  // show info
  [[maybe_unused]] int show() {
    if (this->data == nullptr) {
      return EXIT_FAILURE;
    }
    printf("number of components = %hu\n", static_cast<uint16_t>(num_components));
    printf("width = %u, height = %u\n", static_cast<uint32_t>(this->width),
           static_cast<uint32_t>(this->height));
    // printf("max value = %d\n", this->maxval);
    printf("bit-depth = %d\n", bitDepth);
    if (isBigendian) {
      printf("Big endian\n");
    }
    if (isSigned) {
      printf("Signed\n");
    }
    for (uint_fast32_t i = 0; i < this->height; ++i) {
      for (uint_fast32_t j = 0; j < this->width; ++j) {
        printf("%3d ", static_cast<int32_t>(this->data[i * this->width + j]));
      }
      printf("\n");
    }
    return EXIT_SUCCESS;
  }
};