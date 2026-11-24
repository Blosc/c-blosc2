/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/


#ifndef BLOSC_B2ND_PRIVATE_H
#define BLOSC_B2ND_PRIVATE_H

#include "b2nd.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

/*********************************************************************

  Functions meant to be used internally.

*********************************************************************/

/**
 * @brief Get the chunk indexes needed to get the slice.
 *
 * @param array The b2nd array.
 * @param start The coordinates where the slice will begin.
 * @param stop The coordinates where the slice will end.
 * @param chunks_idx The pointer to the buffer where the indexes of the chunks will be written.
 *
 * @return The number of chunks needed to get the slice. If some problem is
 * detected, a negative code is returned instead.
 */
int64_t b2nd_get_slice_nchunks(const b2nd_array_t *array, const int64_t *start, const int64_t *stop,
                               int64_t **chunks_idx);

/**
 * @brief Zero a box of a C-order buffer, in as few and as long runs of bytes as its layout allows.
 *
 * @param ndim The number of dimensions of the buffer.
 * @param itemsize The size of an item in bytes.
 * @param dst The buffer.
 * @param dst_pad_shape The shape of the buffer.
 * @param box_start The coordinates where the box begins.
 * @param box_stop The coordinates where the box ends (exclusive); nothing is zeroed if the box is empty.
 */
void b2nd_zero_box(int8_t ndim, int32_t itemsize, void *dst, const int64_t *dst_pad_shape,
                   const int64_t *box_start, const int64_t *box_stop);

#endif /* BLOSC_B2ND_PRIVATE_H */
