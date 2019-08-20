/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/


#ifndef IARRAY_BLOSC_PRIVATE_H
#define IARRAY_BLOSC_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "blosc2-common.h"

/*********************************************************************

  Utility functions meant to be used internally.

*********************************************************************/

/* Copy 4 bytes from @p *pa to int32_t, changing endianness if necessary. */
static inline int32_t sw32_(const void* pa) {
  int32_t idest;
  uint8_t* dest = (uint8_t*)&idest;
  uint8_t* pa_ = (uint8_t*)pa;
  int i = 1;                    /* for big/little endian detection */
  char* p = (char*)&i;

  if (p[0] != 1) {
    /* big endian */
    dest[0] = pa_[3];
    dest[1] = pa_[2];
    dest[2] = pa_[1];
    dest[3] = pa_[0];
  }
  else {
    /* little endian */
    dest[0] = pa_[0];
    dest[1] = pa_[1];
    dest[2] = pa_[2];
    dest[3] = pa_[3];
  }
  return idest;
}

/* Copy 4 bytes from int32_t to @p *dest, changing endianness if necessary. */
static inline void _sw32(void* dest, int32_t a) {
  uint8_t* dest_ = (uint8_t*)dest;
  uint8_t* pa = (uint8_t*)&a;
  int i = 1;                    /* for big/little endian detection */
  char* p = (char*)&i;

  if (p[0] != 1) {
    /* big endian */
    dest_[0] = pa[3];
    dest_[1] = pa[2];
    dest_[2] = pa[1];
    dest_[3] = pa[0];
  }
  else {
    /* little endian */
    dest_[0] = pa[0];
    dest_[1] = pa[1];
    dest_[2] = pa[2];
    dest_[3] = pa[3];
  }
}

/* Convert filter pipeline to filter flags */
static uint8_t filters_to_flags(const uint8_t* filters) {
  uint8_t flags = 0;

  for (int i = 0; i < BLOSC2_MAX_FILTERS; i++) {
    switch (filters[i]) {
      case BLOSC_SHUFFLE:
        flags |= BLOSC_DOSHUFFLE;
        break;
      case BLOSC_BITSHUFFLE:
        flags |= BLOSC_DOBITSHUFFLE;
        break;
      case BLOSC_DELTA:
        flags |= BLOSC_DODELTA;
        break;
      default :
        break;
    }
  }
  return flags;
}


/* Convert filter flags to filter pipeline */
static void flags_to_filters(const uint8_t flags, uint8_t* filters) {
  /* Initialize the filter pipeline */
  memset(filters, 0, BLOSC2_MAX_FILTERS);
  /* Fill the filter pipeline */
  if (flags & BLOSC_DOSHUFFLE)
    filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_SHUFFLE;
  if (flags & BLOSC_DOBITSHUFFLE)
    filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_BITSHUFFLE;
  if (flags & BLOSC_DODELTA)
    filters[BLOSC2_MAX_FILTERS - 2] = BLOSC_DELTA;
}

#ifdef __cplusplus
}
#endif

#endif //IARRAY_BLOSC_PRIVATE_H
