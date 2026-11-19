/*********************************************************************
Blosc - Blocked Shuffling and Compression Library

Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
https://blosc.org
License: BSD 3-Clause (see LICENSE.txt)

See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_DICT_STORE_PRIVATE_H
#define BLOSC_DICT_STORE_PRIVATE_H

#include <blosc2/dict-store.h>
#include <utils/ziputils.h>

#define BLOSC2_DICT_STORE_INITIAL_CAPACITY 16
#define BLOSC2_DICT_STORE_LOAD_FACTOR 0.75f
#define BLOSC2_DICT_STORE_RESIZE_FACTOR 2

typedef union {

    struct {
        int64_t offset;
        size_t length;
    }f_zip;

    struct {
        const uint8_t* localpath; // (note): Localpath is used to store de filesystem path of the file associated in the key
        size_t localpath_length; // (note): Storing this, the amount of strlen made is less
    }f_dir;

    struct {
        blosc2_schunk* schunk;
    }f_mem;

}b2_ds_value;

typedef struct {
    const uint8_t* key;
    uint64_t hash;
    b2_ds_value value;
}b2_ds_pair;

struct blosc2_dict_store {
    b2_ds_pair *pairs;
    size_t capacity;
    size_t length;          // Live pairs
    size_t tombstones;      // Tombstones

    const uint8_t* urlpath; // When storage_type is B2_DS_MEMORY_CONTIGUOUS this field stores the pointer to the buffer
    size_t urlpath_length;
    const uint8_t* tmpdir;
    size_t tmpdir_length;

    b2_ds_open_mode open_mode;
    int storage_type;
};

typedef struct {
    b2_ds_pair *pairs;
    size_t capacity;
    size_t length;
    const uint8_t* urlpath;
    size_t urlpath_length;
    const uint8_t* tmpdir;
    size_t tmpdir_length;
    b2_ds_open_mode open_mode;
    int storage_type;
}b2_ds_default;

struct blosc2_dict_store_iter {
    b2_ds_pair* p;
    blosc2_dict_store *ds;
    size_t index;
};

#endif // BLOSC_DICT_STORE_PRIVATE_H
