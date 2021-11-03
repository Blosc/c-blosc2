/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/



#ifndef BLOSC2_ZFP_H
#define BLOSC2_ZFP_H
#include "context.h"

#if defined (__cplusplus)
extern "C" {
#endif

int blosc2_zfp_compress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                   uint8_t meta, blosc2_cparams *cparams);

int blosc2_zfp_decompress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                     uint8_t meta, blosc2_dparams *dparams);

#if defined (__cplusplus)
}
#endif

#endif /* BLOSC2_ZFP_H */
