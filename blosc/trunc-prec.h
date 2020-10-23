/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: The Blosc Developers <blosc@blosc.org>
  Creation date: 2017-08-02

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_TRUNC_PREC_H
#define BLOSC_TRUNC_PREC_H

#include <stdio.h>
#include <stdint.h>

void truncate_precision(uint8_t prec_bits, int32_t typesize, int32_t nbytes,
                        const uint8_t* src, uint8_t* dest);

#endif //BLOSC_TRUNC_PREC_H
