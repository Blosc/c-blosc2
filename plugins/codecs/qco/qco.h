
#ifndef QCO_H
#define QCO_H
#include "context.h"

#if defined (__cplusplus)
extern "C" {
#endif

int q_compress_blosc(
  const uint8_t *input,
  int32_t input_len,
  uint8_t *output,
  int32_t output_len,
  uint8_t meta,
  blosc2_cparams *cparams,
  const void* chunk
);

int q_decompress_blosc(
  const uint8_t *input,
  int32_t input_len,
  uint8_t *output,
  int32_t output_len,
  uint8_t meta,
  blosc2_dparams *dparams,
  const void* chunk
);

int q_compress_ffi(
  const uint8_t *input,
  int32_t input_len,
  uint8_t *output,
  int32_t output_len,
  uint8_t meta,
  const void* chunk
);

int q_decompress_ffi(
  const uint8_t *input,
  int32_t input_len,
  uint8_t *output,
  int32_t output_len,
  uint8_t meta,
  const void* chunk
);

#if defined (__cplusplus)
}
#endif

#endif
