/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/



#ifndef ZFP_PRIVATE_H
#define ZFP_PRIVATE_H

#define ZFP_MAX_DIM 4
#define ZFP_CELL_SHAPE 4


#if defined (__cplusplus)
extern "C" {
#endif
#define XXH_INLINE_ALL

#define ZFP_ERROR_NULL(pointer)         \
    do {                                 \
        if ((pointer) == NULL) {         \
            return 0;                    \
        }                                \
    } while (0)


static void index_unidim_to_multidim(uint8_t ndim, int32_t *shape, int64_t i, int64_t *index) {
    int64_t strides[ZFP_MAX_DIM];
    strides[ndim - 1] = 1;
    for (int j = ndim - 2; j >= 0; --j) {
        strides[j] = shape[j + 1] * strides[j + 1];
    }

    index[0] = i / strides[0];
    for (int j = 1; j < ndim; ++j) {
        index[j] = (i % strides[j - 1]) / strides[j];
    }
}


#if defined (__cplusplus)
}
#endif

#endif /* ZFP_PRIVATE_H */
