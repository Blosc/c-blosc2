/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_PLUGINS_CODECS_ZFP_BLOSC2_ZFP_H
#define BLOSC_PLUGINS_CODECS_ZFP_BLOSC2_ZFP_H

#include "blosc2.h"

#include <stdint.h>

int zfp_acc_compress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                     uint8_t meta, blosc2_cparams *cparams, const void *chunk);

int zfp_acc_decompress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                       uint8_t meta, blosc2_dparams *dparams, const void *chunk);

int zfp_prec_compress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                      uint8_t meta, blosc2_cparams *cparams, const void *chunk);

int zfp_prec_decompress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                        uint8_t meta, blosc2_dparams *dparams, const void *chunk);

int zfp_rate_compress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                      uint8_t meta, blosc2_cparams *cparams, const void *chunk);

int zfp_rate_decompress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                        uint8_t meta, blosc2_dparams *dparams, const void *chunk);

int zfp_getcell(void *thread_context, const uint8_t *block, int32_t cbytes, uint8_t *dest, int32_t destsize);

#endif /* BLOSC_PLUGINS_CODECS_ZFP_BLOSC2_ZFP_H */
