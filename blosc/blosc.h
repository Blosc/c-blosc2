/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
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

#define BLOSC_VERSION_STRING   "2.0.0a6.dev"  /* string version.  Sync with above! */
#define BLOSC_VERSION_REVISION "$Rev$"   /* revision version */
#define BLOSC_VERSION_DATE     "$Date:: 2018-05-18 #$"    /* date version */


/* The *_FORMAT symbols below should be just 1-byte long */
enum {
  /* Blosc format version, starting at 1
     1 -> Basically for Blosc pre-1.0
     2 -> Blosc 1.x series
     3 -> Blosc 2.x series */
  BLOSC_VERSION_FORMAT = 3,
};

enum {
  BLOSC_MIN_HEADER_LENGTH = 16,
  /* Minimum header length (Blosc1) */
  BLOSC_EXTENDED_HEADER_LENGTH = 32,
  /* Extended header length (Blosc2, see README_HEADER) */
  BLOSC_MAX_OVERHEAD = BLOSC_EXTENDED_HEADER_LENGTH,
  /* The maximum overhead during compression in bytes.  This equals to
     BLOSC_EXTENDED_HEADER_LENGTH now, but can be higher in future
     implementations */
  BLOSC_MAX_BUFFERSIZE = (INT_MAX - BLOSC_MAX_OVERHEAD),
  /* Maximum source buffer size to be compressed */
  BLOSC_MAX_TYPESIZE = 255,
  /* Maximum typesize before considering source buffer as a stream of bytes */
  /* Cannot be larger than 255 */
  BLOSC_MIN_BUFFERSIZE = 128,       /* Cannot be smaller than 66 */
  /* Minimum buffer size to be compressed */
};

/* Codes for filters (see blosc_compress) */
enum {
  BLOSC_NOSHUFFLE = 0,   /* no shuffle (for compatibility with Blosc1) */
  BLOSC_NOFILTER = 0,    /* no filter */
  BLOSC_SHUFFLE = 1,     /* byte-wise shuffle */
  BLOSC_BITSHUFFLE = 2,  /* bit-wise shuffle */
  BLOSC_DELTA = 3,       /* delta filter */
  BLOSC_TRUNC_PREC = 4,  /* truncate precision filter */
  BLOSC_LAST_FILTER= 5,  /* sentinel */
};

enum {
  BLOSC_MAX_FILTERS = 5,
  /* Maximum number of filters in the filter pipeline */
};

/* Codes for internal flags (see blosc_cbuffer_metainfo) */
enum {
  BLOSC_DOSHUFFLE = 0x1,     /* byte-wise shuffle */
  BLOSC_MEMCPYED = 0x2,      /* plain copy */
  BLOSC_DOBITSHUFFLE = 0x4,  /* bit-wise shuffle */
  BLOSC_DODELTA = 0x8,       /* delta coding */
};

/* Codes for new internal flags in Blosc2 */
enum {
  BLOSC2_USEDICT = 0x1,            /* use dictionaries with codec */
};

/* Values for different Blosc2 capabilities */
enum {
  BLOSC2_MAXDICTSIZE = 128 * 1024, /* maximum size for compression dicts */
};

/* Codes for the different compressors shipped with Blosc */
enum {
  BLOSC_BLOSCLZ = 0,
  BLOSC_LZ4 = 1,
  BLOSC_LZ4HC = 2,
  BLOSC_SNAPPY = 3,
  BLOSC_ZLIB = 4,
  BLOSC_ZSTD = 5,
  BLOSC_LIZARD = 6,
};

/* Names for the different compressors shipped with Blosc */
#define BLOSC_BLOSCLZ_COMPNAME   "blosclz"
#define BLOSC_LZ4_COMPNAME       "lz4"
#define BLOSC_LZ4HC_COMPNAME     "lz4hc"
#define BLOSC_LIZARD_COMPNAME    "lizard"
#define BLOSC_SNAPPY_COMPNAME    "snappy"
#define BLOSC_ZLIB_COMPNAME      "zlib"
#define BLOSC_ZSTD_COMPNAME      "zstd"

/* Codes for compression libraries shipped with Blosc (code must be < 8) */
enum {
  BLOSC_BLOSCLZ_LIB = 0,
  BLOSC_LZ4_LIB = 1,
  BLOSC_SNAPPY_LIB = 2,
  BLOSC_ZLIB_LIB = 3,
  BLOSC_ZSTD_LIB = 4,
  BLOSC_LIZARD_LIB = 5,
  BLOSC_SCHUNK_LIB = 7,   /* compressor library in super-chunk header */
};

/* Names for the different compression libraries shipped with Blosc */
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

/* The codes for compressor formats shipped with Blosc */
enum {
  BLOSC_BLOSCLZ_FORMAT = BLOSC_BLOSCLZ_LIB,
  BLOSC_LZ4_FORMAT = BLOSC_LZ4_LIB,
  /* LZ4HC and LZ4 share the same format */
  BLOSC_LZ4HC_FORMAT = BLOSC_LZ4_LIB,
  BLOSC_LIZARD_FORMAT = BLOSC_LIZARD_LIB,
  BLOSC_SNAPPY_FORMAT = BLOSC_SNAPPY_LIB,
  BLOSC_ZLIB_FORMAT = BLOSC_ZLIB_LIB,
  BLOSC_ZSTD_FORMAT = BLOSC_ZSTD_LIB,
};

/* The version formats for compressors shipped with Blosc */
/* All versions here starts at 1 */
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
  BLOSC_BITSHUFFLE at a bit level (slower but *may* achieve better
  compression).

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

  BLOSC_DELTA=(1|0): This will call blosc_set_delta() before the
  compression process starts.

  BLOSC_TYPESIZE=(INTEGER): This will overwrite the `typesize`
  parameter before the compression process starts.

  BLOSC_COMPRESSOR=[BLOSCLZ | LZ4 | LZ4HC | LIZARD | SNAPPY | ZLIB]:
  This will call blosc_set_compressor(BLOSC_COMPRESSOR) before the
  compression process starts.

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
  blosc_set_nthreads().  BLOSC_CLEVEL, BLOSC_SHUFFLE, BLOSC_DELTA and
  BLOSC_TYPESIZE environment vars will also be honored.
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
  Return the current compressor that is used for compression.
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
  Select the delta coding filter to be used.  If a value >0 is passed, the
  delta filter will be active.  If 0, it will be de-activated.

  This call should always succeed.
*/
BLOSC_EXPORT void blosc_set_delta(int dodelta);


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
BLOSC_EXPORT int blosc_get_complib_info(char* compname, char** complib,
                                        char** version);


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
    * bit 2: whether the bitshuffle filter has been applied or not
    * bit 3: whether the delta coding filter has been applied or not

  You can use the `BLOSC_DOSHUFFLE`, `BLOSC_DOBITSHUFFLE`, `BLOSC_DODELTA`
  and `BLOSC_MEMCPYED` symbols for extracting the interesting bits
  (e.g. ``flags & BLOSC_DOSHUFFLE`` says whether the buffer is byte-shuffled
  or not).

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

  Structures and functions related with contexts.

*********************************************************************/

typedef struct blosc2_context_s blosc2_context;   /* opaque type */

/**
  The parameters for creating a context for compression purposes.

  In parenthesis it is shown the default value used internally when a 0
  (zero) in the fields of the struct is passed to a function.
*/
typedef struct {
  uint8_t compcode;
  /* the compressor codec */
  uint8_t clevel;
  /* the compression level (5) */
  int use_dict;
  /* use dicts or not when compressing (only for ZSTD) */
  int32_t typesize;
  /* the type size (8) */
  int16_t nthreads;
  /* the number of threads to use internally (1) */
  int32_t blocksize;
  /* the requested size of the compressed blocks (0; meaning automatic) */
  void* schunk;
  /* the associated schunk, if any (NULL) */
  uint8_t filters[BLOSC_MAX_FILTERS];
  /* the (sequence of) filters */
  uint8_t filters_meta[BLOSC_MAX_FILTERS];
  /* metadata for filters */
} blosc2_cparams;

/* Default struct for compression params meant for user initialization */
static const blosc2_cparams BLOSC_CPARAMS_DEFAULTS = {
        BLOSC_BLOSCLZ, 5, 0, 8, 1, 0, NULL,
        {0, 0, 0, 0, BLOSC_SHUFFLE}, {0, 0, 0, 0, 0} };

/**
  The parameters for creating a context for decompression purposes.

  In parenthesis it is shown the default value used internally when a 0
  (zero) in the fields of the struct is passed to a function.
*/
typedef struct {
  int16_t nthreads;
  /* the number of threads to use internally (1) */
  void* schunk;
  /* the associated schunk, if any (NULL) */
} blosc2_dparams;

/* Default struct for decompression params meant for user initialization */
static const blosc2_dparams BLOSC_DPARAMS_DEFAULTS = { 1, NULL };

/**
  Create a context for *_ctx() compression functions.

  A pointer to the new context is returned.  NULL is returned if this fails.
*/
BLOSC_EXPORT blosc2_context* blosc2_create_cctx(blosc2_cparams cparams);

/**
  Create a context for *_ctx() decompression functions.

  A pointer to the new context is returned.  NULL is returned if this fails.
*/
BLOSC_EXPORT blosc2_context* blosc2_create_dctx(blosc2_dparams dparams);

/**
  Free the resources associated with a context.

  This function should always succeed and is valid for contexts meant for
  both compression and decompression.
*/
BLOSC_EXPORT void blosc2_free_ctx(blosc2_context* context);

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
        blosc2_context* context, size_t nbytes, const void* src, void* dest,
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
BLOSC_EXPORT int blosc2_decompress_ctx(blosc2_context* context, const void* src,
                                       void* dest, size_t destsize);

/**
  Context interface counterpart for blosc_getitem().

  It uses similar parameters than the blosc_getitem() function plus a
  `context` parameter.

  Returns the number of bytes copied to `dest` or a negative value if
  some error happens.
*/
BLOSC_EXPORT int blosc2_getitem_ctx(blosc2_context* context, const void* src,
                                    int start, int nitems, void* dest);


/*********************************************************************

  Super-chunk related structures and functions.

*********************************************************************/

typedef struct {
  char* fname;     // the name of the file; if NULL, this is in-memory
  uint8_t* sdata;  // the in-memory serialized data
  int64_t len;     // the current length of the frame in (compressed) bytes
  int64_t maxlen;  // the maximum length of the frame; if 0, there is no maximum
  void* schunk;    // pointer to schunk (if it exists)
} blosc2_frame;

/* Empty in-memory frame */
static const blosc2_frame BLOSC_EMPTY_FRAME = {
  .sdata = NULL,
  .fname = NULL,
  .len = 0,
  .maxlen = 0,
  .schunk = NULL,
};

typedef struct {
  uint8_t version;
  uint8_t flags1;
  uint8_t flags2;
  uint8_t flags3;
  uint8_t compcode;
  /* The default compressor.  Each chunk can override this. */
  uint8_t clevel;
  /* The compression level and other compress params */
  int32_t typesize;
  /* the type size */
  int32_t blocksize;
  /* the requested size of the compressed blocks (0; meaning automatic) */
  int32_t chunksize;
  /* Size of each chunk.  0 if not a fixed chunksize. */
  uint8_t filters[BLOSC_MAX_FILTERS];
  /* The (sequence of) filters.  8-bit per filter. */
  uint8_t filters_meta[BLOSC_MAX_FILTERS];
  /* Metadata for filters. 8-bit per meta-slot. */
  int32_t nchunks;
  /* Number of chunks in super-chunk */
  int64_t nbytes;
  /* data size + metadata size + header size (uncompressed) */
  int64_t cbytes;
  /* data size + metadata size + header size (compressed) */
  uint8_t* metadata_chunk;
  /* Pointer to schunk metadata */
  uint8_t* userdata_chunk;
  /* Pointer to user-defined data */
  uint8_t** data;
  /* Pointer to chunk data pointers */
  blosc2_frame* frame;
  /* Pointer to frame to used as store for chunks */
  //uint8_t* ctx;
  /* Context for the thread holder.  NULL if not acquired. */
  blosc2_context* cctx;
  blosc2_context* dctx;
  /* Contexts for compression and decompression */
  uint8_t* reserved;
  /* Reserved for the future. */
} blosc2_schunk;

/* Create a new super-chunk. */
BLOSC_EXPORT blosc2_schunk *
blosc2_new_schunk(blosc2_cparams cparams, blosc2_dparams dparams, blosc2_frame *frame);

/* Release resources from a super-chunk */
BLOSC_EXPORT int blosc2_free_schunk(blosc2_schunk *schunk);

/* Append a `src` data buffer to a super-chunk.

 `typesize` is the number of bytes of the underlying data type and
 `nbytes` is the size of the `src` buffer.

 This returns the number of chunk in super-chunk.  If some problem is
 detected, this number will be negative.
 */
BLOSC_EXPORT int blosc2_schunk_append_buffer(blosc2_schunk *schunk, void *src, size_t nbytes);

/* Decompress and return the `nchunk` chunk of a super-chunk.

 If the chunk is uncompressed successfully, it is put in the `*dest`
 pointer.  `nbytes` is the size of the area pointed by `*dest`.  You
 must make sure that you have space enough to store the uncompressed
 data.

 The size of the decompressed chunk is returned.  If some problem is
 detected, a negative code is returned instead.
 */
BLOSC_EXPORT int blosc2_schunk_decompress_chunk(blosc2_schunk *schunk,
                                                int nchunk, void *dest, size_t nbytes);


/*********************************************************************

  Frame related structures and functions.

*********************************************************************/

/* Create a frame from a super-chunk.

 If `frame->fname` is NULL, a frame is created in memory; else it is created
 on disk.
 */
BLOSC_EXPORT int64_t blosc2_schunk_to_frame(blosc2_schunk *schunk, blosc2_frame *frame);

/* Free all memory from a frame. */
BLOSC_EXPORT int blosc2_free_frame(blosc2_frame *frame);

/* Write an in-memory frame out to a file. */
BLOSC_EXPORT int64_t blosc2_frame_to_file(blosc2_frame *frame, char *fname);

/* Initialize a frame out of a file */
BLOSC_EXPORT blosc2_frame* blosc2_frame_from_file(char *fname);

/* Create a super-chunk from a frame. */
BLOSC_EXPORT blosc2_schunk* blosc2_schunk_from_frame(blosc2_frame* frame);

/* Append an existing chunk into a frame. */
BLOSC_EXPORT void* blosc2_frame_append_chunk(blosc2_frame* frame, void* chunk);

/* Decompress and return a chunk that is part of a frame. */
BLOSC_EXPORT int blosc2_frame_decompress_chunk(blosc2_frame *frame, int nchunk,
                                               void *dest, size_t nbytes);


/*********************************************************************

  Time measurement utilities.  Visit the include below for the public API.

*********************************************************************/

#include "timestamp.h"


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
BLOSC_EXPORT void blosc_set_schunk(blosc2_schunk* schunk);


/*********************************************************************

  Utility functions meant to be used internally.  // TODO put them in their own header

*********************************************************************/

/* Copy 4 bytes from `*pa` to int32_t, changing endianness if necessary. */
static int32_t sw32_(const void* pa) {
  int32_t idest;
  uint8_t* dest = (uint8_t*)&idest;
  uint8_t* pa_ = (uint8_t*)pa;
  int i = 1;                    /* for big/little endian detection */
  char* p = (char*)&i;

  if (p[0] != 1) {
    /* big endian */
    dest[0] = pa_[3];
    dest[1] = pa_[2];
    dest[2] = pa_[1];
    dest[3] = pa_[0];
  }
  else {
    /* little endian */
    dest[0] = pa_[0];
    dest[1] = pa_[1];
    dest[2] = pa_[2];
    dest[3] = pa_[3];
  }
  return idest;
}


/* Copy 4 bytes from `*pa` to `*dest`, changing endianness if necessary. */
static void _sw32(void* dest, int32_t a) {
  uint8_t* dest_ = (uint8_t*)dest;
  uint8_t* pa = (uint8_t*)&a;
  int i = 1;                    /* for big/little endian detection */
  char* p = (char*)&i;

  if (p[0] != 1) {
    /* big endian */
    dest_[0] = pa[3];
    dest_[1] = pa[2];
    dest_[2] = pa[1];
    dest_[3] = pa[0];
  }
  else {
    /* little endian */
    dest_[0] = pa[0];
    dest_[1] = pa[1];
    dest_[2] = pa[2];
    dest_[3] = pa[3];
  }
}


#ifdef __cplusplus
}
#endif


#endif  /* BLOSC_H */
