/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_PLUGINS_CODECS_NDLZ_NDLZ_H
#define BLOSC_PLUGINS_CODECS_NDLZ_NDLZ_H

#include "blosc2.h"

#include <stdint.h>

int ndlz_compress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                  uint8_t meta, blosc2_cparams *cparams, const void* chunk);

int ndlz_decompress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                    uint8_t meta, blosc2_dparams *dparams, const void* chunk);

#endif /* BLOSC_PLUGINS_CODECS_NDLZ_NDLZ_H */
