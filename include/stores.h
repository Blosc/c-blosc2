/*********************************************************************
Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/*********************************************************************
  @file stores.h
  @brief Blosc2 Stores header file.

  This file contains the API for the general data stores of Blosc2.
  @author Blosc Development Team <blosc@blosc.org>
**********************************************************************/

#ifndef BLOSC_STORES_H
#define BLOSC_STORES_H

#include "blosc2.h"

// To check which value it has returned, first, check the value of schunk
// If it is not NULL, then the store has returned a new SChunk
// If it's NULL, then urlbase and path are not NULL

typedef enum {
    BLOSC2_STORE_VALUE_SCHUNK,
    BLOSC2_STORE_VALUE_REMOTE,
}blosc2_store_value_tag;

typedef struct {
    union {
        blosc2_schunk* schunk;
        struct {
            uint8_t* urlbase;
            uint8_t* path;
        }remote;
    };
    blosc2_store_value_tag tag;
}blosc2_store_value;

typedef enum {
    BLOSC2_STORE_OPEN_MODE_RDONLY,
    BLOSC2_STORE_OPEN_MODE_WRONLY,
    BLOSC2_STORE_OPEN_MODE_RDWR,
}blosc2_store_open_mode;

/* EmbedStore */

#define BLOSC2_EMBED_STORE_DEFAULT_VERSION 2
#define BLOSC2_EMBED_STORE_DEFAULT_CHUNKSIZE 256
#define BLOSC2_EMBED_STORE_DEFAULT_BLOCKSIZE 64

typedef struct blosc2_embed_store blosc2_embed_store; /* Opaque type */
typedef struct blosc2_embed_store_iter blosc2_embed_store_iter; /* Opaque type */

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
BLOSC_EXPORT int blosc2_embed_store_new(blosc2_embed_store** estore, blosc2_storage* storage, int32_t chunksize, int32_t blocksize);

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
 * @param[in] mode B2_ES_RDONLY or B2_ES_RDWR to load an existing store; B2_ES_WRONLY to
 * create a new one (the target must not exist yet).
 * @return BLOSC2_ERROR_SUCCESS on success, a negative error code on failure.
 *
 * When loading, the file must be a valid store: typesize 1, the `b2embed` version metalayer and
 * a deserializable `estore_metadata` vlmeta. The store owns the backing SChunk.
 */
BLOSC_EXPORT int blosc2_embed_store_open(blosc2_embed_store** estore, const uint8_t* urlpath, blosc2_store_open_mode mode);

BLOSC_EXPORT int blosc2_embed_store_open_offset(blosc2_embed_store** estore, const uint8_t* urlpath, blosc2_store_open_mode mode, int64_t offset);

/**
 * Open an existing, persisted store. It does exactly the same operation as blosc2_embed_store_open, but with
 * a custom blosc2_io struct that will be copied when the SChunk is opened
 *
 * @param[out] estore Receives the opened store. Must not be NULL.
 * @param[in] urlpath Path to the backing SChunk. Must not be NULL.
 * @param[in] mode B2_ES_RDONLY or B2_ES_RDWR to load an existing store; B2_ES_WRONLY to
 * create a new one (the target must not exist yet).
 * @param[in] io blosc2_io structure pointer
 * @return BLOSC2_ERROR_SUCCESS on success, a negative error code on failure.
 *
 * When loading, the file must be a valid store: typesize 1, the `b2embed` version metalayer and
 * a deserializable `estore_metadata` vlmeta. The store owns the backing SChunk.
 */
BLOSC_EXPORT int blosc2_embed_store_open_udio(blosc2_embed_store** estore, const uint8_t* urlpath, blosc2_store_open_mode mode, blosc2_io* io);

BLOSC_EXPORT int blosc2_embed_store_open_offset_udio(blosc2_embed_store** estore, const uint8_t* urlpath, blosc2_store_open_mode mode, int64_t offset, blosc2_io* io);

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

/*
 * Persists the EmbedStore in the file at urlpath.
 * If the file does not exist, it creates it.
 * If the file exists it fails
 *
 * It does not close the EmbedStore automatically
 * Returns the fileframe size on success; on failure it returns an error code
 */
BLOSC_EXPORT int64_t blosc2_embed_store_to_file(blosc2_embed_store* estore, const uint8_t* urlpath);

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
BLOSC_EXPORT int blosc2_embed_store_get(blosc2_embed_store* estore, const uint8_t* key, blosc2_store_value* value);

/**
 * Store `schunk` under `key`, appending it to the backing SChunk.
 *
 * @param estore Store opened for writing (i.e. not B2_ES_RDONLY).
 * @param key Store key.
 * @param value Value to store. Not consumed; the caller keeps ownership of it.
 * @return BLOSC2_ERROR_SUCCESS, or a negative error code. Reassigning an existing key is
 * rejected with BLOSC2_ERROR_INVALID_PARAM.
 */
BLOSC_EXPORT int blosc2_embed_store_set(blosc2_embed_store* estore, const uint8_t* key, blosc2_store_value* value);

/**
 * Remove `key` from the store's index.
 *
 * @param estore Valid store.
 * @param key Store key.f
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
BLOSC_EXPORT blosc2_store_open_mode blosc2_embed_store_get_open_mode(blosc2_embed_store* estore);

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
BLOSC_EXPORT int blosc2_embed_store_iter_get_container( blosc2_embed_store_iter* iter, blosc2_store_value* item );

/**
 * Free an iterator.
 */
BLOSC_EXPORT void blosc2_embed_store_iter_free( blosc2_embed_store_iter* iter );

/* DictStore */

/** @brief Opaque handle to a dictionary. */
typedef struct blosc2_dict_store blosc2_dict_store;

/** @brief Opaque handle to a dictionary iterator. */
typedef struct blosc2_dict_store_iter blosc2_dict_store_iter;


/**
 * @brief Creates a new dictionary.
 *
 * @param[out] dstore     Receives the new dictionary. Must not be NULL; it is set
 *                     to NULL on failure.
 * @param[in]  urlpath Destination path. If NULL, the dictionary lives in memory
 *                     (sparse storage). Otherwise it must end in `.b2z`
 *                     (archive) or `.b2d` (directory) and must NOT already
 *                     exist in the filesystem.
 *
 * @retval BLOSC2_ERROR_SUCCESS       Dictionary created.
 * @retval BLOSC2_ERROR_NULL_POINTER  @p pds is NULL.
 * @retval BLOSC2_ERROR_INVALID_PARAM @p urlpath is empty, already exists, or
 *                                    has no recognized extension.
 * @retval BLOSC2_ERROR_MEMORY_ALLOC  Allocation failure.
 * @retval BLOSC2_ERROR_FAILURE       Could not create the temporal directory
 *                                    (b2z) or the target directory (b2d).
 *
 * @note The caller owns the returned dictionary and must release it with
 *       blosc2_dict_store_free(). For b2z/b2d backends, call
 *       blosc2_dict_store_close() first so the contents reach the disk.
 *
 * @sa blosc2_dict_store_open, blosc2_dict_store_close, blosc2_dict_store_free
 */
BLOSC_EXPORT int blosc2_dict_store_new(blosc2_dict_store** dstore, const uint8_t* urlpath, blosc2_embed_store* estore, int64_t threshold);

/**
 * @brief Opens an existing dictionary stored on disk.
 *
 * The backend (b2z or b2d) is inferred from the extension of @p urlpath.
 *
 * @param[out] dstore        Receives the opened dictionary. Must not be NULL; it is
 *                       set to NULL on failure.
 * @param[in]  urlpath   Path of an existing `.b2z` archive or `.b2d` directory.
 * @param[in]  mode Requested access mode.
 *
 * @retval BLOSC2_ERROR_SUCCESS       Dictionary opened.
 * @retval BLOSC2_ERROR_NULL_POINTER  @p ds or @p urlpath is NULL.
 * @retval BLOSC2_ERROR_INVALID_PARAM @p urlpath is empty or has no recognized
 *                                    extension, or @p open_mode is unknown.
 * @retval BLOSC2_ERROR_FILE_OPEN     The path does not exist (RDONLY/RDWR) or
 *                                    already exists (WRONLY), or the archive
 *                                    could not be read.
 *
 * @note RDONLY requires @p urlpath to exist; WRONLY requires it NOT to exist
 *       (a new one is created); RDWR requires it to exist.
 *
 * @sa blosc2_dict_store_new, blosc2_dict_store_close, blosc2_dict_store_free
 */
BLOSC_EXPORT int blosc2_dict_store_open(blosc2_dict_store** dstore, const uint8_t* urlpath, blosc2_store_open_mode mode);

/**
 * @brief Flushes pending changes to disk and destroys the temporary structures.
 *
 * For in-memory and b2d dictionaries this is a no-op. For a b2z dictionary
 * opened WRONLY/RDWR it packs the temporary directory into the final archive.
 * For a b2z dictionary opened RDONLY it is a no-op.
 *
 * @param[in] dstore Dictionary to close. Must not be NULL.
 *
 * @retval BLOSC2_ERROR_SUCCESS       Closed, or nothing to do.
 * @retval BLOSC2_ERROR_NULL_POINTER  @p ds is NULL.
 * @retval BLOSC2_ERROR_INVALID_PARAM The storage type is unknown.
 * @retval BLOSC2_ERROR_FILE_REMOVE   The previous destination could not be removed.
 * @retval BLOSC2_ERROR_FILE_WRITE    The archive could not be written.
 *
 * @warning After closing, calling set/get/remove can corrupt memory. The only
 *          valid operation afterwards is blosc2_dict_store_free().
 *
 * @sa blosc2_dict_store_free
 */
BLOSC_EXPORT int blosc2_dict_store_close(blosc2_dict_store* dstore);

/**
 * @brief Releases all the memory owned by a dictionary.
 *
 * @param[in] dstore Dictionary to free. If NULL, this function does nothing.
 *
 * @note This does NOT free the schunks stored in an in-memory sparse
 *       dictionary; they are owned by the caller.
 * @note For b2z/b2d dictionaries, call blosc2_dict_store_close() first if you
 *       want the changes to reach the disk.
 *
 * @sa blosc2_dict_store_new, blosc2_dict_store_open, blosc2_dict_store_close
 */
BLOSC_EXPORT void blosc2_dict_store_free(blosc2_dict_store* dstore);

/** @} */  /* Alloc / Free API */

/** @name Serialization / conversion API */
/** @{ */

/**
 * @brief Writes the dictionary contents to a `.b2z` archive.
 *
 * @param[in] dstore                  Dictionary to dump. Must not be NULL.
 * @param[in] destination_urlpath Output path. Must end in `.b2z` and must NOT
 *                                already exist.
 *
 * @retval BLOSC2_ERROR_SUCCESS       Archive written.
 * @retval BLOSC2_ERROR_NULL_POINTER  @p ds or @p destination_urlpath is NULL.
 * @retval BLOSC2_ERROR_INVALID_PARAM @p destination_urlpath has no `.b2z` extension.
 * @retval BLOSC2_ERROR_FILE_WRITE    @p destination_urlpath already exists, or
 *                                    the archive could not be written.
 * @retval BLOSC2_ERROR_FAILURE       The source is already a b2z dictionary, or
 *                                    the directory could not be archived.
 *
 * @note Only in-memory and b2d dictionaries can be dumped to b2z; a b2z source
 *       is rejected.
 *
 * @sa blosc2_dict_store_to_b2d, blosc2_dict_store_to_buffer
 */
BLOSC_EXPORT int blosc2_dict_store_to_b2z(blosc2_dict_store* dstore, const uint8_t* destination_urlpath);

/**
 * @brief Writes the dictionary contents to a `.b2d` directory.
 *
 * @param[in] dstore      Dictionary to dump. Must not be NULL.
 * @param[in] urlpath Output directory. Must end in `.b2d` and must NOT already exist.
 *
 * @retval BLOSC2_ERROR_SUCCESS       Directory written.
 * @retval BLOSC2_ERROR_NULL_POINTER  @p ds or @p urlpath is NULL.
 * @retval BLOSC2_ERROR_INVALID_PARAM @p urlpath has no `.b2d` extension.
 * @retval BLOSC2_ERROR_FILE_WRITE    @p urlpath already exists, or the source
 *                                    b2z dictionary is RDONLY, or a file could
 *                                    not be written.
 * @retval BLOSC2_ERROR_FAILURE       The source is already a b2d dictionary, or
 *                                    a path could not be created.
 * @retval BLOSC2_ERROR_FILE_READ     A source schunk could not be read.
 *
 * @note The destination must not exist. A b2z source must be writable
 *       (WRONLY/RDWR), not RDONLY.
 *
 * @sa blosc2_dict_store_to_b2z, blosc2_dict_store_to_buffer
 */
BLOSC_EXPORT int blosc2_dict_store_to_b2d(blosc2_dict_store* dstore, const uint8_t* urlpath);

/**
 * @brief Serializes an in-memory dictionary into a new heap buffer.
 *
 * @param[in]  dstore      Dictionary to serialize. Must not be NULL and must be
 *                     in-memory (sparse or contiguous).
 * @param[out] pbuffer Receives the malloc'ed buffer. Must not be NULL.
 * @param[out] psize   Receives the buffer size in bytes. Must not be NULL.
 *
 * @retval BLOSC2_ERROR_SUCCESS       Buffer produced.
 * @retval BLOSC2_ERROR_NULL_POINTER  @p ds or @p pbuffer is NULL.
 * @retval BLOSC2_ERROR_INVALID_PARAM @p ds is a b2z or b2d dictionary.
 * @retval BLOSC2_ERROR_MEMORY_ALLOC  Allocation failure.
 *
 * @warning The caller owns the returned buffer and must release it with free().
 *
 * @sa blosc2_dict_store_from_buffer
 */
BLOSC_EXPORT int blosc2_dict_store_to_buffer(blosc2_dict_store* dstore, void** pbuffer, size_t* psize);

/**
 * @brief Creates a read-only, contiguous in-memory dictionary from a buffer.
 *
 * The buffer must have been produced by blosc2_dict_store_to_buffer().
 *
 * @param[out] dstore    Receives the new dictionary. Must not be NULL.
 * @param[in]  buffer Buffer with the serialized dictionary. Must not be NULL
 *                    and must stay valid while the dictionary is alive (it is
 *                    NOT copied).
 * @param[in]  size   Buffer size in bytes; must be greater than 0.
 *
 * @retval BLOSC2_ERROR_SUCCESS       Dictionary created.
 * @retval BLOSC2_ERROR_NULL_POINTER  @p pds or @p buffer is NULL.
 * @retval BLOSC2_ERROR_INVALID_PARAM @p size is 0.
 * @retval BLOSC2_ERROR_MEMORY_ALLOC  Allocation failure.
 * @retval BLOSC2_ERROR_FAILURE       The buffer is not a valid dictionary.
 *
 * @note The resulting dictionary has storage type B2_DS_MEMORY_CONTIGUOUS and
 *       open mode B2_DS_RDONLY.
 * @warning The dictionary references the caller's buffer; do NOT free the
 *          buffer until after blosc2_dict_store_free(pds).
 *
 * @sa blosc2_dict_store_to_buffer
 */
BLOSC_EXPORT int blosc2_dict_store_from_buffer(blosc2_dict_store** dstore, void* buffer, size_t size);

/** @} */  /* Serialization / conversion API */

/** @name Introspection API */
/** @{ */

/**
 * @brief Returns the number of keys currently stored.
 * @param[in] dstore Dictionary. Must not be NULL.
 */
BLOSC_EXPORT size_t blosc2_dict_store_get_length(blosc2_dict_store* dstore);

/**
 * @brief Returns the number of slots in the internal hash table.
 *
 * Always greater than or equal to blosc2_dict_store_get_length(). It includes
 * the slots occupied by deleted entries (tombstones), so it is an upper bound
 * of the memory the table currently uses.
 *
 * @param[in] dstore Dictionary. Must not be NULL.
 */
BLOSC_EXPORT size_t blosc2_dict_store_get_capacity(blosc2_dict_store* dstore);

/**
 * @brief Returns the access mode the dictionary was created/opened with.
 * @param[in] dstore Dictionary. Must not be NULL.
 */
BLOSC_EXPORT blosc2_store_open_mode blosc2_dict_store_get_open_mode(blosc2_dict_store* dstore);

/**
 * @brief Returns the storage backend of the dictionary.
 *
 * @param[in] dstore Dictionary. Must not be NULL.
 * @return One of the values of the storage-type enum: B2_DS_MEMORY_SPARSE,
 *         B2_DS_MEMORY_CONTIGUOUS, B2_DS_B2Z or B2_DS_B2D. Returns a negative
 *         value for an unknown backend.
 */
BLOSC_EXPORT int blosc2_dict_store_get_storage_type(blosc2_dict_store* dstore);

/** @} */  /* Introspection API */

/** @name Get / Set / Remove API */
/** @{ */

/**
 * @brief Reports whether a key exists.
 *
 * The key is normalized internally, so equivalent spellings such as "a/b",
 * "/a/b", "a//b" or "a/./b" refer to the same key.
 *
 * @param[in] dstore  Dictionary. If NULL, the function returns false.
 * @param[in] key Key. If NULL, the function returns false.
 * @return true if the key is present, false otherwise (including NULL or
 *         non-normalizable inputs).
 *
 * @sa blosc2_dict_store_get
 */
BLOSC_EXPORT bool blosc2_dict_store_contains_key(blosc2_dict_store* dstore, const uint8_t* key);

/**
 * @brief Retrieves the schunk associated with a key.
 *
 * @param[in]  dstore         Dictionary. Must not be NULL.
 * @param[in]  key        Key. Must not be NULL; it is normalized internally.
 * @param[out] item Receives the schunk. Must not be NULL; it is set to
 *                        NULL on failure.
 *
 * @retval BLOSC2_ERROR_SUCCESS       The key was found.
 * @retval BLOSC2_ERROR_NOT_FOUND     The key does not exist.
 * @retval BLOSC2_ERROR_NULL_POINTER  Any argument is NULL.
 * @retval BLOSC2_ERROR_INVALID_PARAM The dictionary is opened WRONLY.
 *
 * @warning OWNERSHIP of the returned container depends on the storage backend:
 *   - B2_DS_MEMORY_SPARSE: the pointer is BORROWED. Do NOT free it; it belongs
 *     to whoever passed it to blosc2_dict_store_set(). The dictionary is not
 *     its owner.
 *   - B2_DS_MEMORY_CONTIGUOUS, B2_DS_B2Z, B2_DS_B2D: a NEW schunk is opened and
 *     the caller MUST release it with blosc2_schunk_free().
 *
 * @sa blosc2_dict_store_set, blosc2_dict_store_remove, blosc2_dict_store_contains_key
 */
BLOSC_EXPORT int blosc2_dict_store_get(blosc2_dict_store* dstore, const uint8_t* key, blosc2_store_value* item);

/**
 * @brief Inserts or replaces the schunk associated with a key.
 *
 * Overwriting an existing key replaces its value and keeps the length
 * unchanged; inserting a new key increases the length by one.
 *
 * @param[in] dstore        Dictionary. Must not be NULL.
 * @param[in] key       Key. Must not be NULL nor empty; it is normalized internally.
 * @param[in] container Schunk to store. Must not be NULL.
 *
 * @retval BLOSC2_ERROR_SUCCESS       Inserted or replaced.
 * @retval BLOSC2_ERROR_NULL_POINTER  Any argument is NULL.
 * @retval BLOSC2_ERROR_INVALID_PARAM @p key is the empty string.
 * @retval BLOSC2_ERROR_FAILURE       The dictionary is opened RDONLY.
 * @retval BLOSC2_ERROR_MEMORY_ALLOC  Allocation failure.
 * @retval BLOSC2_ERROR_FILE_WRITE    The schunk could not be written to disk (b2x).
 *
 * @warning For B2_DS_MEMORY_SPARSE the dictionary does NOT take ownership of
 *          @p container: it must stay alive while the key remains in the
 *          dictionary.
 * @note In RDONLY mode this returns BLOSC2_ERROR_FAILURE, whereas get()/remove()
 *       return BLOSC2_ERROR_INVALID_PARAM for a wrong mode.
 *
 * @sa blosc2_dict_store_get, blosc2_dict_store_remove
 */
BLOSC_EXPORT int blosc2_dict_store_set(blosc2_dict_store* dstore, const uint8_t* key, blosc2_store_value* value);

BLOSC_EXPORT int64_t blosc2_dict_store_get_threshold(blosc2_dict_store* dstore);

BLOSC_EXPORT int blosc2_dict_store_set_threshold(blosc2_dict_store* dstore, int64_t threshold);

/**
 * @brief Removes a key.
 *
 * @param[in] dstore  Dictionary. Must not be NULL.
 * @param[in] key Key. Must not be NULL; it is normalized internally.
 *
 * @retval BLOSC2_ERROR_SUCCESS       The key was removed, or was not present.
 * @retval BLOSC2_ERROR_NULL_POINTER  Any argument is NULL.
 * @retval BLOSC2_ERROR_INVALID_PARAM The dictionary is opened RDONLY.
 * @retval BLOSC2_ERROR_FILE_REMOVE   The file backing the key could not be removed.
 *
 * @note On disk backends, this also deletes the file backing the key.
 * @note Current behavior: removing a missing key succeeds silently (it is a
 *       no-op), instead of returning BLOSC2_ERROR_NOT_FOUND.
 *
 * @sa blosc2_dict_store_get, blosc2_dict_store_set
 */
BLOSC_EXPORT int blosc2_dict_store_remove(blosc2_dict_store* dstore, const uint8_t* key);

/** @} */  /* Get / Set / Remove API */

/** @name Iterator API */
/** @{ */

/**
 * @brief Creates an iterator over the live keys of a dictionary.
 *
 * @param[in] dstore Dictionary. Must not be NULL.
 * @return A new iterator, or NULL on failure. Release it with
 *         blosc2_dict_store_iter_free().
 *
 * @warning Do not insert or remove entries while iterating; the iteration
 *          order is unspecified.
 */
BLOSC_EXPORT blosc2_dict_store_iter* blosc2_dict_store_iter_new( blosc2_dict_store* dstore );

/**
 * @brief Releases an iterator.
 * @param[in] iter Iterator to free. If NULL, this function does nothing.
 */
BLOSC_EXPORT void blosc2_dict_store_iter_free( blosc2_dict_store_iter* iter );

/**
 * @brief Advances the iterator to the next live entry.
 *
 * @param[in,out] iter Iterator. Must not be NULL.
 * @return true if a new entry is available, false when the iteration is over.
 *
 * @pre It must be called before blosc2_dict_store_iter_get_key() and
 *      blosc2_dict_store_iter_get_container().
 */
BLOSC_EXPORT bool blosc2_dict_store_iter_next( blosc2_dict_store_iter *iter );

/**
 * @brief Returns the key of the current entry.
 *
 * @param[in] iter Iterator.
 * @return The canonical (normalized) key.
 *
 * @pre The last call to blosc2_dict_store_iter_next() returned true.
 */
BLOSC_EXPORT const uint8_t* blosc2_dict_store_iter_get_key( blosc2_dict_store_iter *iter );

/**
 * @brief Returns the schunk of the current entry.
 *
 * @param[in]  iter       Iterator. Must not be NULL.
 * @param[out] item Receives the schunk. Must not be NULL; it is set to
 *                        NULL on failure.
 *
 * @retval BLOSC2_ERROR_SUCCESS       The container was retrieved.
 * @retval BLOSC2_ERROR_NULL_POINTER  @p iter or @p pcontainer is NULL.
 * @retval BLOSC2_ERROR_INVALID_PARAM The dictionary is opened WRONLY.
 *
 * @warning OWNERSHIP follows the same rules as blosc2_dict_store_get():
 *          BORROWED for B2_DS_MEMORY_SPARSE (do not free), and a NEW schunk
 *          (free with blosc2_schunk_free()) for B2_DS_MEMORY_CONTIGUOUS,
 *          B2_DS_B2Z and B2_DS_B2D.
 *
 * @pre The last call to blosc2_dict_store_iter_next() returned true.
 * @sa blosc2_dict_store_get
 */
BLOSC_EXPORT int blosc2_dict_store_iter_get_container( blosc2_dict_store_iter *iter, blosc2_store_value* item );


#endif //BLOSC_STORES_H
