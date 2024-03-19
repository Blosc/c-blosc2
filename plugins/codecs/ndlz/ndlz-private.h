/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef NDLZ_PRIVATE_H
#define NDLZ_PRIVATE_H

#include <stddef.h>

#define XXH_INLINE_ALL

#define NDLZ_ERROR_NULL(pointer) \
  do {                           \
    if ((pointer) == NULL) {     \
      return 0;                  \
    }                            \
  } while (0)

#endif /* NDLZ_PRIVATE_H */
