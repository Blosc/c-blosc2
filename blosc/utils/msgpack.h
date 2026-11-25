/*********************************************************************
Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_MSGPACK_H
#define BLOSC_MSGPACK_H

#include <stdint.h>
#include <stddef.h>
#include <stores.h>

/*
The following functions cannot deserialize an arbitrary hash_map. This was done specifically to support the EmbedStore
hash_map struct and its logic.
On failure, the deserialize functions do not free the memory in the embed store. This is because, embed-store.c already does that
*/

// On success returns by pointer the new allocated buffer and its size
int msgpack_embed_store_serialize(const blosc2_embed_store* es, uint8_t** pout_buffer, size_t* pout_buffer_size);

/* es must be a valid pointer to a blosc2_embed_store struct */
/* On success, allocates a new blosc2_embed_store and returns it by pointer */
/* On failure, the state of the inner hash_map in the blosc2_embed_store is invalid and should be cleaned up */
int msgpack_embed_store_deserialize(const uint8_t* packed_buffer, size_t packed_buffer_size, blosc2_embed_store* es);

/* This function does the same as the above, with the difference that is reads the first version of the EmbedStore serialization. See the docs README_B2E.md */
int msgpack_embed_store_deserialize_v1(const uint8_t* packed_buffer, size_t packed_buffer_size, blosc2_embed_store* es);

/* On success, returns the version, a positive value, on error, a negative error code */
int msgpack_validate_and_return_version(const uint8_t* packed, size_t packed_size);

/* Version is a positive integer */
int msgpack_container_serialize_version( size_t version, uint8_t** pout_buffer, size_t* psize );

#endif //BLOSC_MSGPACK_H
