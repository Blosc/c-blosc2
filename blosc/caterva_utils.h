/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef CATERVA_CATERVA_UTILS_H_
#define CATERVA_CATERVA_UTILS_H_

#include <caterva.h>
#include <../plugins/plugin_utils.h>

#ifdef __cplusplus
extern "C" {
#endif


int caterva_copy_buffer(int8_t ndim,
                        uint8_t itemsize,
                        void *src, const int64_t *src_pad_shape,
                        int64_t *src_start, const int64_t *src_stop,
                        void *dst, const int64_t *dst_pad_shape,
                        int64_t *dst_start);

#ifdef __cplusplus
}
#endif

#endif  // CATERVA_CATERVA_UTILS_H_
