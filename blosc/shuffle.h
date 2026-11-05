/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/*********************************************************************
  Shuffle/unshuffle routines which dynamically dispatch to hardware-
  accelerated routines based on the processor's architecture.
  Consumers should almost always prefer to call these routines instead
  of directly calling one of the hardware-accelerated routines, since
  these are cross-platform and future-proof.
**********************************************************************/

#ifndef BLOSC_SHUFFLE_H
#define BLOSC_SHUFFLE_H

#include "blosc2/blosc2-common.h"

#include <stdint.h>

/**
  Internal bitshuffle routine that accepts a scratch buffer `tmp` of at least `blocksize` bytes.
  With NULL, it allocates a temporary one (as blosc2_bitshuffle() does).
 */
BLOSC_NO_EXPORT int32_t
    bitshuffle(const int32_t bytesoftype, const int32_t blocksize,
               const void* src, void* dest, void* tmp);

/**
  Internal bitunshuffle routine that accepts a scratch buffer (see bitshuffle()) and a format version.
  We don't have to expose these parameters to users, since the public API is new to blosc2, and its
  behavior can be independent of the storage format.
 */
BLOSC_NO_EXPORT int32_t
    bitunshuffle(const int32_t bytesoftype, const int32_t blocksize,
                 const void* src, void* dest, void* tmp,
                 const uint8_t format_version);

#endif /* BLOSC_SHUFFLE_H */
