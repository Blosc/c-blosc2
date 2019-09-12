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

// Return true if platform is little endian; else false
static bool is_little_endian(void) {
  bool little;
  int i = 1;
  char* p = (char*)&i;

  if (p[0] == 1) {
    little = true;
  }
  else {
    little = false;
  }
  return little;
}

/* Copy 4 bytes from @p *pa to int32_t, changing endianness if necessary. */
static inline int32_t sw32_(const void* pa) {
  int32_t idest;
  uint8_t* dest = (uint8_t*)&idest;
  uint8_t* pa_ = (uint8_t*)pa;

  bool little_endian = is_little_endian();
  if (little_endian) {
    dest[0] = pa_[0];
    dest[1] = pa_[1];
    dest[2] = pa_[2];
    dest[3] = pa_[3];
  }
  else {
    dest[0] = pa_[3];
    dest[1] = pa_[2];
    dest[2] = pa_[1];
    dest[3] = pa_[0];
  }
  return idest;
}

/* Copy 4 bytes from int32_t to @p *dest, changing endianness if necessary. */
static inline void _sw32(void* dest, int32_t a) {
  uint8_t* dest_ = (uint8_t*)dest;
  uint8_t* pa = (uint8_t*)&a;

  bool little_endian = is_little_endian();
  if (little_endian) {
    dest_[0] = pa[0];
    dest_[1] = pa[1];
    dest_[2] = pa[2];
    dest_[3] = pa[3];
  }
  else {
    dest_[0] = pa[3];
    dest_[1] = pa[2];
    dest_[2] = pa[1];
    dest_[3] = pa[0];
  }
}

#ifdef __cplusplus
}
#endif

#endif //IARRAY_BLOSC_PRIVATE_H
