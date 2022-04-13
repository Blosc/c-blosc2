/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/



#ifndef NDLZ_PRIVATE_H
#define NDLZ_PRIVATE_H
#include "context.h"

#if defined (__cplusplus)
extern "C" {
#endif
#define XXH_INLINE_ALL

#define NDLZ_ERROR_NULL(pointer)         \
    do {                                 \
        if ((pointer) == NULL) {         \
            return 0;                    \
        }                                \
    } while (0)


#if defined (__cplusplus)
}
#endif

#endif /* NDLZ_PRIVATE_H */
