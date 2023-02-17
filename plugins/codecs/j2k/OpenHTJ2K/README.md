[![CMake](https://github.com/osamu620/OpenHTJ2K/actions/workflows/cmake.yml/badge.svg?branch=main)](https://github.com/osamu620/OpenHTJ2K/actions/workflows/cmake.yml)
[![CodeQL](https://github.com/osamu620/OpenHTJ2K/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/osamu620/OpenHTJ2K/actions/workflows/codeql-analysis.yml)
# OpenHTJ2K
OpenHTJ2K is an open source implementation of ITU-T Rec.814 | ISO/IEC 15444-15 (a.k.a. JPEG 2000 Part 15, High-Throughput JPEG 2000; HTJ2K)

# What OpenHTJ2K provides
OpenHTJ2K provides a shared liberary and sample applications having the following functionalities:
- Decoding of ITU-T Rec.800 | ISO/IEC 15444-1 (JPEG 2000 Part 1) or ITU-T Rec.814 | ISO/IEC 15444-15 (JPEG 2000 Part 15.) compliant codestreams
  - fully compliant with conformance testing defined in ITU-T Rec.803 | ISO 15444-4.
- Encoding an image into a codestream/JPH file which is compliant with HTJ2K
  - currently supports only HTJ2K. The optional markers like COC, POC, etc. are not implemented.
  - encoding of HT SigProp and MagRef passes are not implemented.
  - up to **12 bit** per component sample is currently supported. 
  - **Quality control for lossy compression with ***Qfactor*** feature** 

# Requirements
cmake (version 3.14 or later) and C++11 compliant compiler.

# Building
Type the following command. `./` is a root of cloned repository and `${BUILD_DIR}` is a build directory (for example, `../build` or `./build` and so on)

- You can also specify `-DCMAKE_BUILD_TYPE=Debug` or `-DCMAKE_BUILD_TYPE=RelWithDebInfo` to build with debug information.
- You can also specify `-G "Xcode"` to create a project for Xcode.
- You can also specify `-G "Visual Studio 17 2022"` to create a project for Visual Studio 2022. For the older versions,
see https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html#visual-studio-generators

```
cd ./
cmake -G "Unix Makefiles" -B ${BUILD_DIR} -DCMAKE_BUILD_TYPE=Release
cd  ${BUILD_DIR}
make
```

Then the executables should be found in `${BUILD_DIR}/bin` directory.

# Usage
## Encoder
Only Part 15 compliant encoding is supported. Both .j2c (codestream) and .jph (file format) are available. 
```bash
./open_htj2k_enc -i inputimage(in PNM/PGX/(TIFF, libtiff required) format) -o output [options...]
```
The encoder can take comma-separated multiple files. For example, components in YCbCr color space can be encoded by
```
./open_htj2k_enc -i inputY.pgx,inputCb.pgx,inputCr.pgx -o output 
```

### options
- `Stiles=Size`
  - Size of tile "{height, width}", Default is equal to the image size
- `Sorigin=Size`
  - origin of the input image in the reference grid, Default is {0,0} 
- `Stile_origin=Size`
  - origin of tiles in the reference grid, Default is {0,0}
- `Clevels=Int`
  - Valid range for number of DWT levels is from 0 to 32 (Default is **5**)
- `Creversible=yes or no`
  - `yes` for lossless mode, `no` for lossy mode, Default is **no**
- `Cblk=Size`
  - Code-block size, Default is **64x64**
- `Cprecincts=Size`
  - Precinct size
- `Cycc=yes or no`
  - `yes` to use RGB->YCbCr, Default is **yes**
- `Corder`
  - Progression order: LRCP, RLCP, RPCL, PCRL, CPRL, Default is **LRCP**
- `Cuse_sop=yes or no`
  - Default is **no** 
- `Cuse_eph=yes or no`
  - Default is **no**
- `Qstep=Float`
  - 0.0 < base step size <= 2.0
- `Qguard=Int`
  - 0 to 7 for the number of guard bits, Default is **1** 
- `Qderived=yes or no`
  - `yes` switches the quantyzation style to **derived** (Default is `no`)
- `Qfactor=Int`
  - 0 to 100 for the quality of the lossy compressed image
  - for YCbCr inputs, valid chroma subsampling formats are 4:4:4, 4:2:0, and 4:2:2
- `-jph_color_space`
  - Color space of input components: RGB, YCC
  - if inputs are represented in YCbCr, use YCC
- `-num_threads Int`
  - number of threads to use in encode or decode
  - 0, which is the default, indicates usage of all threads

## Decoder
The both Part 1 and Part 15 compliant decoding are supported.
```bash
./open_htj2k_dec -i codestream -o outputimage [-reduce n: number of DWT level reduction]
```
To see a help, use `-h` option.

## Supported file types
### Encoder
- input image formats: .pgm, .ppm, .pgx, .tif (libtiff required)
- output codestreams: .j2k, .j2c, .jphc (Part 15 codestream), .jph (Part 15 file format)
### Decoder
- input codestreams : .j2k, .j2c, .jphc
- output image formats: .raw, .ppm, .pgm, .pgx
