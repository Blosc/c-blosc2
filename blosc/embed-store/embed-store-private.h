/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_EMBED_STORE_PRIVATE_H
#define BLOSC_EMBED_STORE_PRIVATE_H

#include <stores.h>
#include <utils/hash_map.h>

typedef enum {
    B2_ES_V1 = 1,
    B2_ES_V2 = 2,
}b2_es_version;

typedef struct {
    int64_t offset;
    size_t length;
}b2_es_value_local;

typedef struct {
    uint8_t* urlbase;
    uint8_t* path;
}b2_es_value_remote;

typedef struct {
    union {
        b2_es_value_local f_local;
        b2_es_value_remote f_remote;
    };
    blosc2_store_value_tag tag;
}b2_es_value;

struct blosc2_embed_store {
    blosc2_schunk *schunk;
    int64_t current_offset;
    hash_map hm;
    blosc2_store_open_mode mode;
};

struct blosc2_embed_store_iter {
    blosc2_embed_store* es;
    hash_map_iter map_iter;
};

/* The following functions must be used ONLY by bosc2, not by the end user */

void priv_embed_store_free_value(b2_es_value* value);
void priv_embed_store_iter_init(blosc2_embed_store_iter* iter, blosc2_embed_store* estore);

#endif //BLOSC_EMBED_STORE_PRIVATE_H
