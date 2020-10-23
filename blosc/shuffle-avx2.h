/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: The Blosc Developers <blosc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* AVX2-accelerated shuffle/unshuffle routines. */

#ifndef SHUFFLE_AVX2_H
#define SHUFFLE_AVX2_H

#include "blosc2-common.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif /* SHUFFLE_AVX2_H */
