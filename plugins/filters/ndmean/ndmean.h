//
// Created by sosca on 12/03/2021.
//

#ifndef CATERVA_NDMEAN_H
#define CATERVA_NDMEAN_H

#include <blosc2.h>


static void index_unidim_to_multidim(int8_t ndim, int64_t *shape, int64_t i, int64_t *index);

void swap_store(void *dest, const void *pa, int size);

int32_t deserialize_meta(uint8_t *smeta, uint32_t smeta_len, int8_t *ndim, int64_t *shape,
                         int32_t *chunkshape, int32_t *blockshape);


int ndmean_encoder(const uint8_t* input, uint8_t* output, int32_t length, int8_t meta, blosc2_cparams* cparams);

int ndmean_decoder(const uint8_t* input, uint8_t* output, int32_t length, int8_t meta, blosc2_dparams* dparams);

#endif //CATERVA_NDMEAN_H


