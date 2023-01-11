/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/** @file caterva.h
 * @brief Caterva header file.
 *
 * This file contains Caterva public API and the structures needed to use it.
 * @author Blosc Development team <blosc@blosc.org>
 */

#ifndef CATERVA_CATERVA_H_
#define CATERVA_CATERVA_H_

#include <blosc2.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version numbers */
#define CATERVA_VERSION_MAJOR 0         /* for major interface/format changes  */
#define CATERVA_VERSION_MINOR 5         /* for minor interface/format changes  */
#define CATERVA_VERSION_RELEASE 1       /* for tweaks, bug-fixes, or development */

#define CATERVA_VERSION_STRING "0.5.1.dev0" /* string version. Sync with above! */
#define CATERVA_VERSION_DATE "2021-07-13"  /* date version */

/* Error handling */
#define CATERVA_SUCCEED 0
#define CATERVA_ERR_INVALID_ARGUMENT 1
#define CATERVA_ERR_BLOSC_FAILED 2
#define CATERVA_ERR_CONTAINER_FILLED 3
#define CATERVA_ERR_INVALID_STORAGE 4
#define CATERVA_ERR_NULL_POINTER 5
#define CATERVA_ERR_INVALID_INDEX  5


/* Tracing macros */
#define CATERVA_TRACE_ERROR(fmt, ...) CATERVA_TRACE(error, fmt, ##__VA_ARGS__)
#define CATERVA_TRACE_WARNING(fmt, ...) CATERVA_TRACE(warning, fmt, ##__VA_ARGS__)

#define CATERVA_TRACE(cat, msg, ...)                                 \
    do {                                                             \
         const char *__e = getenv("CATERVA_TRACE");                  \
         if (!__e) { break; }                                        \
         fprintf(stderr, "[%s] - %s:%d\n    " msg "\n", #cat, __FILE__, __LINE__, ##__VA_ARGS__);   \
    } while(0)

#define CATERVA_ERROR(rc)                           \
    do {                                            \
        int rc_ = rc;                               \
        if (rc_ != CATERVA_SUCCEED) {               \
            char *error_msg = print_error(rc_);     \
            CATERVA_TRACE_ERROR("%s", error_msg); \
            return rc_;                             \
        }                                           \
    } while (0)

#define CATERVA_ERROR_NULL(pointer)                                 \
    do {                                                            \
        char *error_msg = print_error(CATERVA_ERR_NULL_POINTER);    \
        if ((pointer) == NULL) {                                    \
            CATERVA_TRACE_ERROR("%s", error_msg);                   \
            return CATERVA_ERR_NULL_POINTER;                        \
        }                                                           \
    } while (0)

#define CATERVA_UNUSED_PARAM(x) ((void) (x))
#ifdef __GNUC__
#define CATERVA_ATTRIBUTE_UNUSED __attribute__((unused))
#else
#define CATERVA_ATTRIBUTE_UNUSED
#endif

static char *print_error(int rc) CATERVA_ATTRIBUTE_UNUSED;
static char *print_error(int rc) {
    switch (rc) {
        case CATERVA_ERR_INVALID_STORAGE:
            return (char*)"Invalid storage";
        case CATERVA_ERR_NULL_POINTER:
            return (char*)"Pointer is null";
        case CATERVA_ERR_BLOSC_FAILED:
            return (char*)"Blosc failed";
        case CATERVA_ERR_INVALID_ARGUMENT:
            return (char*)"Invalid argument";
        default:
            return (char*)"Unknown error";
    }
}

/* The version for metalayer format; starts from 0 and it must not exceed 127 */
#define CATERVA_METALAYER_VERSION 0

/* The maximum number of dimensions for caterva arrays */
#define CATERVA_MAX_DIM 8

/* The maximum number of metalayers for caterva arrays */
#define CATERVA_MAX_METALAYERS (BLOSC2_MAX_METALAYERS - 1)

/**
 * @brief Configuration parameters used to create a caterva context.
 */
typedef struct {
    void *(*alloc)(size_t);
    //!< The memory allocation function used internally.
    void (*free)(void *);
    //!< The memory release function used internally.
    uint8_t compcodec;
    //!< Defines the codec used in compression.
    uint8_t compmeta;
    //!< The metadata for the compressor codec.
    uint8_t complevel;
    //!< Determines the compression level used in Blosc.
    int32_t splitmode;
    //!< Whether the blocks should be split or not.
    int usedict;
    //!< Indicates whether a dictionary is used to compress data or not.
    int16_t nthreads;
    //!< Determines the maximum number of threads that can be used.
    uint8_t filters[BLOSC2_MAX_FILTERS];
    //!< Defines the filters used in compression.
    uint8_t filtersmeta[BLOSC2_MAX_FILTERS];
    //!< Indicates the meta filters used in Blosc.
    blosc2_prefilter_fn prefilter;
    //!< Defines the function that is applied to the data before compressing it.
    blosc2_prefilter_params *pparams;
    //!< Indicates the parameters of the prefilter function.
    blosc2_btune *udbtune;
    //!< Indicates user-defined parameters for btune.
} caterva_config_t;

/**
 * @brief The default configuration parameters used in caterva.
 */
static const caterva_config_t CATERVA_CONFIG_DEFAULTS = {.alloc = malloc,
                                                         .free = free,
                                                         .compcodec = BLOSC_BLOSCLZ,
                                                         .compmeta = 0,
                                                         .complevel = 5,
                                                         .splitmode = BLOSC_AUTO_SPLIT,
                                                         .usedict = 0,
                                                         .nthreads = 1,
                                                         .filters = {0, 0, 0, 0, 0, BLOSC_SHUFFLE},
                                                         .filtersmeta = {0, 0, 0, 0, 0, 0},
                                                         .prefilter = NULL,
                                                         .pparams = NULL,
                                                         .udbtune = NULL,
                                                         };

/**
 * @brief Context for caterva arrays that specifies the functions used to manage memory and
 * the compression/decompression parameters used in Blosc.
 */
typedef struct {
    caterva_config_t *cfg;
    //!< The configuration parameters.
} caterva_ctx_t;


/**
 * @brief The metalayer data needed to store it on an array
 */
typedef struct {
    char *name;
    //!< The name of the metalayer
    uint8_t *sdata;
    //!< The serialized data to store
    int32_t size;
    //!< The size of the serialized data
} caterva_metalayer_t;

/**
 * @brief The storage properties for an array backed by a Blosc super-chunk.
 */
typedef struct {
    int32_t chunkshape[CATERVA_MAX_DIM];
    //!< The shape of each chunk of Blosc.
    int32_t blockshape[CATERVA_MAX_DIM];
    //!< The shape of each block of Blosc.
    bool contiguous;
    //!< Flag to indicate if the super-chunk is stored contiguously or sparsely.
    char *urlpath;
    //!< The super-chunk name. If @p urlpath is not @p NULL, the super-chunk will be stored on
    //!< disk.
    caterva_metalayer_t metalayers[CATERVA_MAX_METALAYERS];
    //!< List with the metalayers desired.
    int32_t nmetalayers;
    //!< The number of metalayers.
} caterva_storage_t;

/**
 * @brief General parameters needed for the creation of a caterva array.
 */
typedef struct {
    uint8_t itemsize;
    //!< The size of each item of the array.
    int64_t shape[CATERVA_MAX_DIM];
    //!< The array shape.
    int8_t ndim;
    //!< The array dimensions.
} caterva_params_t;

/**
 * @brief An *optional* cache for a single block.
 *
 * When a chunk is needed, it is copied into this cache. In this way, if the same chunk is needed
 * again afterwards, it is not necessary to recover it because it is already in the cache.
 */
struct chunk_cache_s {
    uint8_t *data;
    //!< The chunk data.
    int64_t nchunk;
    //!< The chunk number in cache. If @p nchunk equals to -1, it means that the cache is empty.
};

/**
 * @brief A multidimensional array of data that can be compressed.
 */
typedef struct {
    caterva_config_t *cfg;
    //!< Array configuration.
    blosc2_schunk *sc;
    //!< Pointer to a Blosc super-chunk
    int64_t shape[CATERVA_MAX_DIM];
    //!< Shape of original data.
    int32_t chunkshape[CATERVA_MAX_DIM];
    //!< Shape of each chunk.
    int64_t extshape[CATERVA_MAX_DIM];
    //!< Shape of padded data.
    int32_t blockshape[CATERVA_MAX_DIM];
    //!< Shape of each block.
    int64_t extchunkshape[CATERVA_MAX_DIM];
    //!< Shape of padded chunk.
    int64_t nitems;
    //!< Number of items in original data.
    int32_t chunknitems;
    //!< Number of items in each chunk.
    int64_t extnitems;
    //!< Number of items in padded data.
    int32_t blocknitems;
    //!< Number of items in each block.
    int64_t extchunknitems;
    //!< Number of items in a padded chunk.
    int8_t ndim;
    //!< Data dimensions.
    uint8_t itemsize;
    //!< Size of each item.
    int64_t nchunks;
    //!< Number of chunks in the array.
    struct chunk_cache_s chunk_cache;
    //!< A partition cache.
    int64_t item_array_strides[CATERVA_MAX_DIM];
    //!< Item - shape strides.
    int64_t item_chunk_strides[CATERVA_MAX_DIM];
    //!< Item - shape strides.
    int64_t item_extchunk_strides[CATERVA_MAX_DIM];
    //!< Item - shape strides.
    int64_t item_block_strides[CATERVA_MAX_DIM];
    //!< Item - shape strides.
    int64_t block_chunk_strides[CATERVA_MAX_DIM];
    //!< Item - shape strides.
    int64_t chunk_array_strides[CATERVA_MAX_DIM];
    //!< Item - shape strides.
} caterva_array_t;

/**
 * @brief Create a context for caterva.
 *
 * @param cfg The configuration parameters needed for the context creation.
 * @param ctx The memory pointer where the context will be created.
 *
 * @return An error code.
 */
BLOSC_EXPORT int caterva_ctx_new(caterva_config_t *cfg, caterva_ctx_t **ctx);

/**
 * @brief Free a context.
 *
 * @param ctx The The context to be freed.
 *
 * @return An error code.
 */
int caterva_ctx_free(caterva_ctx_t **ctx);


/**
 * @brief Create an uninitialized array.
 *
 * @param ctx The caterva context to be used.
 * @param params The general params of the array desired.
 * @param storage The storage params of the array desired.
 * @param array The memory pointer where the array will be created.
 *
 * @return An error code.
 */
int caterva_uninit(caterva_ctx_t *ctx, caterva_params_t *params,
                   caterva_storage_t *storage, caterva_array_t **array);


/**
 * @brief Create an empty array.
 *
 * @param ctx The caterva context to be used.
 * @param params The general params of the array desired.
 * @param storage The storage params of the array desired.
 * @param array The memory pointer where the array will be created.
 *
 * @return An error code.
 */
int caterva_empty(caterva_ctx_t *ctx, caterva_params_t *params,
                  caterva_storage_t *storage, caterva_array_t **array);


/**
 * Create an array, with zero being used as the default value for
 * uninitialized portions of the array.
 *
 * @param ctx The caterva context to be used.
 * @param params The general params of the array.
 * @param storage The storage params of the array.
 * @param array The memory pointer where the array will be created.
 *
 * @return An error code.
 */
int caterva_zeros(caterva_ctx_t *ctx, caterva_params_t *params,
                  caterva_storage_t *storage, caterva_array_t **array);


/**
 * Create an array, with @p fill_value being used as the default value for
 * uninitialized portions of the array.
 *
 * @param ctx The caterva context to be used.
 * @param params The general params of the array.
 * @param storage The storage params of the array.
 * @param fill_value Default value for uninitialized portions of the array.
 * @param array The memory pointer where the array will be created.
 *
 * @return An error code.
 */
int caterva_full(caterva_ctx_t *ctx, caterva_params_t *params,
                 caterva_storage_t *storage, void *fill_value, caterva_array_t **array);
/**
 * @brief Free an array.
 *
 * @param ctx The caterva context to be used.
 * @param array The memory pointer where the array is placed.
 *
 * @return An error code.
 */
int caterva_free(caterva_ctx_t *ctx, caterva_array_t **array);

/**
 * @brief Create a caterva array from a super-chunk. It can only be used if the array
 * is backed by a blosc super-chunk.
 *
 * @param ctx The caterva context to be used.
 * @param schunk The blosc super-chunk where the caterva array is stored.
 * @param array The memory pointer where the array will be created.
 *
 * @return An error code.
 */
int
caterva_from_schunk(caterva_ctx_t *ctx, blosc2_schunk *schunk, caterva_array_t **array);

/**
 * Create a serialized super-chunk from a caterva array.
 *
 * @param ctx The caterva context to be used.
 * @param array The caterva array to be serialized.
 * @param cframe The pointer of the buffer where the in-memory array will be copied.
 * @param cframe_len The length of the in-memory array buffer.
 * @param needs_free Whether the buffer should be freed or not.
 *
 * @return An error code
 */
int caterva_to_cframe(caterva_ctx_t *ctx, caterva_array_t *array, uint8_t **cframe,
                      int64_t *cframe_len, bool *needs_free);

/**
 * @brief Create a caterva array from a serialized super-chunk.
 *
 * @param ctx The caterva context to be used.
 * @param cframe The buffer of the in-memory array.
 * @param cframe_len The size (in bytes) of the in-memory array.
 * @param copy Whether caterva should make a copy of the cframe data or not. The copy will be made to an internal sparse frame.
 * @param array The memory pointer where the array will be created.
 *
 * @return An error code.
 */
int caterva_from_cframe(caterva_ctx_t *ctx, uint8_t *cframe, int64_t cframe_len, bool copy,
                        caterva_array_t **array);

/**
 * @brief Read a caterva array from disk.
 *
 * @param ctx The caterva context to be used.
 * @param urlpath The urlpath of the caterva array on disk.
 * @param array The memory pointer where the array will be created.
 *
 * @return An error code.
 */
int caterva_open(caterva_ctx_t *ctx, const char *urlpath, caterva_array_t **array);

/**
 * @brief Save caterva array into a specific urlpath.
 *
 * @param ctx The context to be used.
 * @param array The array to be saved.
 * @param urlpath The urlpath where the array will be stored.
 *
 * @return An error code.
 */
BLOSC_EXPORT int caterva_save(caterva_ctx_t *ctx, caterva_array_t *array, char *urlpath);

/**
 * @brief Create a caterva array from the data stored in a buffer.
 *
 * @param ctx The caterva context to be used.
 * @param buffer The buffer where source data is stored.
 * @param buffersize The size (in bytes) of the buffer.
 * @param params The general params of the array desired.
 * @param storage The storage params of the array desired.
 * @param array The memory pointer where the array will be created.
 *
 * @return An error code.
 */
BLOSC_EXPORT int caterva_from_buffer(caterva_ctx_t *ctx, void *buffer, int64_t buffersize,
                                     caterva_params_t *params, caterva_storage_t *storage,
                                     caterva_array_t **array);

/**
 * @brief Extract the data into a C buffer from a caterva array.
 *
 * @param ctx The caterva context to be used.
 * @param array The caterva array.
 * @param buffer The buffer where the data will be stored.
 * @param buffersize Size (in bytes) of the buffer.
 *
 * @return An error code.
 */
BLOSC_EXPORT int caterva_to_buffer(caterva_ctx_t *ctx, caterva_array_t *array, void *buffer,
                                   int64_t buffersize);

/**
 * @brief Get a slice from an array and store it into a new array.
 *
 * @param ctx The caterva context to be used.
 * @param src The array from which the slice will be extracted
 * @param start The coordinates where the slice will begin.
 * @param stop The coordinates where the slice will end.
 * @param storage The storage params of the array desired.
 * @param array The memory pointer where the array will be created.
 *
 * @return An error code.
 */
int caterva_get_slice(caterva_ctx_t *ctx, caterva_array_t *src, const int64_t *start,
                      const int64_t *stop, caterva_storage_t *storage, caterva_array_t **array);

/**
 * @brief Squeeze a caterva array
 *
 * This function remove selected single-dimensional entries from the shape of a
 caterva array.
 *
 * @param ctx The caterva context to be used.
 * @param array The caterva array.
 * @param index Indexes of the single-dimensional entries to remove.
 *
 * @return An error code
 */
int caterva_squeeze_index(caterva_ctx_t *ctx, caterva_array_t *array,
                          const bool *index);

/**
 * @brief Squeeze a caterva array
 *
 * This function remove single-dimensional entries from the shape of a caterva array.
 *
 * @param ctx The caterva context to be used.
 * @param array The caterva array.
 *
 * @return An error code
 */
int caterva_squeeze(caterva_ctx_t *ctx, caterva_array_t *array);

/**
 * @brief Get a slice from an array and store it into a C buffer.
 *
 * @param ctx The caterva context to be used.
 * @param array The array from which the slice will be extracted.
 * @param start The coordinates where the slice will begin.
 * @param stop The coordinates where the slice will end.
 * @param buffershape The shape of the buffer.
 * @param buffer The buffer where the data will be stored.
 * @param buffersize The size (in bytes) of the buffer.
 *
 * @return An error code.
 */
int caterva_get_slice_buffer(caterva_ctx_t *ctx, caterva_array_t *array,
                             int64_t *start, int64_t *stop,
                             void *buffer, int64_t *buffershape, int64_t buffersize);

/**
 * @brief Set a slice into a caterva array from a C buffer.
 *
 * @param ctx The caterva context to be used.
 * @param buffer The buffer where the slice data is.
 * @param buffersize The size (in bytes) of the buffer.
 * @param start The coordinates where the slice will begin.
 * @param stop The coordinates where the slice will end.
 * @param buffershape The shape of the buffer.
 * @param array The caterva array where the slice will be set
 *
 * @return An error code.
 */
int caterva_set_slice_buffer(caterva_ctx_t *ctx,
                             void *buffer, int64_t *buffershape, int64_t buffersize,
                             int64_t *start, int64_t *stop, caterva_array_t *array);

/**
 * @brief Make a copy of the array data. The copy is done into a new caterva array.
 *
 * @param ctx The caterva context to be used.
 * @param src The array from which data is copied.
 * @param storage The storage params of the array desired.
 * @param array The memory pointer where the array will be created.
 *
 * @return An error code
 */
int caterva_copy(caterva_ctx_t *ctx, caterva_array_t *src, caterva_storage_t *storage,
                 caterva_array_t **array);


/**
 * @brief Remove a Caterva file from the file system. Both backends are supported.
 *
 * @param ctx The caterva context to be used.
 * @param urlpath The urlpath of the array to be removed.
 *
 * @return An error code
 */
int caterva_remove(caterva_ctx_t *ctx, char *urlpath);


/**
 * @brief Add a vl-metalayer to the Caterva array.
 *
 * @param ctx The context to be used.
 * @param array The array where the metalayer will be added.
 * @param name The vl-metalayer to add.
 *
 * @return An error code
 */
int caterva_vlmeta_add(caterva_ctx_t *ctx, caterva_array_t *array, caterva_metalayer_t *vlmeta);


/**
 *
 * @brief Get a vl-metalayer from a Caterva array.
 *
 * @param ctx The context to be used.
 * @param array The array where the vl-metalayer will be added.
 * @param name The vl-metalayer name.
 * @param vlmeta Pointer to the metalayer where the data will be stored.
 *
 * @warning The contents of `vlmeta` are allocated inside the function.
 * Therefore, they must be released with a `free`.
 *
 * @return An error code
 */
int caterva_vlmeta_get(caterva_ctx_t *ctx, caterva_array_t *array,
                       const char *name, caterva_metalayer_t *vlmeta);

/**
 * @brief Check if a vl-metalayer exists or not.
 *
 * @param ctx The context to be used.
 * @param array The array where the check will be done.
 * @param name The name of the vl-metalayer to check.
 * @param exists Pointer where the result will be stored.
 *
 * @return An error code
 */
int caterva_vlmeta_exists(caterva_ctx_t *ctx, caterva_array_t *array,
                          const char *name, bool *exists);

/**
 * @brief Update a vl-metalayer content in a Caterva array.
 *
 * @param ctx The context to be used.
 * @param array The array where the vl-metalayer will be updated.
 * @param vlmeta The vl-metalayer to update.
 *
 * @return An error code
 */
int caterva_vlmeta_update(caterva_ctx_t *ctx, caterva_array_t *array,
                          caterva_metalayer_t *vlmeta);

/**
 *
 * @brief Get a metalayer from a Caterva array.
 *
 * @param ctx The context to be used.
 * @param array The array where the metalayer will be added.
 * @param name The vl-metalayer name.
 * @param meta Pointer to the metalayer where the data will be stored.
 *
 * @warning The contents of `meta` are allocated inside the function.
 * Therefore, they must be released with a `free`.
 *
 * @return An error code
 */
int caterva_meta_get(caterva_ctx_t *ctx, caterva_array_t *array,
                       const char *name, caterva_metalayer_t *meta);

/**
 * @brief Check if a metalayer exists or not.
 *
 * @param ctx The context to be used.
 * @param array The array where the check will be done.
 * @param name The name of the metalayer to check.
 * @param exists Pointer where the result will be stored.
 *
 * @return An error code
 */
int caterva_meta_exists(caterva_ctx_t *ctx, caterva_array_t *array,
                          const char *name, bool *exists);

/**
 * @brief Print metalayer parameters.
 *
 * @param array The array where the metalayer is stored.
 *
 * @return An error code
 */
int caterva_print_meta(caterva_array_t *array);

/**
 * @brief Update a metalayer content in a Caterva array.
 *
 * @param ctx The context to be used.
 * @param array The array where the metalayer will be updated.
 * @param meta The metalayer to update.
 *
 * @return An error code
 */
int caterva_meta_update(caterva_ctx_t *ctx, caterva_array_t *array,
                          caterva_metalayer_t *meta);

/**
 * @brief Resize the shape of an array
 *
 * @param ctx The context to be used.
 * @param array The array to be resized.
 * @param new_shape The new shape from the array.
 * @param start The position in which the array will be extended or shrinked.
 *
 * @return An error code
 */
int caterva_resize(caterva_ctx_t *ctx, caterva_array_t *array, const int64_t *new_shape, const int64_t *start);


/**
 * @brief Insert given buffer in an array extending the given axis.
 *
 * @param ctx The context to be used.
 * @param array The array to insert the data.
 * @param buffer The buffer data to be inserted.
 * @param buffersize The size (in bytes) of the buffer.
 * @param axis The axis that will be extended.
 * @param insert_start The position inside the axis to start inserting the data.
 *
 * @return An error code.
 */
int caterva_insert(caterva_ctx_t *ctx, caterva_array_t *array, void *buffer, int64_t buffersize,
                   const int8_t axis, int64_t insert_start);

/**
 * Append a buffer at the end of a caterva array.
 *
 * @param ctx The caterva context to be used.
 * @param array The caterva array.
 * @param buffer The buffer where the data is stored.
 * @param buffersize Size (in bytes) of the buffer.
 * @param axis The axis that will be extended to append the data.
 *
 * @return An error code.
 */
int caterva_append(caterva_ctx_t *ctx, caterva_array_t *array, void *buffer, int64_t buffersize,
                   const int8_t axis);

/**
 * @brief Delete shrinking the given axis delete_len items.
 *
 * @param ctx The context to be used.
 * @param array The array to shrink.
 * @param axis The axis to shrink.
 * @param delete_start The start position from the axis to start deleting chunks.
 * @param delete_len The number of items to delete to the array->shape[axis].
 * The newshape[axis] will be the old array->shape[axis] - delete_len
 *
 * @return An error code.
 *
 * @note See also caterva_resize
 */
int caterva_delete(caterva_ctx_t *ctx, caterva_array_t *array, const int8_t axis,
                   int64_t delete_start, int64_t delete_len);



// Indexing section
int caterva_get_orthogonal_selection(caterva_ctx_t *ctx, caterva_array_t *array,
                                     int64_t **selection, int64_t *selection_size,
                                     void *buffer, int64_t *buffershape,
                                     int64_t buffersize);

int caterva_set_orthogonal_selection(caterva_ctx_t *ctx, caterva_array_t *array,
                                     int64_t **selection, int64_t *selection_size,
                                     void *buffer, int64_t *buffershape,
                                     int64_t buffersize);


// Metainfo section
int32_t caterva_serialize_meta(int8_t ndim, int64_t *shape, const int32_t *chunkshape,
                               const int32_t *blockshape, uint8_t **smeta);

BLOSC_EXPORT int32_t caterva_deserialize_meta(uint8_t *smeta, int32_t smeta_len, int8_t *ndim,
                                              int64_t *shape, int32_t *chunkshape,
                                              int32_t *blockshape);


#ifdef __cplusplus
}
#endif

#endif  // CATERVA_CATERVA_H_
