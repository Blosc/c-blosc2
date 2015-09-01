/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>

  Note: Adapted for NEON by Lucian Marc.

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/

/* NEON-accelerated bitshuffle/bitunshuffle routines. */
   
#ifndef BITSHUFFLE_NEON_H
#define BITSHUFFLE_NEON_H

#include "shuffle-common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
  NEON-accelerated bitshuffle routine.
*/
BLOSC_NO_EXPORT void bitshuffle_neon(const size_t bytesoftype, const size_t blocksize,
                                   const uint8_t* const _src, uint8_t* const _dest, void* tmp_buf);

/**
  NEON-accelerated bitunshuffle routine.
*/
BLOSC_NO_EXPORT void bitunshuffle_neon(const size_t bytesoftype, const size_t blocksize,
                                     const uint8_t* const _src, uint8_t* const _dest, void* tmp_buf);

#ifdef __cplusplus
}
#endif

#endif /* BITSHUFFLE_NEON_H */
