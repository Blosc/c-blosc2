/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* SSE2-accelerated shuffle/unshuffle routines. */

#ifndef SHUFFLE_SSE2_H
#define SHUFFLE_SSE2_H

#include "blosc2/blosc2-common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
  SSE2-accelerated shuffle routine.
*/
BLOSC_NO_EXPORT void shuffle_sse2(const int32_t bytesoftype, const int32_t blocksize,
                                  const uint8_t *_src, uint8_t *_dest);

/**
  SSE2-accelerated unshuffle routine.
*/
BLOSC_NO_EXPORT void unshuffle_sse2(const int32_t bytesoftype, const int32_t blocksize,
                                    const uint8_t *_src, uint8_t *_dest);

#ifdef __cplusplus
}
#endif

#endif /* SHUFFLE_SSE2_H */
