//
// Created by thorns on 20/11/26.
//

#ifndef BLOSC_DICT_STORE_PRIVATE__H
#define BLOSC_DICT_STORE_PRIVATE__H

#include <blosc2/dict-store.h>
#include <utils/hash_map.h>

typedef union {

    struct {
        int64_t offset;
        size_t length;
    }f_local;

    struct {
        uint8_t* localpath;      // (note): Localpath is used to store de filesystem path of the file associated in the key
        size_t localpath_length; // (note): Storing this, the amount of strlen made is less
    }f_zip;

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

    void* vtable;
    b2_ds_open_mode open_mode;
    int storage_type;
};

struct blosc2_dict_store_iter {
    hash_map_iter iter;
    size_t index;
};

#endif //BLOSC_DICT_STORE_PRIVATE__H
