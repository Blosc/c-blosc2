/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/



#ifndef NDLZ_PRIVATE_H
#define NDLZ_PRIVATE_H
#include "context.h"

#if defined (__cplusplus)
extern "C" {
#endif
#define XXH_INLINE_ALL

#define NDLZ_VERSION_STRING "1.0.0"

#define NDLZ_ERROR_NULL(pointer)         \
    do {                                 \
        if ((pointer) == NULL) {         \
            return 0;                    \
        }                                \
    } while (0)


static void swap_store(void *dest, const void *pa, int size) {
    uint8_t *pa_ = (uint8_t *) pa;
    uint8_t pa2_[8];
    int i = 1; /* for big/little endian detection */
    char *p = (char *) &i;

    if (p[0] == 1) {
        /* little endian */
        switch (size) {
            case 8:
                pa2_[0] = pa_[7];
                pa2_[1] = pa_[6];
                pa2_[2] = pa_[5];
                pa2_[3] = pa_[4];
                pa2_[4] = pa_[3];
                pa2_[5] = pa_[2];
                pa2_[6] = pa_[1];
                pa2_[7] = pa_[0];
                break;
            case 4:
                pa2_[0] = pa_[3];
                pa2_[1] = pa_[2];
                pa2_[2] = pa_[1];
                pa2_[3] = pa_[0];
                break;
            case 2:
                pa2_[0] = pa_[1];
                pa2_[1] = pa_[0];
                break;
            case 1:
                pa2_[0] = pa_[0];
                break;
            default:
                fprintf(stderr, "Unhandled nitems: %d\n", size);
        }
    }
    memcpy(dest, pa2_, size);
}

static int32_t deserialize_meta(uint8_t *smeta, uint32_t smeta_len, int8_t *ndim, int64_t *shape,
                         int32_t *chunkshape, int32_t *blockshape) {
    uint8_t *pmeta = smeta;

    // Check that we have an array with 5 entries (version, ndim, shape, chunkshape, blockshape)
    pmeta += 1;

    // version entry
    int8_t version = pmeta[0];  // positive fixnum (7-bit positive integer)
    pmeta += 1;

    // ndim entry
    *ndim = pmeta[0];
    int8_t ndim_aux = *ndim;  // positive fixnum (7-bit positive integer)
    pmeta += 1;

    // shape entry
    // Initialize to ones, as required by Caterva
    for (int i = 0; i < 8; i++) shape[i] = 1;
    pmeta += 1;
    for (int8_t i = 0; i < ndim_aux; i++) {
        pmeta += 1;
        swap_store(shape + i, pmeta, sizeof(int64_t));
        pmeta += sizeof(int64_t);
    }

    // chunkshape entry
    // Initialize to ones, as required by Caterva
    for (int i = 0; i < 8; i++) chunkshape[i] = 1;
    pmeta += 1;
    for (int8_t i = 0; i < ndim_aux; i++) {
        pmeta += 1;
        swap_store(chunkshape + i, pmeta, sizeof(int32_t));
        pmeta += sizeof(int32_t);
    }

    // blockshape entry
    // Initialize to ones, as required by Caterva
    for (int i = 0; i < 8; i++) blockshape[i] = 1;
    pmeta += 1;
    for (int8_t i = 0; i < ndim_aux; i++) {
        pmeta += 1;
        swap_store(blockshape + i, pmeta, sizeof(int32_t));
        pmeta += sizeof(int32_t);
    }
    uint32_t slen = (uint32_t)(pmeta - smeta);
    return 0;
}

#if defined (__cplusplus)
}
#endif

#endif /* NDLZ_PRIVATE_H */
