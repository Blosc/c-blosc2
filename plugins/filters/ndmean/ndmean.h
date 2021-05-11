//
// Created by sosca on 12/03/2021.
//

#ifndef CATERVA_NDMEAN_H
#define CATERVA_NDMEAN_H
#include <caterva.h>

typedef struct {
  caterva_array_t *array;
  int32_t cellshape[CATERVA_MAX_DIM];
} ndmean_params;

int ndmean_encoder(const uint8_t* input, uint8_t* output, int32_t length, void* params);

int ndmean_decoder(const uint8_t* input, uint8_t* output, int32_t length, void* params);

#endif //CATERVA_NDMEAN_H


