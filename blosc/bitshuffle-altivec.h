/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* ALTIVEC-accelerated shuffle/unshuffle routines. */

#ifndef BITSHUFFLE_ALTIVEC_H
#define BITSHUFFLE_ALTIVEC_H

#include "blosc2-common.h"

#ifdef __cplusplus
extern "C" {
#endif


BLOSC_NO_EXPORT int64_t
    bshuf_trans_byte_elem_altivec(void* in, void* out, const size_t size,
                                  const size_t elem_size, void* tmp_buf);

BLOSC_NO_EXPORT int64_t
    bshuf_trans_byte_bitrow_altivec(void* in, void* out, const size_t size,
                                    const size_t elem_size);

BLOSC_NO_EXPORT int64_t
    bshuf_shuffle_bit_eightelem_altivec(void* in, void* out, const size_t size,
                                     const size_t elem_size);

/**
  ALTIVEC-accelerated bitshuffle routine.
*/
BLOSC_NO_EXPORT int64_t
    bshuf_trans_bit_elem_altivec(void* in, void* out, const size_t size,
                              const size_t elem_size, void* tmp_buf);

/**
  ALTIVEC-accelerated bitunshuffle routine.
*/
BLOSC_NO_EXPORT int64_t
    bshuf_untrans_bit_elem_altivec(void* in, void* out, const size_t size,
                                const size_t elem_size, void* tmp_buf);

#ifdef __cplusplus
}
#endif


#endif /* BITSHUFFLE_ALTIVEC_H */
