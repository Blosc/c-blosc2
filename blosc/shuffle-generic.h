/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/*********************************************************************
  Generic (non-hardware-accelerated) shuffle/unshuffle routines.
  These are used when hardware-accelerated functions aren't available
  for a particular platform; they are also used by the hardware-
  accelerated functions to handle any remaining elements in a block
  which isn't a multiple of the hardware's vector size.
**********************************************************************/

#ifndef BLOSC_SHUFFLE_GENERIC_H
#define BLOSC_SHUFFLE_GENERIC_H

#include "blosc2/blosc2-common.h"

#include <stdint.h>
#include <string.h>

/**
  Generic (non-hardware-accelerated) shuffle routine.
  This is the pure element-copying nested loop. It is used by the
  generic shuffle implementation and also by the vectorized shuffle
  implementations to process any remaining elements in a block which
  is not a multiple of (type_size * vector_size).
*/
static inline void shuffle_generic_inline(const int32_t type_size,
                                   const int32_t vectorizable_blocksize, const int32_t blocksize,
                                   const uint8_t *_src, uint8_t *_dest) {
  int32_t i, j;
  /* Calculate the number of elements in the block. */
  const int32_t neblock_quot = blocksize / type_size;
  const int32_t neblock_rem = blocksize % type_size;
  const int32_t vectorizable_elements = vectorizable_blocksize / type_size;


  /* Non-optimized shuffle */
  for (j = 0; j < type_size; j++) {
    for (i = vectorizable_elements; i < (int32_t)neblock_quot; i++) {
      _dest[j * neblock_quot + i] = _src[i * type_size + j];
    }
  }

  /* Copy any leftover bytes in the block without shuffling them. */
  memcpy(_dest + (blocksize - neblock_rem), _src + (blocksize - neblock_rem), neblock_rem);
}

/**
  Generic (non-hardware-accelerated) unshuffle routine.
  This is the pure element-copying nested loop. It is used by the
  generic unshuffle implementation and also by the vectorized unshuffle
  implementations to process any remaining elements in a block which
  is not a multiple of (type_size * vector_size).
*/
static inline void unshuffle_generic_inline(const int32_t type_size,
                                     const int32_t vectorizable_blocksize, const int32_t blocksize,
                                     const uint8_t *_src, uint8_t *_dest) {
  int32_t i, j;

  /* Calculate the number of elements in the block. */
  const int32_t neblock_quot = blocksize / type_size;
  const int32_t neblock_rem = blocksize % type_size;
  const int32_t vectorizable_elements = vectorizable_blocksize / type_size;

  /* Non-optimized unshuffle */
  for (i = vectorizable_elements; i < (int32_t)neblock_quot; i++) {
    for (j = 0; j < type_size; j++) {
      _dest[i * type_size + j] = _src[j * neblock_quot + i];
    }
  }

  /* Copy any leftover bytes in the block without unshuffling them. */
  memcpy(_dest + (blocksize - neblock_rem), _src + (blocksize - neblock_rem), neblock_rem);
}

/**
  Generic (non-hardware-accelerated) shuffle routine.
*/
BLOSC_NO_EXPORT void shuffle_generic(const int32_t bytesoftype, const int32_t blocksize,
                                     const uint8_t *_src, uint8_t *_dest);

/**
  Generic (non-hardware-accelerated) unshuffle routine.
*/
BLOSC_NO_EXPORT void unshuffle_generic(const int32_t bytesoftype, const int32_t blocksize,
                                       const uint8_t *_src, uint8_t *_dest);

#endif /* BLOSC_SHUFFLE_GENERIC_H */
