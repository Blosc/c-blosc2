/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/


#ifndef IARRAY_BLOSC_PRIVATE_H
#define IARRAY_BLOSC_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "blosc2.h"
#include "blosc2/blosc2-common.h"

/*********************************************************************

  Utility functions meant to be used internally.

*********************************************************************/

#define to_little(dest, src, itemsize)    endian_handler(true, dest, src, itemsize)
#define from_little(dest, src, itemsize)  endian_handler(true, dest, src, itemsize)
#define to_big(dest, src, itemsize)       endian_handler(false, dest, src, itemsize)
#define from_big(dest, src, itemsize)     endian_handler(false, dest, src, itemsize)

#define BLOSC_ERROR_NULL(pointer, rc)                           \
    do {                                                        \
        if (pointer == NULL) {                                  \
            BLOSC_TRACE_ERROR("Pointer is NULL");               \
            return rc;                                          \
        }                                                       \
    } while (0)


// Return true if platform is little endian; else false
static bool is_little_endian(void) {
  const static int i = 1;
  char* p = (char*)&i;

  if (p[0] == 1) {
    return true;
  }
  else {
    return false;
  }
}


static void endian_handler(bool little, void *dest, const void *pa, int size) {
  bool little_endian = is_little_endian();
  if (little_endian == little) {
    memcpy(dest, pa, size);
  }
  else {
    uint8_t* pa_ = (uint8_t*)pa;
    uint8_t* pa2_ = malloc((size_t)size);
    switch (size) {
      case 8:
        pa2_[0] = pa_[7];
        pa2_[1] = pa_[6];
        pa2_[2] = pa_[5];
        pa2_[3] = pa_[4];
        pa2_[4] = pa_[3];
        pa2_[5] = pa_[2];
        pa2_[6] = pa_[1];
        pa2_[7] = pa_[0];
        break;
      case 4:
        pa2_[0] = pa_[3];
        pa2_[1] = pa_[2];
        pa2_[2] = pa_[1];
        pa2_[3] = pa_[0];
        break;
      case 2:
        pa2_[0] = pa_[1];
        pa2_[1] = pa_[0];
        break;
      case 1:
        pa2_[0] = pa_[0];
        break;
      default:
        BLOSC_TRACE_ERROR("Unhandled size: %d.", size);
    }
    memcpy(dest, pa2_, size);
    free(pa2_);
  }
}

/* Copy 4 bytes from @p *pa to int32_t, changing endianness if necessary. */
static inline int32_t sw32_(const void* pa) {
  int32_t idest;

  bool little_endian = is_little_endian();
  if (little_endian) {
    idest = *(int32_t *)pa;
  }
  else {
#if defined (__GNUC__)
    return __builtin_bswap32(*(unsigned int *)pa);
#elif defined (_MSC_VER) /* Visual Studio */
    return _byteswap_ulong(*(unsigned int *)pa);
#else
    uint8_t *dest = (uint8_t *)&idest;
    dest[0] = pa_[3];
    dest[1] = pa_[2];
    dest[2] = pa_[1];
    dest[3] = pa_[0];
#endif
  }
  return idest;
}

/* Copy 4 bytes from int32_t to @p *dest, changing endianness if necessary. */
static inline void _sw32(void* dest, int32_t a) {
  uint8_t* dest_ = (uint8_t*)dest;
  uint8_t* pa = (uint8_t*)&a;

  bool little_endian = is_little_endian();
  if (little_endian) {
    *(int32_t *)dest_ = a;
  }
  else {
#if defined (__GNUC__)
    *(int32_t *)dest_ = __builtin_bswap32(*(unsigned int *)pa);
#elif defined (_MSC_VER) /* Visual Studio */
    *(int32_t *)dest_ = _byteswap_ulong(*(unsigned int *)pa);
#else
    dest_[0] = pa[3];
    dest_[1] = pa[2];
    dest_[2] = pa[1];
    dest_[3] = pa[0];
#endif
  }
}

/* Reverse swap bits in a 32-bit integer */
static inline int32_t bswap32_(int32_t a) {
#if defined (__GNUC__)
  return __builtin_bswap32(a);

#elif defined (_MSC_VER) /* Visual Studio */
  return _byteswap_ulong(a);
#else
  a = ((a & 0x000000FF) << 24) |
      ((a & 0x0000FF00) <<  8) |
      ((a & 0x00FF0000) >>  8) |
      ((a & 0xFF000000) >> 24);
  return a;
#endif
}

/**
 * @brief Register a filter in Blosc.
 *
 * @param filter The filter to register.
 *
 * @return 0 if succeeds. Else a negative code is returned.
 */
int register_filter_private(blosc2_filter *filter);

/**
 * @brief Register a codec in Blosc.
 *
 * @param codec The codec to register.
 *
 * @return 0 if succeeds. Else a negative code is returned.
 */
int register_codec_private(blosc2_codec *codec);

#ifdef __cplusplus
}
#endif

#endif //IARRAY_BLOSC_PRIVATE_H
