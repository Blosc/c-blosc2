#ifndef WRAPPER_H
#define WRAPPER_H

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

#include <blosc2.h>
#include <b2nd.h>

typedef struct {
  uint32_t width;
  uint32_t height;
  uint8_t depth; // default 8
  bool sign;     // default 0
  uint8_t ssiz;  // component's bit depth and sign (combines the 2 above)
} component_t;

typedef struct {
    uint8_t *buffer;
    int32_t buffer_len;
    uint32_t width;
    uint32_t height;
    uint8_t max_bpp;
    uint16_t num_components;
    component_t components[3];
} image_t;

int htj2k_read_image(
    image_t *image,
    const char *filename
);

void htj2k_free_image(
    image_t *image
);

int htj2k_encoder(
    const uint8_t* input,
    int32_t input_len,
    uint8_t* output,
    int32_t output_len,
    uint8_t meta,
    blosc2_cparams* cparams,
    const void* chunk
);

int htj2k_decoder(
    const uint8_t *input,
    int32_t input_len,
    uint8_t *output,
    int32_t output_len,
    uint8_t meta,
    blosc2_dparams *dparams,
    const void* chunk
);

int htj2k_write_ppm(
    uint8_t *input,
    int64_t input_len,
    image_t *image,
    char *filename
);

#ifdef __cplusplus
}
#endif

#endif
