/*********************************************************************
Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "hash.h"

uint64_t hash_fnv_1a(const char *key) {
    uint64_t hash = FNV_1A_OFFSET;
    for (const char* p = key; *p != '\0'; p++) {
        hash ^= (uint64_t)(unsigned char)(*p);
        hash *= FNV_1A_PRIME;
    }
    return hash;
}