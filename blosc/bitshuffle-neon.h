/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  The Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Note: Adapted for NEON by Lucian Marc.

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* NEON-accelerated bitshuffle/bitunshuffle routines. */

#ifndef BLOSC_BITSHUFFLE_NEON_H
#define BLOSC_BITSHUFFLE_NEON_H

#include "blosc2/blosc2-common.h"

#include <stddef.h>
#include <stdint.h>

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

#endif /* BLOSC_BITSHUFFLE_NEON_H */
