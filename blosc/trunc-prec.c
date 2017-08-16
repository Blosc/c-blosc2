/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>
  Creation date: 2017-08-02

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdio.h>
#include <string.h>
#include "blosc.h"
#include "trunc-prec.h"

#define BITS_MANTISSA_FLOAT 23
#define BITS_MANTISSA_DOUBLE 52


void truncate_precision32(const uint16_t filter_meta, const int nelems,
                          const int32_t* src, int32_t* dest) {
  int zeroed_bits = BITS_MANTISSA_FLOAT - filter_meta;
  int32_t mask = ~((1 << zeroed_bits) - 1);
  for (int i = 0; i < nelems; i++) {
    dest[i] = src[i] & mask;
  }
}

void truncate_precision64(const uint16_t filter_meta, const int nelems,
                          const int64_t* src, int64_t* dest) {
  int zeroed_bits = BITS_MANTISSA_DOUBLE - filter_meta;
  uint64_t mask = ~((1LL << zeroed_bits) - 1LL);
  for (int i = 0; i < nelems; i++) {
    dest[i] = src[i] & mask;
  }
}

/* Apply the truncate precision to src.  This can never fail. */
void truncate_precision(const uint16_t filter_meta, const int32_t typesize,
                        const int32_t nbytes, const uint8_t* src,
                        uint8_t* dest) {
  switch (typesize) {
    case 4:
      truncate_precision32(filter_meta, nbytes / typesize,
                           (int32_t *)src, (int32_t *)dest);
    case 8:
      truncate_precision64(filter_meta, nbytes / typesize,
                           (int64_t *)src, (int64_t *)dest);
  }
}
