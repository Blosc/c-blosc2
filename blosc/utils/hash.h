/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_HASH_H
#define BLOSC_HASH_H

#include <stdint.h>

#define FNV_1A_PRIME (1099511628211UL)
#define FNV_1A_OFFSET (14695981039346656037UL)
uint64_t hash_fnv_1a(const char *key);

#endif //BLOSC_HASH_H
