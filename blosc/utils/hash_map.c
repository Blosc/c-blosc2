/*********************************************************************
Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "hash_map.h"
#include <blosc2.h>
#include "string_manipulation.h"
#include "hash.h"

#include <string.h>
#include <assert.h>

#define HASH_MAP_INITIAL_CAPACITY 16
#define HASH_MAP_LOAD_FACTOR 0.70
#define HASH_MAP_RESIZE_POLICY 2

static uint8_t tombstone_sentinel;
#define TOMBSTONE (&tombstone_sentinel)

typedef struct {
    hash_map_key* key;
    void* value;
}pair;

/* == Static functions == */

static bool s_key_is_live(const hash_map_key* key) {
    return key->key != NULL && key->key != TOMBSTONE;
}

static bool s_key_is_empty(const hash_map_key* key) {
    return key->key == NULL;
}

static bool s_key_is_tombstone(const hash_map_key* key) {
    return key->key == TOMBSTONE;
}

static void* s_map_calc_value_ptr(const hash_map* hm, size_t index) {
    return (void*) ( (uintptr_t)hm->values + index * hm->value_size );
}

static pair s_map_find_pair_for_key(
    hash_map* hm,
    const uint8_t* key,
    uint64_t hash
) {
    hash_map_key* keys = hm->keys;
    size_t capacity = hm->capacity;
    size_t index = hash % capacity;
    hash_map_key* p = &keys[index];
    hash_map_key* first_tomb = NULL;
    size_t first_tomb_index = index;
    pair pp = {0};

    while (!s_key_is_empty(p)) {
        if (s_key_is_tombstone(p)) {
            if (first_tomb == NULL) {
                first_tomb = p;
                first_tomb_index = index;
            }
        } else if (p->hash == hash && strcmp((const char*)p->key, (const char*)key) == 0) {
            //p->value = *ds_value;
            pp.key = p;
            pp.value = s_map_calc_value_ptr(hm, index);
            return pp;
        }

        index += 1;
        if (index >= capacity) index = 0;
        p = &keys[index];
    }

    if (first_tomb != NULL) {
        p = first_tomb;
        index = first_tomb_index;
    }
    pp.key = p;
    pp.value = s_map_calc_value_ptr(hm, index);
    return pp;
}


static bool s_map_get_pair(
    const hash_map* hm,
    const uint8_t* key,
    pair* ppair
) {
    uint64_t hash = hash_fnv_1a((const char*)key);
    size_t index = hash % hm->capacity;
    for (size_t i = 0; i < hm->capacity; i+=1) {
        hash_map_key* pkey = &hm->keys[index];
        if (s_key_is_empty(pkey)) return false;
        if (s_key_is_live(pkey) && pkey->hash == hash && strcmp((const char*)key, (const char*)pkey->key) == 0) {
            if (ppair != NULL) {
                void* value = s_map_calc_value_ptr(hm, index);
                ppair->key = pkey;
                ppair->value = value;
            }

            return true;
        }

        index += 1;
        if (index >= hm->capacity) index = 0;
    }
    return false;
}

static bool s_map_needs_resize(const hash_map* hm) {
    return (hm->length + hm->tombstones) >= HASH_MAP_LOAD_FACTOR * hm->capacity;
}

static int s_map_resize(hash_map* hm) {
    size_t new_capacity = hm->capacity * HASH_MAP_RESIZE_POLICY;

    hash_map_key* new_keys = calloc(new_capacity, sizeof(hash_map_key));
    if (new_keys == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate memory for new keys");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    void* new_values = malloc(new_capacity * hm->value_size);
    if (new_values == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate memory for new values");
        free(new_keys);
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }

    // Rehash into the new arrays, moving key ownership to them
    for (size_t i = 0; i < hm->capacity; i+=1) {
        hash_map_key* key = &hm->keys[i];
        if (s_key_is_live(key)) {
            size_t index = key->hash % new_capacity;
            while (!s_key_is_empty(&new_keys[index])) {
                index += 1;
                if (index >= new_capacity) index = 0;
            }
            new_keys[index].key = key->key;
            new_keys[index].hash = key->hash;
            memcpy((uint8_t*)new_values + (index * hm->value_size),
                   s_map_calc_value_ptr(hm, i), hm->value_size);
            key->key = NULL;
        }
    }

    free(hm->keys);
    free(hm->values);
    hm->tombstones = 0;
    hm->capacity = new_capacity;
    hm->keys = new_keys;
    hm->values = new_values;

    return BLOSC2_ERROR_SUCCESS;
}

static void s_map_free_pairs_no_cb(hash_map* hm) {
    hash_map_iter iter = {0};
    iter.hm = hm;
    while (hash_map_iter_next(&iter)) {
        free(iter.key->key);
    }
}

static void s_map_free_pairs_with_cb(hash_map* hm) {
    hash_map_iter iter = {0};
    iter.hm = hm;
    while (hash_map_iter_next(&iter)) {
        free(iter.key->key);
        hm->free_value(iter.value);
    }
}

/* == END Static functions == */

int hash_map_init(hash_map* hm, size_t value_size, cb_free_value free_value) {
    assert(hm != NULL);
    assert(value_size > 0);

    // TODO: Check for value_size(s) that end up in unaligned buffer members
    memset(hm, 0, sizeof(hash_map));

    hash_map_key* keys = calloc(HASH_MAP_INITIAL_CAPACITY, sizeof(hash_map_key));
    if (keys == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate memory for keys");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    void* values = malloc(HASH_MAP_INITIAL_CAPACITY * value_size);
    if (values == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate memory for values");
        free(keys);
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }

    hm->keys = keys;
    hm->values = values;
    hm->value_size = value_size;
    hm->capacity = HASH_MAP_INITIAL_CAPACITY;
    hm->free_value = free_value;
    return BLOSC2_ERROR_SUCCESS;
}

int hash_map_new(hash_map** phm, size_t value_size, cb_free_value free_value) {
    assert(phm != NULL);
    assert(value_size > 0);

    if (phm == NULL) {
        BLOSC_TRACE_ERROR("phm cannot be equal to NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    *phm = NULL;

    hash_map* hm = malloc(sizeof(hash_map));
    if (hm == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate memory for hash_map");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    int rc = hash_map_init(hm, value_size, free_value);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not init allocated hash_map");
        free(hm);
        return rc;
    }
    *phm = hm;
    return rc;
}

void hash_map_free_pairs(hash_map* hm) {
    assert(hm != NULL);

    if (hm->free_value == NULL) {
        s_map_free_pairs_no_cb(hm);
    } else {
        s_map_free_pairs_with_cb(hm);
    }
    hm->length = 0;
}

void hash_map_free_struct(hash_map* hm) {
    assert(hm != NULL);

    hash_map_free_pairs(hm);
    free(hm->keys);
    free(hm->values);
}

void hash_map_free(hash_map* hm) {
    assert(hm != NULL);

    hash_map_free_struct(hm);
    free(hm);
}

bool hash_map_contains_key(const hash_map* hm, const uint8_t* key) {
    assert(hm != NULL);
    assert(key != NULL);

    return s_map_get_pair(hm, key, NULL);
}

void* hash_map_get(const hash_map* hm, const uint8_t* key) {
    assert(hm != NULL);
    assert(key != NULL);

    pair p = {0};
    bool found = s_map_get_pair(hm, key, &p);
    if (found) {
        return p.value;
    }
    return NULL;
}

int hash_map_set(hash_map* hm, const uint8_t* key, void* value) {
    assert(hm != NULL);
    assert(key != NULL);
    assert(value != NULL);
    if (s_map_needs_resize(hm)) {
        int rc = s_map_resize(hm);
        if (rc != 0) {
            return rc;
        }
    }

    uint64_t hash = hash_fnv_1a((const char*)key);
    pair p = s_map_find_pair_for_key(hm, key, hash);
    bool is_update = s_key_is_live(p.key);
    if (!is_update) {
        uint8_t* dup = NULL;
        int rc = blosc2_string_duplicate(key, &dup);
        if (rc != 0) {
            BLOSC_TRACE_ERROR("Could not allocate new key");
            return rc;
        }
        if (s_key_is_tombstone(p.key)) {
            hm->tombstones -= 1;
        }
        p.key->key = dup;
        p.key->hash = hash;
        hm->length += 1;
    }
    // Only an already existing value has to be released before being overwritten
    if (is_update && hm->free_value) {
        hm->free_value(p.value);
    }
    memcpy(p.value, value, hm->value_size);
    return BLOSC2_ERROR_SUCCESS;
}

int hash_map_put(hash_map* hm, uint8_t* key, void* value, bool* pneeds_free) {
    assert(hm != NULL);
    assert(key != NULL);
    assert(value != NULL);
    assert(pneeds_free != NULL);
    if (s_map_needs_resize(hm)) {
        int rc = s_map_resize(hm);
        if (rc != 0) {
            return rc;
        }
    }

    bool needs_free = true;
    uint64_t hash = hash_fnv_1a((const char*)key);
    pair p = s_map_find_pair_for_key(hm, key, hash);
    bool is_update = s_key_is_live(p.key);
    if (!is_update) {
        if (s_key_is_tombstone(p.key)) {
            hm->tombstones -= 1;
        }
        p.key->key = key;
        p.key->hash = hash;
        hm->length += 1;
        needs_free = false;
    }
    // Only an already existing value has to be released before being overwritten
    if (is_update && hm->free_value) {
        hm->free_value(p.value);
    }
    memcpy(p.value, value, hm->value_size);
    *pneeds_free = needs_free;
    return BLOSC2_ERROR_SUCCESS;
}

bool hash_map_remove(hash_map* hm, const uint8_t* key) {
    assert(hm != NULL);
    assert(key != NULL);

    pair p = {0};
    bool found = s_map_get_pair(hm, key, &p);
    if (found) {
        free(p.key->key);
        p.key->key = TOMBSTONE;
        p.key->hash = 0;
        hm->tombstones +=1;
        hm->length -= 1;
        if (hm->free_value != NULL) {
            hm->free_value(p.value);
        }
    }
    return found;
}

bool hash_map_remove_take(hash_map* hm, const uint8_t* key, void* out_value) {
    assert(hm != NULL);
    assert(key != NULL);
    assert(out_value != NULL);

    pair p = {0};
    bool found = s_map_get_pair(hm, key, &p);
    if (found) {
        free(p.key->key);
        p.key->key = TOMBSTONE;
        p.key->hash = 0;
        hm->tombstones +=1;
        hm->length -= 1;
        memcpy(out_value, p.value, hm->value_size);
    }
    return found;
}

size_t hash_map_get_length(const hash_map* hm) {
    assert(hm != NULL);

    return hm->length;
}

size_t hash_map_get_value_size(const hash_map* hm) {
    assert(hm != NULL);

    return hm->value_size;
}

hash_map_iter* hash_map_iter_new(const hash_map* hm) {
    assert(hm != NULL);

    hash_map_iter* iter = calloc(1, sizeof(hash_map_iter));
    if (iter == NULL) {
        return NULL;
    }
    hash_map_iter_init(iter, hm);
    return iter;
}

void hash_map_iter_init(hash_map_iter* iter, const hash_map* hm) {
    assert(iter != NULL);
    assert(hm != NULL);

    iter->hm = hm;
}

bool hash_map_iter_next(hash_map_iter* iter) {
    assert(iter != NULL);

    size_t index = iter->index;
    const hash_map* hm = iter->hm;
    for (; index < hm->capacity; index+=1) {
        hash_map_key* pkey = &hm->keys[index];
        if (s_key_is_live(pkey)) {
            iter->key = pkey;
            iter->value = s_map_calc_value_ptr(hm, index);
            iter->index = index + 1;
            return true;
        }
    }

    iter->index = hm->capacity;
    return false;
}

const uint8_t* hash_map_iter_get_key(const hash_map_iter* iter) {
    assert(iter != NULL);

    return iter->key->key;
}

void* hash_map_iter_get_value(const hash_map_iter* iter) {
    assert(iter != NULL);

    return iter->value;
}

void hash_map_iter_free(hash_map_iter* iter) {
    assert(iter != NULL);

    free(iter);
}
