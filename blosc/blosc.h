/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/
#ifndef BLOSC_H
#define BLOSC_H

#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include "blosc-export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Version numbers */
#define BLOSC_VERSION_MAJOR    2    /* for major interface/format changes  */
#define BLOSC_VERSION_MINOR    0    /* for minor interface/format changes  */
#define BLOSC_VERSION_RELEASE  0    /* for tweaks, bug-fixes, or development */

#define BLOSC_VERSION_STRING   "2.0.0a4.dev"  /* string version.  Sync with above! */
#define BLOSC_VERSION_REVISION "$Rev$"   /* revision version */
#define BLOSC_VERSION_DATE     "$Date:: 2016-07-24 #$"    /* date version */

#define BLOSCLZ_VERSION_STRING "1.0.6"   /* the internal compressor version */

/* The *_FORMAT symbols below should be just 1-byte long */

#define BLOSC_VERSION_FORMAT    3
/* Blosc format version, starting at 1
   1 -> Basically for Blosc pre-1.0
   2 -> Blosc 1.x series
   3 -> Blosc 2.x series */

/* Minimum header length */
#define BLOSC_MIN_HEADER_LENGTH 16

/* The maximum overhead during compression in bytes.  This equals to
   BLOSC_MIN_HEADER_LENGTH now, but can be higher in future
   implementations */
#define BLOSC_MAX_OVERHEAD BLOSC_MIN_HEADER_LENGTH

/* Maximum source buffer size to be compressed */
#define BLOSC_MAX_BUFFERSIZE (INT_MAX - BLOSC_MAX_OVERHEAD)

/* Maximum typesize before considering source buffer as a stream of bytes */
#define BLOSC_MAX_TYPESIZE 255         /* Cannot be larger than 255 */

/* Codes for filters (see blosc_compress) */
#define BLOSC_NOSHUFFLE   0  /* no shuffle (for compatibility with Blosc1) */
#define BLOSC_NOFILTER    0  /* no filter */
#define BLOSC_SHUFFLE     1  /* byte-wise shuffle */
#define BLOSC_BITSHUFFLE  2  /* bit-wise shuffle */
#define BLOSC_DELTA       3  /* delta filter */
#define BLOSC_TRUNC_PREC  4  /* truncate precision filter */
#define BLOSC_LAST_FILTER 5  /* sentinel */

/* Maximum number of simultaneous filters */
#define BLOSC_MAX_FILTERS 8

/* Codes for internal flags (see blosc_cbuffer_metainfo) */
#define BLOSC_DOSHUFFLE     0x1  /* byte-wise shuffle */
#define BLOSC_MEMCPYED      0x2  /* plain copy */
#define BLOSC_DOBITSHUFFLE  0x4  /* bit-wise shuffle */
#define BLOSC_FILTER_SCHUNK 0x8  /* filter defined in super-chunk */

/* Codes for the different compressors shipped with Blosc */
#define BLOSC_BLOSCLZ        0
#define BLOSC_LZ4            1
#define BLOSC_LZ4HC          2
#define BLOSC_SNAPPY         3
#define BLOSC_ZLIB           4
#define BLOSC_ZSTD           5

/* Names for the different compressors shipped with Blosc */
#define BLOSC_BLOSCLZ_COMPNAME   "blosclz"
#define BLOSC_LZ4_COMPNAME       "lz4"
#define BLOSC_LZ4HC_COMPNAME     "lz4hc"
#define BLOSC_SNAPPY_COMPNAME    "snappy"
#define BLOSC_ZLIB_COMPNAME      "zlib"
#define BLOSC_ZSTD_COMPNAME      "zstd"

/* Codes for compression libraries shipped with Blosc (code must be < 8) */
#define BLOSC_BLOSCLZ_LIB    0
#define BLOSC_LZ4_LIB        1
#define BLOSC_SNAPPY_LIB     2
#define BLOSC_ZLIB_LIB       3
#define BLOSC_ZSTD_LIB       4
#define BLOSC_SCHUNK_LIB     7   /* compressor library in super-chunk header */

/* Names for the different compression libraries shipped with Blosc */
#define BLOSC_BLOSCLZ_LIBNAME   "BloscLZ"
#define BLOSC_LZ4_LIBNAME       "LZ4"
#define BLOSC_SNAPPY_LIBNAME    "Snappy"
#if defined(HAVE_MINIZ)
  #define BLOSC_ZLIB_LIBNAME    "Zlib (via miniz)"
#else
  #define BLOSC_ZLIB_LIBNAME    "Zlib"
#endif	/* HAVE_MINIZ */
#define BLOSC_ZSTD_LIBNAME      "Zstd"

/* The codes for compressor formats shipped with Blosc */
#define BLOSC_BLOSCLZ_FORMAT  BLOSC_BLOSCLZ_LIB
#define BLOSC_LZ4_FORMAT      BLOSC_LZ4_LIB
/* LZ4HC and LZ4 share the same format */
#define BLOSC_LZ4HC_FORMAT    BLOSC_LZ4_LIB
#define BLOSC_SNAPPY_FORMAT   BLOSC_SNAPPY_LIB
#define BLOSC_ZLIB_FORMAT     BLOSC_ZLIB_LIB
#define BLOSC_ZSTD_FORMAT     BLOSC_ZSTD_LIB


/* The version formats for compressors shipped with Blosc */
/* All versions here starts at 1 */
#define BLOSC_BLOSCLZ_VERSION_FORMAT  1
#define BLOSC_LZ4_VERSION_FORMAT      1
#define BLOSC_LZ4HC_VERSION_FORMAT    1  /* LZ4HC and LZ4 share the same format */
#define BLOSC_SNAPPY_VERSION_FORMAT   1
#define BLOSC_ZLIB_VERSION_FORMAT     1
#define BLOSC_ZSTD_VERSION_FORMAT     1

/**
  Initialize the Blosc library environment.

  You must call this previous to any other Blosc call, unless you want
  Blosc to be used simultaneously in a multi-threaded environment, in
  which case you can use the
  blosc2_compress_ctx()/blosc2_decompress_ctx() pair (see below).
*/
BLOSC_EXPORT void blosc_init(void);


/**
  Destroy the Blosc library environment.

  You must call this after to you are done with all the Blosc calls,
  unless you have not used blosc_init() before (see blosc_init()
  above).
*/
BLOSC_EXPORT void blosc_destroy(void);


/**
  Compress a block of data in the `src` buffer and returns the size of
  compressed block.  The size of `src` buffer is specified by
  `nbytes`.  There is not a minimum for `src` buffer size (`nbytes`).

  `clevel` is the desired compression level and must be a number
  between 0 (no compression) and 9 (maximum compression).

  `doshuffle` specifies whether the shuffle compression preconditioner
  should be applied or not.  BLOSC_NOFILTER means not applying filters,
  BLOSC_SHUFFLE means applying shuffle at a byte level and
  BLOSC_BITSHUFFLE at a bit level (slower but may achieve better
  entropy alignment).

  `typesize` is the number of bytes for the atomic type in binary
  `src` buffer.  This is mainly useful for the shuffle preconditioner.
  For implementation reasons, only a 1 < typesize < 256 will allow the
  shuffle filter to work.  When typesize is not in this range, shuffle
  will be silently disabled.

  The `dest` buffer must have at least the size of `destsize`.  Blosc
  guarantees that if you set `destsize` to, at least,
  (`nbytes`+BLOSC_MAX_OVERHEAD), the compression will always succeed.
  The `src` buffer and the `dest` buffer can not overlap.

  Compression is memory safe and guaranteed not to write the `dest`
  buffer more than what is specified in `destsize`.

  If `src` buffer cannot be compressed into `destsize`, the return
  value is zero and you should discard the contents of the `dest`
  buffer.

  A negative return value means that an internal error happened.  This
  should never happen.  If you see this, please report it back
  together with the buffer data causing this and compression settings.

  Environment variables
  ---------------------

  blosc_compress() honors different environment variables to control
  internal parameters without the need of doing that programatically.
  Here are the ones supported:

  BLOSC_CLEVEL=(INTEGER): This will overwrite the `clevel` parameter
  before the compression process starts.

  BLOSC_SHUFFLE=[NOSHUFFLE | SHUFFLE | BITSHUFFLE]: This will
  overwrite the `doshuffle` parameter before the compression process
  starts.

  BLOSC_TYPESIZE=(INTEGER): This will overwrite the `typesize`
  parameter before the compression process starts.
  BLOSC_COMPRESSOR=[BLOSCLZ | LZ4 | LZ4HC | SNAPPY | ZLIB]: This will
  call blosc_set_compressor(BLOSC_COMPRESSOR) before the compression
  process starts.


  BLOSC_NTHREADS=(INTEGER): This will call
  blosc_set_nthreads(BLOSC_NTHREADS) before the compression process
  starts.

  BLOSC_BLOCKSIZE=(INTEGER): This will call
  blosc_set_blocksize(BLOSC_BLOCKSIZE) before the compression process
  starts.  *NOTE:* The blocksize is a critical parameter with
  important restrictions in the allowed values, so use this with care.

  BLOSC_NOLOCK=(ANY VALUE): This will call blosc2_compress_ctx() under
  the hood, with the `compressor`, `blocksize` and
  `numinternalthreads` parameters set to the same as the last calls to
  blosc_set_compressor(), blosc_set_blocksize() and
  blosc_set_nthreads().  BLOSC_CLEVEL, BLOSC_SHUFFLE, BLOSC_TYPESIZE
  environment vars will also be honored.
*/
BLOSC_EXPORT int blosc_compress(int clevel, int doshuffle, size_t typesize,
                                size_t nbytes, const void* src, void* dest,
                                size_t destsize);


/**
  Decompress a block of compressed data in `src`, put the result in
  `dest` and returns the size of the decompressed block.

  The `src` buffer and the `dest` buffer can not overlap.

  Decompression is memory safe and guaranteed not to write the `dest`
  buffer more than what is specified in `destsize`.

  If an error occurs, e.g. the compressed data is corrupted or the
  output buffer is not large enough, then 0 (zero) or a negative value
  will be returned instead.

  Environment variables
  ---------------------

  blosc_decompress() honors different environment variables to control
  internal parameters without the need of doing that programatically.
  Here are the ones supported:

  BLOSC_NTHREADS=(INTEGER): This will call
  blosc_set_nthreads(BLOSC_NTHREADS) before the proper decompression
  process starts.

  BLOSC_NOLOCK=(ANY VALUE): This will call blosc2_decompress_ctx()
  under the hood, with the `numinternalthreads` parameter set to the
  same value as the last call to blosc_set_nthreads().
*/
BLOSC_EXPORT int blosc_decompress(const void* src, void* dest, size_t destsize);


/**
  Get `nitems` (of typesize size) in `src` buffer starting in `start`.
  The items are returned in `dest` buffer, which has to have enough
  space for storing all items.

  Returns the number of bytes copied to `dest` or a negative value if
  some error happens.
*/
BLOSC_EXPORT int blosc_getitem(const void* src, int start, int nitems, void* dest);


/**
  Returns the current number of threads that are used for
  compression/decompression.
  */
BLOSC_EXPORT int blosc_get_nthreads(void);


/**
  Initialize a pool of threads for compression/decompression.  If
  `nthreads` is 1, then the serial version is chosen and a possible
  previous existing pool is ended.  If this is not called, `nthreads`
  is set to 1 internally.

  Returns the previous number of threads.
  */
BLOSC_EXPORT int blosc_set_nthreads(int nthreads);


/**
  Returns the current compressor that is used for compression.
  */
BLOSC_EXPORT char* blosc_get_compressor(void);


/**
  Select the compressor to be used.  The supported ones are "blosclz",
  "lz4", "lz4hc", "snappy", "zlib" and "ztsd".  If this function is not
  called, then "blosclz" will be used.

  In case the compressor is not recognized, or there is not support
  for it in this build, it returns a -1.  Else it returns the code for
  the compressor (>=0).
*/
BLOSC_EXPORT int blosc_set_compressor(const char* compname);


/**
  Get the `compname` associated with the `compcode`.

  If the compressor code is not recognized, or there is not support
  for it in this build, -1 is returned.  Else, the compressor code is
  returned.
*/
BLOSC_EXPORT int blosc_compcode_to_compname(int compcode, char** compname);


/**
  Return the compressor code associated with the compressor name.

  If the compressor name is not recognized, or there is not support
  for it in this build, -1 is returned instead.
*/
BLOSC_EXPORT int blosc_compname_to_compcode(const char* compname);


/**
  Get a list of compressors supported in the current build.  The
  returned value is a string with a concatenation of "blosclz", "lz4",
  "lz4hc", "snappy", "zlib" or "zstd "separated by commas, depending
  on which ones are present in the build.

  This function does not leak, so you should not free() the returned
  list.

  This function should always succeed.
*/
BLOSC_EXPORT char* blosc_list_compressors(void);


/**
  Return the version of blosc in string format.

  Useful for dynamic libraries.
*/
BLOSC_EXPORT char* blosc_get_version_string(void);


/**
  Get info from compression libraries included in the current build.
  In `compname` you pass the compressor name that you want info from.
  In `complib` and `version` you get the compression library name and
  version (if available) as output.

  In `complib` and `version` you get a pointer to the compressor
  library name and the version in string format respectively.  After
  using the name and version, you should free() them so as to avoid
  leaks.

  If the compressor is supported, it returns the code for the library
  (>=0).  If it is not supported, this function returns -1.
*/
BLOSC_EXPORT int blosc_get_complib_info(char* compname, char** complib, char** version);


/**
  Free possible memory temporaries and thread resources.  Use this
  when you are not going to use Blosc for a long while.  In case of
  problems releasing the resources, it returns a negative number, else
  it returns 0.
*/
BLOSC_EXPORT int blosc_free_resources(void);


/**
  Return information about a compressed buffer, namely the number of
  uncompressed bytes (`nbytes`) and compressed (`cbytes`).  It also
  returns the `blocksize` (which is used internally for doing the
  compression by blocks).

  You only need to pass the first BLOSC_MIN_HEADER_LENGTH bytes of a
  compressed buffer for this call to work.

  This function should always succeed.
*/
BLOSC_EXPORT void blosc_cbuffer_sizes(const void* cbuffer, size_t* nbytes,
                                      size_t* cbytes, size_t* blocksize);


/**
  Return information about a compressed buffer, namely the type size
  (`typesize`), as well as some internal `flags`.

  The `flags` is a set of bits, where the currently used ones are:
    * bit 0: whether the shuffle filter has been applied or not
    * bit 1: whether the internal buffer is a pure memcpy or not

  You can use the `BLOSC_DOSHUFFLE`, `BLOSC_DOBITSHUFFLE` and
  `BLOSC_MEMCPYED` symbols for extracting the interesting bits
  (e.g. ``flags & BLOSC_DOSHUFFLE`` says whether the buffer is
  byte-shuffled or not).

  This function should always succeed.
*/
BLOSC_EXPORT void blosc_cbuffer_metainfo(const void* cbuffer, size_t* typesize,
                                         int* flags);


/**
  Return information about a compressed buffer, namely the internal
  Blosc format version (`version`) and the format for the internal
  Lempel-Ziv compressor used (`versionlz`).

  This function should always succeed.
*/
BLOSC_EXPORT void blosc_cbuffer_versions(const void* cbuffer, int* version,
                                         int* versionlz);


/**
  Return the compressor library/format used in a compressed buffer.

  This function should always succeed.
*/
BLOSC_EXPORT char* blosc_cbuffer_complib(const void* cbuffer);


/*********************************************************************

  Super-chunk related structures and functions.

*********************************************************************/

typedef struct {
  uint8_t version;
  uint8_t flags1;
  uint8_t flags2;
  uint8_t flags3;
  uint16_t compressor;
  /* The default compressor.  Each chunk can override this. */
  uint16_t clevel;
  /* The compression level and other compress params */
  uint32_t chunksize;
  /* Size of each chunk.  0 if not a fixed chunksize. */
  uint64_t filters;
  /* The (sequence of) filters.  8-bit per filter. */
  uint64_t filters_meta;
  /* Metadata for filters */
  int64_t nchunks;
  /* Number of chunks in super-chunk */
  int64_t nbytes;
  /* data size + metadata size + header size (uncompressed) */
  int64_t cbytes;
  /* data size + metadata size + header size (compressed) */
  uint8_t* filters_chunk;
  /* Pointer to chunk hosting filter-related data */
  uint8_t* codec_chunk;
  /* Pointer to chunk hosting codec-related data */
  uint8_t* metadata_chunk;
  /* Pointer to schunk metadata */
  uint8_t* userdata_chunk;
  /* Pointer to user-defined data */
  uint8_t** data;
  /* Pointer to chunk data pointers */
  uint8_t* ctx;
  /* Context for the thread holder.  NULL if not acquired. */
  uint8_t* reserved;
  /* Reserved for the future. */
} blosc2_sheader;


typedef struct {
  uint8_t compressor;
  /* the default compressor */
  uint8_t clevel;
  /* the compression level and other compress params */
  uint8_t filters[BLOSC_MAX_FILTERS];
  /* the (sequence of) filters */
  uint16_t filters_meta;   /* metadata for filters */
} blosc2_sparams;

/* Default struct for schunk params meant for user initialization */
static const blosc2_sparams BLOSC_SPARAMS_DEFAULTS = \
  { BLOSC_ZSTD, 5, {BLOSC_SHUFFLE, 0, 0, 0, 0, 0, 0, 0}, 0 };

/* Create a new super-chunk. */
BLOSC_EXPORT blosc2_sheader* blosc2_new_schunk(blosc2_sparams* sparams);

/* Set a delta reference for the super-chunk */
BLOSC_EXPORT int blosc2_set_delta_ref(blosc2_sheader* sheader,
    size_t typesize, size_t nbytes, void* ref);

/* Free all memory from a super-chunk. */
BLOSC_EXPORT int blosc2_destroy_schunk(blosc2_sheader* sheader);

/* Append a `src` data buffer to a super-chunk.

 `typesize` is the number of bytes of the underlying data type and
 `nbytes` is the size of the `src` buffer.

 This returns the number of chunk in super-chunk.  If some problem is
 detected, this number will be negative.
 */
BLOSC_EXPORT size_t blosc2_append_buffer(blosc2_sheader* sheader,
     size_t typesize, size_t nbytes, void* src);

BLOSC_EXPORT void* blosc2_packed_append_buffer(void* packed, size_t typesize,
                                               size_t nbytes, void* src);

/* Decompress and return the `nchunk` chunk of a super-chunk.

 If the chunk is uncompressed successfully, it is put in the `*dest`
 pointer.  `nbytes` is the size of the area pointed by `*dest`.  You
 must make sure that you have space enough to store the uncompressed
 data.

 The size of the decompressed chunk is returned.  If some problem is
 detected, a negative code is returned instead.
 */
BLOSC_EXPORT int blosc2_decompress_chunk(blosc2_sheader* sheader,
     int64_t nchunk, void* dest, int nbytes);

BLOSC_EXPORT int blosc2_packed_decompress_chunk(void* packed, int nchunk,
      void** dest);

/* Pack a super-chunk by using the header. */
BLOSC_EXPORT void* blosc2_pack_schunk(blosc2_sheader* sheader);

/* Unpack a packed super-chunk */
BLOSC_EXPORT blosc2_sheader* blosc2_unpack_schunk(void* packed);


/*********************************************************************

  Structures and functions related with contexts.

*********************************************************************/

typedef struct blosc_context_s blosc_context;   /* uncomplete type */

/**
  The parameters for creating a context for compression purposes.

  In parenthesis it is shown the default value used internally when a 0
  (zero) in the fields of the struct is passed to a function.
*/
typedef struct {
  uint8_t typesize;
  /* the type size (8) */
  uint8_t compcode;
  /* the compressor code (BLOSC_BLOSCLZ) */
  uint8_t clevel;
  /* the compression level (5) */
  uint8_t filtercode;
  /* the filter code (BLOSC_SHUFFLE) */
  uint8_t nthreads;
  /* the number of threads to use internally (1) */
  int32_t blocksize;
  /* the requested size of the compressed blocks (0; meaning automatic) */
  blosc2_sheader* schunk;
  /* the associated schunk, if any (NULL) */
} blosc2_context_cparams;

/* Default struct for compression params meant for user initialization */
static const blosc2_context_cparams BLOSC_CPARAMS_DEFAULTS = \
  { 8, BLOSC_BLOSCLZ, 5, BLOSC_SHUFFLE, 1, 0, NULL };


/**
  The parameters for creating a context for decompression purposes.

  In parenthesis it is shown the default value used internally when a 0
  (zero) in the fields of the struct is passed to a function.
*/
typedef struct {
  uint8_t nthreads;
  /* the number of threads to use internally (1) */
  blosc2_sheader* schunk;
  /* the associated schunk, if any (NULL) */
} blosc2_context_dparams;

/* Default struct for compression params meant for user initialization */
static const blosc2_context_dparams BLOSC_DPARAMS_DEFAULTS = \
  { 1, NULL };

/**
  Create a context for *_ctx() compression functions.

  A pointer to the new context is returned.  NULL is returned if this fails.
*/
BLOSC_EXPORT blosc_context* blosc2_create_cctx(blosc2_context_cparams* cparams);

/**
  Create a context for *_ctx() decompression functions.

  A pointer to the new context is returned.  NULL is returned if this fails.
*/
BLOSC_EXPORT blosc_context* blosc2_create_dctx(blosc2_context_dparams* dparams);

/**
  Free the resources associated with a context.

  This function should always succeed and is valid for contexts meant for
  both compression and decompression.
*/
BLOSC_EXPORT void blosc2_free_ctx(blosc_context* context);

/**
  Context interface to blosc compression. This does not require a call
  to blosc_init() and can be called from multithreaded applications
  without the global lock being used, so allowing Blosc be executed
  simultaneously in those scenarios.

  It uses similar parameters than the blosc_compress() function plus:

  `context`: a struct with the different compression params.

  A negative return value means that an internal error happened.  It could
  happen that context is not meant for compression (which is stated in stderr).
  Otherwise, please report it back together with the buffer data causing this
  and compression settings.
*/
BLOSC_EXPORT int blosc2_compress_ctx(
  blosc_context* context, size_t nbytes, const void* src, void* dest,
  size_t destsize);


/**
  Context interface to blosc decompression. This does not require a
  call to blosc_init() and can be called from multithreaded
  applications without the global lock being used, so allowing Blosc
  be executed simultaneously in those scenarios.

  It uses similar parameters than the blosc_decompress() function plus:

  `context`: a struct with the different compression params.

  Decompression is memory safe and guaranteed not to write the `dest`
  buffer more than what is specified in `destsize`.

  If an error occurs, e.g. the compressed data is corrupted, `destsize` is not
  large enough or context is not meant for decompression, then 0 (zero) or a
  negative value will be returned instead.
*/
BLOSC_EXPORT int blosc2_decompress_ctx(blosc_context* context, const void* src,
                                       void* dest, size_t destsize);

/**
  Context interface counterpart for blosc_getitem().

  It uses similar parameters than the blosc_getitem() function plus a
  `context` parameter.

  Returns the number of bytes copied to `dest` or a negative value if
  some error happens.
*/
BLOSC_EXPORT int blosc2_getitem_ctx(blosc_context* context, const void* src,
                                    int start, int nitems, void* dest);


/*********************************************************************

  Low-level functions follows.  Use them only if you are an expert!

*********************************************************************/

/* Get the internal blocksize to be used during compression.  0 means
   that an automatic blocksize is computed internally. */
BLOSC_EXPORT int blosc_get_blocksize(void);

/**
  Force the use of a specific blocksize.  If 0, an automatic
  blocksize will be used (the default).
*/
BLOSC_EXPORT void blosc_set_blocksize(size_t blocksize);

/**
  Set pointer to super-chunk.  If NULL, no super-chunk will be
  available (the default).

  The blocksize is a critical parameter with important restrictions in
  the allowed values, so use this with care.
*/
BLOSC_EXPORT void blosc_set_schunk(blosc2_sheader* schunk);


#ifdef __cplusplus
}
#endif


#endif
