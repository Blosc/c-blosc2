/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/



#ifndef NDLZ_H
#define NDLZ_H
#include "context.h"

#if defined (__cplusplus)
extern "C" {
#endif

int ndlz_compress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                   uint8_t meta, blosc2_cparams *cparams);

int ndlz_decompress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                     uint8_t meta, blosc2_dparams *dparams);

#if defined (__cplusplus)
}
#endif

#endif /* NDLZ_H */
