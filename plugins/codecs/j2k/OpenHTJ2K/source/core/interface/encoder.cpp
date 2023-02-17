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

#include <cstring>
#include <fstream>
#include <j2kmarkers.hpp>
#include "encoder.hpp"
#include "coding_units.hpp"
#include "jph.hpp"
#include "ThreadPool.hpp"

#define NO_QFACTOR 0xFF

#define sRGB 0
#define sYCC 1

namespace open_htj2k {
image::image(const std::vector<std::string> &filenames) : width(0), height(0), buf(nullptr) {
  size_t num_files = filenames.size();
  if (num_files > 16384) {
    printf("ERROR: over 16384 components are not supported in the spec.\n");
    throw std::exception();
  }
  num_components = static_cast<uint16_t>(num_files);  // num_components may change after parsing PPM header
  uint16_t c     = 0;
  char ext_name[256];
  char *infile_ext_name = ext_name;
  for (const auto &fname : filenames) {
    bool is_pnm_pgx = false;
    bool is_tiff    = false;
    infile_ext_name = strrchr(const_cast<char *>(fname.c_str()), '.');
    if (strcmp(infile_ext_name, ".pgm") == 0 || strcmp(infile_ext_name, ".PGM") == 0
        || strcmp(infile_ext_name, ".ppm") == 0 || strcmp(infile_ext_name, ".PPM") == 0
        || strcmp(infile_ext_name, ".pgx") == 0 || strcmp(infile_ext_name, ".PGX") == 0) {
      is_pnm_pgx = true;
    }
#if defined(OPENHTJ2K_TIFF_SUPPORT)
    if (strcmp(infile_ext_name, ".tif") == 0 || strcmp(infile_ext_name, ".TIF") == 0
        || strcmp(infile_ext_name, ".tiff") == 0 || strcmp(infile_ext_name, ".TIFF") == 0) {
      is_tiff = true;
    }
#endif

    if (!is_pnm_pgx && !is_tiff) {
      printf("ERROR: Unsupported output file type.\n");
      exit(EXIT_FAILURE);
    }
    if (is_pnm_pgx) {
      if (read_pnmpgx(fname, c)) {
        throw std::exception();
      }
      c++;
    } else if (is_tiff) {
#if defined(OPENHTJ2K_TIFF_SUPPORT)
      if (read_tiff(fname)) {
        throw std::exception();
      }
      c++;
#endif
    }
  }
}

int image::read_pnmpgx(const std::string &filename, const uint16_t nc) {
  constexpr int READ_WIDTH  = 0;
  constexpr int READ_HEIGHT = 1;
  constexpr int READ_MAXVAL = 2;
  constexpr int DONE        = 3;
  constexpr char SP         = ' ';
  constexpr char LF         = '\n';
  constexpr char CR         = 0x0d;
  bool isASCII              = false;
  bool isBigendian          = false;
  bool isSigned             = false;

  FILE *fp = fopen(filename.c_str(), "rb");
  if (fp == nullptr) {
    printf("ERROR: File %s is not found.\n", filename.c_str());
    return EXIT_FAILURE;
  }
  int status = READ_WIDTH;
  int d;
  uint32_t val = 0;
  char comment[256];
  d = fgetc(fp);
  if (d != 'P') {
    printf("ERROR: %s is not a PNM/PGX file.\n", filename.c_str());
    fclose(fp);
    return EXIT_FAILURE;
  }

  bool isPPM = false;
  bool isPGX = false;
  uint32_t compw, comph;
  uint8_t bitDepth;

  d = fgetc(fp);
  switch (d) {
    // PGM
    case '2':
      isASCII = true;
      /* FALLTHRU */
    case '5':
      isBigendian = true;
      break;
      // PPM
    case '3':
      isASCII = true;
      /* FALLTHRU */
    case '6':
      isPPM = true;
      // number of components shall be three here
      num_components = 3;
      isBigendian    = true;
      break;
      // PGX
    case 'G':
      isPGX = true;
      // read endian
      do {
        d = fgetc(fp);
      } while (d != 'M' && d != 'L');
      switch (d) {
        case 'M':
          isBigendian = true;
          d           = fgetc(fp);
          if (d != 'L') {
            printf("ERROR: input PGX file %s is broken.\n", filename.c_str());
          }
          break;
        case 'L':
          d = fgetc(fp);
          if (d != 'M') {
            printf("ERROR: input PGX file %s is broken.\n", filename.c_str());
          }
          break;
        default:
          printf("ERROR: input file does not conform to PGX format.\n");
          return EXIT_FAILURE;
      }
      // check signed or not
      do {
        d = fgetc(fp);
      } while (d != '+' && d != '-' && isdigit(d) == false);
      if (d == '+' || d == '-') {
        if (d == '-') {
          isSigned = true;
        }
        do {
          d = fgetc(fp);
        } while (isdigit(d) == false);
      }
      do {
        val *= 10;
        val += static_cast<unsigned int>(d - '0');
        d = fgetc(fp);
      } while (d != SP && d != LF && d != CR);
      bitDepth = static_cast<uint8_t>(val);
      val      = 0;
      break;
      // PBM (not supported)
    case '1':
    case '4':
      printf("ERROR: PBM file is not supported.\n");
      fclose(fp);
      return EXIT_FAILURE;
      break;
      // error
    default:
      printf("ERROR: %s is not a PNM/PGX file.\n", filename.c_str());
      fclose(fp);
      return EXIT_FAILURE;
      break;
  }
  while (status != DONE) {
    d = fgetc(fp);
    // eat white/LF/CR and comments
    while (d == SP || d == LF || d == CR) {
      d = fgetc(fp);
      if (d == '#') {
        char *nouse = fgets(comment, sizeof(comment), fp);
        if (nouse == nullptr) {
          printf("comment read error\n");
          exit(EXIT_FAILURE);
        }
        d = fgetc(fp);
      }
    }
    // read numerical value
    while (d != SP && d != LF && d != CR) {
      val *= 10;
      val += static_cast<unsigned int>(d - '0');
      d = fgetc(fp);
    }
    // update status
    switch (status) {
      case READ_WIDTH:
        compw       = val;
        this->width = (this->width < compw) ? compw : this->width;
        val         = 0;
        status      = READ_HEIGHT;
        break;
      case READ_HEIGHT:
        comph        = val;
        this->height = (this->height < comph) ? comph : this->height;
        val          = 0;
        if (isPGX) {
          status = DONE;
        } else {
          status = READ_MAXVAL;
        }
        break;
      case READ_MAXVAL:
        bitDepth = static_cast<uint8_t>(log2(static_cast<float>(val)) + 1.0f);
        val      = 0;
        status   = DONE;
        break;
      default:
        break;
    }
  }
  uint16_t num_iterations = 1;
  if (isPPM) {
    num_iterations = 3;
  }
  // setting bit-depth for components
  for (int i = 0; i < num_iterations; ++i) {
    this->component_width.push_back(compw);
    this->component_height.push_back(comph);
    this->bits_per_pixel.push_back(bitDepth);
    this->is_signed.push_back(isSigned);
  }

  // easting trailing spaces/LF/CR or comments
  d = fgetc(fp);
  while (d == SP || d == LF || d == CR) {
    d = fgetc(fp);
    if (d == '#') {
      char *nouse = fgets(comment, sizeof(comment), fp);
      if (nouse == nullptr) {
        printf("comment read error\n");
        exit(EXIT_FAILURE);
      }
      d = fgetc(fp);
    }
  }
  fseek(fp, -1, SEEK_CUR);

  const uint32_t byte_per_sample      = (bitDepth + 8U - 1U) / 8U;
  const uint32_t component_gap        = num_iterations * byte_per_sample;
  const uint32_t line_width           = component_gap * compw;
  std::unique_ptr<uint8_t[]> line_buf = MAKE_UNIQUE<uint8_t[]>(line_width);

  // allocate memory once
  if (this->buf == nullptr) {
    this->buf = MAKE_UNIQUE<std::unique_ptr<int32_t[]>[]>(this->num_components);
  }
  if (isPPM) {
    for (size_t i = 0; i < this->num_components; ++i) {
      this->buf[i] = MAKE_UNIQUE<int32_t[]>(static_cast<size_t>(compw) * comph);
    }
  } else {
    this->buf[nc] = MAKE_UNIQUE<int32_t[]>(static_cast<size_t>(compw) * comph);
  }

  if (!isASCII) {
    for (size_t i = 0; i < comph; ++i) {
      if (fread(line_buf.get(), sizeof(uint8_t), line_width, fp) < line_width) {
        printf("ERROR: not enough samples in the given pnm file.\n");
        fclose(fp);
        return EXIT_FAILURE;
      }

      for (size_t c = 0; c < num_iterations; ++c) {
        uint8_t *src;
        int32_t *dst;
        src = &line_buf[c * byte_per_sample];
        if (isPPM) {
          dst = &this->buf[c][i * compw];
        } else {
          dst = &this->buf[nc][i * compw];
        }
        switch (byte_per_sample) {
          case 1:
            for (size_t j = 0; j < compw; ++j) {
              *dst = (isSigned) ? static_cast<int8_t>(*src) : *src;
              dst++;
              src += component_gap;
            }
            break;
          case 2:
            for (size_t j = 0; j < compw; ++j) {
              if (isSigned) {
                if (isBigendian) {
                  *dst = static_cast<int_least16_t>((static_cast<uint_least16_t>(src[0]) << 8)
                                                    | static_cast<uint_least16_t>(src[1]));
                } else {
                  *dst = static_cast<int_least16_t>(static_cast<uint_least16_t>(src[0])
                                                    | (static_cast<uint_least16_t>(src[1]) << 8));
                }
              } else {
                if (isBigendian) {
                  *dst = (src[0] << 8) | src[1];
                } else {
                  *dst = src[0] | (src[1] << 8);
                }
              }
              dst++;
              src += component_gap;
            }
            break;
          default:
            printf("ERROR: bit-depth over 16 is not supported.\n");
            fclose(fp);
            return EXIT_FAILURE;
            break;
        }
      }
    }
  } else {
    for (size_t i = 0; i < static_cast<size_t>(compw) * comph; ++i) {
      for (size_t c = 0; c < num_iterations; ++c) {
        val = 0;
        d   = fgetc(fp);
        while (d != SP && d != CR && d != LF && d != EOF) {
          val *= 10;
          val += static_cast<unsigned int>(d - '0');
          d = fgetc(fp);
        }
        this->buf[c][i] = static_cast<int>(val);
      }
    }
  }
  fclose(fp);
  return EXIT_SUCCESS;
}

#if defined(OPENHTJ2K_TIFF_SUPPORT)
int image::read_tiff(const std::string &filename) {
  TIFF *tiff_handle;
  size_t bytes_per_line;
  uint16_t planar_configuration;
  if ((tiff_handle = TIFFOpen(filename.c_str(), "r")) == nullptr) {
    printf("ERROR: File %s is not found.\n", filename.c_str());
    return EXIT_FAILURE;
  }

  uint32_t tiff_width  = 0;
  uint32_t tiff_height = 0;
  TIFFGetField(tiff_handle, TIFFTAG_IMAGEWIDTH, &tiff_width);
  TIFFGetField(tiff_handle, TIFFTAG_IMAGELENGTH, &tiff_height);

  uint16_t tiff_bits_per_sample   = 0;
  uint16_t tiff_samples_per_pixel = 0;
  TIFFGetField(tiff_handle, TIFFTAG_BITSPERSAMPLE, &tiff_bits_per_sample);
  TIFFGetField(tiff_handle, TIFFTAG_SAMPLESPERPIXEL, &tiff_samples_per_pixel);
  // some TIFs have tiff_samples_per_pixel=0 when it is a single channel
  // image - set to 1
  tiff_samples_per_pixel = (tiff_samples_per_pixel < 1) ? 1 : tiff_samples_per_pixel;

  uint16_t tiff_planar_configuration = 0;
  uint16_t tiff_photometric          = 0;
  TIFFGetField(tiff_handle, TIFFTAG_PLANARCONFIG, &tiff_planar_configuration);
  TIFFGetField(tiff_handle, TIFFTAG_PHOTOMETRIC, &tiff_photometric);

  planar_configuration = tiff_planar_configuration;

  uint16_t tiff_compression    = 0;
  uint32_t tiff_rows_per_strip = 0;
  TIFFGetField(tiff_handle, TIFFTAG_COMPRESSION, &tiff_compression);
  TIFFGetField(tiff_handle, TIFFTAG_ROWSPERSTRIP, &tiff_rows_per_strip);

  if (tiff_planar_configuration == PLANARCONFIG_SEPARATE) {
    bytes_per_line = tiff_samples_per_pixel * TIFFScanlineSize64(tiff_handle);
  } else {
    bytes_per_line = TIFFScanlineSize64(tiff_handle);
  }
  // Error on known incompatilbe input formats
  if (tiff_bits_per_sample != 8 && tiff_bits_per_sample != 16) {
    printf(
        "ERROR: TIFF IO is currently limited to file limited"
        " to files with TIFFTAG_BITSPERSAMPLE=8 and TIFFTAG_BITSPERSAMPLE=16 \n"
        "input file = %s has TIFFTAG_BITSPERSAMPLE=%d\n",
        filename.c_str(), tiff_bits_per_sample);
    TIFFClose(tiff_handle);
    return EXIT_FAILURE;
  }
  if (TIFFIsTiled(tiff_handle)) {
    printf(
        "ERROR: TIFF IO is currently limited to TIF files"
        "without tiles. \nInput file %s has been detected as tiled\n",
        filename.c_str());
    TIFFClose(tiff_handle);
    return EXIT_FAILURE;
  }
  if (PHOTOMETRIC_RGB != tiff_photometric && PHOTOMETRIC_MINISBLACK != tiff_photometric) {
    printf(
        "ERROR: TIFF IO is currently limited to "
        "TIFFTAG_PHOTOMETRIC=PHOTOMETRIC_MINISBLACK=%d and "
        "PHOTOMETRIC_RGB=%d. \nInput file %s has been detected "
        "TIFFTAG_PHOTOMETRIC=%d\n",
        PHOTOMETRIC_MINISBLACK, PHOTOMETRIC_RGB, filename.c_str(), tiff_photometric);
    TIFFClose(tiff_handle);
    return EXIT_FAILURE;
  }
  this->num_components    = tiff_samples_per_pixel;
  uint16_t num_iterations = this->num_components;
  // setting bit-depth for components
  this->width  = tiff_width;
  this->height = tiff_height;
  for (int i = 0; i < num_iterations; ++i) {
    this->component_width.push_back(width);
    this->component_height.push_back(height);
    this->bits_per_pixel.push_back(static_cast<uint8_t>(tiff_bits_per_sample));
    this->is_signed.push_back(false);
  }

  const uint32_t byte_per_sample = (tiff_bits_per_sample + 8U - 1U) / 8U;
  const uint32_t component_gap   = num_iterations * byte_per_sample;
  // const uint32_t line_width = component_gap * tiff_width;
  std::unique_ptr<uint8_t[]> line_buf = MAKE_UNIQUE<uint8_t[]>(bytes_per_line);

  // allocate memory once
  if (this->buf == nullptr) {
    this->buf = MAKE_UNIQUE<std::unique_ptr<int32_t[]>[]>(this->num_components);
  }
  for (size_t i = 0; i < this->num_components; ++i) {
    this->buf[i] =
        MAKE_UNIQUE<int32_t[]>(static_cast<size_t>(this->component_width[i]) * this->component_height[i]);
  }

  if (PLANARCONFIG_SEPARATE == planar_configuration) {
    // PLANAR read
  } else if (PLANARCONFIG_CONTIG == planar_configuration) {
    // NORMAL read
    for (uint32_t i = 0; i < tiff_height; ++i) {
      if (TIFFReadScanline(tiff_handle, line_buf.get(), i) < 0) {
        printf("ERROR: not enough samples in the given pnm file.\n");
        TIFFClose(tiff_handle);
        return EXIT_FAILURE;
      }

      for (size_t c = 0; c < num_iterations; ++c) {
        uint8_t *src;
        int32_t *dst;
        src = &line_buf[c * byte_per_sample];
        dst = &this->buf[c][static_cast<size_t>(i) * tiff_width];

        switch (byte_per_sample) {
          case 1:
            for (size_t j = 0; j < tiff_width; ++j) {
              *dst = *src;
              dst++;
              src += component_gap;
            }
            break;
          case 2:
            for (size_t j = 0; j < tiff_width; ++j) {
              // suppose little-endian
              *dst = src[0] | (src[1] << 8);
              // *dst >>= 16 - tiff_bits_per_sample; // for future use
              dst++;
              src += component_gap;
            }
            break;
          default:
            printf("ERROR: bit-depth over 16 is not supported.\n");
            TIFFClose(tiff_handle);
            return EXIT_FAILURE;
            break;
        }
      }
    }
  }
  return EXIT_SUCCESS;
}
#endif

// openhtj2k_encoder_impl shall not be public
class openhtj2k_encoder_impl {
 private:
  std::string outfile;
  const std::vector<int32_t *> *buf;
  std::vector<uint8_t> *outbuf;
  siz_params *siz;
  cod_params *cod;
  qcd_params *qcd;
  uint8_t qfactor;
  bool isJPH;
  uint8_t color_space;

 public:
  openhtj2k_encoder_impl(const char *, const std::vector<int32_t *> &, siz_params &, cod_params &,
                         qcd_params &, uint8_t, bool, uint8_t);
  void set_output_buffer(std::vector<uint8_t> &);
  ~openhtj2k_encoder_impl();
  size_t invoke();
};

openhtj2k_encoder_impl::openhtj2k_encoder_impl(const char *filename,
                                               const std::vector<int32_t *> &input_buf, siz_params &s,
                                               cod_params &c, qcd_params &q, uint8_t qf, bool flag,
                                               uint8_t cs)
    : buf(&input_buf), siz(&s), cod(&c), qcd(&q), qfactor(qf), isJPH(flag), color_space(cs) {
  this->outfile = filename;
  this->outbuf  = nullptr;
}

void openhtj2k_encoder_impl::set_output_buffer(std::vector<uint8_t> &output_buf) {
  this->outbuf = &output_buf;
}

openhtj2k_encoder_impl::~openhtj2k_encoder_impl() = default;

size_t openhtj2k_encoder_impl::invoke() {
  std::vector<uint8_t> Ssiz;
  std::vector<uint8_t> XRsiz, YRsiz;

  if ((siz->XOsiz > siz->Xsiz) || (siz->YOsiz > siz->Ysiz)) {
    printf("ERROR: image origin exceeds the size of input image.\n");
    throw std::exception();
  }
  if ((siz->XTOsiz > siz->XOsiz) || (siz->YTOsiz > siz->YOsiz)) {
    printf("ERROR: tile origin shall be no greater than the image origin.\n");
    throw std::exception();
  }
  if (siz->XTsiz * siz->YTsiz == 0) {
    siz->XTsiz = siz->Xsiz - siz->XOsiz;
    siz->YTsiz = siz->Ysiz - siz->YOsiz;
  }
  if (((siz->XTOsiz + siz->XTsiz) <= siz->XOsiz) || ((siz->YTOsiz + siz->YTsiz) <= siz->YOsiz)) {
    printf("ERROR: tile size plus tile origin shall be greater than the image origin.\n");
    throw std::exception();
  }

  for (size_t c = 0; c < siz->Csiz; ++c) {
    Ssiz.push_back(siz->Ssiz[c]);
    XRsiz.push_back(siz->XRsiz[c]);
    YRsiz.push_back(siz->YRsiz[c]);
  }

  // check component size
  if (siz->Csiz == 3 && cod->use_color_trafo == 1 && (XRsiz[0] != XRsiz[1] || XRsiz[1] != XRsiz[2])
      && (YRsiz[0] != YRsiz[1] || YRsiz[1] != YRsiz[2])) {
    cod->use_color_trafo = 0;
    printf("WARNING: Cycc is set to 'no' because size of each component is not identical.\n");
  }

  // check number of components
  if (siz->Csiz < 3 && cod->use_color_trafo == 1) {
    cod->use_color_trafo = 0;
    printf("WARNING: Cycc is set to 'no' because the number of components is not equal to 3.\n");
  }
  // force RGB->YCbCr when Qfactor feature is enabled
  if (qfactor != NO_QFACTOR) {
    if (siz->Csiz == 3) {
      if (cod->use_color_trafo == 0) {
        printf("WARNING: Color conversion is OFF while Qfactor feature is enabled.\n");
        printf("         It is OK if the inputs are in YCbCr color space.\n");
      }
    } else if (siz->Csiz != 1) {
      printf("WARNING: Qfactor is designed for only gray-scale or RGB or YCbCr input.\n");
    }
  }

  // create required marker segments
  SIZ_marker main_SIZ(siz->Rsiz, siz->Xsiz, siz->Ysiz, siz->XOsiz, siz->YOsiz, siz->XTsiz, siz->YTsiz,
                      siz->XTOsiz, siz->YTOsiz, siz->Csiz, Ssiz, XRsiz, YRsiz, true);
  COD_marker main_COD(cod->is_max_precincts, cod->use_SOP, cod->use_EPH, cod->progression_order,
                      cod->number_of_layers, cod->use_color_trafo, cod->dwt_levels,
                      static_cast<uint8_t>(cod->blkwidth), static_cast<uint8_t>(cod->blkheight),
                      cod->codeblock_style, cod->transformation, cod->PPx, cod->PPy);
  QCD_marker main_QCD(qcd->number_of_guardbits, cod->dwt_levels, cod->transformation, qcd->is_derived,
                      static_cast<uint8_t>(Ssiz[0] + 1U), cod->use_color_trafo, qcd->base_step, qfactor);
  // parameters for CAP marker
  uint16_t bits14_15 = 0;                     // 0: HTONLY, 2: HTDECLARED, 3: MIXED
  uint16_t bit13     = 0;                     // 0: SINGLEHT, 1: MULTIHT
  uint16_t bit12     = 0;                     // 0: RGNFREE, 1: RGN
  uint16_t bit11     = 0;                     // 0: HOMOGENEOUS, 1: HETEROGENEOUS
  uint16_t bit5      = !cod->transformation;  // 0: HTREV, 1: HTIRV
  uint16_t bits0_4;
  uint8_t MAGB = main_QCD.get_MAGB();
  if (MAGB < 27) {
    bits0_4 = static_cast<uint16_t>((MAGB > 8) ? MAGB - 8 : 0);
  } else if (MAGB <= 71) {
    bits0_4 = static_cast<uint16_t>((MAGB - 27) / 4 + 19);
  } else {
    bits0_4 = 31;
  }
  auto Ccap15 = static_cast<uint16_t>((bits14_15 << 14) + (bit13 << 13) + (bit12 << 12) + (bit11 << 11)
                                      + (bit5 << 5) + bits0_4);
  CAP_marker main_CAP;
  main_CAP.set_Ccap(Ccap15, 15);

  // create main header
  j2k_main_header main_header(&main_SIZ, &main_COD, &main_QCD, &main_CAP, qfactor);
  COM_marker main_COM("OpenHTJ2K version 0", true);
  main_header.add_COM_marker(main_COM);

  j2c_dst_memory j2c_dst, jph_dst;
  j2c_dst.put_word(_SOC);
  main_header.flush(j2c_dst);

  element_siz numTiles;
  main_header.get_number_of_tiles(numTiles.x, numTiles.y);
  if (numTiles.x * numTiles.y > 65535) {
    printf("ERROR: The number of tiles exceeds its allowable maximum (65535).\n");
    throw std::exception();
  }

  auto tileSet = MAKE_UNIQUE<j2k_tile[]>(static_cast<size_t>(numTiles.x) * numTiles.y);
  for (uint16_t i = 0; i < static_cast<uint16_t>(numTiles.x * numTiles.y); ++i) {
    tileSet[i].enc_init(i, main_header, *buf);
  }
  for (uint32_t i = 0; i < numTiles.x * numTiles.y; ++i) {
    tileSet[i].perform_dc_offset(main_header);
    tileSet[i].rgb_to_ycbcr();
    tileSet[i].encode();
    tileSet[i].construct_packets(main_header);
  }
  for (uint32_t i = 0; i < numTiles.x * numTiles.y; ++i) {
    tileSet[i].write_packets(j2c_dst);
  }
  j2c_dst.put_word(_EOC);
  size_t codestream_size = j2c_dst.get_length();

  // prepare jph box-based format, if necessary
  if (isJPH) {
    bool isSRGB = (color_space == static_cast<uint8_t>(sRGB));
    jph_boxes jph_info(main_header, 1, isSRGB, codestream_size);
    size_t file_format_size = jph_info.write(jph_dst);
    codestream_size += file_format_size - codestream_size;
  }
  if (outbuf != nullptr) {
    if (isJPH) {
      if (jph_dst.flush(outbuf)) {
        printf("illegal attempt to flush empty buffer.\n");
        throw std::exception();
      }
    }
    if (j2c_dst.flush(outbuf)) {
      printf("illegal attempt to flush empty buffer.\n");
      throw std::exception();
    }
  } else {
    std::ofstream dst;
    dst.open(this->outfile, std::ios::out | std::ios::binary);
    if (isJPH) {
      jph_dst.flush(dst);
    }
    j2c_dst.flush(dst);
    dst.close();
  }
  return codestream_size;
}

// public interface
openhtj2k_encoder::openhtj2k_encoder(const char *fname, const std::vector<int32_t *> &input_buf,
                                     siz_params &siz, cod_params &cod, qcd_params &qcd, uint8_t qfactor,
                                     bool isJPH, uint8_t color_space, uint32_t num_threads) {
  if (qfactor != NO_QFACTOR) {
    if (qfactor > 100) {
      printf("Value of Qfactor shall be in the range [0, 100]\n");
      throw std::exception();
    }
  }
#ifdef OPENHTJ2K_THREAD
  ThreadPool::instance(num_threads);
#endif
  this->impl =
      MAKE_UNIQUE<openhtj2k_encoder_impl>(fname, input_buf, siz, cod, qcd, qfactor, isJPH, color_space);
}

void openhtj2k_encoder::set_output_buffer(std::vector<uint8_t> &output_buf) {
  this->impl->set_output_buffer(output_buf);
}

size_t openhtj2k_encoder::invoke() { return this->impl->invoke(); }

openhtj2k_encoder::~openhtj2k_encoder() {
#ifdef OPENHTJ2K_THREAD
  ThreadPool::release();
#endif
}
}  // namespace open_htj2k
