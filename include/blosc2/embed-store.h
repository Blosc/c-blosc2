/*********************************************************************
Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_EMBED_STORE_H
#define BLOSC_EMBED_STORE_H

#define BLOSC2_EMBED_STORE_DEFAULT_VERSION 2
#define BLOSC2_EMBED_STORE_DEFAULT_CHUNKSIZE 256

#include <blosc2.h>
#include <blosc2/blosc2-export.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct blosc2_embed_store blosc2_embed_store; /* Opaque type */
typedef struct blosc2_embed_store_iter blosc2_embed_store_iter; /* Opaque type */

/**
 * Store open modes.
 *
 * B2_ES_RDONLY: existing store, read-only (writes are rejected).
 * B2_ES_WRONLY: create a new store; the target must not exist yet.
 * B2_ES_RDWR:   existing store, read and write.
 */
typedef enum {
    B2_ES_RDONLY = 0,
    B2_ES_WRONLY,
    B2_ES_RDWR,
}b2_es_open_mode;

typedef enum {
    B2_ES_VALUE_LOCAL,
    B2_ES_VALUE_REMOTE
}b2_es_value_tag;

typedef struct {
    b2_es_value_tag tag;
    union {
        blosc2_schunk* schunk; // local
        struct {
            const uint8_t* urlbase;
            const uint8_t* path;
        }remote;
    };
}b2_es_item;

/*
 * A key is a string that must be non-empty, start with '/', not end with '/' (unless it is the
 * root key "/") and not contain ':', NUL, '\n', '\r' or '\t'. Invalid keys are rejected with a
 * negative error code (or `false`, for contains_key).
 */

/**
 * Create a new, empty store.
 *
 * @param[out] estore Receives the new store. Must not be NULL.
 * @param[in] storage Storage for the backing SChunk. If NULL, an in-memory, contiguous storage
 * with cparams.typesize == 1 is used. Otherwise cparams.typesize must be 1 and, if urlpath is
 * set, the target must not exist yet (use blosc2_embed_store_open to reopen an existing store).
 * @return BLOSC2_ERROR_SUCCESS on success, a negative error code on failure.
 *
 * The store owns its backing SChunk and starts in B2_ES_RDWR mode.
 */
BLOSC_EXPORT int blosc2_embed_store_new(blosc2_embed_store** estore, blosc2_storage* storage, int32_t chunksize);

/**
 * Free a store and its backing SChunk.
 *
 * @param estore Store to free. Must be a valid store.
 *
 * The pointer becomes invalid afterward.
 */
BLOSC_EXPORT void blosc2_embed_store_free(blosc2_embed_store* estore);

/**
 * Open an existing, persisted store.
 *
 * @param[out] estore Receives the opened store. Must not be NULL.
 * @param[in] urlpath Path to the backing SChunk. Must not be NULL.
 * @param[in] open_mode B2_ES_RDONLY or B2_ES_RDWR to load an existing store; B2_ES_WRONLY to
 * create a new one (the target must not exist yet).
 * @return BLOSC2_ERROR_SUCCESS on success, a negative error code on failure.
 *
 * When loading, the file must be a valid store: typesize 1, the `b2embed` version metalayer and
 * a deserializable `estore_metadata` vlmeta. The store owns the backing SChunk.
 */
BLOSC_EXPORT int blosc2_embed_store_open(blosc2_embed_store** estore, const uint8_t* urlpath, b2_es_open_mode open_mode);

/**
 * Open an existing, persisted store. It does exactly the same operation as blosc2_embed_store_open, but with
 * a custom blosc2_io struct that will be copied when the SChunk is opened
 *
 * @param[out] estore Receives the opened store. Must not be NULL.
 * @param[in] urlpath Path to the backing SChunk. Must not be NULL.
 * @param[in] open_mode B2_ES_RDONLY or B2_ES_RDWR to load an existing store; B2_ES_WRONLY to
 * create a new one (the target must not exist yet).
 * @param[in] io blosc2_io structure pointer
 * @return BLOSC2_ERROR_SUCCESS on success, a negative error code on failure.
 *
 * When loading, the file must be a valid store: typesize 1, the `b2embed` version metalayer and
 * a deserializable `estore_metadata` vlmeta. The store owns the backing SChunk.
 */
BLOSC_EXPORT int blosc2_embed_store_open_udio(blosc2_embed_store** estore, const uint8_t* urlpath, b2_es_open_mode open_mode, blosc2_io* io);

/**
 * Persist the store's index into its backing SChunk.
 *
 * @param estore Store to flush.
 * @return BLOSC2_ERROR_SUCCESS on success, a negative error code on failure.
 *
 * The index is serialized to msgpack and written to the `estore_metadata` vlmeta. Call it after
 * mutations (set/remove) so that they survive reopening; it can be called more than once.
 */
// TODO: See if the name `close` is the most adequate. This question is made due to the fact that this function only
// serializes the inner hash_map with msgpack.
BLOSC_EXPORT int blosc2_embed_store_close(blosc2_embed_store* estore);

/**
 * Return whether `key` is present in the store.
 *
 * @param estore Valid store.
 * @param key Store key.
 * @return true if present, false otherwise.
 */
BLOSC_EXPORT bool blosc2_embed_store_contains_key(blosc2_embed_store* estore, const uint8_t* key);

/**
 * Retrieve the value stored under `key`.
 *
 * @param estore Valid store.
 * @param key Store key.
 * @param[out] item Receives a newly deserialized SChunk or the location of a remote SChunk. To see whether it is local or remote
 * check the tag
 * @return BLOSC2_ERROR_SUCCESS, BLOSC2_ERROR_NOT_FOUND, or another negative error code.
 *
 * The caller owns *pschunk and must free it with blosc2_schunk_free().
 */
BLOSC_EXPORT int blosc2_embed_store_get(blosc2_embed_store* estore, const uint8_t* key, b2_es_item* item);

/**
 * Store `schunk` under `key`, appending it to the backing SChunk.
 *
 * @param estore Store opened for writing (i.e. not B2_ES_RDONLY).
 * @param key Store key.
 * @param schunk Value to store. Not consumed; the caller keeps ownership of it.
 * @return BLOSC2_ERROR_SUCCESS, or a negative error code. Reassigning an existing key is
 * rejected with BLOSC2_ERROR_INVALID_PARAM.
 */
BLOSC_EXPORT int blosc2_embed_store_set(blosc2_embed_store* estore, const uint8_t* key, blosc2_schunk* schunk);

/**
 *
 * Store a remote value inside the EmbedStore.
 *
 * @param estore EmbedStore pointer.
 * @param key The key to store.
 * @param urlbase A string that represents the server.
 * @param path A path that represents de resource that wants to be accessed.
 * @return A negative error code
 */
BLOSC_EXPORT int blosc2_embed_store_set_remote(blosc2_embed_store* estore, const uint8_t* key, const uint8_t* urlbase, const uint8_t* path);

/**
 * Remove `key` from the store's index.
 *
 * @param estore Valid store.
 * @param key Store key.
 * @return BLOSC2_ERROR_SUCCESS, or a negative error code.
 *
 * Only the index entry is removed; the value's bytes stay in the backing SChunk.
 */
BLOSC_EXPORT int blosc2_embed_store_remove(blosc2_embed_store* estore, const uint8_t* key);

/**
 * Return the number of keys in the store.
 */
BLOSC_EXPORT size_t blosc2_embed_store_get_length(blosc2_embed_store* estore);

/**
 * Return the store's open mode.
 */
BLOSC_EXPORT b2_es_open_mode blosc2_embed_store_get_open_mode(blosc2_embed_store* estore);

/**
 *
 * This function accessed the inner SChunk's fixed length metalayer and retrieves the version from it.
 * The key for the metalayer is 'b2embed'.
 *
 * @param estore The EmbedStore pointer.
 * @return A positive integer representing the version. On error, a negative error code
 */
BLOSC_EXPORT int blosc2_embed_store_get_version(blosc2_embed_store* estore);

/**
 * Build a store from an existing backing SChunk.
 *
 * @param[out] estore Receives the new store. Must not be NULL.
 * @param[in] schunk Backing super-chunk. It must have cparams.typesize equal to 1 and carry the
 * EmbedStore metadata (the `b2embed` metalayer plus the `estore_metadata` vlmeta).
 * @param[in] copy If false, the store adopts `schunk` and takes ownership of it, so freeing the
 * store also frees the SChunk. If true, the store works on a copy made with
 * `blosc2_schunk_copy(schunk, schunk->storage)`, so it is subject to the same rules as
 * `blosc2_schunk_new`: the caller is responsible for how `schunk` was created and with which
 * storage (in particular, copying a persistent SChunk that reuses its own `urlpath` fails, as
 * `blosc2_schunk_new` refuses to overwrite an existing frame).
 * @return BLOSC2_ERROR_SUCCESS on success, a negative error code on failure.
 */
BLOSC_EXPORT int blosc2_embed_store_from_schunk(blosc2_embed_store** estore, blosc2_schunk* schunk, bool copy);

/**
 * Serialize the whole store (its backing SChunk as a cframe) into a new buffer.
 *
 * @param estore Valid store.
 * @param[out] pbuf Receives the newly allocated buffer.
 * @param[out] pbuf_size Receives the buffer length.
 * @return BLOSC2_ERROR_SUCCESS on success, a negative error code on failure.
 *
 * The caller owns *pbuf and must free it.
 */
BLOSC_EXPORT int blosc2_embed_store_to_buffer(blosc2_embed_store* estore, uint8_t** pbuf, size_t* pbuf_size);

/**
 *
 * @param[in] estore EmbedStore pointer
 * @param[out] cparams A pointer to a blosc2_cparams pointer
 * @return In case that the arguments are NULL, returns BLOSC2_ERROR_NULL_POINTER, if the allocation
 * of the blosc2_cparams struct was not successful, returns, BLOSC2_ERROR_MEMORY_ALLOC. On success, 0
 */
BLOSC_EXPORT int blosc2_embed_store_get_cparams(blosc2_embed_store* estore, blosc2_cparams** cparams);

/**
 *
 * @param[in] estore EmbedStore pointer
 * @param[out] dparams A pointer to a blosc2_dparams pointer
 * @return In case that the arguments are NULL, returns BLOSC2_ERROR_NULL_POINTER, if the allocation
 * of the blosc2_dparams struct was not successful, returns, BLOSC2_ERROR_MEMORY_ALLOC. On success, 0
 */
BLOSC_EXPORT int blosc2_embed_store_get_dparams(blosc2_embed_store* estore, blosc2_dparams** dparams);

/**
 *
 * @param[in] estore EmbedStore pointer
 * @param[out] mode A pointer to a char*
 * @return In case that the arguments are NULL, returns BLOSC2_ERROR_NULL_POINTER, if the allocation
 * of the mode string struct was not successful, returns, BLOSC2_ERROR_MEMORY_ALLOC. On success, 0
 */
BLOSC_EXPORT int blosc2_embed_store_get_mmap_mode(blosc2_embed_store* estore, char** mode);

/*
 * Iteration over a store's keys. The order is unspecified.
 */

/**
 * Allocate an iterator over `estore`. Returns NULL on failure.
 */
BLOSC_EXPORT blosc2_embed_store_iter* blosc2_embed_store_iter_new(blosc2_embed_store* estore);

/**
 * Advance the iterator. Returns true while it points to a valid entry, false at the end.
 */
BLOSC_EXPORT bool blosc2_embed_store_iter_next( blosc2_embed_store_iter* iter );

/**
 * Return the key at the current position (valid after iter_next() returned true).
 */
BLOSC_EXPORT const uint8_t* blosc2_embed_store_iter_get_key( blosc2_embed_store_iter* iter );

/**
 * Load the value at the current position into a newly allocated SChunk (caller frees it).
 * Only local values are supported.
 */
BLOSC_EXPORT int blosc2_embed_store_iter_get_container( blosc2_embed_store_iter* iter, b2_es_item* item );

/**
 * Free an iterator.
 */
BLOSC_EXPORT void blosc2_embed_store_iter_free( blosc2_embed_store_iter* iter );

#endif //BLOSC_EMBED_STORE_H
