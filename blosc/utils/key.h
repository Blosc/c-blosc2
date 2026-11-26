/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_KEY_H
#define BLOSC_KEY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* This is the function that the EmbedStore uses to check whether the keys passed to it are valid */
bool key_embed_is_valid(const char* key);

/* Returns true when key is already in canonical form.  Never allocates. */
bool key_is_canonical(const uint8_t* key);

/* Canonicalizes key in place.  On success, writes the resulting length to
 * out_length when it is not NULL.  The buffer is left in an unspecified
 * state on error. */
int key_normalize_inplace(uint8_t* key, size_t* out_length);

/* Returns a canonical copy owned by the caller.  The caller must free it
 * with free().  On error, *out is set to NULL. */
int key_normalize_copy(const uint8_t* key, uint8_t** out);

#endif //BLOSC_KEY_H
