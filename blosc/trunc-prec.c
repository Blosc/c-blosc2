/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "trunc-prec.h"
#include "blosc2.h"

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define BITS_MANTISSA_FLOAT 23
#define BITS_MANTISSA_DOUBLE 52


int truncate_precision32(int8_t prec_bits, int32_t nelems,
                         const int32_t* src, int32_t* dest) {
  // Make sure that we don't remove all the bits in mantissa so that we
  // don't mess with NaNs or Infinite representation in IEEE 754:
  // https://en.wikipedia.org/wiki/NaN
  if ((abs(prec_bits) > BITS_MANTISSA_FLOAT)) {
    BLOSC_TRACE_ERROR("The precision cannot be larger than %d bits for floats (asking for %d bits)",
                      BITS_MANTISSA_FLOAT, prec_bits);
    return -1;
  }
  int zeroed_bits = (prec_bits >= 0) ? BITS_MANTISSA_FLOAT - prec_bits : -prec_bits;
  if (zeroed_bits >= BITS_MANTISSA_FLOAT) {
    BLOSC_TRACE_ERROR("The reduction in precision cannot be larger or equal than %d bits for floats (asking for %d bits)",
                      BITS_MANTISSA_FLOAT, zeroed_bits);
    return -1;
  }
  int32_t mask = ~((1 << zeroed_bits) - 1);
  for (int i = 0; i < nelems; i++) {
    dest[i] = src[i] & mask;
  }
  return 0;
}

int truncate_precision64(int8_t prec_bits, int32_t nelems,
                          const int64_t* src, int64_t* dest) {
  // Make sure that we don't remove all the bits in mantissa so that we
  // don't mess with NaNs or Infinite representation in IEEE 754:
  // https://en.wikipedia.org/wiki/NaN
  if ((abs(prec_bits) > BITS_MANTISSA_DOUBLE)) {
    BLOSC_TRACE_ERROR("The precision cannot be larger than %d bits for floats (asking for %d bits)",
                      BITS_MANTISSA_DOUBLE, prec_bits);
    return -1;
  }
  int zeroed_bits = (prec_bits >= 0) ? BITS_MANTISSA_DOUBLE - prec_bits : -prec_bits;
  if (zeroed_bits >= BITS_MANTISSA_DOUBLE) {
    BLOSC_TRACE_ERROR("The reduction in precision cannot be larger or equal than %d bits for floats (asking for %d bits)",
                      BITS_MANTISSA_DOUBLE, zeroed_bits);
    return -1;
  }
  uint64_t mask = ~((1ULL << zeroed_bits) - 1ULL);
  for (int i = 0; i < nelems; i++) {
    dest[i] = (int64_t)(src[i] & mask);
  }
  return 0;
}

/* Apply the truncate precision to src.  This can never fail. */
int truncate_precision(int8_t prec_bits, int32_t typesize, int32_t nbytes,
                       const uint8_t* src, uint8_t* dest) {
  // Positive values of prec_bits will set absolute precision bits, whereas negative
  // values will reduce the precision bits (similar to Python slicing convention).
  switch (typesize) {
    case 4:
      return truncate_precision32(prec_bits, nbytes / typesize,
                              (int32_t *)src, (int32_t *)dest);
    case 8:
      return truncate_precision64(prec_bits, nbytes / typesize,
                              (int64_t *)src, (int64_t *)dest);
    default:
      BLOSC_TRACE_ERROR("Error in trunc-prec filter: Precision for typesize %d not handled",
                        (int)typesize);
      return -1;
  }
}
