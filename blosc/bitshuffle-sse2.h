/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* SSE2-accelerated shuffle/unshuffle routines. */

#ifndef BLOSC_BITSHUFFLE_SSE2_H
#define BLOSC_BITSHUFFLE_SSE2_H

#include "blosc2/blosc2-common.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * SSE2-accelerated bit(un)shuffle routines availability.
*/
extern const bool is_bshuf_SSE;

BLOSC_NO_EXPORT int64_t
    bshuf_trans_byte_elem_SSE(const void* in, void* out, const size_t size,
                              const size_t elem_size);

BLOSC_NO_EXPORT int64_t
    bshuf_trans_byte_bitrow_SSE(const void* in, void* out, const size_t size,
                                const size_t elem_size);

BLOSC_NO_EXPORT int64_t
    bshuf_shuffle_bit_eightelem_SSE(const void* in, void* out, const size_t size,
                                    const size_t elem_size);

/**
  SSE2-accelerated bitshuffle routine.
*/
BLOSC_NO_EXPORT int64_t
    bshuf_trans_bit_elem_SSE(const void* in, void* out, const size_t size,
                             const size_t elem_size);

/**
  SSE2-accelerated bitunshuffle routine.
*/
BLOSC_NO_EXPORT int64_t
    bshuf_untrans_bit_elem_SSE(const void* in, void* out, const size_t size,
                               const size_t elem_size);

#endif /* BLOSC_BITSHUFFLE_SSE2_H */
