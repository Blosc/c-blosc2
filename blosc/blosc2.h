/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/** @file blosc.h
 * @brief Blosc header file.
 *
 * This file contains Blosc public API and the structures needed to use it.
 * @author Francesc Alted <francesc@blosc.org>
 */

#ifndef BLOSC_H
#define BLOSC_H

#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "blosc2-export.h"
#include "blosc2-common.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Version numbers */
#define BLOSC_VERSION_MAJOR    2    /* for major interface/format changes  */
#define BLOSC_VERSION_MINOR    0    /* for minor interface/format changes  */
#define BLOSC_VERSION_RELEASE  0    /* for tweaks, bug-fixes, or development */

#define BLOSC_VERSION_STRING   "2.0.0.beta.6.dev"  /* string version.  Sync with above! */
#define BLOSC_VERSION_REVISION "$Rev$"   /* revision version */
#define BLOSC_VERSION_DATE     "$Date:: 2020-04-21 #$"    /* date version */


/* The VERSION_FORMAT symbols below should be just 1-byte long */
enum {
  /* Blosc format version, starting at 1
     1 -> Blosc pre-1.0
     2 -> Blosc 1.x series
     3 -> Blosc 2-alpha.x series
     4 -> Blosc 2.x beta.1 series
     */
  BLOSC_VERSION_FORMAT_PRE1 = 1,
  BLOSC1_VERSION_FORMAT = 2,
  BLOSC2_VERSION_FORMAT_ALPHA = 3,
  BLOSC2_VERSION_FORMAT_BETA1 = 4,
  BLOSC_VERSION_FORMAT = BLOSC2_VERSION_FORMAT_BETA1,
};


/* The FRAME_FORMAT_VERSION symbols below should be just 4-bit long */
enum {
  /* Blosc format version
   *  4 -> First version (introduced in beta.1)
   *  1 -> Second version (introduced in beta.2)
   *
   *  *Important note*: version 4 should be avoided because it was used
   *  for beta.1 and before, and it won't be supported anymore.
   */
  BLOSC2_VERSION_FRAME_FORMAT_BETA1 = 4,  // for beta.1 and before
  BLOSC2_VERSION_FRAME_FORMAT_BETA2 = 1,  // for beta.2 and after
  BLOSC2_VERSION_FRAME_FORMAT = BLOSC2_VERSION_FRAME_FORMAT_BETA2,
};

enum {
  BLOSC_MIN_HEADER_LENGTH = 16,
  //!< Minimum header length (Blosc1)
  BLOSC_EXTENDED_HEADER_LENGTH = 32,
  //!< Extended header length (Blosc2, see README_HEADER)
  BLOSC_MAX_OVERHEAD = BLOSC_EXTENDED_HEADER_LENGTH,
  /** The maximum overhead during compression in bytes.  This equals to
   *  BLOSC_EXTENDED_HEADER_LENGTH now, but can be higher in future
   *  implementations
   */
  BLOSC_MAX_BUFFERSIZE = (INT_MAX - BLOSC_MAX_OVERHEAD),
  //!< Maximum source buffer size to be compressed
  BLOSC_MAX_TYPESIZE = 255,
  /** Maximum typesize before considering source buffer as a stream of bytes
   * Cannot be larger than 255
   */
  BLOSC_MIN_BUFFERSIZE = 128,
  /** Minimum buffer size to be compressed.
   *  Cannot be smaller than 66.
   */
};

/**
 * @brief Codes for filters.
 * @see #blosc_compress
 */
enum {
  BLOSC_NOSHUFFLE = 0,   //!< no shuffle (for compatibility with Blosc1)
  BLOSC_NOFILTER = 0,    //!< no filter
  BLOSC_SHUFFLE = 1,     //!< byte-wise shuffle
  BLOSC_BITSHUFFLE = 2,  //!< bit-wise shuffle
  BLOSC_DELTA = 3,       //!< delta filter
  BLOSC_TRUNC_PREC = 4,  //!< truncate precision filter
  BLOSC_LAST_FILTER= 5,  //!< sentinel
};

enum {
  BLOSC2_MAX_FILTERS = 6,
  //!< Maximum number of filters in the filter pipeline
};

/**
 * @brief Codes for internal flags (see blosc_cbuffer_metainfo)
 */
enum {
  BLOSC_DOSHUFFLE = 0x1,     //!< byte-wise shuffle
  BLOSC_MEMCPYED = 0x2,      //!< plain copy
  BLOSC_DOBITSHUFFLE = 0x4,  //!< bit-wise shuffle
  BLOSC_DODELTA = 0x8,       //!< delta coding
};

/**
 * @brief Codes for new internal flags in Blosc2
 */
enum {
  BLOSC2_USEDICT = 0x1,          //!< use dictionaries with codec
  BLOSC2_BIGENDIAN = 0x2,        //!< data is in big-endian ordering
};

/**
 * @brief Values for different Blosc2 capabilities
 */
enum {
  BLOSC2_MAXDICTSIZE = 128 * 1024, //!< maximum size for compression dicts
};

/**
 * @brief Codes for the different compressors shipped with Blosc
 */
enum {
  BLOSC_BLOSCLZ = 0,
  BLOSC_LZ4 = 1,
  BLOSC_LZ4HC = 2,
  BLOSC_SNAPPY = 3,
  BLOSC_ZLIB = 4,
  BLOSC_ZSTD = 5,
  BLOSC_LIZARD = 6,
  BLOSC_MAX_CODECS = 7,  //!< maximum number of reserved codecs
};


// Names for the different compressors shipped with Blosc

#define BLOSC_BLOSCLZ_COMPNAME   "blosclz"
#define BLOSC_LZ4_COMPNAME       "lz4"
#define BLOSC_LZ4HC_COMPNAME     "lz4hc"
#define BLOSC_LIZARD_COMPNAME    "lizard"
#define BLOSC_SNAPPY_COMPNAME    "snappy"
#define BLOSC_ZLIB_COMPNAME      "zlib"
#define BLOSC_ZSTD_COMPNAME      "zstd"

/**
 * @brief Codes for compression libraries shipped with Blosc (code must be < 8)
 */
enum {
  BLOSC_BLOSCLZ_LIB = 0,
  BLOSC_LZ4_LIB = 1,
  BLOSC_SNAPPY_LIB = 2,
  BLOSC_ZLIB_LIB = 3,
  BLOSC_ZSTD_LIB = 4,
  BLOSC_LIZARD_LIB = 5,
  BLOSC_SCHUNK_LIB = 7,   //!< compressor library in super-chunk header
};

/**
 * @brief Names for the different compression libraries shipped with Blosc
 */
#define BLOSC_BLOSCLZ_LIBNAME   "BloscLZ"
#define BLOSC_LZ4_LIBNAME       "LZ4"
#define BLOSC_LIZARD_LIBNAME    "Lizard"
#define BLOSC_SNAPPY_LIBNAME    "Snappy"
#if defined(HAVE_MINIZ)
  #define BLOSC_ZLIB_LIBNAME    "Zlib (via miniz)"
#else
  #define BLOSC_ZLIB_LIBNAME    "Zlib"
#endif	/* HAVE_MINIZ */
#define BLOSC_ZSTD_LIBNAME      "Zstd"

/**
 * @brief The codes for compressor formats shipped with Blosc
 */
enum {
  BLOSC_BLOSCLZ_FORMAT = BLOSC_BLOSCLZ_LIB,
  BLOSC_LZ4_FORMAT = BLOSC_LZ4_LIB,
  //!< LZ4HC and LZ4 share the same format
  BLOSC_LZ4HC_FORMAT = BLOSC_LZ4_LIB,
  BLOSC_LIZARD_FORMAT = BLOSC_LIZARD_LIB,
  BLOSC_SNAPPY_FORMAT = BLOSC_SNAPPY_LIB,
  BLOSC_ZLIB_FORMAT = BLOSC_ZLIB_LIB,
  BLOSC_ZSTD_FORMAT = BLOSC_ZSTD_LIB,
};

/**
 * @brief The version formats for compressors shipped with Blosc.
 * All versions here starts at 1
 */
enum {
  BLOSC_BLOSCLZ_VERSION_FORMAT = 1,
  BLOSC_LZ4_VERSION_FORMAT = 1,
  BLOSC_LZ4HC_VERSION_FORMAT = 1,  /* LZ4HC and LZ4 share the same format */
  BLOSC_LIZARD_VERSION_FORMAT = 1,
  BLOSC_SNAPPY_VERSION_FORMAT = 1,
  BLOSC_ZLIB_VERSION_FORMAT = 1,
  BLOSC_ZSTD_VERSION_FORMAT = 1,
};

/**
 * @brief Initialize the Blosc library environment.
 *
 * You must call this previous to any other Blosc call, unless you want
 * Blosc to be used simultaneously in a multi-threaded environment, in
 * which case you can use the
 * @see #blosc2_compress_ctx #blosc2_decompress_ctx pair.
 */
BLOSC_EXPORT void blosc_init(void);


/**
 * @brief Destroy the Blosc library environment.
 *
 * You must call this after to you are done with all the Blosc calls,
 * unless you have not used blosc_init() before
 * @see #blosc_init.
 */
BLOSC_EXPORT void blosc_destroy(void);


/**
 * @brief Compress a block of data in the @p src buffer and returns the size of
 * compressed block.
 *
 * @remark Compression is memory safe and guaranteed not to write @p dest
 * more than what is specified in @p destsize.
 * There is not a minimum for @p src buffer size @p nbytes.
 *
 * @warning The @p src buffer and the @p dest buffer can not overlap.
 *
 * @param clevel The desired compression level and must be a number
 * between 0 (no compression) and 9 (maximum compression).
 * @param doshuffle Specifies whether the shuffle compression preconditioner
 * should be applied or not. @a BLOSC_NOFILTER means not applying filters,
 * @a BLOSC_SHUFFLE means applying shuffle at a byte level and
 * @a BLOSC_BITSHUFFLE at a bit level (slower but *may* achieve better
 * compression).
 * @param typesize Is the number of bytes for the atomic type in binary
 * @p src buffer.  This is mainly useful for the shuffle preconditioner.
 * For implementation reasons, only a 1 < typesize < 256 will allow the
 * shuffle filter to work.  When typesize is not in this range, shuffle
 * will be silently disabled.
 * @param nbytes The number of bytes to compress in the @p src buffer.
 * @param src The buffer containing the data to compress.
 * @param dest The buffer where the compressed data will be put,
 * must have at least the size of @p destsize.
 * @param destsize The size of the dest buffer. Blosc
 * guarantees that if you set @p destsize to, at least,
 * (@p nbytes + @a BLOSC_MAX_OVERHEAD), the compression will always succeed.
 *
 * @return The number of bytes compressed.
 * If @p src buffer cannot be compressed into @p destsize, the return
 * value is zero and you should discard the contents of the @p dest
 * buffer. A negative return value means that an internal error happened. This
 * should never happen. If you see this, please report it back
 * together with the buffer data causing this and compression settings.
 *
 * This function supports all the configuration based environment variables
 * available in blosc2_compress.
 */
BLOSC_EXPORT int blosc_compress(int clevel, int doshuffle, size_t typesize,
                                size_t nbytes, const void* src, void* dest,
                                size_t destsize);


/**
 * @brief Decompress a block of compressed data in @p src, put the result in
 * @p dest and returns the size of the decompressed block.
 *
 * @warning The @p src buffer and the @p dest buffer can not overlap.
 *
 * @remark Decompression is memory safe and guaranteed not to write the @p dest
 * buffer more than what is specified in @p destsize.
 *
 * @remark In case you want to keep under control the number of bytes read from
 * source, you can call #blosc_cbuffer_sizes first to check whether the
 * @p nbytes (i.e. the number of bytes to be read from @p src buffer by this
 * function) in the compressed buffer is ok with you.
 *
 * @param src The buffer to be decompressed.
 * @param dest The buffer where the decompressed data will be put.
 * @param destsize The size of the @p dest buffer.
 *
 * @return The number of bytes decompressed.
 * If an error occurs, e.g. the compressed data is corrupted or the
 * output buffer is not large enough, then 0 (zero) or a negative value
 * will be returned instead.
 *
 * This function supports all the configuration based environment variables
 * available in blosc2_decompress.
 */
BLOSC_EXPORT int blosc_decompress(const void* src, void* dest, size_t destsize);


/**
 * @brief Get @p nitems (of @p typesize size) in @p src buffer starting in @p start.
 * The items are returned in @p dest buffer, which has to have enough
 * space for storing all items.
 *
 * @param src The compressed buffer from data will be decompressed.
 * @param start The position of the first item (of @p typesize size) from where data
 * will be retrieved.
 * @param nitems The number of items (of @p typesize size) that will be retrieved.
 * @param dest The buffer where the decompressed data retrieved will be put.
 *
 * @return The number of bytes copied to @p dest or a negative value if
 * some error happens.
 */
BLOSC_EXPORT int blosc_getitem(const void* src, int start, int nitems, void* dest);


/**
  Pointer to a callback function that executes `dojob(jobdata + i*jobdata_elsize)` for `i = 0 to numjobs-1`,
  possibly in parallel threads (but not returning until all `dojob` calls have returned).   This allows the
  caller to provide a custom threading backend as an alternative to the default Blosc-managed threads.
  `callback_data` is passed through from `blosc_set_threads_callback`.
 */
typedef void (*blosc_threads_callback)(void *callback_data, void (*dojob)(void *), int numjobs, size_t jobdata_elsize, void *jobdata);

/**
  Set the threading backend for parallel compression/decompression to use `callback` to execute work
  instead of using the Blosc-managed threads.   This function is *not* thread-safe and should be called
  before any other Blosc function: it affects all Blosc contexts.  Passing `NULL` uses the default
  Blosc threading backend.  The `callback_data` argument is passed through to the callback.
 */
BLOSC_EXPORT void blosc_set_threads_callback(blosc_threads_callback callback, void *callback_data);


/**
 * @brief Returns the current number of threads that are used for
 * compression/decompression.
 */
BLOSC_EXPORT int blosc_get_nthreads(void);


/**
 * @brief Initialize a pool of threads for compression/decompression. If
 * @p nthreads is 1, then the serial version is chosen and a possible
 * previous existing pool is ended. If this is not called, @p nthreads
 * is set to 1 internally.
 *
 * @param nthreads The number of threads to use.
 *
 * @return The previous number of threads.
 */
BLOSC_EXPORT int blosc_set_nthreads(int nthreads);


/**
 * @brief Get the current compressor that is used for compression.
 *
 * @return The string identifying the compressor being used.
 */
BLOSC_EXPORT const char* blosc_get_compressor(void);


/**
 * @brief Select the compressor to be used. The supported ones are "blosclz",
 * "lz4", "lz4hc", "snappy", "zlib" and "ztsd". If this function is not
 * called, then "blosclz" will be used.
 *
 * @param compname The name identifier of the compressor to be set.
 *
 * @return The code for the compressor (>=0). In case the compressor
 * is not recognized, or there is not support for it in this build,
 * it returns a -1.
 */
BLOSC_EXPORT int blosc_set_compressor(const char* compname);


/**
 * @brief Select the delta coding filter to be used.
 *
 * @param dodelta A value >0 will activate the delta filter.
 * If 0, it will be de-activated
 *
 * This call should always succeed.
 */
BLOSC_EXPORT void blosc_set_delta(int dodelta);


/**
 * @brief Get the compressor name associated with the compressor code.
 *
 * @param compcode The code identifying the compressor
 * @param compname The pointer to a string where the compressor name will be put.
 *
 * @return The compressor code. If the compressor code is not recognized,
 * or there is not support for it in this build, -1 is returned.
 */
BLOSC_EXPORT int blosc_compcode_to_compname(int compcode, const char** compname);


/**
 * @brief Get the compressor code associated with the compressor name.
 *
 * @param compname The string containing the compressor name.
 *
 * @return The compressor code. If the compressor name is not recognized,
 * or there is not support for it in this build, -1 is returned instead.
 */
BLOSC_EXPORT int blosc_compname_to_compcode(const char* compname);


/**
 * @brief Get a list of compressors supported in the current build.
 *
 * @return The comma separated string with the list of compressor names
 * supported.
 *
 * This function does not leak, so you should not free() the returned
 * list.
 *
 * This function should always succeed.
 */
BLOSC_EXPORT const char* blosc_list_compressors(void);


/**
 * @brief Get the version of Blosc in string format.
 *
 * @return The string with the current Blosc version.
 * Useful for dynamic libraries.
 */
BLOSC_EXPORT const char* blosc_get_version_string(void);


/**
 * @brief Get info from compression libraries included in the current build.
 *
 * @param compname The compressor name that you want info from.
 * @param complib The pointer to a string where the
 * compression library name, if available, will be put.
 * @param version The pointer to a string where the
 * compression library version, if available, will be put.
 *
 * @warning You are in charge of the @p complib and @p version strings,
 * you should free() them so as to avoid leaks.
 *
 * @return The code for the compression library (>=0). If it is not supported,
 * this function returns -1.
 */
BLOSC_EXPORT int blosc_get_complib_info(const char* compname, char** complib,
                                        char** version);


/**
 * @brief Free possible memory temporaries and thread resources. Use this
 * when you are not going to use Blosc for a long while.
 *
 * @return A 0 if succeeds, in case of problems releasing the resources,
 * it returns a negative number.
 */
BLOSC_EXPORT int blosc_free_resources(void);


/**
 * @brief Get information about a compressed buffer, namely the number of
 * uncompressed bytes (@p nbytes) and compressed (@p cbytes). It also
 * returns the @p blocksize (which is used internally for doing the
 * compression by blocks).
 *
 * @param cbuffer The buffer of compressed data.
 * @param nbytes The pointer where the number of uncompressed bytes will be put.
 * @param cbytes The pointer where the number of compressed bytes will be put.
 * @param blocksize The pointer where the block size will be put.
 *
 * You only need to pass the first BLOSC_EXTENDED_HEADER_LENGTH bytes of a
 * compressed buffer for this call to work.
 *
 * This function should always succeed.
 */
BLOSC_EXPORT void blosc_cbuffer_sizes(const void* cbuffer, size_t* nbytes,
                                      size_t* cbytes, size_t* blocksize);

/**
 * @brief Checks that the compressed buffer starting at @cbuffer of length @cbytes may
 * contain valid blosc compressed data, and that it is safe to call
 * blosc_decompress/blosc_decompress_ctx/blosc_getitem.
 * On success, returns 0 and sets @nbytes to the size of the uncompressed data.
 * This does not guarantee that the decompression function won't return an error,
 * but does guarantee that it is safe to attempt decompression.
 *
 * @param cbuffer The buffer of compressed data.
 * @param cbytes The number of compressed bytes.
 * @param nbytes The pointer where the number of uncompressed bytes will be put.
 *
 * @return On failure, returns -1.
 */
BLOSC_EXPORT int blosc_cbuffer_validate(const void* cbuffer, size_t cbytes,
                                         size_t* nbytes);

/**
 * @brief Get information about a compressed buffer, namely the type size
 * (@p typesize), as well as some internal @p flags.
 *
 * @param cbuffer The buffer of compressed data.
 * @param typesize The pointer where the type size will be put.
 * @param flags The pointer of the integer where the additional info is encoded.
 * The @p flags is a set of bits, where the currently used ones are:
 *   * bit 0: whether the shuffle filter has been applied or not
 *   * bit 1: whether the internal buffer is a pure memcpy or not
 *   * bit 2: whether the bitshuffle filter has been applied or not
 *   * bit 3: whether the delta coding filter has been applied or not
 *
 * You can use the @p BLOSC_DOSHUFFLE, @p BLOSC_DOBITSHUFFLE, @p BLOSC_DODELTA
 * and @p BLOSC_MEMCPYED symbols for extracting the interesting bits
 * (e.g. @p flags & @p BLOSC_DOSHUFFLE says whether the buffer is byte-shuffled
 * or not).
 *
 * This function should always succeed.
 */
BLOSC_EXPORT void blosc_cbuffer_metainfo(const void* cbuffer, size_t* typesize,
                                         int* flags);


/**
 * @brief Get information about a compressed buffer, namely the internal
 * Blosc format version (@p version) and the format for the internal
 * Lempel-Ziv compressor used (@p versionlz).
 *
 * @param cbuffer The buffer of compressed data.
 * @param version The pointer where the Blosc format version will be put.
 * @param versionlz The pointer where the Lempel-Ziv version will be put.
 *
 * This function should always succeed.
 */
BLOSC_EXPORT void blosc_cbuffer_versions(const void* cbuffer, int* version,
                                         int* versionlz);


/**
 * @brief Get the compressor library/format used in a compressed buffer.
 *
 * @param cbuffer The buffer of compressed data.
 *
 * @return The string identifying the compressor library/format used.
 *
 * This function should always succeed.
 */
BLOSC_EXPORT const char* blosc_cbuffer_complib(const void* cbuffer);


/*********************************************************************
  Structures and functions related with contexts.
*********************************************************************/

typedef struct blosc2_context_s blosc2_context;   /* opaque type */

/**
 * @brief The parameters for a prefilter function.
 *
 */
typedef struct {
  void *user_data;  // user-provided info (optional)
  uint8_t *out;  // the output buffer
  int32_t out_size;  // the output size (in bytes)
  int32_t out_typesize;  // the output typesize
  int32_t out_offset; // offset to reach the start of the output buffer
  int32_t tid;  // thread id
  uint8_t *ttmp;  // a temporary that is able to hold several blocks for the output and is private for each thread
  size_t ttmp_nbytes;  // the size of the temporary in bytes
  blosc2_context *ctx;  // the decompression context
} blosc2_prefilter_params;

/**
 * @brief The type of the prefilter function.
 *
 * If the function call is successful, the return value should be 0; else, a negative value.
 */
typedef int (*blosc2_prefilter_fn)(blosc2_prefilter_params* params);

/**
 * @brief The parameters for creating a context for compression purposes.
 *
 * In parenthesis it is shown the default value used internally when a 0
 * (zero) in the fields of the struct is passed to a function.
 */
typedef struct {
  uint8_t compcode;
  //!< The compressor codec.
  uint8_t clevel;
  //!< The compression level (5).
  int use_dict;
  //!< Use dicts or not when compressing (only for ZSTD).
  int32_t typesize;
  //!< The type size (8).
  int16_t nthreads;
  //!< The number of threads to use internally (1).
  int32_t blocksize;
  //!< The requested size of the compressed blocks (0; meaning automatic).
  void* schunk;
  //!< The associated schunk, if any (NULL).
  uint8_t filters[BLOSC2_MAX_FILTERS];
  //!< The (sequence of) filters.
  uint8_t filters_meta[BLOSC2_MAX_FILTERS];
  //!< The metadata for filters.
  blosc2_prefilter_fn prefilter;
  //!< The prefilter function.
  blosc2_prefilter_params *pparams;
  //!< The prefilter parameters.
} blosc2_cparams;

/**
 * @brief Default struct for compression params meant for user initialization.
 */
static const blosc2_cparams BLOSC2_CPARAMS_DEFAULTS = {
        BLOSC_BLOSCLZ, 5, 0, 8, 1, 0, NULL,
        {0, 0, 0, 0, 0, BLOSC_SHUFFLE}, {0, 0, 0, 0, 0, 0},
        NULL, NULL };

/**
  @brief The parameters for creating a context for decompression purposes.

  In parenthesis it is shown the default value used internally when a 0
  (zero) in the fields of the struct is passed to a function.
 */
typedef struct {
  int nthreads;
  //!< The number of threads to use internally (1).
  void* schunk;
  //!< The associated schunk, if any (NULL).
} blosc2_dparams;

/**
 * @brief Default struct for decompression params meant for user initialization.
 */
static const blosc2_dparams BLOSC2_DPARAMS_DEFAULTS = {1, NULL};

/**
 * @brief Create a context for @a *_ctx() compression functions.
 *
 * @param cparams The blosc2_cparams struct with the compression parameters.
 *
 * @return A pointer to the new context. NULL is returned if this fails.
 */
BLOSC_EXPORT blosc2_context* blosc2_create_cctx(blosc2_cparams cparams);

/**
 * @brief Create a context for *_ctx() decompression functions.
 *
 * @param dparams The blosc2_dparams struct with the decompression parameters.
 *
 * @return A pointer to the new context. NULL is returned if this fails.
 */
BLOSC_EXPORT blosc2_context* blosc2_create_dctx(blosc2_dparams dparams);

/**
 * @brief Free the resources associated with a context.
 *
 * @param context The context to free.
 *
 * This function should always succeed and is valid for contexts meant for
 * both compression and decompression.
 */
BLOSC_EXPORT void blosc2_free_ctx(blosc2_context* context);

/**
 * @brief Set a maskout so as to avoid decompressing specified blocks.
 *
 * @param ctx The decompression context to update.
 *
 * @param maskout The boolean mask for the blocks where decompression
 * is to be avoided.
 *
 * @remark The maskout is valid for contexts *only* meant for decompressing
 * a chunk via #blosc2_decompress_ctx.  Once a call to #blosc2_decompress_ctx
 * is done, this mask is reset so that next call to #blosc2_decompress_ctx
 * will decompress the whole chunk.
 *
 * @param nblocks The number of blocks in maskout above.
 *
 * @return If success, a 0 values is returned.  An error is signaled with a
 * negative int.
 *
 */
BLOSC_EXPORT int blosc2_set_maskout(blosc2_context *ctx, bool *maskout, int nblocks);
/**
 * @brief Compress a block of data in the @p src buffer and returns the size of
 * compressed block.
 *
 * @remark Compression is memory safe and guaranteed not to write @p dest
 * more than what is specified in @p destsize.
 * There is not a minimum for @p src buffer size @p nbytes.
 *
 * @warning The @p src buffer and the @p dest buffer can not overlap.
 *
 * @param clevel The desired compression level and must be a number
 * between 0 (no compression) and 9 (maximum compression).
 * @param doshuffle Specifies whether the shuffle compression preconditioner
 * should be applied or not. @a BLOSC_NOFILTER means not applying filters,
 * @a BLOSC_SHUFFLE means applying shuffle at a byte level and
 * @a BLOSC_BITSHUFFLE at a bit level (slower but *may* achieve better
 * compression).
 * @param typesize Is the number of bytes for the atomic type in binary
 * @p src buffer.  This is mainly useful for the shuffle preconditioner.
 * For implementation reasons, only a 1 < typesize < 256 will allow the
 * shuffle filter to work.  When typesize is not in this range, shuffle
 * will be silently disabled.
 * @param src The buffer containing the data to compress.
 * @param srcsize The number of bytes to compress in the @p src buffer.
 * @param dest The buffer where the compressed data will be put,
 * must have at least the size of @p destsize.
 * @param destsize The size of the dest buffer. Blosc
 * guarantees that if you set @p destsize to, at least,
 * (@p nbytes + @a BLOSC_MAX_OVERHEAD), the compression will always succeed.
 *
 * @return The number of bytes compressed.
 * If @p src buffer cannot be compressed into @p destsize, the return
 * value is zero and you should discard the contents of the @p dest
 * buffer. A negative return value means that an internal error happened. This
 * should never happen. If you see this, please report it back
 * together with the buffer data causing this and compression settings.
*/
/*
 * Environment variables
 * _____________________
 *
 * *blosc_compress()* honors different environment variables to control
 * internal parameters without the need of doing that programatically.
 * Here are the ones supported:
 *
 * **BLOSC_CLEVEL=(INTEGER)**: This will overwrite the @p clevel parameter
 * before the compression process starts.
 *
 * **BLOSC_SHUFFLE=[NOSHUFFLE | SHUFFLE | BITSHUFFLE]**: This will
 * overwrite the *doshuffle* parameter before the compression process
 * starts.
 *
 * **BLOSC_DELTA=(1|0)**: This will call *blosc_set_delta()^* before the
 * compression process starts.
 *
 * **BLOSC_TYPESIZE=(INTEGER)**: This will overwrite the *typesize*
 * parameter before the compression process starts.
 *
 * **BLOSC_COMPRESSOR=[BLOSCLZ | LZ4 | LZ4HC | LIZARD | SNAPPY | ZLIB]**:
 * This will call *blosc_set_compressor(BLOSC_COMPRESSOR)* before the
 * compression process starts.
 *
 * **BLOSC_NTHREADS=(INTEGER)**: This will call
 * *blosc_set_nthreads(BLOSC_NTHREADS)* before the compression process
 * starts.
 *
 * **BLOSC_BLOCKSIZE=(INTEGER)**: This will call
 * *blosc_set_blocksize(BLOSC_BLOCKSIZE)* before the compression process
 * starts.  *NOTE:* The blocksize is a critical parameter with
 * important restrictions in the allowed values, so use this with care.
 *
 * **BLOSC_NOLOCK=(ANY VALUE)**: This will call *blosc2_compress_ctx()* under
 * the hood, with the *compressor*, *blocksize* and
 * *numinternalthreads* parameters set to the same as the last calls to
 * *blosc_set_compressor*, *blosc_set_blocksize* and
 * *blosc_set_nthreads*. *BLOSC_CLEVEL*, *BLOSC_SHUFFLE*, *BLOSC_DELTA* and
 * *BLOSC_TYPESIZE* environment vars will also be honored.
 *
 */
BLOSC_EXPORT int blosc2_compress(int clevel, int doshuffle, int32_t typesize,
                                 const void* src, int32_t srcsize, void* dest,
                                 int32_t destsize);


/**
 * @brief Decompress a block of compressed data in @p src, put the result in
 * @p dest and returns the size of the decompressed block.
 *
 * @warning The @p src buffer and the @p dest buffer can not overlap.
 *
 * @remark Decompression is memory safe and guaranteed not to write the @p dest
 * buffer more than what is specified in @p destsize.
 *
 * @remark In case you want to keep under control the number of bytes read from
 * source, you can call #blosc_cbuffer_sizes first to check whether the
 * @p nbytes (i.e. the number of bytes to be read from @p src buffer by this
 * function) in the compressed buffer is ok with you.
 *
 * @param src The buffer to be decompressed.
 * @param srcsize The size of the buffer to be decompressed.
 * @param dest The buffer where the decompressed data will be put.
 * @param destsize The size of the @p dest buffer.
 *
 * @return The number of bytes decompressed.
 * If an error occurs, e.g. the compressed data is corrupted or the
 * output buffer is not large enough, then 0 (zero) or a negative value
 * will be returned instead.
*/
/*
 * Environment variables
 * _____________________
 *
 * *blosc_decompress* honors different environment variables to control
 * internal parameters without the need of doing that programatically.
 * Here are the ones supported:
 *
 * **BLOSC_NTHREADS=(INTEGER)**: This will call
 * *blosc_set_nthreads(BLOSC_NTHREADS)* before the proper decompression
 * process starts.
 *
 * **BLOSC_NOLOCK=(ANY VALUE)**: This will call *blosc2_decompress_ctx*
 * under the hood, with the *numinternalthreads* parameter set to the
 * same value as the last call to *blosc_set_nthreads*.
 *
 */
BLOSC_EXPORT int blosc2_decompress(const void* src, int32_t srcsize,
                                   void* dest, int32_t destsize);

/**
 * @brief Context interface to Blosc compression. This does not require a call
 * to #blosc_init and can be called from multithreaded applications
 * without the global lock being used, so allowing Blosc be executed
 * simultaneously in those scenarios.
 *
 * @param context A blosc2_context struct with the different compression params.
 * @param src The buffer containing the data to be compressed.
 * @param srcsize The number of bytes to be compressed from the @p src buffer.
 * @param dest The buffer where the compressed data will be put.
 * @param destsize The size in bytes of the @p dest buffer.
 *
 * @return The number of bytes compressed.
 * If @p src buffer cannot be compressed into @p destsize, the return
 * value is zero and you should discard the contents of the @p dest
 * buffer.  A negative return value means that an internal error happened.
 * It could happen that context is not meant for compression (which is stated in stderr).
 * Otherwise, please report it back together with the buffer data causing this
 * and compression settings.
 */
BLOSC_EXPORT int blosc2_compress_ctx(
        blosc2_context* context, const void* src, int32_t srcsize, void* dest,
        int32_t destsize);


/**
 * @brief Context interface to Blosc decompression. This does not require a
 * call to #blosc_init and can be called from multithreaded
 * applications without the global lock being used, so allowing Blosc
 * be executed simultaneously in those scenarios.
 *
 * @param context The blosc2_context struct with the different compression params.
 * @param src The buffer of compressed data.
 * @param srcsize The length of buffer of compressed data.
 * @param dest The buffer where the decompressed data will be put.
 * @param destsize The size in bytes of the @p dest buffer.
 *
 * @warning The @p src buffer and the @p dest buffer can not overlap.
 *
 * @remark Decompression is memory safe and guaranteed not to write the @p dest
 * buffer more than what is specified in @p destsize.
 *
 * @remark In case you want to keep under control the number of bytes read from
 * source, you can call #blosc_cbuffer_sizes first to check the @p nbytes
 * (i.e. the number of bytes to be read from @p src buffer by this function)
 * in the compressed buffer.
 *
 * @remark If #blosc2_set_maskout is called prior to this function, its
 * @p block_maskout parameter will be honored for just *one single* shot;
 * i.e. the maskout in context will be automatically reset to NULL, so
 * mask won't be used next time (unless #blosc2_set_maskout is called again).
 *
 * @return The number of bytes decompressed (i.e. the maskout blocks are not
 * counted). If an error occurs, e.g. the compressed data is corrupted,
 * @p destsize is not large enough or context is not meant for decompression,
 * then 0 (zero) or a negative value will be returned instead.
 */
BLOSC_EXPORT int blosc2_decompress_ctx(blosc2_context* context, const void* src,
                                       int32_t srcsize, void* dest, int32_t destsize);

/**
 * @brief Context interface counterpart for #blosc_getitem.
 *
 * It uses similar parameters than the blosc_getitem() function plus a
 * @p context parameter and @srcsize compressed buffer length parameter.
 *
 * @return The number of bytes copied to @p dest or a negative value if
 * some error happens.
 */
BLOSC_EXPORT int blosc2_getitem_ctx(blosc2_context* context, const void* src,
                                    int32_t srcsize, int start, int nitems, void* dest);


/*********************************************************************
  Super-chunk related structures and functions.
*********************************************************************/

#define BLOSC2_MAX_METALAYERS 16
#define BLOSC2_METALAYER_NAME_MAXLEN 31

/**
 * @brief This struct is meant for holding storage parameters for a
 * for a blosc2 container, allowing to specify, for example, how to interpret
 * the contents included in the schunk.
 */
typedef struct {
    bool sparse;  //!< Whether the chunks are sparse or sequential (frame)
    char* path;   //!< The path for storage; if NULL, that means in-memory
} blosc2_storage;

/**
 * @brief Default struct for #blosc2_storage meant for user initialization.
 */
static const blosc2_storage BLOSC2_STORAGE_DEFAULTS = {true, NULL};

typedef struct {
  char* fname;             //!< The name of the file; if NULL, this is in-memory
  uint8_t* sdata;          //!< The in-memory serialized data
  uint8_t* coffsets;       //!< Pointers to the (compressed, on-disk) chunk offsets
  int64_t len;             //!< The current length of the frame in (compressed) bytes
  int64_t maxlen;          //!< The maximum length of the frame; if 0, there is no maximum
  uint32_t trailer_len;    //!< The current length of the trailer in (compressed) bytes
} blosc2_frame;

/**
 * @brief This struct is meant to store metadata information inside
 * a #blosc2_schunk, allowing to specify, for example, how to interpret
 * the contents included in the schunk.
 */
typedef struct blosc2_metalayer {
  char* name;          //!< The metalayer identifier for Blosc client (e.g. Caterva).
  uint8_t* content;    //!< The serialized (msgpack preferably) content of the metalayer.
  int32_t content_len; //!< The length in bytes of the content.
} blosc2_metalayer;

/**
 * @brief This struct is the standard container for Blosc 2 compressed data.
 *
 * This is essentially a container for Blosc 1 chunks of compressed data,
 * and it allows to overcome the 32-bit limitation in Blosc 1. Optionally,
 * a #blosc2_frame can be attached so as to store the compressed chunks contiguously.
 */
typedef struct blosc2_schunk {
  uint8_t version;
  uint8_t compcode;
  //!< The default compressor. Each chunk can override this.
  uint8_t clevel;
  //!< The compression level and other compress params.
  int32_t typesize;
  //!< The type size.
  int32_t blocksize;
  //!< The requested size of the compressed blocks (0; meaning automatic).
  int32_t chunksize;
  //!< Size of each chunk. 0 if not a fixed chunksize.
  uint8_t filters[BLOSC2_MAX_FILTERS];
  //!< The (sequence of) filters.  8-bit per filter.
  uint8_t filters_meta[BLOSC2_MAX_FILTERS];
  //!< Metadata for filters. 8-bit per meta-slot.
  int32_t nchunks;
  //!< Number of chunks in super-chunk.
  int64_t nbytes;
  //!< The data size + metadata size + header size (uncompressed).
  int64_t cbytes;
  //!< The data size + metadata size + header size (compressed).
  uint8_t** data;
  //!< Pointer to chunk data pointers buffer.
  size_t data_len;
  //!< Length of the chunk data pointers buffer.
  blosc2_storage* storage;
  //!< Pointer to storage info.
  blosc2_frame* frame;
  //!< Pointer to frame used as store for chunks.
  //!<uint8_t* ctx;
  //!< Context for the thread holder. NULL if not acquired.
  blosc2_context* cctx;
  //!< Context for compression
  blosc2_context* dctx;
  //!< Context for decompression.
  struct blosc2_metalayer *metalayers[BLOSC2_MAX_METALAYERS];
  //!< The array of metalayers.
  int16_t nmetalayers;
  //!< The number of metalayers in the frame
  uint8_t* usermeta;
  //<! The user-defined metadata.
  int32_t usermeta_len;
  //<! The (compressed) length of the user-defined metadata.
} blosc2_schunk;

/**
 * @brief Create a new super-chunk.
 *
 * @param cparams The compression parameters.
 * @param dparams The decompression parameters.
 * @param frame The frame to be used. NULL if not needed.
 *
 * @return The new super-chunk.
 */
BLOSC_EXPORT blosc2_schunk *
blosc2_new_schunk(blosc2_cparams cparams, blosc2_dparams dparams, blosc2_frame *frame);

/**
 * @brief Create a non-initialized super-chunk.
 *
 * @param cparams The compression parameters.
 * @param dparams The decompression parameters.
 * @param nchunks The number of non-initialized chunks in the super-chunk.
 * @param frame If not NULL, the super-chunk will be backed by this frame object.
 *
 * @return The new super-chunk.
 */
BLOSC_EXPORT blosc2_schunk *
blosc2_empty_schunk(blosc2_cparams cparams, blosc2_dparams dparams, int nchunks,
                    blosc2_storage *storage);

/**
 * @brief Release resources from a super-chunk.
 *
 * @param schunk The super-chunk to be freed.
 *
 * @return 0 if succeeds.
 */
BLOSC_EXPORT int blosc2_free_schunk(blosc2_schunk *schunk);

/**
 * @brief Append an existing @p chunk o a super-chunk.
 *
 * @param schunk The super-chunk where the chunk will be appended.
 * @param chunk The @p chunk to append.  An internal copy is made, so @p chunk can be reused or
 * freed if desired.
 * @param copy Whether the chunk should be copied internally or can be used as-is.
 *
 * @return The number of chunks in super-chunk. If some problem is
 * detected, this number will be negative.
 */
BLOSC_EXPORT int blosc2_schunk_append_chunk(blosc2_schunk *schunk, uint8_t *chunk, bool copy);


/**
  * @brief Update a chunk at a specific position in a super-chunk.
  *
  * @param schunk The super-chunk where the chunk will be updated.
  * @param nchunk The position where the chunk will be updated.
  * @param chunk The new @p chunk. If an internal copy is made, the @p chunk can be reused or
  * freed if desired.
  * @param copy Whether the chunk should be copied internally or can be used as-is.
  *
  * @return The number of chunks in super-chunk. If some problem is
  * detected, this number will be negative.
  */
BLOSC_EXPORT int blosc2_schunk_update_chunk(blosc2_schunk *schunk, int nchunk, uint8_t *chunk, bool copy);

/**
 * @brief Insert a chunk at a specific position in a super-chunk.
 *
 * @param schunk The super-chunk where the chunk will be appended.
 * @param nchunk The position where the chunk will be inserted.
 * @param chunk The @p chunk to insert. If an internal copy is made, the @p chunk can be reused or
 * freed if desired.
 * @param copy Whether the chunk should be copied internally or can be used as-is.
 *
 * @return The number of chunks in super-chunk. If some problem is
 * detected, this number will be negative.
 */
BLOSC_EXPORT int blosc2_schunk_insert_chunk(blosc2_schunk *schunk, int nchunk, uint8_t *chunk, bool copy);

/**
 * @brief Append a @p src data buffer to a super-chunk.
 *
 * @param schunk The super-chunk where data will be appended.
 * @param src The buffer of data to compress.
 * @param nbytes The size of the @p src buffer.
 *
 * @return The number of chunks in super-chunk. If some problem is
 * detected, this number will be negative.
 */
BLOSC_EXPORT int blosc2_schunk_append_buffer(blosc2_schunk *schunk, void *src, int32_t nbytes);

/**
 * @brief Decompress and return the @p nchunk chunk of a super-chunk.
 *
 * If the chunk is uncompressed successfully, it is put in the @p *dest
 * pointer.
 *
 * @param schunk The super-chunk from where the chunk will be decompressed.
 * @param nchunk The chunk to be decompressed (0 indexed).
 * @param dest The buffer where the decompressed data will be put.
 * @param nbytes The size of the area pointed by @p *dest.
 *
 * @warning You must make sure that you have space enough to store the
 * uncompressed data.
 *
 * @return The size of the decompressed chunk or 0 if it is non-initialized. If some problem is
 * detected, a negative code is returned instead.
 */
BLOSC_EXPORT int blosc2_schunk_decompress_chunk(blosc2_schunk *schunk, int nchunk, void *dest, int32_t nbytes);

/**
 * @brief Return a compressed chunk that is part of a super-chunk in the @p chunk parameter.
 *
 * @param schunk The super-chunk from where to extract a chunk.
 * @param nchunk The chunk to be extracted (0 indexed).
 * @param chunk The pointer to the chunk of compressed data.
 * @param needs_free The pointer to a boolean indicating if it is the user's
 * responsibility to free the chunk returned or not.
 *
 * @warning If the super-chunk is backed by a frame that is disk-based, a buffer is allocated for the
 * (compressed) chunk, and hence a free is needed. You can check if the chunk requires a free
 * with the @p needs_free parameter.
 * If the chunk does not need a free, it means that a pointer to the location in the super-chunk
 * (or the backing in-memory frame) is returned in the @p chunk parameter.
 *
 * @return The size of the (compressed) chunk or 0 if it is non-initialized. If some problem is
 * detected, a negative code is returned instead.
 */
BLOSC_EXPORT int blosc2_schunk_get_chunk(blosc2_schunk *schunk, int nchunk, uint8_t **chunk,
                                         bool *needs_free);

/**
 * @brief Return the @p cparams associated to a super-chunk.
 *
 * @param schunk The super-chunk from where to extract the compression parameters.
 * @param cparams The pointer where the compression params will be returned.
 *
 * @warning A new struct is allocated, and the user should free it after use.
 *
 * @return 0 if succeeds. Else a negative code is returned.
 */
BLOSC_EXPORT int blosc2_schunk_get_cparams(blosc2_schunk *schunk, blosc2_cparams **cparams);

/**
 * @brief Return the @p dparams struct associated to a super-chunk.
 *
 * @param schunk The super-chunk from where to extract the decompression parameters.
 * @param dparams The pointer where the decompression params will be returned.
 *
 * @warning A new struct is allocated, and the user should free it after use.
 *
 * @return 0 if succeeds. Else a negative code is returned.
 */
BLOSC_EXPORT int blosc2_schunk_get_dparams(blosc2_schunk *schunk, blosc2_dparams **dparams);

/**
 * @brief Reorder the chunk offsets of an existing super-chunk.
 *
 * @param schunk The super-chunk whose chunk offsets are to be reordered.
 * @param offsets_order The new order of the chunk offsets.
 *
 * @return 0 if suceeds. Else a negative code is returned.
 */
BLOSC_EXPORT int blosc2_schunk_reorder_offsets(blosc2_schunk *schunk, int *offsets_order);


/*********************************************************************
  Functions related with metalayers.
*********************************************************************/

/**
 * @brief Find whether the schunk has a metalayer or not.
 *
 * @param schunk The super-chunk from which the metalayer will be checked.
 * @param name The name of the metalayer to be checked.
 *
 * @return If successful, return the index of the metalayer. Else, return a negative value.
 */
BLOSC_EXPORT int blosc2_has_metalayer(blosc2_schunk *schunk, const char *name);

/**
 * @brief Add content into a new metalayer.
 *
 * @param schunk The super-chunk to which the metalayer should be added.
 * @param name The name of the metalayer.
 * @param content The content of the metalayer.
 * @param content_len The length of the content.
 *
 * @return If successful, the index of the new metalayer. Else, return a negative value.
 */
BLOSC_EXPORT int blosc2_add_metalayer(blosc2_schunk *schunk, const char *name, uint8_t *content,
                                      uint32_t content_len);

/**
 * @brief Update the content of an existing metalayer.
 *
 * @param schunk The frame containing the metalayer.
 * @param name The name of the metalayer to be updated.
 * @param content The new content of the metalayer.
 * @param content_len The length of the content.
 *
 * @note Contrarily to #blosc2_add_metalayer the updates to metalayers
 * are automatically serialized into a possible attached frame.
 *
 * @return If successful, the index of the metalayer. Else, return a negative value.
 */
BLOSC_EXPORT int blosc2_update_metalayer(blosc2_schunk *schunk, const char *name, uint8_t *content,
                                         uint32_t content_len);

/**
 * @brief Get the content out of a metalayer.
 *
 * @param schunk The frame containing the metalayer.
 * @param name The name of the metalayer.
 * @param content The pointer where the content will be put.
 * @param content_len The length of the content.
 *
 * @warning The @p **content receives a malloc'ed copy of the content.
 * The user is responsible of freeing it.
 *
 * @return If successful, the index of the new metalayer. Else, return a negative value.
 */
BLOSC_EXPORT int blosc2_get_metalayer(blosc2_schunk *schunk, const char *name, uint8_t **content,
                                      uint32_t *content_len);


/*********************************************************************
  Usermeta functions.
*********************************************************************/

/**
 * @brief Update content into a usermeta chunk.
 *
 * If the @p schunk has an attached frame, the later will be updated accordingly too.
 *
 * @param schunk The super-chunk to which one should add the usermeta chunk.
 * @param content The content of the usermeta chunk.
 * @param content_len The length of the content.
 * @param cparams The parameters for compressing the usermeta chunk.
 *
 * @note The previous content, if any, will be overwritten by the new content.
 * The user is responsible to keep the new content in sync with any previous content.
 *
 * @return If successful, return the number of compressed bytes that takes the content.
 * Else, a negative value.
 */
BLOSC_EXPORT int blosc2_update_usermeta(blosc2_schunk *schunk, uint8_t *content,
                                        int32_t content_len, blosc2_cparams cparams);

/* @brief Retrieve the usermeta chunk in a decompressed form.
 *
 * @param schunk The super-chunk to which add the usermeta chunk.
 * @param content The content of the usermeta chunk (output).
 *
 * @note The user is responsible to free the @p content buffer.
 *
 * @return If successful, return the size of the (decompressed) chunk.
 * Else, a negative value.
 */
BLOSC_EXPORT int blosc2_get_usermeta(blosc2_schunk* schunk, uint8_t** content);


/*********************************************************************
  Frame related structures and functions.
*********************************************************************/

/**
 * @brief Create a new frame.
 *
 * @param fname The filename of the frame.  If not persistent, pass NULL.
 *
 * @return The new frame.
 */
BLOSC_EXPORT blosc2_frame* blosc2_new_frame(const char* fname);

/**
 * @brief Create a frame from a super-chunk.
 *
 * @param schunk The super-chunk from where the frame will be created.
 * @param frame The pointer for the frame that will be populated.
 *
 * If frame->fname is NULL, a frame is created in memory; else it is created
 * on disk.
 *
 * @return The size in bytes of the frame. If an error occurs it returns a negative value.
 */
BLOSC_EXPORT int64_t blosc2_schunk_to_frame(blosc2_schunk* schunk, blosc2_frame* frame);

/**
 * @brief Free all memory from a frame.
 *
 * @param frame The frame to be freed.
 *
 * @return 0 if succeeds.
 */
BLOSC_EXPORT int blosc2_free_frame(blosc2_frame *frame);

/**
 * @brief Write an in-memory frame out to a file.
 *
 * The frame used must be an in-memory frame, i.e. frame->fname == NULL.
 *
 * @param frame The frame to be written into a file.
 * @param fname The name of the file.
 *
 * @return The size of the frame.  If negative, an error happened (including
 * that the original frame is not in-memory).
 */
BLOSC_EXPORT int64_t blosc2_frame_to_file(blosc2_frame *frame, const char *fname);

/**
 * @brief Initialize a frame out of a file.
 *
 * @param fname The file name.
 *
 * @return The frame created from the file.
 */
BLOSC_EXPORT blosc2_frame* blosc2_frame_from_file(const char *fname);

/**
 * @brief Initialize a frame out of a serialized frame.
 *
 * @param buffer The buffer for the serialized frame.
 * @param len The length of buffer for the serialized frame.
 * @param copy Whether the serialized frame should be copied internally or not.
 *
 * @return The frame created from the serialized frame.
 */
BLOSC_EXPORT blosc2_frame* blosc2_frame_from_sframe(uint8_t *sframe, int64_t len, bool copy);

/**
 * @brief Create a super-chunk from a frame.
 *
 * @param frame The frame from which the super-chunk will be created.
 * @param copy If true, a new, sparse in-memory super-chunk is created.
 * Else, a frame-backed one is created (i.e. no copies are made)
 *
 * @return The super-chunk corresponding to the frame.
 */
BLOSC_EXPORT blosc2_schunk* blosc2_schunk_from_frame(blosc2_frame* frame, bool copy);


/*********************************************************************
  Time measurement utilities.
*********************************************************************/

#if defined(_WIN32)
/* For QueryPerformanceCounter(), etc. */
  #include <windows.h>
#elif defined(__MACH__)
#include <mach/clock.h>
#include <mach/mach.h>
#include <time.h>
#elif defined(__unix__)
#if defined(__linux__)
    #include <time.h>
  #else
    #include <sys/time.h>
  #endif
#else
  #error Unable to detect platform.
#endif

/* The type of timestamp used on this system. */
#if defined(_WIN32)
#define blosc_timestamp_t LARGE_INTEGER
#else
#define blosc_timestamp_t struct timespec
#endif

/*
 * Set a timestamp.
 */
BLOSC_EXPORT void blosc_set_timestamp(blosc_timestamp_t* timestamp);

/*
 * Return the nanoseconds between 2 timestamps.
 */
BLOSC_EXPORT double blosc_elapsed_nsecs(blosc_timestamp_t start_time,
                                        blosc_timestamp_t end_time);

/*
 * Return the seconds between 2 timestamps.
 */
BLOSC_EXPORT double blosc_elapsed_secs(blosc_timestamp_t start_time,
                                       blosc_timestamp_t end_time);


/*********************************************************************
  Low-level functions follows.  Use them only if you are an expert!
*********************************************************************/

/**
 * @brief Get the internal blocksize to be used during compression. 0 means
 * that an automatic blocksize is computed internally.
 *
 * @return The size in bytes of the internal block size.
 */
BLOSC_EXPORT int blosc_get_blocksize(void);

/**
 * @brief Force the use of a specific blocksize. If 0, an automatic
 * blocksize will be used (the default).
 *
 * @warning The blocksize is a critical parameter with important
 * restrictions in the allowed values, so use this with care.
 */
BLOSC_EXPORT void blosc_set_blocksize(size_t blocksize);

/**
 * @brief Set pointer to super-chunk. If NULL, no super-chunk will be
 * available (the default).
 */
BLOSC_EXPORT void blosc_set_schunk(blosc2_schunk* schunk);


#ifdef __cplusplus
}
#endif


#endif  /* BLOSC_H */
