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
#include <embed-store/embed-store-private.h>
#include <utils/hash_map.h>

#define BLOSC2_DICT_STORE_INITIAL_CAPACITY 16
#define BLOSC2_DICT_STORE_LOAD_FACTOR 0.75f
#define BLOSC2_DICT_STORE_RESIZE_FACTOR 2

typedef union {

    struct {
        int64_t offset;
        size_t length;
    }f_zip;

    struct {
        uint8_t* localpath; // (note): Localpath is used to store de filesystem path of the file associated in the key
        size_t localpath_length; // (note): Storing this, the amount of strlen made is less
    }f_dir;

    struct {
        blosc2_schunk* schunk;
    }f_mem;

}b2_ds_value;

struct blosc2_dict_store {
    hash_map map;

    uint8_t* urlpath; // When storage_type is B2_DS_MEMORY_CONTIGUOUS this field stores the pointer to the buffer
    size_t urlpath_length;
    uint8_t* tmpdir;
    size_t tmpdir_length;

    blosc2_embed_store* estore;
    int64_t threshold;

    b2_ds_open_mode open_mode;
    b2_ds_storage_type storage_type;
};

typedef struct {
    const uint8_t* urlpath;
    size_t urlpath_length;
    const uint8_t* tmpdir;
    size_t tmpdir_length;
    b2_ds_open_mode open_mode;
    b2_ds_storage_type storage_type;
}b2_ds_default;

typedef enum {
    B2_DS_ITER_EMBED,
    B2_DS_ITER_DICT,
}b2_ds_iter_state;

struct blosc2_dict_store_iter {
    union {
        hash_map_iter map_iter;
        blosc2_embed_store_iter embed_iter;
    };
    blosc2_dict_store* dstore;
    b2_ds_iter_state state;
};

#endif // BLOSC_DICT_STORE_PRIVATE_H
