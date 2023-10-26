/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  The Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

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

BLOSC_EXPORT int b2nd_copy_buffer(int8_t ndim,
                                  uint8_t itemsize,
                                  void *src, const int64_t *src_pad_shape,
                                  int64_t *src_start, const int64_t *src_stop,
                                  void *dst, const int64_t *dst_pad_shape,
                                  int64_t *dst_start);

#ifdef __cplusplus
}
#endif

#endif /* BLOSC_B2ND_UTILS_H */
