/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: The Blosc Developers <blosc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "shuffle-generic.h"

/* Shuffle a block.  This can never fail. */
void shuffle_generic(const int32_t bytesoftype, const int32_t blocksize,
                     const uint8_t *_src, uint8_t *_dest) {
  /* Non-optimized shuffle */
  shuffle_generic_inline(bytesoftype, 0, blocksize, _src, _dest);
}

/* Unshuffle a block.  This can never fail. */
void unshuffle_generic(const int32_t bytesoftype, const int32_t blocksize,
                       const uint8_t *_src, uint8_t *_dest) {
  /* Non-optimized unshuffle */
  unshuffle_generic_inline(bytesoftype, 0, blocksize, _src, _dest);
}
