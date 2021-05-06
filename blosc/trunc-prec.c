/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdio.h>
#include "blosc2.h"
#include "assert.h"
#include "trunc-prec.h"

#define BITS_MANTISSA_FLOAT 23
#define BITS_MANTISSA_DOUBLE 52


void truncate_precision32(uint8_t prec_bits, int32_t nelems,
                          const int32_t* src, int32_t* dest) {
  if (prec_bits > BITS_MANTISSA_FLOAT) {
    fprintf(stderr, "The precision cannot be larger than %d bits for floats",
            BITS_MANTISSA_FLOAT);
  }
  assert (prec_bits <= BITS_MANTISSA_FLOAT);
  int zeroed_bits = BITS_MANTISSA_FLOAT - prec_bits;
  int32_t mask = ~((1 << zeroed_bits) - 1);
  for (int i = 0; i < nelems; i++) {
    dest[i] = src[i] & mask;
  }
}

void truncate_precision64(uint8_t prec_bits, int32_t nelems,
                          const int64_t* src, int64_t* dest) {
  if (prec_bits > BITS_MANTISSA_DOUBLE) {
    fprintf(stderr, "The precision cannot be larger than %d bits for doubles",
            BITS_MANTISSA_DOUBLE);
  }
  assert (prec_bits <= BITS_MANTISSA_DOUBLE);
  int zeroed_bits = BITS_MANTISSA_DOUBLE - prec_bits;
  uint64_t mask = ~((1ULL << zeroed_bits) - 1ULL);
  for (int i = 0; i < nelems; i++) {
    dest[i] = src[i] & mask;
  }
}

/* Apply the truncate precision to src.  This can never fail. */
void truncate_precision(uint8_t prec_bits, int32_t typesize, int32_t nbytes,
                        const uint8_t* src, uint8_t* dest) {
  // Make sure that we don't remove all the bits in mantissa so that we
  // don't mess with NaNs or Infinite representation in IEEE 754:
  // https://en.wikipedia.org/wiki/NaN
  if (prec_bits <= 0) {
    fprintf(stderr, "The precision needs to be at least 1 bit");
  }
  assert (prec_bits > 0);
  switch (typesize) {
    case 4:
      truncate_precision32(prec_bits, nbytes / typesize,
                           (int32_t *)src, (int32_t *)dest);
      break;
    case 8:
      truncate_precision64(prec_bits, nbytes / typesize,
                           (int64_t *)src, (int64_t *)dest);
      break;
    default:
      fprintf(stderr, "Error in trunc-prec filter: Precision for typesize %d "
              "not handled", (int)typesize);
      assert(0);
  }
}
