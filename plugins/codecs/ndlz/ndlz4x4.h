/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_PLUGINS_CODECS_NDLZ_NDLZ4X4_H
#define BLOSC_PLUGINS_CODECS_NDLZ_NDLZ4X4_H

#include "ndlz-private.h"
#include "ndlz.h"
#include "blosc2.h"

/*
#include <stdio.h>
#include "blosc2/blosc2-common.h"
#include "fastcopy.h"
*/

/**
  Compress a block of data in the input buffer and returns the size of
  compressed block. The size of input buffer is specified by
  length. The minimum input buffer size is 16.

  The output buffer must be at least 5% larger than the input buffer
  and can not be smaller than 66 bytes.

  If the input is not compressible, or output does not fit in maxout
  bytes, the return value will be 0 and you will have to discard the
  output buffer.

  The acceleration parameter is related with the frequency for
  updating the internal hash.  An acceleration of 1 means that the
  internal hash is updated at full rate.  A value < 1 is not allowed
  and will be silently set to 1.

  The input buffer and the output buffer can not overlap.
*/

int ndlz4_compress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                  uint8_t meta, blosc2_cparams *cparams);

/**
  Decompress a block of compressed data and returns the size of the
  decompressed block. If error occurs, e.g. the compressed data is
  corrupted or the output buffer is not large enough, then 0 (zero)
  will be returned instead.

  The input buffer and the output buffer can not overlap.

  Decompression is memory safe and guaranteed not to write the output buffer
  more than what is specified in maxout.
 */

int ndlz4_decompress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                     uint8_t meta, blosc2_dparams *dparams);

#endif /* BLOSC_PLUGINS_CODECS_NDLZ_NDLZ4X4_H */
