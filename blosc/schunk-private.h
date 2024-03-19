/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/


#ifndef BLOSC_SCHUNK_PRIVATE_H
#define BLOSC_SCHUNK_PRIVATE_H

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
 * @param schunk The super-chunk.
 * @param start Index (0-based) where the slice begins.
 * @param stop The first index (0-based) that is not in the selected slice.
 * @param chunks_idx The pointer to the buffer where the indexes will be written.
 *
 *
 * @return The number of chunks needed to get the slice. If some problem is
 * detected, a negative code is returned instead.
 */
int schunk_get_slice_nchunks(blosc2_schunk *schunk, int64_t start, int64_t stop, int64_t **chunks_idx);
#endif /* BLOSC_SCHUNK_PRIVATE_H */
