/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* ALTIVEC-accelerated shuffle/unshuffle routines. */

#ifndef BLOSC_SHUFFLE_ALTIVEC_H
#define BLOSC_SHUFFLE_ALTIVEC_H

#include "blosc2/blosc2-common.h"

#include <stdint.h>
#include <stdbool.h>

/**
 * ALTIVEC-accelerated (un)shuffle routines availability.
*/
extern const bool is_shuffle_altivec;

/**
  ALTIVEC-accelerated shuffle routine.
*/
BLOSC_NO_EXPORT void shuffle_altivec(const int32_t bytesoftype, const int32_t blocksize,
                                     const uint8_t *_src, uint8_t *_dest);

/**
  ALTIVEC-accelerated unshuffle routine.
*/
BLOSC_NO_EXPORT void unshuffle_altivec(const int32_t bytesoftype, const int32_t blocksize,
                                       const uint8_t *_src, uint8_t *_dest);

#endif /* BLOSC_SHUFFLE_ALTIVEC_H */
