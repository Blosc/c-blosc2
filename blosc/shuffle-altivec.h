/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* ALTIVEC-accelerated shuffle/unshuffle routines. */

#ifndef SHUFFLE_ALTIVEC_H
#define SHUFFLE_ALTIVEC_H

#include "blosc2-common.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif /* SHUFFLE_ALTIVEC_H */
