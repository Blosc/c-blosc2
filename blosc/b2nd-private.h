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
int b2nd_get_slice_nchunks(b2nd_array_t *array, const int64_t *start, const int64_t *stop, int64_t **chunks_idx);

#endif /* BLOSC_B2ND_PRIVATE_H */
