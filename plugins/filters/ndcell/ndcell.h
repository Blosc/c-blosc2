//
// Created by sosca on 12/03/2021.
//

#ifndef CATERVA_NDCELL_H
#define CATERVA_NDCELL_H

#include <blosc2.h>

#define NDCELL_MAX_DIM 8


int ndcell_encoder(const uint8_t* input, uint8_t* output, int32_t length, int8_t meta, blosc2_cparams* cparams);

int ndcell_decoder(const uint8_t* input, uint8_t* output, int32_t length, int8_t meta, blosc2_dparams* dparams);

#endif //CATERVA_NDCELL_H


