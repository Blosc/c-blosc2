/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_PLUGINS_FILTERS_BYTEDELTA_BYTEDELTA_H
#define BLOSC_PLUGINS_FILTERS_BYTEDELTA_BYTEDELTA_H

#include "blosc2.h"

#include <stdint.h>

/* Internal, out-of-place pair kernels. Return 1 when handled, 0 for fallback.
 * They emit/consume exactly the existing SHUFFLE + BYTEDELTA representation. */
int shuffle_bytedelta_forward(const uint8_t* input, uint8_t* output, int32_t length, int typesize);
int shuffle_bytedelta_backward(const uint8_t* input, uint8_t* output, int32_t length, int typesize);
/* Availability is initialized with the existing thread-safe shuffle dispatcher. */
int shuffle_bytedelta_available(void);
int shuffle_bytedelta_neon_forward(const uint8_t* input, uint8_t* output, int32_t length, int typesize);
int shuffle_bytedelta_neon_backward(const uint8_t* input, uint8_t* output, int32_t length, int typesize);
int shuffle_bytedelta_ssse3_forward(const uint8_t* input, uint8_t* output, int32_t length, int typesize);
int shuffle_bytedelta_ssse3_backward(const uint8_t* input, uint8_t* output, int32_t length, int typesize);

int bytedelta_forward(const uint8_t* input, uint8_t* output, int32_t length, uint8_t meta,
                      blosc2_cparams* cparams, uint8_t id);

int bytedelta_backward(const uint8_t* input, uint8_t* output, int32_t length, uint8_t meta,
                       blosc2_dparams* dparams, uint8_t id);

int bytedelta_forward_buggy(const uint8_t* input, uint8_t* output, int32_t length, uint8_t meta,
                            blosc2_cparams* cparams, uint8_t id);

int bytedelta_backward_buggy(const uint8_t* input, uint8_t* output, int32_t length, uint8_t meta,
                             blosc2_dparams* dparams, uint8_t id);

#endif /* BLOSC_PLUGINS_FILTERS_BYTEDELTA_BYTEDELTA_H */
