/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_DELTA_H
#define BLOSC_DELTA_H

#include <stdio.h>
#include <stdint.h>

void delta_encoder(const uint8_t* dref, int32_t offset, int32_t nbytes,
                   int32_t typesize, const uint8_t* src, uint8_t* dest);

void delta_decoder(const uint8_t* dref, int32_t offset, int32_t nbytes,
                   int32_t typesize, uint8_t* dest);

#endif //BLOSC_DELTA_H
