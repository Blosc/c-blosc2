//
// Created by sosca on 12/03/2021.
//

#ifndef CATERVA_NDMEAN_H
#define CATERVA_NDMEAN_H

#include <blosc2.h>

#define NDMEAN_MAX_DIM 8


int ndmean_encoder(const uint8_t* input, uint8_t* output, int32_t length, int8_t meta, blosc2_cparams* cparams);

int ndmean_decoder(const uint8_t* input, uint8_t* output, int32_t length, int8_t meta, blosc2_dparams* dparams);

#endif //CATERVA_NDMEAN_H


