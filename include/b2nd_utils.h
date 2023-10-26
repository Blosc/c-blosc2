/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  The Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/** @file b2nd_utils.h
 * @brief Blosc2 NDim utilities header file.
 *
 * This file contains Blosc2 NDim utility functions for working with C buffers
 * representing multidimensional arrays.
 * @author Blosc Development Team <blosc@blosc.org>
 */

#ifndef BLOSC_B2ND_UTILS_H
#define BLOSC_B2ND_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif
#include "blosc2/blosc2-export.h"
#ifdef __cplusplus
}
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Copy a slice of a source array into another array. The arrays have
 * the same number of dimensions (though their shapes may differ), the same
 * item size, and they are stored as C buffers with contiguous data (any
 * padding is considered part of the array).
 *
 * @param ndim The number of dimensions in both arrays.
 * @param itemsize The size of the individual data item in both arrays.
 * @param src The buffer for getting the data from the source array.
 * @param src_pad_shape The shape of the source array, including padding.
 * @param src_start The source coordinates where the slice will begin.
 * @param src_stop The source coordinates where the slice will end.
 * @param dst The buffer for setting the data into the destination array.
 * @param dst_pad_shape The shape of the destination array, including padding.
 * @param dst_start The destination coordinates where the slice will be placed.
 *
 * @return An error code.
 *
 * @note Please make sure that slice boundaries fit within the source and
 * destination arrays before using this function, as it does not perform these
 * checks itself.
 */
BLOSC_EXPORT int b2nd_copy_buffer(int8_t ndim,
                                  uint8_t itemsize,
                                  const void *src, const int64_t *src_pad_shape,
                                  const int64_t *src_start, const int64_t *src_stop,
                                  void *dst, const int64_t *dst_pad_shape,
                                  const int64_t *dst_start);

#ifdef __cplusplus
}
#endif

#endif /* BLOSC_B2ND_UTILS_H */
