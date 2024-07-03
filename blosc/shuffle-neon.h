/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Note: Adapted for NEON by Lucian Marc.

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* NEON-accelerated shuffle/unshuffle routines. */

#ifndef BLOSC_SHUFFLE_NEON_H
#define BLOSC_SHUFFLE_NEON_H

#include "blosc2/blosc2-common.h"

#include <stdint.h>
#include <stdbool.h>

/**
 * NEON-accelerated (un)shuffle routines availability.
*/
extern const bool is_shuffle_neon;

/**
  NEON-accelerated shuffle routine.
*/
BLOSC_NO_EXPORT void shuffle_neon(const int32_t bytesoftype, const int32_t blocksize,
                                  const uint8_t* const _src, uint8_t* const _dest);

/**
  NEON-accelerated unshuffle routine.
*/
BLOSC_NO_EXPORT void unshuffle_neon(const int32_t bytesoftype, const int32_t blocksize,
                                    const uint8_t *_src, uint8_t *_dest);

#endif /* BLOSC_SHUFFLE_NEON_H */
