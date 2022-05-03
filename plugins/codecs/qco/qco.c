#include <stdio.h>
#include "qco.h"

int q_compress_blosc(
  const uint8_t *input,
  int32_t input_len,
  uint8_t *output,
  int32_t output_len,
  uint8_t meta,
  blosc2_cparams *cparams,
  const void* chunk
) {
  return q_compress_ffi(
    *input,
    input_len,
    *output,
    output_len,
    meta,
    chunk
  );
}

int q_decompress_blosc(
  const uint8_t *input,
  int32_t input_len,
  uint8_t *output,
  int32_t output_len,
  uint8_t meta,
  blosc2_dparams *dparams,
  const void* chunk
) {
  return q_decompress_ffi(
    *input,
    input_len,
    *output,
    output_len,
    meta,
    chunk
  );
}

extern int q_compress_ffi(
  const uint8_t *input,
  int32_t input_len,
  uint8_t *output,
  int32_t output_len,
  uint8_t meta,
  const void* chunk
);

extern int q_decompress_ffi(
  const uint8_t *input,
  int32_t input_len,
  uint8_t *output,
  int32_t output_len,
  uint8_t meta,
  const void* chunk
);
