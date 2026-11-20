#include "dict-store-private_.h"

typedef blosc2_dict_store dict_store;

/* == Static functions == */



/* == END Static functions == */

int blosc2_dict_store_new(blosc2_dict_store** dstore, const uint8_t* urlpath) {

}

int blosc2_dict_store_open(blosc2_dict_store** dstore, const uint8_t* urlpath, b2_ds_open_mode open_mode) {

}

int blosc2_dict_store_close(blosc2_dict_store* dstore) {

}

void blosc2_dict_store_free(blosc2_dict_store* dstore) {

}

int blosc2_dict_store_to_b2z( blosc2_dict_store *ds, const uint8_t* destination_urlpath ) {

}

int blosc2_dict_store_to_b2d( blosc2_dict_store *ds, const uint8_t* urlpath ) {

}

int blosc2_dict_store_to_buffer( blosc2_dict_store *ds, void** pbuffer, size_t* psize ) {

}

int blosc2_dict_store_from_buffer(blosc2_dict_store** pds, void* buffer, size_t size) {

}

size_t blosc2_dict_store_get_length( blosc2_dict_store *ds ) {

}

size_t blosc2_dict_store_get_capacity( blosc2_dict_store *ds ) {

}

b2_ds_open_mode blosc2_dict_store_get_open_mode( blosc2_dict_store *ds ) {

}

int blosc2_dict_store_get_storage_type( blosc2_dict_store *ds ) {

}

bool blosc2_dict_store_contains_key( blosc2_dict_store *ds, const uint8_t* key ) {

}

int blosc2_dict_store_get(blosc2_dict_store* ds, const uint8_t* key, blosc2_schunk** pcontainer) {

}

int blosc2_dict_store_set(blosc2_dict_store* ds, const uint8_t* key, blosc2_schunk* container) {

}

int blosc2_dict_store_remove(blosc2_dict_store* ds, const uint8_t* key) {

}

blosc2_dict_store_iter* blosc2_dict_store_iter_new( blosc2_dict_store *ds ) {

}

void blosc2_dict_store_iter_free( blosc2_dict_store_iter* iter ) {

}

bool blosc2_dict_store_iter_next( blosc2_dict_store_iter *iter ) {

}

const uint8_t* blosc2_dict_store_iter_get_key( blosc2_dict_store_iter *iter ) {

}

int blosc2_dict_store_iter_get_container( blosc2_dict_store_iter *iter, blosc2_schunk** pcontainer ) {

}
