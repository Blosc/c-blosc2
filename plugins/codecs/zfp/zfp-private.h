/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_PLUGINS_CODECS_ZFP_ZFP_PRIVATE_H
#define BLOSC_PLUGINS_CODECS_ZFP_ZFP_PRIVATE_H

#include <stddef.h>

#define ZFP_MAX_DIM 4
#define ZFP_CELL_SHAPE 4

#define XXH_INLINE_ALL

#define ZFP_ERROR_NULL(pointer) \
  do {                          \
    if ((pointer) == NULL) {    \
      return 0;                 \
    }                           \
  } while (0)

#endif /* BLOSC_PLUGINS_CODECS_ZFP_ZFP_PRIVATE_H */
