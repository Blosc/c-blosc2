/*********************************************************************
Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "embed-store-private.h"
#include "string_manipulation.h"
#include <utils/msgpack.h>

#include <sys/stat.h>
#include <assert.h>
#include <errno.h>

#include "filesystem.h"

typedef blosc2_embed_store embed_store;
typedef b2_es_value value;

/* == Static functions == */

static const char* s_vlmeta_key_msgpack_dict = "estore_metadata";
static const char* s_meta_key_version = "b2embed";
static const char s_invalid_key_chars[] = ":\0\n\r\t"; // TODO: Check if '.' and ".." should be admitted (recall that ".." is not a character, it is a string)
static const size_t s_invalid_key_chars_size = sizeof(s_invalid_key_chars) - 1;

static value s_value_create_local(size_t offset, size_t length) {
    return (value){.tag = BLOSC2_STORE_VALUE_SCHUNK, .f_local = { .offset = offset, .length = length }};
}

static b2_es_value_local s_value_get_local(value* pvalue) {
    return pvalue->f_local;
}

// It does not matter if urlpath is NULL
static blosc2_schunk* s_embed_create_schunk_with_default_storage(const uint8_t* urlpath, blosc2_io* pio) {
    blosc2_storage storage = blosc2_get_blosc2_storage_defaults();
    blosc2_cparams cparams = blosc2_get_blosc2_cparams_defaults();
    cparams.typesize = 1;
    blosc2_dparams dparams = blosc2_get_blosc2_dparams_defaults();
    storage.cparams = &cparams;
    storage.dparams = &dparams;
    if (pio == NULL) {
        storage.io = (blosc2_io*)&BLOSC2_IO_DEFAULTS;
    } else {
        storage.io = pio;
    }
    storage.urlpath = (char*)urlpath;
    storage.contiguous = true;
    return blosc2_schunk_new(&storage);
}

// NOTE: This callback is used by the hash map to free the values when needed
static void hm_value_free_cb(void* pvalue) {
    value* val = (value*)pvalue;
    if (val->tag == BLOSC2_STORE_VALUE_REMOTE) {
        free(val->f_remote.urlbase);
        free(val->f_remote.path);
    }
}

void priv_embed_store_free_value(b2_es_value* val) {
    hm_value_free_cb(val);
}

static int s_embed_alloc(embed_store** pes, blosc2_schunk* schunk) {
    embed_store* es = calloc(1, sizeof(embed_store));
    if (es == NULL) {
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    int rc = hash_map_init(&es->hm, sizeof(value), hm_value_free_cb);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not initialize EmbedStore's inner hash map");
        free(es);
        return rc;
    }
    es->schunk = schunk;
    *pes = es;
    return BLOSC2_ERROR_SUCCESS;
}

static bool s_embed_key_is_valid(const uint8_t* key) {
    size_t key_len = strlen((const char*)key);
    if (key_len == 0) {
        BLOSC_TRACE_ERROR("Key cannot be empty");
        return false;
    }
    if (key[0] != '/') {
        BLOSC_TRACE_ERROR("The key must always start with '/'");
        return false;
    }
    if (key_len > 1 && key[key_len - 1] == '/') {
        BLOSC_TRACE_ERROR("Unless the key is the root key, it cannot end with a '/'");
        return false;
    }
    // TODO: Maybe use a better method (hash tables?... maybe?)
    for (size_t i = 0; i < key_len; i+=1) {
        for (size_t j = 0; j < s_invalid_key_chars_size; j+=1) {
            if (key[i] == s_invalid_key_chars[j]) {
                BLOSC_TRACE_ERROR("Key cannot contain '%c'", s_invalid_key_chars[j]);
                return false;
            }
        }
    }

    return true;
}

static int s_embed_load_local_schunk(blosc2_schunk* origin, b2_es_value_local local, blosc2_schunk** destination) {

    size_t offset = local.offset;
    size_t length = local.length;

    uint8_t* cframe = malloc(length);
    if (cframe == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate space for cframe");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    int rc = blosc2_schunk_get_slice_buffer(origin, (int64_t)offset, (int64_t)(offset + length), (void*)cframe);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not retrieve slice buffer with start at %ld and stop at %ld", (int64_t)offset, (int64_t)(offset + length));
        free(cframe);
        return rc;
    }

    // TODO: According with the [blosc2_schunk_from_buffer docs](https://blosc.org/c-blosc2/reference/schunk.html#c.blosc2_schunk_from_buffer)
    // the innner cframe should be fred by blosc2_schunk_free. This does not really happen. when te funcion is called (from_buffer)
    // the function frame_from_cframe puts frame->avoid_cframe_free = true. So the frame never gets freed
    // The fact that in this functionm, copy is setted to true, it's just provisional
    blosc2_schunk* schunk = blosc2_schunk_from_buffer(cframe, (int64_t)length, true);
    if (schunk == NULL) {
        BLOSC_TRACE_ERROR("Could not obtain schunk from cframe");
        rc = BLOSC2_ERROR_SCHUNK_COPY;
    }
    free(cframe);
    if (rc == 0) {
        *destination = schunk;
    }
    return rc;
}

static int s_embed_load_value(embed_store* es, b2_es_value* value, blosc2_store_value* item) {
    int rc = BLOSC2_ERROR_SUCCESS;
    switch (value->tag) {
    case BLOSC2_STORE_VALUE_SCHUNK:
        blosc2_schunk* schunk = NULL;
        rc = s_embed_load_local_schunk(es->schunk, s_value_get_local(value), &schunk);
        if (rc < 0) {
            BLOSC_TRACE_ERROR("Could not load local SChunk");
            return rc;
        }
        item->tag = BLOSC2_STORE_VALUE_SCHUNK;
        item->schunk = schunk;
        break;
    case BLOSC2_STORE_VALUE_REMOTE:
        item->tag = BLOSC2_STORE_VALUE_REMOTE;
        item->remote.urlbase = value->f_remote.urlbase;
        item->remote.path = value->f_remote.path;
        break;
    default:
        BLOSC_TRACE_ERROR("Invalid value tag found");
        rc = BLOSC2_ERROR_INVALID_PARAM;
        break;
    }
    return rc;
}

/*
 * Escribe buffer[0 .. stop-start) en el rango [start, stop) del schunk,
 * extendiéndolo si stop > nbytes actual.
 *
 * Precondiciones (responsabilidad del llamador):
 *   - schunk != NULL, buffer != NULL, schunk->typesize == 1
 *   - 0 <= start <= stop, ya normalizados
 *   - buffer_len >= stop - start
 *   - Sin huecos: start <= schunk->nbytes
 *   - Locking a cargo del llamador
 *
 * Un schunk vacío tiene chunksize == -1: el primer append lo fija.
 */
static int s_set_slice(blosc2_schunk *schunk, int64_t start, int64_t stop,
                       const void *buffer, int64_t buffer_len) {
    assert(schunk != NULL);
    assert(buffer != NULL);
    assert(buffer_len >= 0);
    assert(schunk->typesize == 1);
    assert(start >= 0 && start <= stop);
    assert(stop - start <= buffer_len);
    assert(start <= schunk->nbytes);

    const uint8_t *src = (const uint8_t *)buffer;
    int64_t index = start;
    int rc = BLOSC2_ERROR_SUCCESS;

    while (index < stop) {
        const int64_t remaining = stop - index;
        const int64_t nbytes = schunk->nbytes;      /* se relee: cambia en cada vuelta */
        const int64_t chunksize = schunk->chunksize;
        const uint8_t *cur = src + (index - start); /* el puntero avanza */
        int64_t to_fill;

        if (chunksize <= 0) {
            /* Schunk vacío: el primer append fija chunksize */
            if (remaining > INT32_MAX) {
                rc = BLOSC2_ERROR_INVALID_PARAM;
                break;
            }
            to_fill = remaining;
            int64_t res = blosc2_schunk_append_buffer(schunk, (void *)cur, (int32_t)to_fill);
            if (res < 0) {
                BLOSC_TRACE_ERROR("Could not append the first chunk");
                rc = (int)res;
                break;
            }
        } else {
            const int64_t offset = index % chunksize;           /* posición dentro del chunk */
            const int64_t room = chunksize - offset;
            to_fill = remaining < room ? remaining : room;

            if (index == nbytes && offset == 0) {
                /* Chunk nuevo */
                int64_t res = blosc2_schunk_append_buffer(schunk, (void *)cur, (int32_t)to_fill);
                if (res < 0) {
                    BLOSC_TRACE_ERROR("Could not append the chunk");
                    rc = (int)res;
                    break;
                }
            } else if (index + to_fill <= nbytes) {
                /* Todo cae dentro de datos existentes: sobrescritura simple */
                rc = blosc2_schunk_set_slice_buffer(schunk, index, index + to_fill, (void *)cur);
                if (rc < 0) {
                    BLOSC_TRACE_ERROR("There was an error setting the slice");
                    break;
                }
            } else {
                /* Último chunk parcial que hay que extender:
                 * descomprimir, sobrescribir/rellenar, recomprimir y actualizar */
                const int64_t last = schunk->nchunks - 1;
                const int32_t old_len = (int32_t)(nbytes - last * chunksize);
                const int32_t new_len = (int32_t)(offset + to_fill);

                uint8_t *data = malloc((size_t)new_len);
                uint8_t *out = malloc((size_t)new_len + BLOSC2_MAX_OVERHEAD);
                if (data == NULL || out == NULL) {
                    free(data); free(out);
                    rc = BLOSC2_ERROR_MEMORY_ALLOC;
                    break;
                }

                rc = blosc2_schunk_decompress_chunk(schunk, last, data, new_len);
                if (rc == old_len) {
                    memcpy(data + offset, cur, (size_t)to_fill);
                    rc = blosc2_compress_ctx(schunk->cctx, data, new_len,
                                             out, new_len + BLOSC2_MAX_OVERHEAD);
                    if (rc > 0)
                        rc = blosc2_schunk_update_chunk(schunk, last, out, true);
                    else if (rc == 0)
                        rc = BLOSC2_ERROR_FAILURE;
                } else if (rc >= 0) {
                    rc = BLOSC2_ERROR_FAILURE;  /* tamaño descomprimido inesperado */
                }
                free(data); free(out);
                if (rc < 0) {
                    BLOSC_TRACE_ERROR("Could not extend the last chunk");
                    break;
                }
            }
        }
        index += to_fill;
    }

    return rc < 0 ? rc : BLOSC2_ERROR_SUCCESS;
}

static int s_embed_save_schunk(embed_store* es, const uint8_t* key, blosc2_schunk* to_store) {

    uint8_t* frame = NULL;
    bool needs_free = false;
    int64_t frame_length = blosc2_schunk_to_buffer(to_store, &frame, &needs_free); // On failure, returns a BLOSC2_ERROR_*
    if (frame_length < 0) {
        BLOSC_TRACE_ERROR("Could not load schunk");
        return (int)frame_length;
    }

    value val = s_value_create_local(es->current_offset, (size_t)frame_length);
    int rc = hash_map_set(&es->hm, key, &val);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not set key '%s'", (const char*)key);
        goto cleanup;
    }

    int64_t start = es->current_offset;
    int64_t stop = start + frame_length;
    rc = s_set_slice(es->schunk, start, stop, frame, frame_length);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not set slice buffer");
        goto cleanup;
    }
    es->current_offset = stop;

cleanup:
    if (needs_free) {
        free(frame);
    }
    return rc;
}

static int s_embed_save_remote(embed_store* estore, const uint8_t* key, const uint8_t* urlbase, const uint8_t* path) {
    b2_es_value val = {0};
    val.tag = BLOSC2_STORE_VALUE_REMOTE;
    int rc = string_duplicate(urlbase, &val.f_remote.urlbase);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not allocate memory to duplicate urlbase");
        return rc;
    }
    rc = string_duplicate(path, &val.f_remote.path);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not allocate memory to duplicate path");
        free(val.f_remote.urlbase);
        return rc;
    }

    rc = hash_map_set(&estore->hm, key, &val);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not set remote value in hash map");
        free(val.f_remote.urlbase);
        free(val.f_remote.path);
    }
    return rc;
}

static int s_embed_check_set_op(embed_store* estore, const uint8_t* key) {
    if (estore->mode == BLOSC2_STORE_OPEN_MODE_RDONLY) {
        BLOSC_TRACE_ERROR("Cannot call set with open_mode equal to BLOSC2_STORE_OPEN_MODE_RDONLY");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    if (!s_embed_key_is_valid(key)) {
        BLOSC_TRACE_ERROR("Key '%s' is invalid", (const char*)key);
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    if (hash_map_contains_key(&estore->hm, key)) {
        BLOSC_TRACE_ERROR("EmbedStore already contains key '%s'", (const char*)key);
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    return BLOSC2_ERROR_SUCCESS;
}

static int s_embed_validate_schunk_and_return_version(blosc2_schunk* schunk) {
    if (schunk == NULL) {
        BLOSC_TRACE_ERROR("SChunk cannot be equal to NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (schunk->storage->cparams->typesize != 1) {
        BLOSC_TRACE_ERROR("Schunk's cparams typesize must be equal to 1");
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    // Validate version
    int rc = blosc2_meta_exists(schunk, s_meta_key_version);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("EmbedStore metalayer was not found and it is required");
        return rc;
    }
    uint8_t* version_buffer = NULL;
    int32_t version_buffer_size = 0;
    rc = blosc2_meta_get(schunk, s_meta_key_version, &version_buffer, &version_buffer_size);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not obtain version metalayer");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    rc = msgpack_validate_and_return_version(version_buffer, version_buffer_size);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not validate version metalayer");
    }
    free(version_buffer);
    return rc;
}

static int s_embed_get_serialized_vlmeta(embed_store* es, uint8_t** out_buffer, size_t* out_buffer_size) {
    blosc2_schunk* schunk = es->schunk;
    int rc = blosc2_vlmeta_exists(schunk, s_vlmeta_key_msgpack_dict);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("vlmetalayer does not exist");
        return rc;
    }
    int32_t buf_size = 0;
    rc = blosc2_vlmeta_get(schunk, s_vlmeta_key_msgpack_dict, out_buffer, &buf_size);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not retrieve the current_offset and the hash map, an error occurred while accessing vlmeta");
        return rc;
    }
    *out_buffer_size = (size_t)buf_size;
    return BLOSC2_ERROR_SUCCESS;
}

static int s_embed_deserialize_map_v1(embed_store* es) {
    uint8_t* packed = NULL;
    size_t packed_size = 0;
    int rc = s_embed_get_serialized_vlmeta(es, &packed, &packed_size);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not retrieve serialized vlmeta");
        return rc;
    }
    rc = msgpack_embed_store_deserialize_v1(packed, packed_size, es);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not deserialize msgpack buffer v1");
    }
    free(packed);
    return rc;
}

static int s_embed_deserialize_map_v2(embed_store* es) {
    uint8_t* packed = NULL;
    size_t packed_size = 0;
    int rc = s_embed_get_serialized_vlmeta(es, &packed, &packed_size);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not retrieve serialized vlmeta");
        return rc;
    }

    rc = msgpack_embed_store_deserialize(packed, packed_size, es);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not deserialize msgpack buffer v2");
    }
    free(packed);
    return rc;
}

static int s_embed_deserialize_map(embed_store* es, b2_es_version version) {
    int rc;
    switch (version){
    case B2_ES_V1:
        rc = s_embed_deserialize_map_v1(es);
        break;
    case B2_ES_V2:
        rc = s_embed_deserialize_map_v2(es);
        break;
    default:
        BLOSC_TRACE_ERROR("Invalid version number found");
        rc = BLOSC2_ERROR_INVALID_PARAM;
        break;
    }
    return rc;
}

static int s_embed_serialize_version(blosc2_schunk* schunk, b2_es_version version) {
    // The version is stored in a metalayer (see s_embed_validate_schunk_for_embed_store), while the
    // serialized index is stored in the s_vlmeta_key_msgpack_dict vlmetalayer.

    uint8_t* version_buffer = NULL;
    size_t version_buffer_size = 0;
    int rc = msgpack_container_serialize_version(version, &version_buffer, &version_buffer_size);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not serialize version");
        return rc;
    }
    rc = blosc2_meta_add(schunk, s_meta_key_version, version_buffer, (int32_t)version_buffer_size);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not set the version metalayer");
    }
    free(version_buffer);
    return rc;
}

static int s_embed_init_new_store(blosc2_schunk* schunk, blosc2_store_open_mode open_mode, blosc2_embed_store** estore) {
    embed_store* es = NULL;
    int rc = s_embed_alloc(&es, schunk);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not allocate EmbedStore");
        return rc;
    }
    rc = s_embed_serialize_version(schunk, B2_ES_V2);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not serialize version");
        blosc2_embed_store_free(es);
        return rc;
    }
    es->mode = open_mode;
    *estore = es;
    return rc;
}

// This function will check if the schunk is valid and if it has
static int s_embed_create_from_schunk(blosc2_schunk* schunk, embed_store** estore) {
    int rc = s_embed_validate_schunk_and_return_version(schunk);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not validate schunk");
        return rc;
    }
    b2_es_version version = rc;
    embed_store* es = NULL;
    rc = s_embed_alloc(&es, schunk);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not allocate EmbedStore");
        return rc;
    }
    rc = blosc2_vlmeta_exists(schunk, s_vlmeta_key_msgpack_dict);
    if (rc >= 0) { // It does exist
        rc = s_embed_deserialize_map(es, version);
        if (rc < 0) {
            BLOSC_TRACE_ERROR("Could not deserialize msgpack map");
            blosc2_embed_store_free(es);
            return rc;
        }
    }
    *estore = es;
    return BLOSC2_ERROR_SUCCESS;
}

static int s_exists_inode(const char* urlpath) {
    struct stat buf = {0};
    if (lstat(urlpath, &buf) != 0) {
        int err = errno;
        if (err == ENOENT) return BLOSC2_ERROR_NOT_FOUND;
        return BLOSC2_ERROR_FAILURE;
    }
    if ((S_IFMT & buf.st_mode) != S_IFREG) {
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    return BLOSC2_ERROR_SUCCESS;
}

/* == END Static functions == */


int blosc2_embed_store_new(blosc2_embed_store** estore, blosc2_storage* storage, int32_t chunksize, int32_t blocksize) {
    if (estore == NULL) {
        BLOSC_TRACE_ERROR("pes must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (chunksize <= 0 || blocksize <= 0) {
        BLOSC_TRACE_ERROR("Neither blocksize or chunksize can be less or equal to 0");
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    *estore = NULL;
    blosc2_schunk* schunk = NULL;
    if (storage == NULL) {
        // Create default storage
        schunk = s_embed_create_schunk_with_default_storage(NULL, NULL);
    } else {
        blosc2_cparams cparams = blosc2_get_blosc2_cparams_defaults();
        cparams.typesize = 1;
        if (storage->cparams == NULL) {
            storage->cparams = &cparams;
        } else if (storage->cparams->typesize != 1) {
            BLOSC_TRACE_ERROR("Cparmas typesize must be equal to 1");
            return BLOSC2_ERROR_INVALID_PARAM;
        }
        if (storage->urlpath == NULL || (storage->urlpath != NULL && s_exists_inode(storage->urlpath) == BLOSC2_ERROR_NOT_FOUND)) {
            schunk = blosc2_schunk_new(storage);
        } else {
            BLOSC_TRACE_ERROR("Could not resolve path provided by storage.urlpath or the file already exists, in the latter case, blosc2_embed_store_open should be used");
            return BLOSC2_ERROR_FILE_OPEN;
        }
    }

    if (schunk == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate space for an schunk");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    schunk->chunksize = chunksize;
    schunk->blocksize = blocksize;

    int rc = s_embed_init_new_store(schunk, BLOSC2_STORE_OPEN_MODE_RDWR, estore);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not create new EmbedStore");
    }
    return rc;
}

void blosc2_embed_store_free(blosc2_embed_store* estore) {
    assert(estore != NULL);
    if (estore->schunk != NULL) blosc2_schunk_free(estore->schunk);
    hash_map_free_struct(&estore->hm);
    free(estore);
}

int blosc2_embed_store_open_offset_udio(blosc2_embed_store** estore, const uint8_t* urlpath, blosc2_store_open_mode open_mode, int64_t offset, blosc2_io* io) {
    if (estore == NULL || urlpath == NULL || io == NULL) {
        BLOSC_TRACE_ERROR("Every argument must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (offset < 0) {
        BLOSC_TRACE_ERROR("Offset should be >= 0");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    *estore = NULL;
    blosc2_schunk* schunk = NULL;
    embed_store* es = NULL;
    int rc = BLOSC2_ERROR_SUCCESS;
    if (open_mode == BLOSC2_STORE_OPEN_MODE_RDONLY || open_mode == BLOSC2_STORE_OPEN_MODE_RDWR) {
        // Load schunk
        schunk = blosc2_schunk_open_offset_udio((const char*)urlpath, offset, io);
        if (schunk == NULL) {
            BLOSC_TRACE_ERROR("Embed store could not open schunk at '%s'", (const char*)urlpath);
            return BLOSC2_ERROR_FILE_OPEN;
        }
        rc = s_embed_create_from_schunk(schunk, &es);
        if (rc < 0) {
            BLOSC_TRACE_ERROR("Could not create EmbedStore from SChunk");
        } else {
            es->mode = open_mode;
        }
    } else if (open_mode == BLOSC2_STORE_OPEN_MODE_WRONLY) {
        rc = s_exists_inode((const char*)urlpath);
        if (rc == BLOSC2_ERROR_SUCCESS) {
            BLOSC_TRACE_ERROR("Cannot overwrite EmbedStore located at '%s'", (const char*)urlpath);
            return BLOSC2_ERROR_FILE_OPEN;
        }
        if (rc == BLOSC2_ERROR_FAILURE) {
            BLOSC_TRACE_ERROR("There was an error with lstat");
            return rc;
        }
        if (rc == BLOSC2_ERROR_INVALID_PARAM) {
            BLOSC_TRACE_ERROR("The file was found, but it is not a regular file");
            return rc;
        }
        // The file does not exist
        schunk = s_embed_create_schunk_with_default_storage(urlpath, io);
        if (schunk == NULL) {
            BLOSC_TRACE_ERROR("Could not create new EmbedStore");
            return BLOSC2_ERROR_FILE_OPEN;
        }
        rc = s_embed_init_new_store(schunk, open_mode, &es);
    } else {
        BLOSC_TRACE_ERROR("Invalid open_mode detected, it should be either RDONLY, WRONLY or RDWR");
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    *estore = es;

    return rc;
}

int blosc2_embed_store_open_offset(blosc2_embed_store** estore, const uint8_t* urlpath, blosc2_store_open_mode open_mode, int64_t offset) {
    blosc2_io io = blosc2_get_blosc2_io_defaults();
    return blosc2_embed_store_open_offset_udio(estore, urlpath, open_mode, offset, &io);
}

int blosc2_embed_store_open_udio(blosc2_embed_store** estore, const uint8_t* urlpath, blosc2_store_open_mode open_mode, blosc2_io* io) {
    return blosc2_embed_store_open_offset_udio(estore, urlpath, open_mode, 0, io);
}

int blosc2_embed_store_open(blosc2_embed_store** estore, const uint8_t* urlpath, blosc2_store_open_mode open_mode) {
    blosc2_io io = blosc2_get_blosc2_io_defaults();
    return blosc2_embed_store_open_offset_udio(estore, urlpath, open_mode, 0, &io);
}

int blosc2_embed_store_close(blosc2_embed_store* estore) {
    if (estore == NULL) {
        BLOSC_TRACE_ERROR("es cannot be equal to NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    uint8_t* packed_buffer = NULL;
    size_t packed_buffer_size = 0;
    int rc = msgpack_embed_store_serialize(estore, &packed_buffer, &packed_buffer_size );
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not serialize embed store in blosc2_embed_store_close");
        return rc;
    }

    rc = blosc2_vlmeta_exists(estore->schunk, s_vlmeta_key_msgpack_dict);
    if (rc >= 0) {
        // This vlmeta exists
        rc = blosc2_vlmeta_update(estore->schunk, s_vlmeta_key_msgpack_dict, packed_buffer, (int32_t)packed_buffer_size, NULL);
        if (rc < 0) {
            BLOSC_TRACE_ERROR("Could not update the vlmeta layer with key '%s'", s_vlmeta_key_msgpack_dict);
        }
    } else {
        // It does not exist
        rc = blosc2_vlmeta_add(estore->schunk, s_vlmeta_key_msgpack_dict, packed_buffer, (int32_t)packed_buffer_size, NULL);
        if (rc < 0) {
            BLOSC_TRACE_ERROR("Could not set for the first time the vlmeta layer with key '%s'", s_vlmeta_key_msgpack_dict);
        }
    }

    free(packed_buffer);
    if (rc >= 0) {
        return BLOSC2_ERROR_SUCCESS;
    }
    return rc;
}

int64_t blosc2_embed_store_to_file(blosc2_embed_store* estore, const uint8_t* urlpath) {
    if (estore == NULL || urlpath == NULL) {
        BLOSC_TRACE_ERROR("Arguments must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }

    int rc = BLOSC2_ERROR_SUCCESS;
    rc = fs_atomic_schunk_write((const char*)urlpath, estore->schunk, estore->schunk->storage->urlpath == NULL);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not write EmbedStore atomically");
    }
    return rc;
}

bool blosc2_embed_store_contains_key(blosc2_embed_store* estore, const uint8_t* key) {
    if (estore == NULL || key == NULL) {
        BLOSC_TRACE_ERROR("Neither es nor key can be equal to NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (!s_embed_key_is_valid(key)) {
        BLOSC_TRACE_ERROR("Key '%s' is invalid", (const char*)key);
        return false;
    }
    return hash_map_contains_key(&estore->hm, key);
}

int blosc2_embed_store_get(blosc2_embed_store* estore, const uint8_t* key, blosc2_store_value* item) {
    if (estore == NULL || key == NULL || item == NULL) {
        BLOSC_TRACE_ERROR("Neither es, nor key, nor pschunk can be equal to NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }

    if (!s_embed_key_is_valid(key)) {
        BLOSC_TRACE_ERROR("Key '%s' is invalid", (const char*)key);
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    value *pval = (value*)hash_map_get(&estore->hm, key);
    if (pval == NULL) {
        BLOSC_TRACE_ERROR("There is no value associated with key '%s'", (const char*)key);
        return BLOSC2_ERROR_NOT_FOUND;
    }

    // Load schunk
    int rc = s_embed_load_value(estore, pval, item);
    if (rc != 0) {
        BLOSC_TRACE_ERROR("Could not load schunk with key '%s', offset '%ld' and length '%ld'", (const char*)key, pval->f_local.offset, pval->f_local.length);
    }
    return rc;
}

int blosc2_embed_store_set(blosc2_embed_store* estore, const uint8_t* key, blosc2_store_value* value) {
    if (estore == NULL || key == NULL || value == NULL) {
        BLOSC_TRACE_ERROR("Neither es, nor key, nor schunk can be equal to NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    int rc = s_embed_check_set_op(estore, key);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Cannot call blosc2_embed_store_set");
        return rc;
    }

    switch (value->tag) {
    case BLOSC2_STORE_VALUE_SCHUNK:
        blosc2_schunk* schunk = value->schunk;
        rc = s_embed_save_schunk(estore, key, schunk);
        break;
    case BLOSC2_STORE_VALUE_REMOTE:
        rc = s_embed_save_remote(estore, key, value->remote.urlbase, value->remote.path);
        break;
    }
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not insert value in EmbedStore");
    }
    return rc;
}

int blosc2_embed_store_remove(blosc2_embed_store* estore, const uint8_t* key) {
    if (estore == NULL || key == NULL) {
        BLOSC_TRACE_ERROR("Neither es nor key can be euqual to NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    if (estore->mode == BLOSC2_STORE_OPEN_MODE_RDONLY) {
        BLOSC_TRACE_ERROR("Cannot remove element when open_mode is RDONLY");
        return BLOSC2_ERROR_INVALID_PARAM;
    }
    // Just removes an entry from the map.
    if (!s_embed_key_is_valid(key)) {
        BLOSC_TRACE_ERROR("Key '%s' is invalid", (const char*)key);
        return BLOSC2_ERROR_INVALID_PARAM;
    }

    value val = {0};
    bool removed = hash_map_remove_take(&estore->hm, key, &val);
    if (removed) {
        // Then the element was found
        if (val.tag == BLOSC2_STORE_VALUE_REMOTE) {
            free(val.f_remote.urlbase);
            free(val.f_remote.path);
        }
    }
    return BLOSC2_ERROR_SUCCESS;
}

size_t blosc2_embed_store_get_length(blosc2_embed_store* estore) {
    return estore->hm.length;
}

blosc2_store_open_mode blosc2_embed_store_get_open_mode(blosc2_embed_store* estore) {
    return estore->mode;
}

int blosc2_embed_store_get_version(blosc2_embed_store* estore) {
    if (estore == NULL) {
        BLOSC_TRACE_ERROR("Cannot return version from a NULL pointer");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    blosc2_schunk* schunk = estore->schunk;
    int rc = blosc2_meta_exists(schunk, s_meta_key_version);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Metalayer 'b2embed' was not found");
        return rc;
    }

    uint8_t* packed = NULL;
    int32_t packed_size = 0;
    rc = blosc2_meta_get(schunk, s_meta_key_version, &packed, &packed_size);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("The 'b2embed' metalayer exists, but it could not be retrieved");
        return rc;
    }
    rc = msgpack_validate_and_return_version(packed, (size_t)packed_size);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("The 'b2embed' metalayer is not well formatted, could not deserialize msgpack");
    }
    free(packed);
    return rc;
}

int blosc2_embed_store_from_schunk(blosc2_embed_store** estore, blosc2_schunk* schunk, bool copy) {
    if (estore == NULL || schunk == NULL) {
        BLOSC_TRACE_ERROR("Arguments cannot be NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }

    blosc2_schunk* embed_schunk;
    if (copy) {
        embed_schunk = blosc2_schunk_copy(schunk, schunk->storage);
        if (embed_schunk == NULL) {
            BLOSC_TRACE_ERROR("Could not copy schunk");
            return BLOSC2_ERROR_SCHUNK_COPY;
        }
    } else {
        embed_schunk = schunk;
    }

    blosc2_embed_store* es = NULL;
    int rc = s_embed_create_from_schunk(embed_schunk, &es);
    if (rc < 0) {
        BLOSC_TRACE_ERROR("Could not create EmbedStore from SChunk");
        if (copy) blosc2_schunk_free(embed_schunk);
    } else {
        es->mode = BLOSC2_STORE_OPEN_MODE_RDWR;
        *estore = es;
    }
    return rc;
}

int blosc2_embed_store_to_buffer(blosc2_embed_store* estore, uint8_t** pbuf, size_t* pbuf_size) {
    if (estore == NULL || pbuf == NULL || pbuf_size == NULL) {
        BLOSC_TRACE_ERROR("All function arguments must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }

    bool needs_free = false;
    uint8_t* cframe;
    int64_t cframe_len = blosc2_schunk_to_buffer(estore->schunk, &cframe, &needs_free);
    if (cframe_len < 0) {
        BLOSC_TRACE_ERROR("There was an error while converting the EmbedStore to a buffer");
        return (int)cframe_len;
    }
    if (needs_free) {
        *pbuf = cframe;
        *pbuf_size = (size_t)cframe_len;
        return BLOSC2_ERROR_SUCCESS;
    }

    uint8_t* copied_cframe = malloc((size_t)cframe_len);
    if (copied_cframe == NULL) {
        BLOSC_TRACE_ERROR("There was an error while allocating new memory");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    memcpy(copied_cframe, cframe, (size_t)cframe_len);
    *pbuf = copied_cframe;
    *pbuf_size = (size_t)cframe_len;
    return BLOSC2_ERROR_SUCCESS;
}

int blosc2_embed_store_get_cparams(blosc2_embed_store* estore, blosc2_cparams** cparams) {
    if (estore == NULL || cparams == NULL) {
        BLOSC_TRACE_ERROR("Arguments must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    return blosc2_schunk_get_cparams(estore->schunk, cparams);
}

int blosc2_embed_store_get_dparams(blosc2_embed_store* estore, blosc2_dparams** dparams) {
    if (estore == NULL || dparams == NULL) {
        BLOSC_TRACE_ERROR("Arguments must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }

    return blosc2_schunk_get_dparams(estore->schunk, dparams);
}

int blosc2_embed_store_get_mmap_mode(blosc2_embed_store* estore, char** mode) {
    if (estore == NULL || mode == NULL) {
        BLOSC_TRACE_ERROR("Arguments must be different from NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }

    blosc2_stdio_mmap* mmap_file = estore->schunk->storage->io->params;
    size_t mode_size = strlen(mmap_file->mode) + 1;
    char* mode_alloc = malloc(mode_size);
    if (mode == NULL) {
        BLOSC_TRACE_ERROR("Could not allocate space for the mmap_mode");
        return BLOSC2_ERROR_MEMORY_ALLOC;
    }
    memcpy(mode_alloc, mmap_file->mode, mode_size);
    *mode = mode_alloc;
    return BLOSC2_ERROR_SUCCESS;
}

void priv_embed_store_iter_init(blosc2_embed_store_iter* iter, embed_store* estore) {
    iter->es = estore;
    hash_map_iter_init(&iter->map_iter, &estore->hm);
}

blosc2_embed_store_iter* blosc2_embed_store_iter_new(blosc2_embed_store* estore) {
    if (estore == NULL) {
        BLOSC_TRACE_ERROR("es cannot be equal to NULL");
        return NULL;
    }

    blosc2_embed_store_iter* iter = calloc(1, sizeof(blosc2_embed_store_iter));
    if (iter == NULL) {
        return NULL;
    }
    priv_embed_store_iter_init(iter, estore);
    return iter;
}

bool blosc2_embed_store_iter_next(blosc2_embed_store_iter* iter) {
    if (iter == NULL) {
        BLOSC_TRACE_ERROR("iter cannot be equal to NULL");
        return false;
    }

    return hash_map_iter_next(&iter->map_iter);
}

const uint8_t* blosc2_embed_store_iter_get_key(blosc2_embed_store_iter* iter) {
    if (iter == NULL) {
        BLOSC_TRACE_ERROR("iter cannot be equal to NULL");
        return NULL;
    }
    return iter->map_iter.key->key;
}

int blosc2_embed_store_iter_get_container(blosc2_embed_store_iter* iter, blosc2_store_value* item) {
    if (iter == NULL || item == NULL) {
        BLOSC_TRACE_ERROR("Neither iter nor pschunk can be equal to NULL");
        return BLOSC2_ERROR_NULL_POINTER;
    }
    *item = (blosc2_store_value){0};
    value* val = iter->map_iter.value;
    return s_embed_load_value(iter->es, val, item);
}

void blosc2_embed_store_iter_free(blosc2_embed_store_iter* iter) {
    if (iter != NULL) free(iter);
}
