/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_HASH_MAP_H
#define BLOSC_HASH_MAP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint8_t* key;
    uint64_t hash;
}hash_map_key;

typedef void (*cb_free_value)(void* value);

typedef struct {
    size_t capacity;
    size_t length;
    size_t tombstones;
    size_t value_size; // This value size is aligned forward

    hash_map_key* keys;
    void* values;
    cb_free_value free_value; // Callback used to free a value. The user must insert the callback
}hash_map;

typedef struct {
    const hash_map* hm;
    size_t index;
    hash_map_key* key;
    void* value;
}hash_map_iter;

// TODO: Check that value_size cannot create unaligned members in hash_map.values
// NOTE: free_value can be NULL
int hash_map_init(hash_map* hm, size_t value_size, cb_free_value free_value);

// NOTE: free_value can be NULL
int hash_map_new(hash_map** phm, size_t value_size, cb_free_value);

/* Free functions */
void hash_map_free_pairs(hash_map* hm);
void hash_map_free_struct(hash_map* hm);
void hash_map_free(hash_map* hm);

bool hash_map_contains_key(const hash_map* hm, const uint8_t* key);
void* hash_map_get(const hash_map* hm, const uint8_t* key);

// NOTE: The map never takes ownership of the @param key
int hash_map_set(hash_map* hm, const uint8_t* key, void* value);

// This function can, POTENTIALLY, take ownership of the key
int hash_map_put(hash_map* hm, uint8_t* key, void* value, bool* pneeds_free);

bool hash_map_remove(hash_map* hm, const uint8_t* key);

// True if removed, false if not. This function cannot fail, that means that cannot leave hm in an invalid state
// If out_value is not NULL, then the function copies the contents of the value associated with the key
// to the pointer in out_value. It is up to the user to make out_value with the right size
bool hash_map_remove_take(hash_map* hm, const uint8_t* key, void* out_value);

size_t hash_map_get_length(const hash_map* hm);

// TODO: If value size is padded, then this function must return the padded value size that the hash_map will use internally
size_t hash_map_get_value_size(const hash_map* hm);

/* Iter API */
hash_map_iter* hash_map_iter_new(const hash_map* hm);
void hash_map_iter_init(hash_map_iter* iter, const hash_map* hm);
bool hash_map_iter_next(hash_map_iter* iter);
const uint8_t* hash_map_iter_get_key(const hash_map_iter* iter);
void* hash_map_iter_get_value(const hash_map_iter* iter);
void hash_map_iter_free(hash_map_iter* iter);

#endif //BLOSC_HASH_MAP_H
