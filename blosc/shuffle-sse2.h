/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: The Blosc Developers <blosc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* SSE2-accelerated shuffle/unshuffle routines. */

#ifndef SHUFFLE_SSE2_H
#define SHUFFLE_SSE2_H

#include "blosc2-common.h"

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
