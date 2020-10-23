/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: The Blosc Developers <blosc@blosc.org>

  Note: Adapted for NEON by Lucian Marc.

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* NEON-accelerated bitshuffle/bitunshuffle routines. */

#ifndef BITSHUFFLE_NEON_H
#define BITSHUFFLE_NEON_H

#include "blosc2-common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
  NEON-accelerated bitshuffle routine.
*/
BLOSC_NO_EXPORT int64_t bitshuffle_neon(void* _src, void* _dest, const size_t blocksize,
                                        const size_t bytesoftype, void* tmp_buf);

/**
  NEON-accelerated bitunshuffle routine.
*/
BLOSC_NO_EXPORT int64_t bitunshuffle_neon(void* _src, void* _dest, const size_t blocksize,
                                          const size_t bytesoftype, void* tmp_buf);

#ifdef __cplusplus
}
#endif

#endif /* BITSHUFFLE_NEON_H */
