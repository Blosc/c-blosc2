/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_PLUGINS_FILTERS_NDCELL_NDCELL_H
#define BLOSC_PLUGINS_FILTERS_NDCELL_NDCELL_H

#include "blosc2.h"

#define NDCELL_MAX_DIM 8


int ndcell_forward(const uint8_t* input, uint8_t* output, int32_t length, uint8_t meta, blosc2_cparams* cparams, uint8_t id);

int ndcell_backward(const uint8_t* input, uint8_t* output, int32_t length, uint8_t meta, blosc2_dparams* dparams, uint8_t id);

#endif /* BLOSC_PLUGINS_FILTERS_NDCELL_NDCELL_H */
