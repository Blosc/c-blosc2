/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_FILTER_INT_TRUNC_H
#define BLOSC_FILTER_INT_TRUNC_H

#include <stdint.h>

int int_trunc_forward(const uint8_t* input, uint8_t* output, int32_t length, uint8_t meta,
                      blosc2_cparams* cparams, uint8_t id);

int int_trunc_backward(const uint8_t* input, uint8_t* output, int32_t length, uint8_t meta,
                       blosc2_dparams* dparams, uint8_t id);

#endif /* BLOSC_FILTER_INT_TRUNC_H */
