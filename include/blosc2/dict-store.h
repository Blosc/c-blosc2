/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/*********************************************************************
  @file dict-store.h
  @brief Blosc2 header file.

  This file contains the blosc2_dict_store public API
  @author Blosc Development Team <blosc@blosc.org>
**********************************************************************/

#ifndef BLOSC_DICT_STORE_H
#define BLOSC_DICT_STORE_H

#include <blosc2.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup dict_store DictStore
 * @brief Key -> schunk dictionary with in-memory and on-disk backends.
 * @{
 */

/** @brief Opaque handle to a dictionary. */
typedef struct blosc2_dict_store blosc2_dict_store;

/** @brief Opaque handle to a dictionary iterator. */
typedef struct blosc2_dict_store_iter blosc2_dict_store_iter;

/**
 * @brief Mode in which a dictionary is opened or created.
 */
typedef enum {
    B2_DS_RDONLY = 0,  /**< Read only: modify operations fail. */
    B2_DS_WRONLY,      /**< Write only: read operations fail; a new destination is created. */
    B2_DS_RDWR,        /**< Read and write. */
} b2_ds_open_mode;

/**
 * @brief Storage backend of a dictionary.
 *
 * Value returned by blosc2_dict_store_get_storage_type().
 */
typedef enum {
    B2_DS_MEMORY_SPARSE = 0, /**< In memory, one schunk pointer per key (the caller keeps ownership). */
    B2_DS_B2Z = 1,               /**< A .b2z archive (ZIP container). */
    B2_DS_B2D = 2,               /**< A .b2d directory holding one file per key. */
    B2_DS_MEMORY_CONTIGUOUS, /**< In memory, all entries packed into a single buffer (see blosc2_dict_store_from_buffer()). */
}b2_ds_storage_type;

typedef enum {
    B2_DS_ITEM_LOCAL,
    B2_DS_ITEM_REMOTE
}b2_ds_item_tag;

typedef struct {
    b2_ds_item_tag tag;
    union {
        blosc2_schunk* schunk; // Local
        struct {
            uint8_t* urlbase;
            uint8_t* path;
        }remote;
    };
}b2_ds_item;

/** @name Alloc / Free API */
/** @{ */

/**
 * @brief Creates a new dictionary.
 *
 * @param[out] pds     Receives the new dictionary. Must not be NULL; it is set
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
int blosc2_dict_store_new( blosc2_dict_store **pds, const uint8_t* urlpath );

/**
 * @brief Opens an existing dictionary stored on disk.
 *
 * The backend (b2z or b2d) is inferred from the extension of @p urlpath.
 *
 * @param[out] pds        Receives the opened dictionary. Must not be NULL; it is
 *                       set to NULL on failure.
 * @param[in]  urlpath   Path of an existing `.b2z` archive or `.b2d` directory.
 * @param[in]  open_mode Requested access mode.
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
int blosc2_dict_store_open(blosc2_dict_store** pds, const uint8_t* urlpath, b2_ds_open_mode open_mode);

/**
 * @brief Flushes pending changes to disk and destroys the temporary structures.
 *
 * For in-memory and b2d dictionaries this is a no-op. For a b2z dictionary
 * opened WRONLY/RDWR it packs the temporary directory into the final archive.
 * For a b2z dictionary opened RDONLY it is a no-op.
 *
 * @param[in] ds Dictionary to close. Must not be NULL.
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
int blosc2_dict_store_close( blosc2_dict_store *ds );

/**
 * @brief Releases all the memory owned by a dictionary.
 *
 * @param[in] ds Dictionary to free. If NULL, this function does nothing.
 *
 * @note This does NOT free the schunks stored in an in-memory sparse
 *       dictionary; they are owned by the caller.
 * @note For b2z/b2d dictionaries, call blosc2_dict_store_close() first if you
 *       want the changes to reach the disk.
 *
 * @sa blosc2_dict_store_new, blosc2_dict_store_open, blosc2_dict_store_close
 */
void blosc2_dict_store_free( blosc2_dict_store *ds );

/** @} */  /* Alloc / Free API */

/** @name Serialization / conversion API */
/** @{ */

/**
 * @brief Writes the dictionary contents to a `.b2z` archive.
 *
 * @param[in] ds                  Dictionary to dump. Must not be NULL.
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
int blosc2_dict_store_to_b2z( blosc2_dict_store *ds, const uint8_t* destination_urlpath );

/**
 * @brief Writes the dictionary contents to a `.b2d` directory.
 *
 * @param[in] ds      Dictionary to dump. Must not be NULL.
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
int blosc2_dict_store_to_b2d( blosc2_dict_store *ds, const uint8_t* urlpath );

/**
 * @brief Serializes an in-memory dictionary into a new heap buffer.
 *
 * @param[in]  ds      Dictionary to serialize. Must not be NULL and must be
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
int blosc2_dict_store_to_buffer( blosc2_dict_store *ds, void** pbuffer, size_t* psize );

/**
 * @brief Creates a read-only, contiguous in-memory dictionary from a buffer.
 *
 * The buffer must have been produced by blosc2_dict_store_to_buffer().
 *
 * @param[out] pds    Receives the new dictionary. Must not be NULL.
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
int blosc2_dict_store_from_buffer(blosc2_dict_store** pds, void* buffer, size_t size);

/** @} */  /* Serialization / conversion API */

/** @name Introspection API */
/** @{ */

/**
 * @brief Returns the number of keys currently stored.
 * @param[in] ds Dictionary. Must not be NULL.
 */
size_t blosc2_dict_store_get_length( blosc2_dict_store *ds );

/**
 * @brief Returns the number of slots in the internal hash table.
 *
 * Always greater than or equal to blosc2_dict_store_get_length(). It includes
 * the slots occupied by deleted entries (tombstones), so it is an upper bound
 * of the memory the table currently uses.
 *
 * @param[in] ds Dictionary. Must not be NULL.
 */
size_t blosc2_dict_store_get_capacity( blosc2_dict_store *ds );

/**
 * @brief Returns the access mode the dictionary was created/opened with.
 * @param[in] ds Dictionary. Must not be NULL.
 */
b2_ds_open_mode blosc2_dict_store_get_open_mode( blosc2_dict_store *ds );

/**
 * @brief Returns the storage backend of the dictionary.
 *
 * @param[in] ds Dictionary. Must not be NULL.
 * @return One of the values of the storage-type enum: B2_DS_MEMORY_SPARSE,
 *         B2_DS_MEMORY_CONTIGUOUS, B2_DS_B2Z or B2_DS_B2D. Returns a negative
 *         value for an unknown backend.
 */
int blosc2_dict_store_get_storage_type( blosc2_dict_store *ds );

/** @} */  /* Introspection API */

/** @name Get / Set / Remove API */
/** @{ */

/**
 * @brief Reports whether a key exists.
 *
 * The key is normalized internally, so equivalent spellings such as "a/b",
 * "/a/b", "a//b" or "a/./b" refer to the same key.
 *
 * @param[in] ds  Dictionary. If NULL, the function returns false.
 * @param[in] key Key. If NULL, the function returns false.
 * @return true if the key is present, false otherwise (including NULL or
 *         non-normalizable inputs).
 *
 * @sa blosc2_dict_store_get
 */
bool blosc2_dict_store_contains_key( blosc2_dict_store *ds, const uint8_t* key );

/**
 * @brief Retrieves the schunk associated with a key.
 *
 * @param[in]  ds         Dictionary. Must not be NULL.
 * @param[in]  key        Key. Must not be NULL; it is normalized internally.
 * @param[out] pcontainer Receives the schunk. Must not be NULL; it is set to
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
int blosc2_dict_store_get(blosc2_dict_store* ds, const uint8_t* key, b2_ds_item* item);

/**
 * @brief Inserts or replaces the schunk associated with a key.
 *
 * Overwriting an existing key replaces its value and keeps the length
 * unchanged; inserting a new key increases the length by one.
 *
 * @param[in] ds        Dictionary. Must not be NULL.
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
int blosc2_dict_store_set(blosc2_dict_store* ds, const uint8_t* key, blosc2_schunk* container);

/**
 * @brief Removes a key.
 *
 * @param[in] ds  Dictionary. Must not be NULL.
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
int blosc2_dict_store_remove(blosc2_dict_store* ds, const uint8_t* key);

/** @} */  /* Get / Set / Remove API */

/** @name Iterator API */
/** @{ */

/**
 * @brief Creates an iterator over the live keys of a dictionary.
 *
 * @param[in] ds Dictionary. Must not be NULL.
 * @return A new iterator, or NULL on failure. Release it with
 *         blosc2_dict_store_iter_free().
 *
 * @warning Do not insert or remove entries while iterating; the iteration
 *          order is unspecified.
 */
blosc2_dict_store_iter* blosc2_dict_store_iter_new( blosc2_dict_store *ds );

/**
 * @brief Releases an iterator.
 * @param[in] iter Iterator to free. If NULL, this function does nothing.
 */
void blosc2_dict_store_iter_free( blosc2_dict_store_iter* iter );

/**
 * @brief Advances the iterator to the next live entry.
 *
 * @param[in,out] iter Iterator. Must not be NULL.
 * @return true if a new entry is available, false when the iteration is over.
 *
 * @pre It must be called before blosc2_dict_store_iter_get_key() and
 *      blosc2_dict_store_iter_get_container().
 */
bool blosc2_dict_store_iter_next( blosc2_dict_store_iter *iter );

/**
 * @brief Returns the key of the current entry.
 *
 * @param[in] iter Iterator.
 * @return The canonical (normalized) key.
 *
 * @pre The last call to blosc2_dict_store_iter_next() returned true.
 */
const uint8_t* blosc2_dict_store_iter_get_key( blosc2_dict_store_iter *iter );

/**
 * @brief Returns the schunk of the current entry.
 *
 * @param[in]  iter       Iterator. Must not be NULL.
 * @param[out] pcontainer Receives the schunk. Must not be NULL; it is set to
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
int blosc2_dict_store_iter_get_container( blosc2_dict_store_iter *iter, b2_ds_item* item );

/** @} */  /* Iterator API */

/** @} */  /* defgroup dict_store */

#endif // BLOSC_DICT_STORE_H
