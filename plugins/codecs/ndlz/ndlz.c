/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/*********************************************************************
  This codec is meant to leverage multidimensionality for getting
  better compression ratios.  The idea is to look for similarities
  in places that are closer in a euclidean metric, not the typical
  linear one.
**********************************************************************/

#include "ndlz-private.h"
#include "ndlz4x4.h"
#include "ndlz8x8.h"
#include "ndlz.h"

int ndlz_compress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                  uint8_t meta, blosc2_cparams *cparams, const void *chunk) {
  NDLZ_ERROR_NULL(input);
  NDLZ_ERROR_NULL(output);
  NDLZ_ERROR_NULL(cparams);
  BLOSC_UNUSED_PARAM(chunk);

  switch (meta) {
    case 4:
      return ndlz4_compress(input, input_len, output, output_len, meta, cparams);
    case 8:
      return ndlz8_compress(input, input_len, output, output_len, meta, cparams);
    default:
      BLOSC_TRACE_ERROR("NDLZ is not available for this cellsize: %d", meta);
  }
  return BLOSC2_ERROR_FAILURE;
}

int ndlz_decompress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                    uint8_t meta, blosc2_dparams *dparams, const void *chunk) {
  NDLZ_ERROR_NULL(input);
  NDLZ_ERROR_NULL(output);
  NDLZ_ERROR_NULL(dparams);
  BLOSC_UNUSED_PARAM(chunk);

  switch (meta) {
    case 4:
      return ndlz4_decompress(input, input_len, output, output_len, meta, dparams);
    case 8:
      return ndlz8_decompress(input, input_len, output, output_len, meta, dparams);
    default:
      BLOSC_TRACE_ERROR("NDLZ is not available for this cellsize: %d", meta);
  }
  return BLOSC2_ERROR_FAILURE;
}
