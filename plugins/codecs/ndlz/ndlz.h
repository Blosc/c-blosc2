/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/



#ifndef NDLZ_H
#define NDLZ_H
#include "context.h"

#if defined (__cplusplus)
extern "C" {
#endif
#define XXH_INLINE_ALL
#include <xxhash.h>

#define NDLZ_VERSION_STRING "1.0.0"

#define NDLZ_ERROR_NULL(pointer)                             \
    do {                                                        \
        if ((pointer) == NULL) {                                  \
            return 0;                    \
        }                                                       \
    } while (0)




void swap_store(void *dest, const void *pa, int size);

int32_t deserialize_meta(uint8_t *smeta, uint32_t smeta_len, int8_t *ndim, int64_t *shape,
                         int32_t *chunkshape, int32_t *blockshape);


int ndlz_compress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                   uint8_t meta, blosc2_cparams *cparams);

int ndlz_decompress(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len,
                     uint8_t meta, blosc2_dparams *dparams);


#if defined (__cplusplus)
}
#endif

#endif /* NDLZ_H */
