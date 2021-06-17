/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Note: Adapted for NEON by Lucian Marc.

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* NEON-accelerated shuffle/unshuffle routines. */

#ifndef SHUFFLE_NEON_H
#define SHUFFLE_NEON_H

#include "blosc2/blosc2-common.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif /* SHUFFLE_NEON_H */
