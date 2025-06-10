/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* AVX2-accelerated shuffle/unshuffle routines. */

#ifndef SHUFFLE_AVX2_H
#define SHUFFLE_AVX2_H

#include "blosc2/blosc2-common.h"

#include <stdint.h>
#include <stdbool.h>

/**
 * AVX2-accelerated (un)shuffle routines availability.
*/
extern const bool is_shuffle_avx2;

/**
  AVX2-accelerated shuffle routine.
*/
BLOSC_NO_EXPORT void shuffle_avx2(const int32_t bytesoftype, const int32_t blocksize,
                                  const uint8_t *_src, uint8_t *_dest);

/**
  AVX2-accelerated unshuffle routine.
*/
BLOSC_NO_EXPORT void unshuffle_avx2(const int32_t bytesoftype, const int32_t blocksize,
                                    const uint8_t *_src, uint8_t *_dest);

#endif /* SHUFFLE_AVX2_H */
