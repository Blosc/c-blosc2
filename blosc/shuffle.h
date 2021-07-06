/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
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


#ifndef SHUFFLE_H
#define SHUFFLE_H

#include "blosc2/blosc2-common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
  Primary shuffle and bitshuffle routines.
  This function dynamically dispatches to the appropriate hardware-accelerated
  routine based on the host processor's architecture. If the host processor is
  not supported by any of the hardware-accelerated routines, the generic
  (non-accelerated) implementation is used instead.
  Consumers should almost always prefer to call this routine instead of directly
  calling the hardware-accelerated routines because this method is both cross-
  platform and future-proof.
*/
BLOSC_NO_EXPORT void
    shuffle(const int32_t bytesoftype, const int32_t blocksize,
            const uint8_t* _src, const uint8_t* _dest);

BLOSC_NO_EXPORT int32_t
    bitshuffle(const int32_t bytesoftype, const int32_t blocksize,
               const uint8_t *_src, const uint8_t *_dest,
               const uint8_t *_tmp);

/**
  Primary unshuffle and bitunshuffle routine.
  This function dynamically dispatches to the appropriate hardware-accelerated
  routine based on the host processor's architecture. If the host processor is
  not supported by any of the hardware-accelerated routines, the generic
  (non-accelerated) implementation is used instead.
  Consumers should almost always prefer to call this routine instead of directly
  calling the hardware-accelerated routines because this method is both cross-
  platform and future-proof.
*/
BLOSC_NO_EXPORT void
    unshuffle(const int32_t bytesoftype, const int32_t blocksize,
              const uint8_t* _src, const uint8_t* _dest);


BLOSC_NO_EXPORT int32_t
    bitunshuffle(const int32_t bytesoftype, const int32_t blocksize,
                 const uint8_t *_src, const uint8_t *_dest,
                 const uint8_t *_tmp, const uint8_t format_version);

#ifdef __cplusplus
}
#endif

#endif /* SHUFFLE_H */
