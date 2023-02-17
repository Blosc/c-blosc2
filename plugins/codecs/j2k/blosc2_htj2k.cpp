/*

typedef int (*blosc2_codec_encoder_cb)(
     const uint8_t *input,
     int32_t input_len,
     uint8_t *output,
     int32_t output_len,
     uint8_t meta,
     blosc2_cparams *cparams,
     const void *chunk)

typedef int (*blosc2_codec_decoder_cb)(
     const uint8_t *input,
     int32_t input_len,
     uint8_t *output,
     int32_t output_len,
     uint8_t meta,
     blosc2_dparams *dparams,
     const void *chunk)

*/

#include <cstring>
#include <encoder.hpp>
#include <decoder.hpp>
#include <dec_utils.hpp>
#include "blosc2_htj2k.h"


#define NO_QFACTOR 0xFF

//#define JFNAME "output/teapot.jphc"
#define JFNAME "output/teapot.j2c"

int htj2k_read_image(image_t *image, const char *filename)
{
    // Parse image
    std::vector<std::string> filenames = {filename};
    open_htj2k::image img(filenames);

    // Load general information
    image->num_components = img.get_num_components();
    image->width = img.get_width();
    image->height = img.get_height();
    image->max_bpp = img.get_max_bpp();

    // Load component information
    image->buffer_len = 0;
    for (uint16_t c = 0; c < image->num_components; ++c) {
        uint32_t width = img.get_component_width(c);
        uint32_t height = img.get_component_height(c);
        uint32_t size = width * height * sizeof(int32_t);
        uint8_t ssiz = img.get_Ssiz_value(c);
        image->components[c].width = width;
        image->components[c].height = height;
        image->components[c].ssiz = ssiz;
        image->components[c].depth = (ssiz & 0x7F) + 1;
        image->components[c].sign = ssiz >> 7;
        image->buffer_len += size;
    }

    // Copy data to contiguous array
    image->buffer = (uint8_t*)malloc(image->buffer_len);
    uint8_t *dest = image->buffer;
    for (uint16_t c = 0; c < image->num_components; ++c) {
        uint32_t width = image->components[c].width;
        uint32_t height = image->components[c].height;
        uint32_t size = width * height * sizeof(int32_t);
        memcpy(dest, img.get_buf(c), size);
        dest += size;
    }

    return 0;
}

void htj2k_free_image(image_t *image)
{
    free(image->buffer);
    image->buffer = NULL;
}

int htj2k_encoder(
    const uint8_t *input,
    int32_t input_len,
    uint8_t *output,
    int32_t output_len,
    uint8_t meta,
    blosc2_cparams* cparams,
    const void* chunk
)
{
    uint8_t *content;
    int32_t content_len;
    int error = blosc2_meta_get((blosc2_schunk*)cparams->schunk, "b2nd", &content, &content_len);

    int8_t ndim;
    int64_t shape[3];
    int32_t chunkshape[3];
    int32_t blockshape[3];
    char *dtype;
    int8_t dtype_format;
    error = b2nd_deserialize_meta(content, content_len, &ndim, shape, chunkshape, blockshape, &dtype, &dtype_format);
    free(content);
    free(dtype);

    image_t tmp;
    tmp.num_components = blockshape[0];
    tmp.width = blockshape[1];
    tmp.height= blockshape[2];
    tmp.max_bpp = 8;
    for (int i = 0; i < tmp.num_components; i++) {
        tmp.components[i].width = tmp.width;
        tmp.components[i].height = tmp.height;
        tmp.components[i].depth = tmp.max_bpp;
        tmp.components[i].sign = 0;
        tmp.components[i].ssiz = tmp.max_bpp; // FIXME sign
    }

    image_t *image = &tmp;

    // Input variables
    const char *ofname = JFNAME;
    uint8_t qfactor = NO_QFACTOR; // 255
    bool isJPH = false;
    uint8_t color_space = 0;
    uint32_t num_threads = 1;
    int32_t num_iterations = 1;     // Number of iterations (1-INT32_MAX)

    // Input buffer
    const uint8_t *ptr = input;
    std::vector<int32_t *> input_buf;
    for (uint16_t c = 0; c < image->num_components; ++c) {
        input_buf.push_back((int32_t*)ptr);
        uint32_t width = image->components[c].width;
        uint32_t height = image->components[c].height;
        uint32_t size = width * height * sizeof(int32_t);
        ptr += size;
    }

    // Information of input image
    uint32_t img_orig_x = 0, img_orig_y = 0;
    open_htj2k::siz_params siz;
    siz.Rsiz   = 0;
    siz.Xsiz   = image->width + img_orig_x;
    siz.Ysiz   = image->height + img_orig_y;
    siz.XOsiz  = img_orig_x;
    siz.YOsiz  = img_orig_y;
    siz.XTsiz  = image->width;      // Tiles size (X) default to image size
    siz.YTsiz  = image->height;     // Tiles size (Y) default to image size
    siz.XTOsiz = 0;                 // Origin of first tile (X)
    siz.YTOsiz = 0;                 // Origin of first tile (Y)
    siz.Csiz   = image->num_components;
    for (uint16_t c = 0; c < siz.Csiz; ++c) {
        siz.Ssiz.push_back(image->components[c].ssiz);
        auto compw = image->components[c].width;
        auto comph = image->components[c].height;
        siz.XRsiz.push_back(static_cast<unsigned char>(((siz.Xsiz - siz.XOsiz) + compw - 1) / compw));
        siz.YRsiz.push_back(static_cast<unsigned char>(((siz.Ysiz - siz.YOsiz) + comph - 1) / comph));
    }

    // Parameters related to COD marker
    uint32_t cblk_width = 4, cblk_height = 4; // Must be power of 2 and >= 4
    open_htj2k::cod_params cod;
    cod.blkwidth                      = static_cast<uint16_t>(cblk_width);
    cod.blkheight                     = static_cast<uint16_t>(cblk_height);
    cod.is_max_precincts              = true;   // If false then precincts size must be defined
    cod.use_SOP                       = false;  // Use SOP (Start Of Packet) marker
    cod.use_EPH                       = false;  // Use EPH (End of Packet Header) marker
    cod.progression_order             = 0;      // 0:LRCP 1:RLCP 2:RPCL 3:PCRL 4:CPRL
    cod.number_of_layers              = 1;
    cod.use_color_trafo               = 1;      // Use RGB->YCbCr color space conversion (1 or 0)
    cod.dwt_levels                    = 5;      // Number of DWT decomposition (0-32)
    cod.codeblock_style               = 0x040;
    cod.transformation                = 1;      // 0:lossy 1:lossless

    // Parameters related to QCD marker
    open_htj2k::qcd_params qcd{};
    qcd.is_derived          = false;
    qcd.number_of_guardbits = 1;        // Number of guard bits (0-8)
    qcd.base_step           = 0.0;      // Base step size for quantization (0.0 - 2.0)
    if (qcd.base_step == 0.0) {
        qcd.base_step = 1.0f / static_cast<float>(1 << image->max_bpp);
    }

    // Encode
    std::vector<uint8_t> outbuf;
    for (int i = 0; i < num_iterations; ++i) {
        open_htj2k::openhtj2k_encoder encoder(
            ofname,                 // output filename
            input_buf,
            siz,                    // information of input image
            cod,                    // parameters related to COD marker
            qcd,                    // parameters related to QCD marker
            qfactor,                // quality factor (0-100 or 255)
            isJPH,
            color_space,            // 0: RGB or 1: YCC
            num_threads             // num_threads
        );

        encoder.set_output_buffer(outbuf);
        try {
            encoder.invoke(); // Returns size_t total_size
        } catch (std::exception &exc) {
            return EXIT_FAILURE;
        }
    }

    // Save file
    int size = outbuf.size();
    if (size > output_len) {
        return 0;
    }
    memcpy(output, outbuf.data(), size);

    return size;
}

int htj2k_decoder(
    const uint8_t *input,
    int32_t input_len,
    uint8_t *output,
    int32_t output_len,
    uint8_t meta,
    blosc2_dparams *dparams,
    const void* chunk
)
{
    // Input variables
    uint8_t reduce_NL = 0;          // Number of DWT resolution reduction (0-32)
    uint32_t num_threads = 1;
    int32_t num_iterations = 1;     // Number of iterations (1-INT32_MAX)

    // Decode
    std::vector<int32_t *> buf;
    std::vector<uint32_t> img_width;
    std::vector<uint32_t> img_height;
    std::vector<uint8_t> img_depth;
    std::vector<bool> img_signed;
    for (int i = 0; i < num_iterations; ++i) {
        // Create decoder
        open_htj2k::openhtj2k_decoder decoder(input, input_len, reduce_NL, num_threads);

        // Clear vectors
        for (auto &j : buf) {
            delete[] j;
        }
        buf.clear();
        img_width.clear();
        img_height.clear();
        img_depth.clear();
        img_signed.clear();

        // Invoke decoder
        try {
            decoder.invoke(buf, img_width, img_height, img_depth, img_signed);
        } catch (std::exception &exc) {
            return EXIT_FAILURE;
        }
    }

    // Transform to a C structure
    int num_components = buf.size();
    int buffer_len = 0;

//  image->buffer = NULL;
    for (uint16_t c = 0; c < num_components; c++) {
        uint32_t width = img_width[c];
        uint32_t height = img_height[c];
        uint32_t size = width * height * sizeof(int32_t);
        //uint32_t size = width * height * sizeof(int32_t);
//      image->components[c].width = width;
//      image->components[c].height = height;
//      image->components[c].depth = img_depth[c];
//      image->components[c].sign = img_signed[c];
        buffer_len += size;
    }

    if (buffer_len > output_len) {
        return 0;
    }

    // Copy data to contiguous array
    uint8_t *dest = output;
    for (uint16_t c = 0; c < num_components; c++) {
        uint32_t width = img_width[c];
        uint32_t height = img_height[c];
        uint32_t size = width * height * sizeof(int32_t);
        memcpy(dest, buf[c], size);
        dest += size;
    }

    return buffer_len;
}

int htj2k_write_ppm(
    uint8_t *input,
    int64_t input_len,
    image_t *image,
    char *filename
)
{
    std::vector<int32_t *> buf;
    std::vector<uint32_t> img_width;
    std::vector<uint32_t> img_height;
    std::vector<uint8_t> img_depth;
    std::vector<bool> img_signed;

    uint8_t *dest = input;
    for (uint16_t c = 0; c < image->num_components; c++) {
        uint32_t width = image->components[c].width;
        uint32_t height = image->components[c].height;
        uint32_t size = width * height * sizeof(int32_t);
        img_width.push_back(width);
        img_height.push_back(height);
        img_depth.push_back(image->components[c].depth);
        img_signed.push_back(image->components[c].sign);
        buf.push_back((int32_t*)dest);
        dest += size;
    }

    char *ext = strrchr(filename, '.');
    write_ppm(filename, ext, buf, img_width, img_height, img_depth, img_signed);

    return 0;
}
