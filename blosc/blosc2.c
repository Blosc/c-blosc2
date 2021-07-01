/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <assert.h>
#include <math.h>

#include "blosc2.h"
#include "blosc-private.h"
#include "frame.h"


#if defined(USING_CMAKE)
  #include "config.h"
#endif /*  USING_CMAKE */
#include "context.h"

#include "shuffle.h"
#include "delta.h"
#include "trunc-prec.h"
#include "blosclz.h"
#include "stune.h"
#include "config.h"
#include "blosc2/codecs-registry.h"
#include "blosc2/filters-registry.h"

#include "lz4.h"
#include "lz4hc.h"
#ifdef HAVE_IPP
  #include <ipps.h>
  #include <ippdc.h>
#endif
#if defined(HAVE_ZLIB_NG)
  #include "zlib.h"
#elif defined(HAVE_ZLIB)
  #include "zlib.h"
#endif /*  HAVE_MINIZ */
#if defined(HAVE_ZSTD)
  #include "zstd.h"
  #include "zstd_errors.h"
  // #include "cover.h"  // for experimenting with fast cover training for building dicts
  #include "zdict.h"
#endif /*  HAVE_ZSTD */


#if defined(_WIN32) && !defined(__MINGW32__)
  #include <windows.h>
  #include <malloc.h>
  #include <process.h>
  #define getpid _getpid
#endif  /* _WIN32 */

#if defined(_WIN32) && !defined(__GNUC__)
  #include "win32/pthread.c"
#endif

/* Synchronization variables */

/* Global context for non-contextual API */
static blosc2_context* g_global_context;
static pthread_mutex_t global_comp_mutex;
static int g_compressor = BLOSC_BLOSCLZ;
static int g_delta = 0;
/* the compressor to use by default */
static int16_t g_nthreads = 1;
static int32_t g_force_blocksize = 0;
static int g_initlib = 0;
static blosc2_schunk* g_schunk = NULL;   /* the pointer to super-chunk */

blosc2_codec g_codecs[256] = {0};
uint8_t g_ncodecs = 0;

static blosc2_filter g_filters[256] = {0};
static uint64_t g_nfilters = 0;

static blosc2_io_cb g_io[256] = {0};
static uint64_t g_nio = 0;


// Forward declarations
int init_threadpool(blosc2_context *context);
int release_threadpool(blosc2_context *context);

/* Macros for synchronization */

/* Wait until all threads are initialized */
#ifdef BLOSC_POSIX_BARRIERS
#define WAIT_INIT(RET_VAL, CONTEXT_PTR)  \
  rc = pthread_barrier_wait(&(CONTEXT_PTR)->barr_init); \
  if (rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) { \
    BLOSC_TRACE_ERROR("Could not wait on barrier (init): %d", rc); \
    return((RET_VAL));                            \
  }
#else
#define WAIT_INIT(RET_VAL, CONTEXT_PTR)   \
  pthread_mutex_lock(&(CONTEXT_PTR)->count_threads_mutex); \
  if ((CONTEXT_PTR)->count_threads < (CONTEXT_PTR)->nthreads) { \
    (CONTEXT_PTR)->count_threads++;  \
    pthread_cond_wait(&(CONTEXT_PTR)->count_threads_cv, \
                      &(CONTEXT_PTR)->count_threads_mutex); \
  } \
  else { \
    pthread_cond_broadcast(&(CONTEXT_PTR)->count_threads_cv); \
  } \
  pthread_mutex_unlock(&(CONTEXT_PTR)->count_threads_mutex);
#endif

/* Wait for all threads to finish */
#ifdef BLOSC_POSIX_BARRIERS
#define WAIT_FINISH(RET_VAL, CONTEXT_PTR)   \
  rc = pthread_barrier_wait(&(CONTEXT_PTR)->barr_finish); \
  if (rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) { \
    BLOSC_TRACE_ERROR("Could not wait on barrier (finish)"); \
    return((RET_VAL));                              \
  }
#else
#define WAIT_FINISH(RET_VAL, CONTEXT_PTR)                           \
  pthread_mutex_lock(&(CONTEXT_PTR)->count_threads_mutex); \
  if ((CONTEXT_PTR)->count_threads > 0) { \
    (CONTEXT_PTR)->count_threads--; \
    pthread_cond_wait(&(CONTEXT_PTR)->count_threads_cv, \
                      &(CONTEXT_PTR)->count_threads_mutex); \
  } \
  else { \
    pthread_cond_broadcast(&(CONTEXT_PTR)->count_threads_cv); \
  } \
  pthread_mutex_unlock(&(CONTEXT_PTR)->count_threads_mutex);
#endif


/* global variable to change threading backend from Blosc-managed to caller-managed */
static blosc_threads_callback threads_callback = 0;
static void *threads_callback_data = 0;

/* non-threadsafe function should be called before any other Blosc function in
   order to change how threads are managed */
void blosc_set_threads_callback(blosc_threads_callback callback, void *callback_data)
{
  threads_callback = callback;
  threads_callback_data = callback_data;
}


/* A function for aligned malloc that is portable */
static uint8_t* my_malloc(size_t size) {
  void* block = NULL;
  int res = 0;

/* Do an alignment to 32 bytes because AVX2 is supported */
#if defined(_WIN32)
  /* A (void *) cast needed for avoiding a warning with MINGW :-/ */
  block = (void *)_aligned_malloc(size, 32);
#elif _POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600
  /* Platform does have an implementation of posix_memalign */
  res = posix_memalign(&block, 32, size);
#else
  block = malloc(size);
#endif  /* _WIN32 */

  if (block == NULL || res != 0) {
    BLOSC_TRACE_ERROR("Error allocating memory!");
    return NULL;
  }

  return (uint8_t*)block;
}


/* Release memory booked by my_malloc */
static void my_free(void* block) {
#if defined(_WIN32)
  _aligned_free(block);
#else
  free(block);
#endif  /* _WIN32 */
}


/*
 * Conversion routines between compressor and compression libraries
 */

/* Return the library code associated with the compressor name */
static int compname_to_clibcode(const char* compname) {
  if (strcmp(compname, BLOSC_BLOSCLZ_COMPNAME) == 0)
    return BLOSC_BLOSCLZ_LIB;
  if (strcmp(compname, BLOSC_LZ4_COMPNAME) == 0)
    return BLOSC_LZ4_LIB;
  if (strcmp(compname, BLOSC_LZ4HC_COMPNAME) == 0)
    return BLOSC_LZ4_LIB;
  if (strcmp(compname, BLOSC_ZLIB_COMPNAME) == 0)
    return BLOSC_ZLIB_LIB;
  if (strcmp(compname, BLOSC_ZSTD_COMPNAME) == 0)
    return BLOSC_ZSTD_LIB;
  for (int i = 0; i < g_ncodecs; ++i) {
    if (strcmp(compname, g_codecs[i].compname) == 0)
      return g_codecs[i].complib;
  }
  return BLOSC2_ERROR_NOT_FOUND;
}

/* Return the library name associated with the compressor code */
static const char* clibcode_to_clibname(int clibcode) {
  if (clibcode == BLOSC_BLOSCLZ_LIB) return BLOSC_BLOSCLZ_LIBNAME;
  if (clibcode == BLOSC_LZ4_LIB) return BLOSC_LZ4_LIBNAME;
  if (clibcode == BLOSC_ZLIB_LIB) return BLOSC_ZLIB_LIBNAME;
  if (clibcode == BLOSC_ZSTD_LIB) return BLOSC_ZSTD_LIBNAME;
  for (int i = 0; i < g_ncodecs; ++i) {
    if (clibcode == g_codecs[i].complib)
      return g_codecs[i].compname;
  }
  return NULL;                  /* should never happen */
}


/*
 * Conversion routines between compressor names and compressor codes
 */

/* Get the compressor name associated with the compressor code */
int blosc_compcode_to_compname(int compcode, const char** compname) {
  int code = -1;    /* -1 means non-existent compressor code */
  const char* name = NULL;

  /* Map the compressor code */
  if (compcode == BLOSC_BLOSCLZ)
    name = BLOSC_BLOSCLZ_COMPNAME;
  else if (compcode == BLOSC_LZ4)
    name = BLOSC_LZ4_COMPNAME;
  else if (compcode == BLOSC_LZ4HC)
    name = BLOSC_LZ4HC_COMPNAME;
  else if (compcode == BLOSC_ZLIB)
    name = BLOSC_ZLIB_COMPNAME;
  else if (compcode == BLOSC_ZSTD)
    name = BLOSC_ZSTD_COMPNAME;
  else {
    for (int i = 0; i < g_ncodecs; ++i) {
      if (compcode == g_codecs[i].compcode) {
        name = g_codecs[i].compname;
        break;
      }
    }
  }

  *compname = name;

  /* Guess if there is support for this code */
  if (compcode == BLOSC_BLOSCLZ)
    code = BLOSC_BLOSCLZ;
  else if (compcode == BLOSC_LZ4)
    code = BLOSC_LZ4;
  else if (compcode == BLOSC_LZ4HC)
    code = BLOSC_LZ4HC;
#if defined(HAVE_ZLIB)
  else if (compcode == BLOSC_ZLIB)
    code = BLOSC_ZLIB;
#endif /* HAVE_ZLIB */
#if defined(HAVE_ZSTD)
  else if (compcode == BLOSC_ZSTD)
    code = BLOSC_ZSTD;
#endif /* HAVE_ZSTD */
  else if (compcode >= BLOSC_LAST_CODEC)
    code = compcode;
  return code;
}

/* Get the compressor code for the compressor name. -1 if it is not available */
int blosc_compname_to_compcode(const char* compname) {
  int code = -1;  /* -1 means non-existent compressor code */

  if (strcmp(compname, BLOSC_BLOSCLZ_COMPNAME) == 0) {
    code = BLOSC_BLOSCLZ;
  }
  else if (strcmp(compname, BLOSC_LZ4_COMPNAME) == 0) {
    code = BLOSC_LZ4;
  }
  else if (strcmp(compname, BLOSC_LZ4HC_COMPNAME) == 0) {
    code = BLOSC_LZ4HC;
  }
#if defined(HAVE_ZLIB)
  else if (strcmp(compname, BLOSC_ZLIB_COMPNAME) == 0) {
    code = BLOSC_ZLIB;
  }
#endif /*  HAVE_ZLIB */
#if defined(HAVE_ZSTD)
  else if (strcmp(compname, BLOSC_ZSTD_COMPNAME) == 0) {
    code = BLOSC_ZSTD;
  }
#endif /*  HAVE_ZSTD */
  else{
    for (int i = 0; i < g_ncodecs; ++i) {
      if (strcmp(compname, g_codecs[i].compname) == 0) {
        code = g_codecs[i].compcode;
        break;
      }
    }
  }
  return code;
}


/* Convert compressor code to blosc compressor format code */
static int compcode_to_compformat(int compcode) {
  switch (compcode) {
    case BLOSC_BLOSCLZ: return BLOSC_BLOSCLZ_FORMAT;
    case BLOSC_LZ4:     return BLOSC_LZ4_FORMAT;
    case BLOSC_LZ4HC:   return BLOSC_LZ4HC_FORMAT;

#if defined(HAVE_ZLIB)
    case BLOSC_ZLIB:    return BLOSC_ZLIB_FORMAT;
#endif /*  HAVE_ZLIB */

#if defined(HAVE_ZSTD)
    case BLOSC_ZSTD:    return BLOSC_ZSTD_FORMAT;
      break;
#endif /*  HAVE_ZSTD */
    default:
      return BLOSC_UDCODEC_FORMAT;
  }
  return -1;
}


/* Convert compressor code to blosc compressor format version */
static int compcode_to_compversion(int compcode) {
  /* Write compressor format */
  switch (compcode) {
    case BLOSC_BLOSCLZ: return BLOSC_BLOSCLZ_VERSION_FORMAT;
    case BLOSC_LZ4:     return BLOSC_LZ4_VERSION_FORMAT;
    case BLOSC_LZ4HC:   return BLOSC_LZ4HC_VERSION_FORMAT;

#if defined(HAVE_ZLIB)
    case BLOSC_ZLIB:    return BLOSC_ZLIB_VERSION_FORMAT;
      break;
#endif /*  HAVE_ZLIB */

#if defined(HAVE_ZSTD)
    case BLOSC_ZSTD:    return BLOSC_ZSTD_VERSION_FORMAT;
      break;
#endif /*  HAVE_ZSTD */
    default:
      for (int i = 0; i < g_ncodecs; ++i) {
        if (compcode == g_codecs[i].compcode) {
          return g_codecs[i].compver;
        }
      }
  }
  return -1;
}


static int lz4_wrap_compress(const char* input, size_t input_length,
                             char* output, size_t maxout, int accel, void* hash_table) {
  BLOSC_UNUSED_PARAM(accel);
  int cbytes;
#ifdef HAVE_IPP
  if (hash_table == NULL) {
    return BLOSC2_ERROR_INVALID_PARAM;  // the hash table should always be initialized
  }
  int outlen = (int)maxout;
  int inlen = (int)input_length;
  // I have not found any function that uses `accel` like in `LZ4_compress_fast`, but
  // the IPP LZ4Safe call does a pretty good job on compressing well, so let's use it
  IppStatus status = ippsEncodeLZ4Safe_8u((const Ipp8u*)input, &inlen,
                                           (Ipp8u*)output, &outlen, (Ipp8u*)hash_table);
  if (status == ippStsDstSizeLessExpected) {
    return 0;  // we cannot compress in required outlen
  }
  else if (status != ippStsNoErr) {
    return BLOSC2_ERROR_FAILURE;  // an unexpected error happened
  }
  cbytes = outlen;
#else
  BLOSC_UNUSED_PARAM(hash_table);
  accel = 1;  // deactivate acceleration to match IPP behaviour
  cbytes = LZ4_compress_fast(input, output, (int)input_length, (int)maxout, accel);
#endif
  return cbytes;
}


static int lz4hc_wrap_compress(const char* input, size_t input_length,
                               char* output, size_t maxout, int clevel) {
  int cbytes;
  if (input_length > (size_t)(UINT32_C(2) << 30))
    return BLOSC2_ERROR_2GB_LIMIT;
  /* clevel for lz4hc goes up to 12, at least in LZ4 1.7.5
   * but levels larger than 9 do not buy much compression. */
  cbytes = LZ4_compress_HC(input, output, (int)input_length, (int)maxout,
                           clevel);
  return cbytes;
}


static int lz4_wrap_decompress(const char* input, size_t compressed_length,
                               char* output, size_t maxout) {
  int nbytes;
#ifdef HAVE_IPP
  int outlen = (int)maxout;
  int inlen = (int)compressed_length;
  IppStatus status;
  status = ippsDecodeLZ4_8u((const Ipp8u*)input, inlen, (Ipp8u*)output, &outlen);
  //status = ippsDecodeLZ4Dict_8u((const Ipp8u*)input, &inlen, (Ipp8u*)output, 0, &outlen, NULL, 1 << 16);
  nbytes = (status == ippStsNoErr) ? outlen : -outlen;
#else
  nbytes = LZ4_decompress_safe(input, output, (int)compressed_length, (int)maxout);
#endif
  if (nbytes != (int)maxout) {
    return 0;
  }
  return (int)maxout;
}

#if defined(HAVE_ZLIB)
/* zlib is not very respectful with sharing name space with others.
 Fortunately, its names do not collide with those already in blosc. */
static int zlib_wrap_compress(const char* input, size_t input_length,
                              char* output, size_t maxout, int clevel) {
  int status;
  uLongf cl = (uLongf)maxout;
  status = compress2(
      (Bytef*)output, &cl, (Bytef*)input, (uLong)input_length, clevel);
  if (status != Z_OK) {
    return 0;
  }
  return (int)cl;
}

static int zlib_wrap_decompress(const char* input, size_t compressed_length,
                                char* output, size_t maxout) {
  int status;
  uLongf ul = (uLongf)maxout;
  status = uncompress(
      (Bytef*)output, &ul, (Bytef*)input, (uLong)compressed_length);
  if (status != Z_OK) {
    return 0;
  }
  return (int)ul;
}
#endif /*  HAVE_ZLIB */


#if defined(HAVE_ZSTD)
static int zstd_wrap_compress(struct thread_context* thread_context,
                              const char* input, size_t input_length,
                              char* output, size_t maxout, int clevel) {
  size_t code;
  blosc2_context* context = thread_context->parent_context;

  clevel = (clevel < 9) ? clevel * 2 - 1 : ZSTD_maxCLevel();
  /* Make the level 8 close enough to maxCLevel */
  if (clevel == 8) clevel = ZSTD_maxCLevel() - 2;

  if (thread_context->zstd_cctx == NULL) {
    thread_context->zstd_cctx = ZSTD_createCCtx();
  }

  if (context->use_dict) {
    assert(context->dict_cdict != NULL);
    code = ZSTD_compress_usingCDict(
            thread_context->zstd_cctx, (void*)output, maxout, (void*)input,
            input_length, context->dict_cdict);
  } else {
    code = ZSTD_compressCCtx(thread_context->zstd_cctx,
        (void*)output, maxout, (void*)input, input_length, clevel);
  }
  if (ZSTD_isError(code) != ZSTD_error_no_error) {
    // Do not print anything because blosc will just memcpy this buffer
    // fprintf(stderr, "Error in ZSTD compression: '%s'.  Giving up.\n",
    //         ZDICT_getErrorName(code));
    return 0;
  }
  return (int)code;
}

static int zstd_wrap_decompress(struct thread_context* thread_context,
                                const char* input, size_t compressed_length,
                                char* output, size_t maxout) {
  size_t code;
  blosc2_context* context = thread_context->parent_context;

  if (thread_context->zstd_dctx == NULL) {
    thread_context->zstd_dctx = ZSTD_createDCtx();
  }

  if (context->use_dict) {
    assert(context->dict_ddict != NULL);
    code = ZSTD_decompress_usingDDict(
            thread_context->zstd_dctx, (void*)output, maxout, (void*)input,
            compressed_length, context->dict_ddict);
  } else {
    code = ZSTD_decompressDCtx(thread_context->zstd_dctx,
        (void*)output, maxout, (void*)input, compressed_length);
  }
  if (ZSTD_isError(code) != ZSTD_error_no_error) {
    BLOSC_TRACE_ERROR("Error in ZSTD decompression: '%s'.  Giving up.",
                      ZDICT_getErrorName(code));
    return 0;
  }
  return (int)code;
}
#endif /*  HAVE_ZSTD */

/* Compute acceleration for blosclz */
static int get_accel(const blosc2_context* context) {
  int clevel = context->clevel;

  if (context->compcode == BLOSC_LZ4) {
    /* This acceleration setting based on discussions held in:
     * https://groups.google.com/forum/#!topic/lz4c/zosy90P8MQw
     */
    return (10 - clevel);
  }
  return 1;
}


int do_nothing(int8_t filter, char cmode) {
  if (cmode == 'c') {
    return (filter == BLOSC_NOFILTER);
  } else {
    // TRUNC_PREC do not have to be applied during decompression
    return ((filter == BLOSC_NOFILTER) || (filter == BLOSC_TRUNC_PREC));
  }
}


int next_filter(const uint8_t* filters, int current_filter, char cmode) {
  for (int i = current_filter - 1; i >= 0; i--) {
    if (!do_nothing(filters[i], cmode)) {
      return filters[i];
    }
  }
  return BLOSC_NOFILTER;
}


int last_filter(const uint8_t* filters, char cmode) {
  int last_index = -1;
  for (int i = BLOSC2_MAX_FILTERS - 1; i >= 0; i--) {
    if (!do_nothing(filters[i], cmode))  {
      last_index = i;
    }
  }
  return last_index;
}


/* Convert filter pipeline to filter flags */
static uint8_t filters_to_flags(const uint8_t* filters) {
  uint8_t flags = 0;

  for (int i = 0; i < BLOSC2_MAX_FILTERS; i++) {
    switch (filters[i]) {
      case BLOSC_SHUFFLE:
        flags |= BLOSC_DOSHUFFLE;
        break;
      case BLOSC_BITSHUFFLE:
        flags |= BLOSC_DOBITSHUFFLE;
        break;
      case BLOSC_DELTA:
        flags |= BLOSC_DODELTA;
        break;
      default :
        break;
    }
  }
  return flags;
}


/* Convert filter flags to filter pipeline */
static void flags_to_filters(const uint8_t flags, uint8_t* filters) {
  /* Initialize the filter pipeline */
  memset(filters, 0, BLOSC2_MAX_FILTERS);
  /* Fill the filter pipeline */
  if (flags & BLOSC_DOSHUFFLE)
    filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_SHUFFLE;
  if (flags & BLOSC_DOBITSHUFFLE)
    filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_BITSHUFFLE;
  if (flags & BLOSC_DODELTA)
    filters[BLOSC2_MAX_FILTERS - 2] = BLOSC_DELTA;
}


/* Get filter flags from header flags */
static uint8_t get_filter_flags(const uint8_t header_flags,
                                const int32_t typesize) {
  uint8_t flags = 0;

  if ((header_flags & BLOSC_DOSHUFFLE) && (typesize > 1)) {
    flags |= BLOSC_DOSHUFFLE;
  }
  if (header_flags & BLOSC_DOBITSHUFFLE) {
    flags |= BLOSC_DOBITSHUFFLE;
  }
  if (header_flags & BLOSC_DODELTA) {
    flags |= BLOSC_DODELTA;
  }
  if (header_flags & BLOSC_MEMCPYED) {
    flags |= BLOSC_MEMCPYED;
  }
  return flags;
}

typedef struct blosc_header_s {
  uint8_t version;
  uint8_t versionlz;
  uint8_t flags;
  uint8_t typesize;
  int32_t nbytes;
  int32_t blocksize;
  int32_t cbytes;
  // Extended Blosc2 header
  uint8_t filter_codes[BLOSC2_MAX_FILTERS];
  uint8_t udcompcode;
  uint8_t compcode_meta;
  uint8_t filter_meta[BLOSC2_MAX_FILTERS];
  uint8_t reserved2;
  uint8_t blosc2_flags;
} blosc_header;


int read_chunk_header(const uint8_t* src, int32_t srcsize, bool extended_header, blosc_header* header)
{
  memset(header, 0, sizeof(blosc_header));

  if (srcsize < BLOSC_MIN_HEADER_LENGTH) {
    BLOSC_TRACE_ERROR("Not enough space to read Blosc header.");
    return BLOSC2_ERROR_READ_BUFFER;
  }

  memcpy(header, src, BLOSC_MIN_HEADER_LENGTH);

  bool little_endian = is_little_endian();

  if (!little_endian) {
    header->nbytes = bswap32_(header->nbytes);
    header->blocksize = bswap32_(header->blocksize);
    header->cbytes = bswap32_(header->cbytes);
  }

  if (header->version > BLOSC_VERSION_FORMAT) {
    /* Version from future */
    return BLOSC2_ERROR_VERSION_SUPPORT;
  }
  if (header->cbytes < BLOSC_MIN_HEADER_LENGTH) {
    BLOSC_TRACE_ERROR("`cbytes` is too small to read min header.");
    return BLOSC2_ERROR_INVALID_HEADER;
  }
  if (header->blocksize <= 0 || (header->nbytes > 0 && (header->blocksize > header->nbytes))) {
    BLOSC_TRACE_ERROR("`blocksize` is zero or greater than uncompressed size");
    return BLOSC2_ERROR_INVALID_HEADER;
  }
  if (header->blocksize > BLOSC2_MAXBLOCKSIZE) {
    BLOSC_TRACE_ERROR("`blocksize` greater than maximum allowed");
    return BLOSC2_ERROR_INVALID_HEADER;
  }
  if (header->typesize <= 0 || header->typesize > BLOSC_MAX_TYPESIZE) {
    BLOSC_TRACE_ERROR("`typesize` is zero or greater than max allowed.");
    return BLOSC2_ERROR_INVALID_HEADER;
  }

  /* Read extended header if it is wanted */
  if ((extended_header) && (header->flags & BLOSC_DOSHUFFLE) && (header->flags & BLOSC_DOBITSHUFFLE)) {
    if (header->cbytes < BLOSC_EXTENDED_HEADER_LENGTH) {
      BLOSC_TRACE_ERROR("`cbytes` is too small to read extended header.");
      return BLOSC2_ERROR_INVALID_HEADER;
    }
    if (srcsize < BLOSC_EXTENDED_HEADER_LENGTH) {
      BLOSC_TRACE_ERROR("Not enough space to read Blosc extended header.");
      return BLOSC2_ERROR_READ_BUFFER;
    }

    memcpy((uint8_t *)header + BLOSC_MIN_HEADER_LENGTH, src + BLOSC_MIN_HEADER_LENGTH,
      BLOSC_EXTENDED_HEADER_LENGTH - BLOSC_MIN_HEADER_LENGTH);

    int32_t special_type = (header->blosc2_flags >> 4) & BLOSC2_SPECIAL_MASK;
    if (special_type != 0) {
      if (header->nbytes % header->typesize != 0) {
        BLOSC_TRACE_ERROR("`nbytes` is not a multiple of typesize");
        return BLOSC2_ERROR_INVALID_HEADER;
      }
      if (special_type == BLOSC2_SPECIAL_VALUE) {
        if (header->cbytes < BLOSC_EXTENDED_HEADER_LENGTH + header->typesize) {
          BLOSC_TRACE_ERROR("`cbytes` is too small for run length encoding");
          return BLOSC2_ERROR_READ_BUFFER;
        }
      }
    }
    // The number of filters depends on the version of the header. Blosc2 alpha series
    // did not initialize filters to zero beyond the max supported.
    if (header->version == BLOSC2_VERSION_FORMAT_ALPHA) {
      header->filter_codes[5] = 0;
      header->filter_meta[5] = 0;
    }
  }
  else {
    flags_to_filters(header->flags, header->filter_codes);
  }
  return 0;
}

static inline void blosc2_calculate_blocks(blosc2_context* context) {
  /* Compute number of blocks in buffer */
  context->nblocks = context->sourcesize / context->blocksize;
  context->leftover = context->sourcesize % context->blocksize;
  context->nblocks = (context->leftover > 0) ?
                     (context->nblocks + 1) : context->nblocks;
}

static int blosc2_initialize_context_from_header(blosc2_context* context, blosc_header* header) {
  context->header_flags = header->flags;
  context->typesize = header->typesize;
  context->sourcesize = header->nbytes;
  context->blocksize = header->blocksize;
  context->blosc2_flags = header->blosc2_flags;
  context->compcode = header->flags >> 5;
  if (context->compcode == BLOSC_UDCODEC_FORMAT) {
    context->compcode = header->udcompcode;
  }
  blosc2_calculate_blocks(context);

  bool is_lazy = false;
  if ((context->header_flags & BLOSC_DOSHUFFLE) &&
      (context->header_flags & BLOSC_DOBITSHUFFLE)) {
    /* Extended header */
    context->header_overhead = BLOSC_EXTENDED_HEADER_LENGTH;

    memcpy(context->filters, header->filter_codes, BLOSC2_MAX_FILTERS);
    memcpy(context->filters_meta, header->filter_meta, BLOSC2_MAX_FILTERS);
    context->compcode_meta = header->compcode_meta;

    context->filter_flags = filters_to_flags(header->filter_codes);
    context->special_type = (header->blosc2_flags >> 4) & BLOSC2_SPECIAL_MASK;

    is_lazy = (context->blosc2_flags & 0x08u);
  }
  else {
    context->header_overhead = BLOSC_MIN_HEADER_LENGTH;
    context->filter_flags = get_filter_flags(context->header_flags, context->typesize);
    flags_to_filters(context->header_flags, context->filters);
  }

  // Some checks for malformed headers
  if (!is_lazy && header->cbytes > context->srcsize) {
    return BLOSC2_ERROR_INVALID_HEADER;
  }

  return 0;
}


static int blosc2_intialize_header_from_context(blosc2_context* context, blosc_header* header, bool extended_header) {
  memset(header, 0, sizeof(blosc_header));

  header->version = BLOSC_VERSION_FORMAT;
  header->versionlz = compcode_to_compversion(context->compcode);
  header->flags = context->header_flags;
  header->typesize = (uint8_t)context->typesize;
  header->nbytes = (int32_t)context->sourcesize;
  header->blocksize = (int32_t)context->blocksize;

  int little_endian = is_little_endian();
  if (!little_endian) {
    header->nbytes = bswap32_(header->nbytes);
    header->blocksize = bswap32_(header->blocksize);
    // cbytes written after compression
  }

  if (extended_header) {
    /* Store filter pipeline info at the end of the header */
    for (int i = 0; i < BLOSC2_MAX_FILTERS; i++) {
      header->filter_codes[i] = context->filters[i];
      header->filter_meta[i] = context->filters_meta[i];
    }
    header->udcompcode = context->compcode;
    header->compcode_meta = context->compcode_meta;

    if (!little_endian) {
      header->blosc2_flags |= BLOSC2_BIGENDIAN;
    }
    if (context->use_dict) {
      header->blosc2_flags |= BLOSC2_USEDICT;
    }
  }

  return 0;
}


uint8_t* pipeline_forward(struct thread_context* thread_context, const int32_t bsize,
                          const uint8_t* src, const int32_t offset,
                          uint8_t* dest, uint8_t* tmp, uint8_t* tmp2) {
  blosc2_context* context = thread_context->parent_context;
  uint8_t* _src = (uint8_t*)src + offset;
  uint8_t* _tmp = tmp;
  uint8_t* _dest = dest;
  int32_t typesize = context->typesize;
  uint8_t* filters = context->filters;
  uint8_t* filters_meta = context->filters_meta;
  bool memcpyed = context->header_flags & (uint8_t)BLOSC_MEMCPYED;

  /* Prefilter function */
  if (context->prefilter != NULL) {
    // Create new prefilter parameters for this block (must be private for each thread)
    blosc2_prefilter_params preparams;
    memcpy(&preparams, context->preparams, sizeof(preparams));
    preparams.in = _src;
    preparams.out = _dest;
    preparams.out_size = (size_t)bsize;
    preparams.out_typesize = typesize;
    preparams.out_offset = offset;
    preparams.tid = thread_context->tid;
    preparams.ttmp = thread_context->tmp;
    preparams.ttmp_nbytes = thread_context->tmp_nbytes;
    preparams.ctx = context;

    if (context->prefilter(&preparams) != 0) {
      BLOSC_TRACE_ERROR("Execution of prefilter function failed");
      return NULL;
    }

    if (memcpyed) {
      // No more filters are required
      return _dest;
    }
    // Cycle buffers
    _src = _dest;
    _dest = _tmp;
    _tmp = _src;
  }

  /* Process the filter pipeline */
  for (int i = 0; i < BLOSC2_MAX_FILTERS; i++) {
    int rc = BLOSC2_ERROR_SUCCESS;
    if (filters[i] <= BLOSC2_DEFINED_FILTERS_STOP) {
      switch (filters[i]) {
        case BLOSC_SHUFFLE:
          for (int j = 0; j <= filters_meta[i]; j++) {
            shuffle(typesize, bsize, _src, _dest);
            // Cycle filters when required
            if (j < filters_meta[i]) {
              _src = _dest;
              _dest = _tmp;
              _tmp = _src;
            }
          }
          break;
        case BLOSC_BITSHUFFLE:
          if (bitshuffle(typesize, bsize, _src, _dest, tmp2) < 0) {
            return NULL;
          }
          break;
        case BLOSC_DELTA:
          delta_encoder(src, offset, bsize, typesize, _src, _dest);
          break;
        case BLOSC_TRUNC_PREC:
          truncate_precision(filters_meta[i], typesize, bsize, _src, _dest);
          break;
        default:
          if (filters[i] != BLOSC_NOFILTER) {
            BLOSC_TRACE_ERROR("Filter %d not handled during compression\n", filters[i]);
            return NULL;
          }
      }
    }
    else {
      // Look for the filters_meta in user filters and run it
      for (int j = 0; j < g_nfilters; ++j) {
        if (g_filters[j].id == filters[i]) {
          if (g_filters[j].forward != NULL) {
            blosc2_cparams cparams;
            blosc2_ctx_get_cparams(context, &cparams);
            rc = g_filters[j].forward(_src, _dest, bsize, filters_meta[i], &cparams);
          } else {
            BLOSC_TRACE_ERROR("Forward function is NULL");
            return NULL;
          }
          if (rc != BLOSC2_ERROR_SUCCESS) {
            BLOSC_TRACE_ERROR("User-defined filter %d failed during compression\n", filters[i]);
            return NULL;
          }
          goto urfiltersuccess;
        }
      }
      BLOSC_TRACE_ERROR("User-defined filter %d not found during compression\n", filters[i]);
      return NULL;

      urfiltersuccess:;

    }

    // Cycle buffers when required
    if (filters[i] != BLOSC_NOFILTER) {
      _src = _dest;
      _dest = _tmp;
      _tmp = _src;
    }
  }
  return _src;
}


// Optimized version for detecting runs.  It compares 8 bytes values wherever possible.
static bool get_run(const uint8_t* ip, const uint8_t* ip_bound) {
  uint8_t x = *ip;
  int64_t value, value2;
  /* Broadcast the value for every byte in a 64-bit register */
  memset(&value, x, 8);
  while (ip < (ip_bound - 8)) {
#if defined(BLOSC_STRICT_ALIGN)
    memcpy(&value2, ip, 8);
#else
    value2 = *(int64_t*)ip;
#endif
    if (value != value2) {
      // Values differ.  We don't have a run.
      return false;
    }
    else {
      ip += 8;
    }
  }
  /* Look into the remainder */
  while ((ip < ip_bound) && (*ip == x)) ip++;
  return ip == ip_bound ? true : false;
}


/* Shuffle & compress a single block */
static int blosc_c(struct thread_context* thread_context, int32_t bsize,
                   int32_t leftoverblock, int32_t ntbytes, int32_t destsize,
                   const uint8_t* src, const int32_t offset, uint8_t* dest,
                   uint8_t* tmp, uint8_t* tmp2) {
  blosc2_context* context = thread_context->parent_context;
  int dont_split = (context->header_flags & 0x10) >> 4;
  int dict_training = context->use_dict && context->dict_cdict == NULL;
  int32_t j, neblock, nstreams;
  int32_t cbytes;                   /* number of compressed bytes in split */
  int32_t ctbytes = 0;              /* number of compressed bytes in block */
  int64_t maxout;
  int32_t typesize = context->typesize;
  const char* compname;
  int accel;
  const uint8_t* _src;
  uint8_t *_tmp = tmp, *_tmp2 = tmp2;
  uint8_t *_tmp3 = thread_context->tmp4;
  int last_filter_index = last_filter(context->filters, 'c');
  bool memcpyed = context->header_flags & (uint8_t)BLOSC_MEMCPYED;

  if (last_filter_index >= 0 || context->prefilter != NULL) {
    /* Apply the filter pipeline just for the prefilter */
    if (memcpyed && context->prefilter != NULL) {
      // We only need the prefilter output
      _src = pipeline_forward(thread_context, bsize, src, offset, dest, _tmp2, _tmp3);
      if (_src == NULL) {
        return BLOSC2_ERROR_FILTER_PIPELINE;
      }
      return bsize;
    }
    /* Apply regular filter pipeline */
    _src = pipeline_forward(thread_context, bsize, src, offset, _tmp, _tmp2, _tmp3);
    if (_src == NULL) {
      return BLOSC2_ERROR_FILTER_PIPELINE;
    }
  } else {
    _src = src + offset;
  }

  assert(context->clevel > 0);

  /* Calculate acceleration for different compressors */
  accel = get_accel(context);

  /* The number of compressed data streams for this block */
  if (!dont_split && !leftoverblock && !dict_training) {
    nstreams = (int32_t)typesize;
  }
  else {
    nstreams = 1;
  }
  neblock = bsize / nstreams;
  for (j = 0; j < nstreams; j++) {
    if (!dict_training) {
      dest += sizeof(int32_t);
      ntbytes += sizeof(int32_t);
      ctbytes += sizeof(int32_t);

      const uint8_t *ip = (uint8_t *) _src + j * neblock;
      const uint8_t *ipbound = (uint8_t *) _src + (j + 1) * neblock;

      // See whether we have a run here
      if (context->header_overhead == BLOSC_EXTENDED_HEADER_LENGTH && get_run(ip, ipbound)) {
        // A run
        int32_t value = _src[j * neblock];
        if (ntbytes > destsize) {
          return 0;    /* Non-compressible data */
        }
        // Encode the repeated byte in the first (LSB) byte of the length of the split.
        _sw32(dest - 4, -value);    // write the value in two's complement
        if (value > 0) {
          // Mark the encoding as a run-length (== 0 is always a 0's run)
          ntbytes += 1;
          ctbytes += 1;
          if (ntbytes > destsize) {
            return 0;    /* Non-compressible data */
          }
          // Set MSB bit (sign) to 1 (not really necessary here, but for demonstration purposes)
          // dest[-1] |= 0x80;
          dest[0] = 0x1;   // set run-length bit (0) in token
          dest += 1;
        }
        continue;
      }
    }

    maxout = neblock;
    if (ntbytes + maxout > destsize) {
      /* avoid buffer * overrun */
      maxout = (int64_t)destsize - (int64_t)ntbytes;
      if (maxout <= 0) {
        return 0;                  /* non-compressible block */
      }
    }
    if (dict_training) {
      // We are in the build dict state, so don't compress
      // TODO: copy only a percentage for sampling
      memcpy(dest, _src + j * neblock, (unsigned int)neblock);
      cbytes = (int32_t)neblock;
    }
    else if (context->compcode == BLOSC_BLOSCLZ) {
      cbytes = blosclz_compress(context->clevel, _src + j * neblock,
                                (int)neblock, dest, (int)maxout);
    }
    else if (context->compcode == BLOSC_LZ4) {
      void *hash_table = NULL;
    #ifdef HAVE_IPP
      hash_table = (void*)thread_context->lz4_hash_table;
    #endif
      cbytes = lz4_wrap_compress((char*)_src + j * neblock, (size_t)neblock,
                                 (char*)dest, (size_t)maxout, accel, hash_table);
    }
    else if (context->compcode == BLOSC_LZ4HC) {
      cbytes = lz4hc_wrap_compress((char*)_src + j * neblock, (size_t)neblock,
                                   (char*)dest, (size_t)maxout, context->clevel);
    }
  #if defined(HAVE_ZLIB)
    else if (context->compcode == BLOSC_ZLIB) {
      cbytes = zlib_wrap_compress((char*)_src + j * neblock, (size_t)neblock,
                                  (char*)dest, (size_t)maxout, context->clevel);
    }
  #endif /* HAVE_ZLIB */
  #if defined(HAVE_ZSTD)
    else if (context->compcode == BLOSC_ZSTD) {
      cbytes = zstd_wrap_compress(thread_context,
                                  (char*)_src + j * neblock, (size_t)neblock,
                                  (char*)dest, (size_t)maxout, context->clevel);
    }
  #endif /* HAVE_ZSTD */
    else if (context->compcode > BLOSC2_DEFINED_CODECS_STOP) {
      for (int i = 0; i < g_ncodecs; ++i) {
        if (g_codecs[i].compcode == context->compcode) {
          blosc2_cparams cparams;
          blosc2_ctx_get_cparams(context, &cparams);
          cbytes = g_codecs[i].encoder(_src + j * neblock,
                                        neblock,
                                        dest,
                                        maxout,
                                        context->compcode_meta,
                                        &cparams);
          goto urcodecsuccess;
        }
      }
      BLOSC_TRACE_ERROR("User-defined compressor codec %d not found during compression", context->compcode);
      return BLOSC2_ERROR_CODEC_SUPPORT;
    urcodecsuccess:
      ;
    } else {
      blosc_compcode_to_compname(context->compcode, &compname);
      BLOSC_TRACE_ERROR("Blosc has not been compiled with '%s' compression support."
                        "Please use one having it.", compname);
      return BLOSC2_ERROR_CODEC_SUPPORT;
    }

    if (cbytes > maxout) {
      /* Buffer overrun caused by compression (should never happen) */
      return BLOSC2_ERROR_WRITE_BUFFER;
    }
    if (cbytes < 0) {
      /* cbytes should never be negative */
      return BLOSC2_ERROR_DATA;
    }
    if (!dict_training) {
      if (cbytes == 0 || cbytes == neblock) {
        /* The compressor has been unable to compress data at all. */
        /* Before doing the copy, check that we are not running into a
           buffer overflow. */
        if ((ntbytes + neblock) > destsize) {
          return 0;    /* Non-compressible data */
        }
        memcpy(dest, _src + j * neblock, (unsigned int)neblock);
        cbytes = neblock;
      }
      _sw32(dest - 4, cbytes);
    }
    dest += cbytes;
    ntbytes += cbytes;
    ctbytes += cbytes;
  }  /* Closes j < nstreams */

  //printf("c%d", ctbytes);
  return ctbytes;
}


/* Process the filter pipeline (decompression mode) */
int pipeline_backward(struct thread_context* thread_context, const int32_t bsize, uint8_t* dest,
               const int32_t offset, uint8_t* src, uint8_t* tmp,
               uint8_t* tmp2, int last_filter_index, int32_t nblock) {
  blosc2_context* context = thread_context->parent_context;
  int32_t typesize = context->typesize;
  uint8_t* filters = context->filters;
  uint8_t* filters_meta = context->filters_meta;
  blosc2_filter * urfilters = context->urfilters;
  uint8_t* _src = src;
  uint8_t* _dest = tmp;
  uint8_t* _tmp = tmp2;
  int errcode = 0;

  for (int i = BLOSC2_MAX_FILTERS - 1; i >= 0; i--) {
    // Delta filter requires the whole chunk ready
    int last_copy_filter = (last_filter_index == i) || (next_filter(filters, i, 'd') == BLOSC_DELTA);
    if (last_copy_filter && context->postfilter == NULL) {
      _dest = dest + offset;
    }
    int rc = BLOSC2_ERROR_SUCCESS;
    if (filters[i] <= BLOSC2_DEFINED_FILTERS_STOP) {
      switch (filters[i]) {
        case BLOSC_SHUFFLE:
          for (int j = 0; j <= filters_meta[i]; j++) {
            unshuffle(typesize, bsize, _src, _dest);
            // Cycle filters when required
            if (j < filters_meta[i]) {
              _src = _dest;
              _dest = _tmp;
              _tmp = _src;
            }
            // Check whether we have to copy the intermediate _dest buffer to final destination
            if (last_copy_filter && (filters_meta[i] % 2) == 1 && j == filters_meta[i]) {
              memcpy(dest + offset, _dest, (unsigned int) bsize);
            }
          }
          break;
        case BLOSC_BITSHUFFLE:
          if (bitunshuffle(typesize, bsize, _src, _dest, _tmp, context->src[BLOSC2_CHUNK_VERSION]) < 0) {
            return BLOSC2_ERROR_FILTER_PIPELINE;
          }
          break;
        case BLOSC_DELTA:
          if (context->nthreads == 1) {
            /* Serial mode */
            delta_decoder(dest, offset, bsize, typesize, _dest);
          } else {
            /* Force the thread in charge of the block 0 to go first */
            pthread_mutex_lock(&context->delta_mutex);
            if (context->dref_not_init) {
              if (offset != 0) {
                pthread_cond_wait(&context->delta_cv, &context->delta_mutex);
              } else {
                delta_decoder(dest, offset, bsize, typesize, _dest);
                context->dref_not_init = 0;
                pthread_cond_broadcast(&context->delta_cv);
              }
            }
            pthread_mutex_unlock(&context->delta_mutex);
            if (offset != 0) {
              delta_decoder(dest, offset, bsize, typesize, _dest);
            }
          }
          break;
        case BLOSC_TRUNC_PREC:
          // TRUNC_PREC filter does not need to be undone
          break;
        default:
          if (filters[i] != BLOSC_NOFILTER) {
            BLOSC_TRACE_ERROR("Filter %d not handled during decompression.",
                              filters[i]);

            errcode = -1;
          }
      }
    } else {
        // Look for the filters_meta in user filters and run it
        for (int j = 0; j < g_nfilters; ++j) {
          if (g_filters[j].id == filters[i]) {
            if (g_filters[j].backward != NULL) {
              blosc2_dparams dparams;
              blosc2_ctx_get_dparams(context, &dparams);
              rc = g_filters[j].backward(_src, _dest, bsize, filters_meta[i], &dparams);
            } else {
              BLOSC_TRACE_ERROR("Backward function is NULL");
              return BLOSC2_ERROR_FILTER_PIPELINE;
            }
            if (rc != BLOSC2_ERROR_SUCCESS) {
              BLOSC_TRACE_ERROR("User-defined filter %d failed during decompression.", filters[i]);
              return rc;
            }
            goto urfiltersuccess;
          }
        }
      BLOSC_TRACE_ERROR("User-defined filter %d not found during decompression.", filters[i]);
      return BLOSC2_ERROR_FILTER_PIPELINE;
      urfiltersuccess:;
    }

    // Cycle buffers when required
    if ((filters[i] != BLOSC_NOFILTER) && (filters[i] != BLOSC_TRUNC_PREC)) {
      _src = _dest;
      _dest = _tmp;
      _tmp = _src;
    }
    if (last_filter_index == i) {
      break;
    }
  }

  /* Postfilter function */
  if (context->postfilter != NULL) {
    // Create new postfilter parameters for this block (must be private for each thread)
    blosc2_postfilter_params postparams;
    memcpy(&postparams, context->postparams, sizeof(postparams));
    postparams.in = _src;
    postparams.out = dest + offset;
    postparams.size = bsize;
    postparams.typesize = typesize;
    postparams.offset = nblock * context->blocksize;
    postparams.tid = thread_context->tid;
    postparams.ttmp = thread_context->tmp;
    postparams.ttmp_nbytes = thread_context->tmp_nbytes;
    postparams.ctx = context;

    if (context->postfilter(&postparams) != 0) {
      BLOSC_TRACE_ERROR("Execution of postfilter function failed");
      return BLOSC2_ERROR_POSTFILTER;
    }
  }

  return errcode;
}


static int32_t set_nans(int32_t typesize, uint8_t* dest, int32_t destsize) {
  // destsize can only be a multiple of typesize
  if (destsize % typesize != 0) {
    return -1;
  }
  int32_t nitems = destsize / typesize;
  if (nitems == 0) {
    return 0;
  }

  if (typesize == 4) {
    float* dest_ = (float*)dest;
    float val = nanf("");
    for (int i = 0; i < nitems; i++) {
      dest_[i] = val;
    }
    return nitems;
  }
  else if (typesize == 8) {
    double* dest_ = (double*)dest;
    double val = nan("");
    for (int i = 0; i < nitems; i++) {
      dest_[i] = val;
    }
    return nitems;
  }

  BLOSC_TRACE_ERROR("Unsupported typesize for NaN");
  return BLOSC2_ERROR_DATA;
}


static int32_t set_values(int32_t typesize, const uint8_t* src, uint8_t* dest, int32_t destsize) {
  // destsize can only be a multiple of typesize
  int64_t val8;
  int64_t* dest8;
  int32_t val4;
  int32_t* dest4;
  int16_t val2;
  int16_t* dest2;
  int8_t val1;
  int8_t* dest1;

  if (destsize % typesize != 0) {
    return -1;
  }
  int32_t nitems = destsize / typesize;
  if (nitems == 0) {
    return 0;
  }

  switch (typesize) {
    case 8:
      val8 = ((int64_t*)(src + BLOSC_EXTENDED_HEADER_LENGTH))[0];
      dest8 = (int64_t*)dest;
      for (int i = 0; i < nitems; i++) {
        dest8[i] = val8;
      }
      break;
    case 4:
      val4 = ((int32_t*)(src + BLOSC_EXTENDED_HEADER_LENGTH))[0];
      dest4 = (int32_t*)dest;
      for (int i = 0; i < nitems; i++) {
        dest4[i] = val4;
      }
      break;
    case 2:
      val2 = ((int16_t*)(src + BLOSC_EXTENDED_HEADER_LENGTH))[0];
      dest2 = (int16_t*)dest;
      for (int i = 0; i < nitems; i++) {
        dest2[i] = val2;
      }
      break;
    case 1:
      val1 = ((int8_t*)(src + BLOSC_EXTENDED_HEADER_LENGTH))[0];
      dest1 = (int8_t*)dest;
      for (int i = 0; i < nitems; i++) {
        dest1[i] = val1;
      }
      break;
    default:
      for (int i = 0; i < nitems; i++) {
        memcpy(dest + i * typesize, src + BLOSC_EXTENDED_HEADER_LENGTH, typesize);
      }
  }

  return nitems;
}


/* Decompress & unshuffle a single block */
static int blosc_d(
    struct thread_context* thread_context, int32_t bsize,
    int32_t leftoverblock, bool memcpyed, const uint8_t* src, int32_t srcsize, int32_t src_offset,
    int32_t nblock, uint8_t* dest, int32_t dest_offset, uint8_t* tmp, uint8_t* tmp2) {
  blosc2_context* context = thread_context->parent_context;
  uint8_t* filters = context->filters;
  uint8_t *tmp3 = thread_context->tmp4;
  int32_t compformat = (context->header_flags & (uint8_t)0xe0) >> 5u;
  int dont_split = (context->header_flags & (uint8_t)0x10) >> 4u;
  int32_t chunk_nbytes;
  int32_t chunk_cbytes;
  int nstreams;
  int32_t neblock;
  int32_t nbytes;                /* number of decompressed bytes in split */
  int32_t cbytes;                /* number of compressed bytes in split */
  int32_t ctbytes = 0;           /* number of compressed bytes in block */
  int32_t ntbytes = 0;           /* number of uncompressed bytes in block */
  uint8_t* _dest;
  int32_t typesize = context->typesize;
  const char* compname;
  int rc;

  rc = blosc2_cbuffer_sizes(src, &chunk_nbytes, &chunk_cbytes, NULL);
  if (rc < 0) {
    return rc;
  }

  if (context->block_maskout != NULL && context->block_maskout[nblock]) {
    // Do not decompress, but act as if we successfully decompressed everything
    return bsize;
  }

  // In some situations (lazychunks) the context can arrive uninitialized
  // (but BITSHUFFLE needs it for accessing the format of the chunk)
  if (context->src == NULL) {
    context->src = src;
  }

  // Chunks with special values cannot be lazy
  bool is_lazy = ((context->header_overhead == BLOSC_EXTENDED_HEADER_LENGTH) &&
          (context->blosc2_flags & 0x08u) && !context->special_type);
  if (is_lazy) {
    // The chunk is on disk, so just lazily load the block
    if (context->schunk == NULL) {
      BLOSC_TRACE_ERROR("Lazy chunk needs an associated super-chunk.");
      return BLOSC2_ERROR_INVALID_PARAM;
    }
    if (context->schunk->frame == NULL) {
      BLOSC_TRACE_ERROR("Lazy chunk needs an associated frame.");
      return BLOSC2_ERROR_INVALID_PARAM;
    }
    blosc2_frame_s* frame = (blosc2_frame_s*)context->schunk->frame;
    char* urlpath = frame->urlpath;
    int32_t trailer_len = sizeof(int32_t) + sizeof(int64_t) + context->nblocks * sizeof(int32_t);
    size_t trailer_offset = BLOSC_EXTENDED_HEADER_LENGTH + context->nblocks * sizeof(int32_t);
    int32_t nchunk;
    int64_t chunk_offset;
    // The nchunk and the offset of the current chunk are in the trailer
    nchunk = *(int32_t*)(src + trailer_offset);
    chunk_offset = *(int64_t*)(src + trailer_offset + sizeof(int32_t));
    // Get the csize of the nblock
    int32_t *block_csizes = (int32_t *)(src + trailer_offset + sizeof(int32_t) + sizeof(int64_t));
    int32_t block_csize = block_csizes[nblock];
    // Read the lazy block on disk
    void* fp = NULL;
    blosc2_io_cb *io_cb = blosc2_get_io_cb(context->schunk->storage->io->id);
    if (io_cb == NULL) {
      BLOSC_TRACE_ERROR("Error getting the input/output API");
      return BLOSC2_ERROR_PLUGIN_IO;
    }

    if (frame->sframe) {
      // The chunk is not in the frame
      char* chunkpath = malloc(strlen(frame->urlpath) + 1 + 8 + strlen(".chunk") + 1);
      BLOSC_ERROR_NULL(chunkpath, BLOSC2_ERROR_MEMORY_ALLOC);
      sprintf(chunkpath, "%s/%08X.chunk", frame->urlpath, nchunk);
      fp = io_cb->open(chunkpath, "rb", context->schunk->storage->io->params);
      free(chunkpath);
      // The offset of the block is src_offset
      io_cb->seek(fp, src_offset, SEEK_SET);
    }
    else {
      fp = io_cb->open(urlpath, "rb", context->schunk->storage->io->params);
      // The offset of the block is src_offset
      io_cb->seek(fp, chunk_offset + src_offset, SEEK_SET);
    }
    // We can make use of tmp3 because it will be used after src is not needed anymore
    int64_t rbytes = io_cb->read(tmp3, 1, block_csize, fp);
    io_cb->close(fp);
    if ((int32_t)rbytes != block_csize) {
      BLOSC_TRACE_ERROR("Cannot read the (lazy) block out of the fileframe.");
      return BLOSC2_ERROR_READ_BUFFER;
    }
    src = tmp3;
    src_offset = 0;
    srcsize = block_csize;
  }

  // If the chunk is memcpyed, we just have to copy the block to dest and return
  if (memcpyed) {
    int bsize_ = leftoverblock ? chunk_nbytes % context->blocksize : bsize;
    if (!context->special_type) {
      if (chunk_nbytes + context->header_overhead != chunk_cbytes) {
        return BLOSC2_ERROR_WRITE_BUFFER;
      }
      if (chunk_cbytes < context->header_overhead + (nblock * context->blocksize) + bsize_) {
        /* Not enough input to copy block */
        return BLOSC2_ERROR_READ_BUFFER;
      }
    }
    if (!is_lazy) {
      src += context->header_overhead + nblock * context->blocksize;
    }
    _dest = dest + dest_offset;
    if (context->postfilter != NULL) {
      // We are making use of a postfilter, so use a temp for destination
      _dest = tmp;
    }
    rc = 0;
    switch (context->special_type) {
      case BLOSC2_SPECIAL_VALUE:
        // All repeated values
        rc = set_values(context->typesize, context->src, _dest, bsize_);
        if (rc < 0) {
          BLOSC_TRACE_ERROR("set_values failed");
          return BLOSC2_ERROR_DATA;
        }
        break;
      case BLOSC2_SPECIAL_NAN:
        rc = set_nans(context->typesize, _dest, bsize_);
        if (rc < 0) {
          BLOSC_TRACE_ERROR("set_nans failed");
          return BLOSC2_ERROR_DATA;
        }
        break;
      case BLOSC2_SPECIAL_ZERO:
        memset(_dest, 0, bsize_);
        break;
      case BLOSC2_SPECIAL_UNINIT:
        // We do nothing here
        break;
      default:
        memcpy(_dest, src, bsize_);
    }
    if (context->postfilter != NULL) {
      // Create new postfilter parameters for this block (must be private for each thread)
      blosc2_postfilter_params postparams;
      memcpy(&postparams, context->postparams, sizeof(postparams));
      postparams.in = tmp;
      postparams.out = dest + dest_offset;
      postparams.size = bsize;
      postparams.typesize = typesize;
      postparams.offset = nblock * context->blocksize;
      postparams.tid = thread_context->tid;
      postparams.ttmp = thread_context->tmp;
      postparams.ttmp_nbytes = thread_context->tmp_nbytes;
      postparams.ctx = context;

      // Execute the postfilter (the processed block will be copied to dest)
      if (context->postfilter(&postparams) != 0) {
        BLOSC_TRACE_ERROR("Execution of postfilter function failed");
        return BLOSC2_ERROR_POSTFILTER;
      }
    }
    return bsize_;
  }

  if (!is_lazy && (src_offset <= 0 || src_offset >= srcsize)) {
    /* Invalid block src offset encountered */
    return BLOSC2_ERROR_DATA;
  }

  src += src_offset;
  srcsize -= src_offset;

  int last_filter_index = last_filter(filters, 'd');

  if (((last_filter_index >= 0) &&
      (next_filter(filters, BLOSC2_MAX_FILTERS, 'd') != BLOSC_DELTA)) ||
      context->postfilter != NULL) {
   // We are making use of some filter, so use a temp for destination
   _dest = tmp;
  } else {
    // If no filters, or only DELTA in pipeline
   _dest = dest + dest_offset;
  }

  /* The number of compressed data streams for this block */
  if (!dont_split && !leftoverblock && !context->use_dict) {
    // We don't want to split when in a training dict state
    nstreams = (int32_t)typesize;
  }
  else {
    nstreams = 1;
  }

  neblock = bsize / nstreams;
  if (neblock == 0) {
    /* Not enough space to output bytes */
    return -1;
  }
  for (int j = 0; j < nstreams; j++) {
    if (srcsize < (signed)sizeof(int32_t)) {
      /* Not enough input to read compressed size */
      return BLOSC2_ERROR_READ_BUFFER;
    }
    srcsize -= sizeof(int32_t);
    cbytes = sw32_(src);      /* amount of compressed bytes */
    if (cbytes > 0) {
      if (srcsize < cbytes) {
        /* Not enough input to read compressed bytes */
        return BLOSC2_ERROR_READ_BUFFER;
      }
      srcsize -= cbytes;
    }
    src += sizeof(int32_t);
    ctbytes += (signed)sizeof(int32_t);

    /* Uncompress */
    if (cbytes == 0) {
      // A run of 0's
      memset(_dest, 0, (unsigned int)neblock);
      nbytes = neblock;
    }
    else if (cbytes < 0) {
      // A negative number means some encoding depending on the token that comes next
      uint8_t token;

      if (srcsize < (signed)sizeof(uint8_t)) {
        // Not enough input to read token */
        return BLOSC2_ERROR_READ_BUFFER;
      }
      srcsize -= sizeof(uint8_t);

      token = src[0];
      src += 1;
      ctbytes += 1;

      if (token & 0x1) {
        // A run of bytes that are different than 0
        if (cbytes < -255) {
          // Runs can only encode a byte
          return BLOSC2_ERROR_RUN_LENGTH;
        }
        uint8_t value = -cbytes;
        memset(_dest, value, (unsigned int)neblock);
      } else {
        BLOSC_TRACE_ERROR("Invalid or unsupported compressed stream token value - %d", token);
        return BLOSC2_ERROR_RUN_LENGTH;
      }
      nbytes = neblock;
      cbytes = 0;  // everything is encoded in the cbytes token
    }
    else if (cbytes == neblock) {
      memcpy(_dest, src, (unsigned int)neblock);
      nbytes = (int32_t)neblock;
    }
    else {
      if (compformat == BLOSC_BLOSCLZ_FORMAT) {
        nbytes = blosclz_decompress(src, cbytes, _dest, (int)neblock);
      }
      else if (compformat == BLOSC_LZ4_FORMAT) {
        nbytes = lz4_wrap_decompress((char*)src, (size_t)cbytes,
                                     (char*)_dest, (size_t)neblock);
      }
  #if defined(HAVE_ZLIB)
      else if (compformat == BLOSC_ZLIB_FORMAT) {
        nbytes = zlib_wrap_decompress((char*)src, (size_t)cbytes,
                                      (char*)_dest, (size_t)neblock);
      }
  #endif /*  HAVE_ZLIB */
  #if defined(HAVE_ZSTD)
      else if (compformat == BLOSC_ZSTD_FORMAT) {
        nbytes = zstd_wrap_decompress(thread_context,
                                      (char*)src, (size_t)cbytes,
                                      (char*)_dest, (size_t)neblock);
      }
  #endif /*  HAVE_ZSTD */
      else if (compformat == BLOSC_UDCODEC_FORMAT) {
        for (int i = 0; i < g_ncodecs; ++i) {
          if (g_codecs[i].compcode == context->compcode) {
            blosc2_dparams dparams;
            blosc2_ctx_get_dparams(context, &dparams);
            nbytes = g_codecs[i].decoder(src,
                                          cbytes,
                                          _dest,
                                          neblock,
                                          context->compcode_meta,
                                          &dparams);
            goto urcodecsuccess;
          }
        }
        BLOSC_TRACE_ERROR("User-defined compressor codec %d not found during decompression", context->compcode);
        return BLOSC2_ERROR_CODEC_SUPPORT;
      urcodecsuccess:
        ;
      }
      else {
        compname = clibcode_to_clibname(compformat);
        BLOSC_TRACE_ERROR(
                "Blosc has not been compiled with decompression "
                "support for '%s' format.  "
                "Please recompile for adding this support.", compname);
        return BLOSC2_ERROR_CODEC_SUPPORT;
      }

      /* Check that decompressed bytes number is correct */
      if (nbytes != neblock) {
        return BLOSC2_ERROR_DATA;
      }

    }
    src += cbytes;
    ctbytes += cbytes;
    _dest += nbytes;
    ntbytes += nbytes;
  } /* Closes j < nstreams */

  if (last_filter_index >= 0 || context->postfilter != NULL) {
    /* Apply regular filter pipeline */
    int errcode = pipeline_backward(thread_context, bsize, dest, dest_offset, tmp, tmp2, tmp3,
                             last_filter_index, nblock);
    if (errcode < 0)
      return errcode;
  }

  /* Return the number of uncompressed bytes */
  return (int)ntbytes;
}


/* Serial version for compression/decompression */
static int serial_blosc(struct thread_context* thread_context) {
  blosc2_context* context = thread_context->parent_context;
  int32_t j, bsize, leftoverblock;
  int32_t cbytes;
  int32_t ntbytes = (int32_t)context->output_bytes;
  int32_t* bstarts = context->bstarts;
  uint8_t* tmp = thread_context->tmp;
  uint8_t* tmp2 = thread_context->tmp2;
  int dict_training = context->use_dict && (context->dict_cdict == NULL);
  bool memcpyed = context->header_flags & (uint8_t)BLOSC_MEMCPYED;
  if (!context->do_compress && context->special_type) {
    // Fake a runlen as if its a memcpyed chunk
    memcpyed = true;
  }

  for (j = 0; j < context->nblocks; j++) {
    if (context->do_compress && !memcpyed && !dict_training) {
      _sw32(bstarts + j, ntbytes);
    }
    bsize = context->blocksize;
    leftoverblock = 0;
    if ((j == context->nblocks - 1) && (context->leftover > 0)) {
      bsize = context->leftover;
      leftoverblock = 1;
    }
    if (context->do_compress) {
      if (memcpyed && !context->prefilter) {
        /* We want to memcpy only */
        memcpy(context->dest + context->header_overhead + j * context->blocksize,
               context->src + j * context->blocksize, (unsigned int)bsize);
        cbytes = (int32_t)bsize;
      }
      else {
        /* Regular compression */
        cbytes = blosc_c(thread_context, bsize, leftoverblock, ntbytes,
                         context->destsize, context->src, j * context->blocksize,
                         context->dest + ntbytes, tmp, tmp2);
        if (cbytes == 0) {
          ntbytes = 0;              /* uncompressible data */
          break;
        }
      }
    }
    else {
      /* Regular decompression */
      // If memcpyed we don't have a bstarts section (because it is not needed)
      int32_t src_offset = memcpyed ?
          context->header_overhead + j * context->blocksize : sw32_(bstarts + j);
      cbytes = blosc_d(thread_context, bsize, leftoverblock, memcpyed,
                       context->src, context->srcsize, src_offset, j,
                       context->dest, j * context->blocksize, tmp, tmp2);
    }

    if (cbytes < 0) {
      ntbytes = cbytes;         /* error in blosc_c or blosc_d */
      break;
    }
    ntbytes += cbytes;
  }

  return ntbytes;
}

static void t_blosc_do_job(void *ctxt);

/* Threaded version for compression/decompression */
static int parallel_blosc(blosc2_context* context) {
#ifdef BLOSC_POSIX_BARRIERS
  int rc;
#endif
  /* Set sentinels */
  context->thread_giveup_code = 1;
  context->thread_nblock = -1;

  if (threads_callback) {
    threads_callback(threads_callback_data, t_blosc_do_job,
                     context->nthreads, sizeof(struct thread_context), (void*) context->thread_contexts);
  }
  else {
    /* Synchronization point for all threads (wait for initialization) */
    WAIT_INIT(-1, context);

    /* Synchronization point for all threads (wait for finalization) */
    WAIT_FINISH(-1, context);
  }

  if (context->thread_giveup_code <= 0) {
    /* Compression/decompression gave up.  Return error code. */
    return context->thread_giveup_code;
  }

  /* Return the total bytes (de-)compressed in threads */
  return (int)context->output_bytes;
}

/* initialize a thread_context that has already been allocated */
static int init_thread_context(struct thread_context* thread_context, blosc2_context* context, int32_t tid)
{
  int32_t ebsize;

  thread_context->parent_context = context;
  thread_context->tid = tid;

  ebsize = context->blocksize + context->typesize * (signed)sizeof(int32_t);
  thread_context->tmp_nbytes = (size_t)4 * ebsize;
  thread_context->tmp = my_malloc(thread_context->tmp_nbytes);
  BLOSC_ERROR_NULL(thread_context->tmp, BLOSC2_ERROR_MEMORY_ALLOC);
  thread_context->tmp2 = thread_context->tmp + ebsize;
  thread_context->tmp3 = thread_context->tmp2 + ebsize;
  thread_context->tmp4 = thread_context->tmp3 + ebsize;
  thread_context->tmp_blocksize = context->blocksize;
  #if defined(HAVE_ZSTD)
  thread_context->zstd_cctx = NULL;
  thread_context->zstd_dctx = NULL;
  #endif

  /* Create the hash table for LZ4 in case we are using IPP */
#ifdef HAVE_IPP
  IppStatus status;
  int inlen = thread_context->tmp_blocksize > 0 ? thread_context->tmp_blocksize : 1 << 16;
  int hash_size = 0;
  status = ippsEncodeLZ4HashTableGetSize_8u(&hash_size);
  if (status != ippStsNoErr) {
      BLOSC_TRACE_ERROR("Error in ippsEncodeLZ4HashTableGetSize_8u.");
  }
  Ipp8u *hash_table = ippsMalloc_8u(hash_size);
  status = ippsEncodeLZ4HashTableInit_8u(hash_table, inlen);
  if (status != ippStsNoErr) {
    BLOSC_TRACE_ERROR("Error in ippsEncodeLZ4HashTableInit_8u.");
  }
  thread_context->lz4_hash_table = hash_table;
#endif
  return 0;
}

static struct thread_context*
create_thread_context(blosc2_context* context, int32_t tid) {
  struct thread_context* thread_context;
  thread_context = (struct thread_context*)my_malloc(sizeof(struct thread_context));
  BLOSC_ERROR_NULL(thread_context, NULL);
  int rc = init_thread_context(thread_context, context, tid);
  if (rc < 0) {
    return NULL;
  }
  return thread_context;
}

/* free members of thread_context, but not thread_context itself */
static void destroy_thread_context(struct thread_context* thread_context) {
  my_free(thread_context->tmp);
#if defined(HAVE_ZSTD)
  if (thread_context->zstd_cctx != NULL) {
    ZSTD_freeCCtx(thread_context->zstd_cctx);
  }
  if (thread_context->zstd_dctx != NULL) {
    ZSTD_freeDCtx(thread_context->zstd_dctx);
  }
#endif
#ifdef HAVE_IPP
  if (thread_context->lz4_hash_table != NULL) {
    ippsFree(thread_context->lz4_hash_table);
  }
#endif
}

void free_thread_context(struct thread_context* thread_context) {
  destroy_thread_context(thread_context);
  my_free(thread_context);
}


int check_nthreads(blosc2_context* context) {
  if (context->nthreads <= 0) {
    BLOSC_TRACE_ERROR("nthreads must be a positive integer.");
    return BLOSC2_ERROR_INVALID_PARAM;
  }

  if (context->new_nthreads != context->nthreads) {
    if (context->nthreads > 1) {
      release_threadpool(context);
    }
    context->nthreads = context->new_nthreads;
  }
  if (context->new_nthreads > 1 && context->threads_started == 0) {
    init_threadpool(context);
  }

  return context->nthreads;
}

/* Do the compression or decompression of the buffer depending on the
   global params. */
static int do_job(blosc2_context* context) {
  int32_t ntbytes;

  /* Set sentinels */
  context->dref_not_init = 1;

  /* Check whether we need to restart threads */
  check_nthreads(context);

  /* Run the serial version when nthreads is 1 or when the buffers are
     not larger than blocksize */
  if (context->nthreads == 1 || (context->sourcesize / context->blocksize) <= 1) {
    /* The context for this 'thread' has no been initialized yet */
    if (context->serial_context == NULL) {
      context->serial_context = create_thread_context(context, 0);
    }
    else if (context->blocksize != context->serial_context->tmp_blocksize) {
      free_thread_context(context->serial_context);
      context->serial_context = create_thread_context(context, 0);
    }
    BLOSC_ERROR_NULL(context->serial_context, BLOSC2_ERROR_THREAD_CREATE);
    ntbytes = serial_blosc(context->serial_context);
  }
  else {
    ntbytes = parallel_blosc(context);
  }

  return ntbytes;
}


static int initialize_context_compression(
  blosc2_context* context, const void* src, int32_t srcsize, void* dest,
  int32_t destsize, int clevel, uint8_t const *filters,
  uint8_t const *filters_meta, int32_t typesize, int compressor,
  int32_t blocksize, int16_t new_nthreads, int16_t nthreads,
  blosc2_btune *udbtune, void *btune_config,
  blosc2_schunk* schunk) {

  /* Set parameters */
  context->do_compress = 1;
  context->src = (const uint8_t*)src;
  context->srcsize = srcsize;
  context->dest = (uint8_t*)dest;
  context->output_bytes = 0;
  context->destsize = destsize;
  context->sourcesize = srcsize;
  context->typesize = (int32_t)typesize;
  context->filter_flags = filters_to_flags(filters);
  for (int i = 0; i < BLOSC2_MAX_FILTERS; i++) {
    context->filters[i] = filters[i];
    context->filters_meta[i] = filters_meta[i];
  }
  context->compcode = compressor;
  context->nthreads = nthreads;
  context->new_nthreads = new_nthreads;
  context->end_threads = 0;
  context->clevel = clevel;
  context->schunk = schunk;
  context->btune = btune_config;
  context->udbtune = udbtune;
  /* Tune some compression parameters */
  context->blocksize = (int32_t)blocksize;
  if (context->btune != NULL) {
    context->udbtune->btune_next_cparams(context);
  } else {
    context->udbtune->btune_next_blocksize(context);
  }

  char* envvar = getenv("BLOSC_WARN");
  int warnlvl = 0;
  if (envvar != NULL) {
    warnlvl = strtol(envvar, NULL, 10);
  }

  /* Check buffer size limits */
  if (srcsize > BLOSC_MAX_BUFFERSIZE) {
    if (warnlvl > 0) {
      BLOSC_TRACE_ERROR("Input buffer size cannot exceed %d bytes.",
                        BLOSC_MAX_BUFFERSIZE);
    }
    return 0;
  }

  if (destsize < BLOSC_MAX_OVERHEAD) {
    if (warnlvl > 0) {
      BLOSC_TRACE_ERROR("Output buffer size should be larger than %d bytes.",
                        BLOSC_MAX_OVERHEAD);
    }
    return 0;
  }

  /* Compression level */
  if (clevel < 0 || clevel > 9) {
    /* If clevel not in 0..9, print an error */
    BLOSC_TRACE_ERROR("`clevel` parameter must be between 0 and 9!.");
    return BLOSC2_ERROR_CODEC_PARAM;
  }

  /* Check typesize limits */
  if (context->typesize > BLOSC_MAX_TYPESIZE) {
    /* If typesize is too large, treat buffer as an 1-byte stream. */
    context->typesize = 1;
  }

  blosc2_calculate_blocks(context);

  return 1;
}


static int initialize_context_decompression(blosc2_context* context, blosc_header* header, const void* src,
                                            int32_t srcsize, void* dest, int32_t destsize) {
  int32_t bstarts_end;

  context->do_compress = 0;
  context->src = (const uint8_t*)src;
  context->srcsize = srcsize;
  context->dest = (uint8_t*)dest;
  context->destsize = destsize;
  context->output_bytes = 0;
  context->end_threads = 0;

  int rc = blosc2_initialize_context_from_header(context, header);
  if (rc < 0) {
    return rc;
  }

  /* Check that we have enough space to decompress */
  if (context->sourcesize > (int32_t)context->destsize) {
    return BLOSC2_ERROR_WRITE_BUFFER;
  }

  if (context->block_maskout != NULL && context->block_maskout_nitems != context->nblocks) {
    BLOSC_TRACE_ERROR("The number of items in block_maskout (%d) must match the number"
                      " of blocks in chunk (%d).",
                      context->block_maskout_nitems, context->nblocks);
    return BLOSC2_ERROR_DATA;
  }

  context->special_type = (header->blosc2_flags >> 4) & BLOSC2_SPECIAL_MASK;
  if (context->special_type > BLOSC2_SPECIAL_LASTID) {
    BLOSC_TRACE_ERROR("Unknown special values ID (%d) ",
                      context->special_type);
    return BLOSC2_ERROR_DATA;
  }

  int memcpyed = (context->header_flags & (uint8_t) BLOSC_MEMCPYED);
  if (memcpyed && (header->cbytes != header->nbytes + context->header_overhead)) {
    BLOSC_TRACE_ERROR("Wrong header info for this memcpyed chunk");
    return BLOSC2_ERROR_DATA;
  }

  if ((header->nbytes == 0) && (header->cbytes == context->header_overhead) &&
      !context->special_type) {
    // A compressed buffer with only a header can only contain a zero-length buffer
    return 0;
  }

  context->bstarts = (int32_t *) (context->src + context->header_overhead);
  bstarts_end = context->header_overhead;
  if (!context->special_type && !memcpyed) {
    /* If chunk is not special or a memcpyed, we do have a bstarts section */
    bstarts_end = context->header_overhead + (context->nblocks * sizeof(int32_t));
  }

  if (srcsize < bstarts_end) {
    BLOSC_TRACE_ERROR("`bstarts` exceeds length of source buffer.");
    return BLOSC2_ERROR_READ_BUFFER;
  }
  srcsize -= bstarts_end;

  /* Read optional dictionary if flag set */
  if (context->blosc2_flags & BLOSC2_USEDICT) {
#if defined(HAVE_ZSTD)
    context->use_dict = 1;
    if (context->dict_ddict != NULL) {
      // Free the existing dictionary (probably from another chunk)
      ZSTD_freeDDict(context->dict_ddict);
    }
    // The trained dictionary is after the bstarts block
    if (srcsize < (signed)sizeof(int32_t)) {
      BLOSC_TRACE_ERROR("Not enough space to read size of dictionary.");
      return BLOSC2_ERROR_READ_BUFFER;
    }
    srcsize -= sizeof(int32_t);
    // Read dictionary size
    context->dict_size = (size_t)sw32_(context->src + bstarts_end);
    if (context->dict_size <= 0 || context->dict_size > BLOSC2_MAXDICTSIZE) {
      BLOSC_TRACE_ERROR("Dictionary size is smaller than minimum or larger than maximum allowed.");
      return BLOSC2_ERROR_CODEC_DICT;
    }
    if (srcsize < (int32_t)context->dict_size) {
      BLOSC_TRACE_ERROR("Not enough space to read entire dictionary.");
      return BLOSC2_ERROR_READ_BUFFER;
    }
    srcsize -= context->dict_size;
    // Read dictionary
    context->dict_buffer = (void*)(context->src + bstarts_end + sizeof(int32_t));
    context->dict_ddict = ZSTD_createDDict(context->dict_buffer, context->dict_size);
#endif   // HAVE_ZSTD
  }

  return 0;
}

static int write_compression_header(blosc2_context* context, bool extended_header) {
  blosc_header header;
  int dont_split;
  int dict_training = context->use_dict && (context->dict_cdict == NULL);

  context->header_flags = 0;

  if (context->clevel == 0) {
    /* Compression level 0 means buffer to be memcpy'ed */
    context->header_flags |= (uint8_t)BLOSC_MEMCPYED;
  }
  if (context->sourcesize < BLOSC_MIN_BUFFERSIZE) {
    /* Buffer is too small.  Try memcpy'ing. */
    context->header_flags |= (uint8_t)BLOSC_MEMCPYED;
  }

  bool memcpyed = context->header_flags & (uint8_t)BLOSC_MEMCPYED;
  if (extended_header) {
    /* Indicate that we are building an extended header */
    context->header_overhead = BLOSC_EXTENDED_HEADER_LENGTH;
    context->header_flags |= (BLOSC_DOSHUFFLE | BLOSC_DOBITSHUFFLE);
    /* Store filter pipeline info at the end of the header */
    if (dict_training || memcpyed) {
      context->bstarts = NULL;
      context->output_bytes = context->header_overhead;
    } else {
      context->bstarts = (int32_t*)(context->dest + context->header_overhead);
      context->output_bytes = context->header_overhead + sizeof(int32_t) * context->nblocks;
    }
  } else {
    // Regular header
    context->header_overhead = BLOSC_MIN_HEADER_LENGTH;
    if (memcpyed) {
      context->bstarts = NULL;
      context->output_bytes = context->header_overhead;
    } else {
      context->bstarts = (int32_t *) (context->dest + context->header_overhead);
      context->output_bytes = context->header_overhead + sizeof(int32_t) * context->nblocks;
    }
  }

  // when memcpyed bit is set, there is no point in dealing with others
  if (!memcpyed) {
    if (context->filter_flags & BLOSC_DOSHUFFLE) {
      /* Byte-shuffle is active */
      context->header_flags |= BLOSC_DOSHUFFLE;
    }

    if (context->filter_flags & BLOSC_DOBITSHUFFLE) {
      /* Bit-shuffle is active */
      context->header_flags |= BLOSC_DOBITSHUFFLE;
    }

    if (context->filter_flags & BLOSC_DODELTA) {
      /* Delta is active */
      context->header_flags |= BLOSC_DODELTA;
    }

    dont_split = !split_block(context, context->typesize,
                              context->blocksize, extended_header);

    /* dont_split is in bit 4 */
    context->header_flags |= dont_split << 4;
    /* codec starts at bit 5 */
    uint8_t compformat = compcode_to_compformat(context->compcode);
    context->header_flags |= compformat << 5;
  }

  // Create blosc header and store to dest
  blosc2_intialize_header_from_context(context, &header, extended_header);

  memcpy(context->dest, &header, (extended_header) ?
    BLOSC_EXTENDED_HEADER_LENGTH : BLOSC_MIN_HEADER_LENGTH);

  return 1;
}


int blosc_compress_context(blosc2_context* context) {
  int ntbytes = 0;
  blosc_timestamp_t last, current;
  bool memcpyed = context->header_flags & (uint8_t)BLOSC_MEMCPYED;

  blosc_set_timestamp(&last);

  if (!memcpyed) {
    /* Do the actual compression */
    ntbytes = do_job(context);
    if (ntbytes < 0) {
      return ntbytes;
    }
    if (ntbytes == 0) {
      // Try out with a memcpy later on (last chance for fitting src buffer in dest).
      context->header_flags |= (uint8_t)BLOSC_MEMCPYED;
      memcpyed = true;
    }
  }

  if (memcpyed) {
    if (context->sourcesize + context->header_overhead > context->destsize) {
      /* We are exceeding maximum output size */
      ntbytes = 0;
    }
    else {
      context->output_bytes = context->header_overhead;
      ntbytes = do_job(context);
      if (ntbytes < 0) {
        return ntbytes;
      }
      // Success!  update the memcpy bit in header
      context->dest[BLOSC2_CHUNK_FLAGS] = context->header_flags;
      // and clear the memcpy bit in context (for next reuse)
      context->header_flags &= ~(uint8_t)BLOSC_MEMCPYED;
    }
  }
  else {
    // Check whether we have a run for the whole chunk
    int start_csizes = context->header_overhead + 4 * context->nblocks;
    int dont_split = (context->header_flags & 0x10) >> 4;
    int nstreams = context->nblocks;
    if (!dont_split) {
      // When splitting, the number of streams is computed differently
      if (context->leftover) {
        nstreams = (context->nblocks - 1) * context->typesize + 1;
      }
      else {
        nstreams *= context->typesize;
      }
    }
    if (ntbytes == start_csizes + nstreams * sizeof(int32_t)) {
      // The streams are all zero runs (by construction).  Encode it...
      context->dest[BLOSC2_CHUNK_BLOSC2_FLAGS] |= BLOSC2_SPECIAL_ZERO << 4;
      // ...and assign the new chunk length
      ntbytes = context->header_overhead;
    }
  }

  /* Set the number of compressed bytes in header */
  _sw32(context->dest + BLOSC2_CHUNK_CBYTES, ntbytes);

  /* Set the number of bytes in dest buffer (might be useful for btune) */
  context->destsize = ntbytes;

  if (context->btune != NULL) {
    blosc_set_timestamp(&current);
    double ctime = blosc_elapsed_secs(last, current);
    context->udbtune->btune_update(context, ctime);
  }

  return ntbytes;
}


/* The public secure routine for compression with context. */
int blosc2_compress_ctx(blosc2_context* context, const void* src, int32_t srcsize,
                        void* dest, int32_t destsize) {
  int error, cbytes;

  if (context->do_compress != 1) {
    BLOSC_TRACE_ERROR("Context is not meant for compression.  Giving up.");
    return BLOSC2_ERROR_INVALID_PARAM;
  }

  error = initialize_context_compression(
    context, src, srcsize, dest, destsize,
    context->clevel, context->filters, context->filters_meta,
    context->typesize, context->compcode, context->blocksize,
    context->new_nthreads, context->nthreads,
    context->udbtune, context->btune, context->schunk);
  if (error <= 0) {
    return error;
  }

  /* Write the extended header */
  error = write_compression_header(context, true);
  if (error < 0) {
    return error;
  }

  cbytes = blosc_compress_context(context);
  if (cbytes < 0) {
    return cbytes;
  }

  if (context->use_dict && context->dict_cdict == NULL) {

    if (context->compcode != BLOSC_ZSTD) {
      const char* compname;
      compname = clibcode_to_clibname(context->compcode);
      BLOSC_TRACE_ERROR("Codec %s does not support dicts.  Giving up.",
                        compname);
      return BLOSC2_ERROR_CODEC_DICT;
    }

#ifdef HAVE_ZSTD
    // Build the dictionary out of the filters outcome and compress with it
    int32_t dict_maxsize = BLOSC2_MAXDICTSIZE;
    // Do not make the dict more than 5% larger than uncompressed buffer
    if (dict_maxsize > srcsize / 20) {
      dict_maxsize = srcsize / 20;
    }
    void* samples_buffer = context->dest + context->header_overhead;
    unsigned nblocks = 8;  // the minimum that accepts zstd as of 1.4.0
    unsigned sample_fraction = 1;  // 1 allows to use most of the chunk for training
    size_t sample_size = context->sourcesize / nblocks / sample_fraction;

    // Populate the samples sizes for training the dictionary
    size_t* samples_sizes = malloc(nblocks * sizeof(void*));
    BLOSC_ERROR_NULL(samples_sizes, BLOSC2_ERROR_MEMORY_ALLOC);
    for (size_t i = 0; i < nblocks; i++) {
      samples_sizes[i] = sample_size;
    }

    // Train from samples
    void* dict_buffer = malloc(dict_maxsize);
    BLOSC_ERROR_NULL(dict_buffer, BLOSC2_ERROR_MEMORY_ALLOC);
    int32_t dict_actual_size = (int32_t)ZDICT_trainFromBuffer(dict_buffer, dict_maxsize, samples_buffer, samples_sizes, nblocks);

    // TODO: experiment with parameters of low-level fast cover algorithm
    // Note that this API is still unstable.  See: https://github.com/facebook/zstd/issues/1599
    // ZDICT_fastCover_params_t fast_cover_params;
    // memset(&fast_cover_params, 0, sizeof(fast_cover_params));
    // fast_cover_params.d = nblocks;
    // fast_cover_params.steps = 4;
    // fast_cover_params.zParams.compressionLevel = context->clevel;
    //size_t dict_actual_size = ZDICT_optimizeTrainFromBuffer_fastCover(dict_buffer, dict_maxsize, samples_buffer, samples_sizes, nblocks, &fast_cover_params);

    if (ZDICT_isError(dict_actual_size) != ZSTD_error_no_error) {
      BLOSC_TRACE_ERROR("Error in ZDICT_trainFromBuffer(): '%s'."
                        "  Giving up.", ZDICT_getErrorName(dict_actual_size));
      return BLOSC2_ERROR_CODEC_DICT;
    }
    assert(dict_actual_size > 0);
    free(samples_sizes);

    // Update bytes counter and pointers to bstarts for the new compressed buffer
    context->bstarts = (int32_t*)(context->dest + context->header_overhead);
    context->output_bytes = context->header_overhead + sizeof(int32_t) * context->nblocks;
    /* Write the size of trained dict at the end of bstarts */
    _sw32(context->dest + context->output_bytes, (int32_t)dict_actual_size);
    context->output_bytes += sizeof(int32_t);
    /* Write the trained dict afterwards */
    context->dict_buffer = context->dest + context->output_bytes;
    memcpy(context->dict_buffer, dict_buffer, (unsigned int)dict_actual_size);
    context->dict_cdict = ZSTD_createCDict(dict_buffer, dict_actual_size, 1);  // TODO: use get_accel()
    free(dict_buffer);      // the dictionary is copied in the header now
    context->output_bytes += (int32_t)dict_actual_size;
    context->dict_size = dict_actual_size;

    /* Compress with dict */
    cbytes = blosc_compress_context(context);

    // Invalidate the dictionary for compressing other chunks using the same context
    context->dict_buffer = NULL;
    ZSTD_freeCDict(context->dict_cdict);
    context->dict_cdict = NULL;
#endif  // HAVE_ZSTD
  }

  return cbytes;
}


void build_filters(const int doshuffle, const int delta,
                   const size_t typesize, uint8_t* filters) {

  /* Fill the end part of the filter pipeline */
  if ((doshuffle == BLOSC_SHUFFLE) && (typesize > 1))
    filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_SHUFFLE;
  if (doshuffle == BLOSC_BITSHUFFLE)
    filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_BITSHUFFLE;
  if (delta)
    filters[BLOSC2_MAX_FILTERS - 2] = BLOSC_DELTA;
}

/* The public secure routine for compression. */
int blosc2_compress(int clevel, int doshuffle, int32_t typesize,
                    const void* src, int32_t srcsize, void* dest, int32_t destsize) {
  int error;
  int result;
  char* envvar;

  /* Check whether the library should be initialized */
  if (!g_initlib) blosc_init();

  /* Check for a BLOSC_CLEVEL environment variable */
  envvar = getenv("BLOSC_CLEVEL");
  if (envvar != NULL) {
    long value;
    value = strtol(envvar, NULL, 10);
    if ((value != EINVAL) && (value >= 0)) {
      clevel = (int)value;
    }
  }

  /* Check for a BLOSC_SHUFFLE environment variable */
  envvar = getenv("BLOSC_SHUFFLE");
  if (envvar != NULL) {
    if (strcmp(envvar, "NOSHUFFLE") == 0) {
      doshuffle = BLOSC_NOSHUFFLE;
    }
    if (strcmp(envvar, "SHUFFLE") == 0) {
      doshuffle = BLOSC_SHUFFLE;
    }
    if (strcmp(envvar, "BITSHUFFLE") == 0) {
      doshuffle = BLOSC_BITSHUFFLE;
    }
  }

  /* Check for a BLOSC_DELTA environment variable */
  envvar = getenv("BLOSC_DELTA");
  if (envvar != NULL) {
    if (strcmp(envvar, "1") == 0) {
      blosc_set_delta(1);
    } else {
      blosc_set_delta(0);
    }
  }

  /* Check for a BLOSC_TYPESIZE environment variable */
  envvar = getenv("BLOSC_TYPESIZE");
  if (envvar != NULL) {
    long value;
    value = strtol(envvar, NULL, 10);
    if ((value != EINVAL) && (value > 0)) {
      typesize = (size_t)value;
    }
  }

  /* Check for a BLOSC_COMPRESSOR environment variable */
  envvar = getenv("BLOSC_COMPRESSOR");
  if (envvar != NULL) {
    result = blosc_set_compressor(envvar);
    if (result < 0) { return result; }
  }

  /* Check for a BLOSC_COMPRESSOR environment variable */
  envvar = getenv("BLOSC_BLOCKSIZE");
  if (envvar != NULL) {
    long blocksize;
    blocksize = strtol(envvar, NULL, 10);
    if ((blocksize != EINVAL) && (blocksize > 0)) {
      blosc_set_blocksize((size_t)blocksize);
    }
  }

  /* Check for a BLOSC_NTHREADS environment variable */
  envvar = getenv("BLOSC_NTHREADS");
  if (envvar != NULL) {
    long nthreads;
    nthreads = strtol(envvar, NULL, 10);
    if ((nthreads != EINVAL) && (nthreads > 0)) {
      result = blosc_set_nthreads((int)nthreads);
      if (result < 0) { return result; }
    }
  }

  /* Check for a BLOSC_NOLOCK environment variable.  It is important
     that this should be the last env var so that it can take the
     previous ones into account */
  envvar = getenv("BLOSC_NOLOCK");
  if (envvar != NULL) {
    // TODO: here is the only place that returns an extended header from
    //   a blosc_compress() call.  This should probably be fixed.
    const char *compname;
    blosc2_context *cctx;
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;

    blosc_compcode_to_compname(g_compressor, &compname);
    /* Create a context for compression */
    build_filters(doshuffle, g_delta, typesize, cparams.filters);
    // TODO: cparams can be shared in a multithreaded environment.  do a copy!
    cparams.typesize = (uint8_t)typesize;
    cparams.compcode = (uint8_t)g_compressor;
    cparams.clevel = (uint8_t)clevel;
    cparams.nthreads = g_nthreads;
    cctx = blosc2_create_cctx(cparams);
    /* Do the actual compression */
    result = blosc2_compress_ctx(cctx, src, srcsize, dest, destsize);
    /* Release context resources */
    blosc2_free_ctx(cctx);
    return result;
  }

  pthread_mutex_lock(&global_comp_mutex);

  /* Initialize a context compression */
  uint8_t* filters = calloc(1, BLOSC2_MAX_FILTERS);
  BLOSC_ERROR_NULL(filters, BLOSC2_ERROR_MEMORY_ALLOC);
  uint8_t* filters_meta = calloc(1, BLOSC2_MAX_FILTERS);
  BLOSC_ERROR_NULL(filters_meta, BLOSC2_ERROR_MEMORY_ALLOC);
  build_filters(doshuffle, g_delta, typesize, filters);
  error = initialize_context_compression(
    g_global_context, src, srcsize, dest, destsize, clevel, filters,
    filters_meta, (int32_t)typesize, g_compressor, g_force_blocksize, g_nthreads, g_nthreads,
    &BTUNE_DEFAULTS, NULL, g_schunk);
  free(filters);
  free(filters_meta);
  if (error <= 0) {
    pthread_mutex_unlock(&global_comp_mutex);
    return error;
  }

  envvar = getenv("BLOSC_BLOSC1_COMPAT");
  if (envvar != NULL) {
    /* Write chunk header without extended header (Blosc1 compatibility mode) */
    error = write_compression_header(g_global_context, false);
  }
  else {
    error = write_compression_header(g_global_context, true);
  }
  if (error < 0) {
    pthread_mutex_unlock(&global_comp_mutex);
    return error;
  }

  result = blosc_compress_context(g_global_context);

  pthread_mutex_unlock(&global_comp_mutex);

  return result;
}


/* The public routine for compression. */
int blosc_compress(int clevel, int doshuffle, size_t typesize, size_t nbytes,
                   const void* src, void* dest, size_t destsize) {
  return blosc2_compress(clevel, doshuffle, (int32_t)typesize, src, (int32_t)nbytes, dest, (int32_t)destsize);
}



int blosc_run_decompression_with_context(blosc2_context* context, const void* src, int32_t srcsize,
                                         void* dest, int32_t destsize) {
  blosc_header header;
  int32_t ntbytes;
  uint8_t* _src = (uint8_t*)src;
  int rc;

  rc = read_chunk_header(src, srcsize, true, &header);
  if (rc < 0) {
    return rc;
  }

  if (header.nbytes > destsize) {
    // Not enough space for writing into the destination
    return BLOSC2_ERROR_WRITE_BUFFER;
  }

  rc = initialize_context_decompression(context, &header, src, srcsize, dest, destsize);
  if (rc < 0) {
    return rc;
  }

  /* Do the actual decompression */
  ntbytes = do_job(context);
  if (ntbytes < 0) {
    return ntbytes;
  }

  assert(ntbytes <= (int32_t)destsize);
  return ntbytes;
}


/* The public secure routine for decompression with context. */
int blosc2_decompress_ctx(blosc2_context* context, const void* src, int32_t srcsize,
                          void* dest, int32_t destsize) {
  int result;

  if (context->do_compress != 0) {
    BLOSC_TRACE_ERROR("Context is not meant for decompression.  Giving up.");
    return BLOSC2_ERROR_INVALID_PARAM;
  }

  result = blosc_run_decompression_with_context(context, src, srcsize, dest, destsize);

  // Reset a possible block_maskout
  if (context->block_maskout != NULL) {
    free(context->block_maskout);
    context->block_maskout = NULL;
  }
  context->block_maskout_nitems = 0;

  return result;
}


/* The public secure routine for decompression. */
int blosc2_decompress(const void* src, int32_t srcsize, void* dest, int32_t destsize) {
  int result;
  char* envvar;
  long nthreads;
  blosc2_context *dctx;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;

  /* Check whether the library should be initialized */
  if (!g_initlib) blosc_init();

  /* Check for a BLOSC_NTHREADS environment variable */
  envvar = getenv("BLOSC_NTHREADS");
  if (envvar != NULL) {
    nthreads = strtol(envvar, NULL, 10);
    if ((nthreads != EINVAL) && (nthreads > 0)) {
      result = blosc_set_nthreads((int)nthreads);
      if (result < 0) { return result; }
    }
  }

  /* Check for a BLOSC_NOLOCK environment variable.  It is important
     that this should be the last env var so that it can take the
     previous ones into account */
  envvar = getenv("BLOSC_NOLOCK");
  if (envvar != NULL) {
    dparams.nthreads = g_nthreads;
    dctx = blosc2_create_dctx(dparams);
    result = blosc2_decompress_ctx(dctx, src, srcsize, dest, destsize);
    blosc2_free_ctx(dctx);
    return result;
  }

  pthread_mutex_lock(&global_comp_mutex);

  result = blosc_run_decompression_with_context(
          g_global_context, src, srcsize, dest, destsize);

  pthread_mutex_unlock(&global_comp_mutex);

  return result;
}


/* The public routine for decompression. */
int blosc_decompress(const void* src, void* dest, size_t destsize) {
  return blosc2_decompress(src, INT32_MAX, dest, (int32_t)destsize);
}


/* Specific routine optimized for decompression a small number of
   items out of a compressed chunk.  This does not use threads because
   it would affect negatively to performance. */
int _blosc_getitem(blosc2_context* context, blosc_header* header, const void* src, int32_t srcsize,
                   int start, int nitems, void* dest, int32_t destsize) {
  uint8_t* _src = (uint8_t*)(src);  /* current pos for source buffer */
  uint8_t* _dest = (uint8_t*)(dest);
  int32_t ntbytes = 0;              /* the number of uncompressed bytes */
  int32_t bsize, bsize2, ebsize, leftoverblock;
  int32_t cbytes;
  int32_t startb, stopb;
  int32_t stop = start + nitems;
  int j, rc;

  if (nitems == 0) {
    // We have nothing to do
    return 0;
  }
  if (nitems * header->typesize > destsize) {
    BLOSC_TRACE_ERROR("`nitems`*`typesize` out of dest bounds.");
    return BLOSC2_ERROR_WRITE_BUFFER;
  }

  context->bstarts = (int32_t*)(_src + context->header_overhead);

  /* Check region boundaries */
  if ((start < 0) || (start * header->typesize > header->nbytes)) {
    BLOSC_TRACE_ERROR("`start` out of bounds.");
    return BLOSC2_ERROR_INVALID_PARAM;
  }

  if ((stop < 0) || (stop * header->typesize > header->nbytes)) {
    BLOSC_TRACE_ERROR("`start`+`nitems` out of bounds.");
    return BLOSC2_ERROR_INVALID_PARAM;
  }

  if (!context->special_type &&
      (_src + srcsize < (uint8_t *)(context->bstarts + context->nblocks))) {
    BLOSC_TRACE_ERROR("`bstarts` out of bounds.");
    return BLOSC2_ERROR_READ_BUFFER;
  }

  bool memcpyed = header->flags & (uint8_t)BLOSC_MEMCPYED;
  if (context->special_type) {
    // Fake a runlen as if its a memcpyed chunk
    memcpyed = true;
  }

  bool is_lazy = ((context->header_overhead == BLOSC_EXTENDED_HEADER_LENGTH) &&
                  (context->blosc2_flags & 0x08u) && !context->special_type);
  if (memcpyed && !is_lazy && !context->postfilter) {
    // Short-circuit for (non-lazy) memcpyed or special values
    ntbytes = nitems * header->typesize;
    switch (context->special_type) {
      case BLOSC2_SPECIAL_VALUE:
        // All repeated values
        rc = set_values(context->typesize, _src, _dest, ntbytes);
        if (rc < 0) {
          BLOSC_TRACE_ERROR("set_values failed");
          return BLOSC2_ERROR_DATA;
        }
        break;
      case BLOSC2_SPECIAL_NAN:
        rc = set_nans(context->typesize, _dest, ntbytes);
        if (rc < 0) {
          BLOSC_TRACE_ERROR("set_nans failed");
          return BLOSC2_ERROR_DATA;
        }
        break;
      case BLOSC2_SPECIAL_ZERO:
        memset(_dest, 0, ntbytes);
        break;
      case BLOSC2_SPECIAL_UNINIT:
        // We do nothing here
        break;
      case BLOSC2_NO_SPECIAL:
        _src += context->header_overhead + start * context->typesize;
        memcpy(_dest, _src, ntbytes);
        break;
      default:
        BLOSC_TRACE_ERROR("Unhandled special value case");
        return -1;
    }
    return ntbytes;
  }

  ebsize = header->blocksize + header->typesize * (signed)sizeof(int32_t);
  struct thread_context* scontext = context->serial_context;
  /* Resize the temporaries in serial context if needed */
  if (header->blocksize > scontext->tmp_blocksize) {
    my_free(scontext->tmp);
    scontext->tmp_nbytes = (size_t)4 * ebsize;
    scontext->tmp = my_malloc(scontext->tmp_nbytes);
    BLOSC_ERROR_NULL(scontext->tmp, BLOSC2_ERROR_MEMORY_ALLOC);
    scontext->tmp2 = scontext->tmp + ebsize;
    scontext->tmp3 = scontext->tmp2 + ebsize;
    scontext->tmp4 = scontext->tmp3 + ebsize;
    scontext->tmp_blocksize = (int32_t)header->blocksize;
  }

  for (j = 0; j < context->nblocks; j++) {
    bsize = header->blocksize;
    leftoverblock = 0;
    if ((j == context->nblocks - 1) && (context->leftover > 0)) {
      bsize = context->leftover;
      leftoverblock = 1;
    }

    /* Compute start & stop for each block */
    startb = start * header->typesize - j * header->blocksize;
    stopb = stop * header->typesize - j * header->blocksize;
    if (stopb <= 0) {
      // We can exit as soon as this block is beyond stop
      break;
    }
    if (startb >= header->blocksize) {
      continue;
    }
    if (startb < 0) {
      startb = 0;
    }
    if (stopb > header->blocksize) {
      stopb = header->blocksize;
    }
    bsize2 = stopb - startb;

    /* Do the actual data copy */
    // Regular decompression.  Put results in tmp2.
    // If the block is aligned and the worst case fits in destination, let's avoid a copy
    bool get_single_block = ((startb == 0) && (bsize == nitems * header->typesize));
    uint8_t* tmp2 = get_single_block ? dest : scontext->tmp2;

    // If memcpyed we don't have a bstarts section (because it is not needed)
    int32_t src_offset = memcpyed ?
      context->header_overhead + j * bsize : sw32_(context->bstarts + j);

    int cbytes = blosc_d(context->serial_context, bsize, leftoverblock, memcpyed,
                         src, srcsize, src_offset, j,
                         tmp2, 0, scontext->tmp, scontext->tmp3);
    if (cbytes < 0) {
      ntbytes = cbytes;
      break;
    }
    if (!get_single_block) {
      /* Copy to destination */
      memcpy((uint8_t *) dest + ntbytes, tmp2 + startb, (unsigned int) bsize2);
    }
    ntbytes += bsize2;
  }

  return ntbytes;
}

int blosc2_getitem(const void* src, int32_t srcsize, int start, int nitems, void* dest, int32_t destsize) {
  blosc2_context context;
  int result;

  /* Minimally populate the context */
  memset(&context, 0, sizeof(blosc2_context));

  context.schunk = g_schunk;
  context.nthreads = 1;  // force a serial decompression; fixes #95

  /* Call the actual getitem function */
  result = blosc2_getitem_ctx(&context, src, srcsize, start, nitems, dest, destsize);

  /* Release resources */
  if (context.serial_context != NULL) {
    free_thread_context(context.serial_context);
  }
  return result;
}

/* Specific routine optimized for decompression a small number of
   items out of a compressed chunk.  Public non-contextual API. */
int blosc_getitem(const void* src, int start, int nitems, void* dest) {
  return blosc2_getitem(src, INT32_MAX, start, nitems, dest, INT32_MAX);
}

int blosc2_getitem_ctx(blosc2_context* context, const void* src, int32_t srcsize,
    int start, int nitems, void* dest, int32_t destsize) {
  blosc_header header;
  int result;

  /* Minimally populate the context */
  result = read_chunk_header((uint8_t *) src, srcsize, true, &header);
  if (result < 0) {
    return result;
  }

  context->src = src;
  context->srcsize = srcsize;
  context->dest = dest;
  context->destsize = destsize;

  result = blosc2_initialize_context_from_header(context, &header);
  if (result < 0) {
    return result;
  }

  if (context->serial_context == NULL) {
    context->serial_context = create_thread_context(context, 0);
  }
  BLOSC_ERROR_NULL(context->serial_context, BLOSC2_ERROR_THREAD_CREATE);
  /* Call the actual getitem function */
  result = _blosc_getitem(context, &header, src, srcsize, start, nitems, dest, destsize);

  return result;
}

/* execute single compression/decompression job for a single thread_context */
static void t_blosc_do_job(void *ctxt)
{
  struct thread_context* thcontext = (struct thread_context*)ctxt;
  blosc2_context* context = thcontext->parent_context;
  int32_t cbytes;
  int32_t ntdest;
  int32_t tblocks;               /* number of blocks per thread */
  int32_t tblock;                /* limit block on a thread */
  int32_t nblock_;              /* private copy of nblock */
  int32_t bsize;
  int32_t leftoverblock;
  /* Parameters for threads */
  int32_t blocksize;
  int32_t ebsize;
  int32_t srcsize;
  bool compress = context->do_compress != 0;
  int32_t maxbytes;
  int32_t nblocks;
  int32_t leftover;
  int32_t leftover2;
  int32_t* bstarts;
  const uint8_t* src;
  uint8_t* dest;
  uint8_t* tmp;
  uint8_t* tmp2;
  uint8_t* tmp3;

  /* Get parameters for this thread before entering the main loop */
  blocksize = context->blocksize;
  ebsize = blocksize + context->typesize * sizeof(int32_t);
  maxbytes = context->destsize;
  nblocks = context->nblocks;
  leftover = context->leftover;
  bstarts = context->bstarts;
  src = context->src;
  srcsize = context->srcsize;
  dest = context->dest;

  /* Resize the temporaries if needed */
  if (blocksize > thcontext->tmp_blocksize) {
    my_free(thcontext->tmp);
    thcontext->tmp_nbytes = (size_t) 4 * ebsize;
    thcontext->tmp = my_malloc(thcontext->tmp_nbytes);
    thcontext->tmp2 = thcontext->tmp + ebsize;
    thcontext->tmp3 = thcontext->tmp2 + ebsize;
    thcontext->tmp4 = thcontext->tmp3 + ebsize;
    thcontext->tmp_blocksize = blocksize;
  }

  tmp = thcontext->tmp;
  tmp2 = thcontext->tmp2;
  tmp3 = thcontext->tmp3;

  // Determine whether we can do a static distribution of workload among different threads
  bool memcpyed = context->header_flags & (uint8_t)BLOSC_MEMCPYED;
  if (!context->do_compress && context->special_type) {
    // Fake a runlen as if its a memcpyed chunk
    memcpyed = true;
  }

  bool static_schedule = (!compress || memcpyed) && context->block_maskout == NULL;
  if (static_schedule) {
      /* Blocks per thread */
      tblocks = nblocks / context->nthreads;
      leftover2 = nblocks % context->nthreads;
      tblocks = (leftover2 > 0) ? tblocks + 1 : tblocks;
      nblock_ = thcontext->tid * tblocks;
      tblock = nblock_ + tblocks;
      if (tblock > nblocks) {
          tblock = nblocks;
      }
  }
  else {
    // Use dynamic schedule via a queue.  Get the next block.
    pthread_mutex_lock(&context->count_mutex);
    context->thread_nblock++;
    nblock_ = context->thread_nblock;
    pthread_mutex_unlock(&context->count_mutex);
    tblock = nblocks;
  }

  /* Loop over blocks */
  leftoverblock = 0;
  while ((nblock_ < tblock) && (context->thread_giveup_code > 0)) {
    bsize = blocksize;
    if (nblock_ == (nblocks - 1) && (leftover > 0)) {
      bsize = leftover;
      leftoverblock = 1;
    }
    if (compress) {
      if (memcpyed) {
        if (!context->prefilter) {
          /* We want to memcpy only */
          memcpy(dest + context->header_overhead + nblock_ * blocksize,
                 src + nblock_ * blocksize, (unsigned int) bsize);
          cbytes = (int32_t) bsize;
        }
        else {
          /* Only the prefilter has to be executed, and this is done in blosc_c().
           * However, no further actions are needed, so we can put the result
           * directly in dest. */
          cbytes = blosc_c(thcontext, bsize, leftoverblock, 0,
                           ebsize, src, nblock_ * blocksize,
                           dest + context->header_overhead + nblock_ * blocksize,
                           tmp, tmp3);
        }
      }
      else {
        /* Regular compression */
        cbytes = blosc_c(thcontext, bsize, leftoverblock, 0,
                          ebsize, src, nblock_ * blocksize, tmp2, tmp, tmp3);
      }
    }
    else {
      /* Regular decompression */
      if (context->special_type == BLOSC2_NO_SPECIAL && !memcpyed &&
          (srcsize < (int32_t)(context->header_overhead + (sizeof(int32_t) * nblocks)))) {
        /* Not enough input to read all `bstarts` */
        cbytes = -1;
      }
      else {
        // If memcpyed we don't have a bstarts section (because it is not needed)
        int32_t src_offset = memcpyed ?
            context->header_overhead + nblock_ * blocksize : sw32_(bstarts + nblock_);
        cbytes = blosc_d(thcontext, bsize, leftoverblock, memcpyed,
                          src, srcsize, src_offset, nblock_,
                          dest, nblock_ * blocksize, tmp, tmp2);
      }
    }

    /* Check whether current thread has to giveup */
    if (context->thread_giveup_code <= 0) {
      break;
    }

    /* Check results for the compressed/decompressed block */
    if (cbytes < 0) {            /* compr/decompr failure */
      /* Set giveup_code error */
      pthread_mutex_lock(&context->count_mutex);
      context->thread_giveup_code = cbytes;
      pthread_mutex_unlock(&context->count_mutex);
      break;
    }

    if (compress && !memcpyed) {
      /* Start critical section */
      pthread_mutex_lock(&context->count_mutex);
      ntdest = context->output_bytes;
      // Note: do not use a typical local dict_training variable here
      // because it is probably cached from previous calls if the number of
      // threads does not change (the usual thing).
      if (!(context->use_dict && context->dict_cdict == NULL)) {
        _sw32(bstarts + nblock_, (int32_t) ntdest);
      }

      if ((cbytes == 0) || (ntdest + cbytes > maxbytes)) {
        context->thread_giveup_code = 0;  /* uncompressible buf */
        pthread_mutex_unlock(&context->count_mutex);
        break;
      }
      context->thread_nblock++;
      nblock_ = context->thread_nblock;
      context->output_bytes += cbytes;
      pthread_mutex_unlock(&context->count_mutex);
      /* End of critical section */

      /* Copy the compressed buffer to destination */
      memcpy(dest + ntdest, tmp2, (unsigned int) cbytes);
    }
    else if (static_schedule) {
      nblock_++;
    }
    else {
      pthread_mutex_lock(&context->count_mutex);
      context->thread_nblock++;
      nblock_ = context->thread_nblock;
      context->output_bytes += cbytes;
      pthread_mutex_unlock(&context->count_mutex);
    }

  } /* closes while (nblock_) */

  if (static_schedule) {
    context->output_bytes = context->sourcesize;
    if (compress) {
      context->output_bytes += context->header_overhead;
    }
  }

}

/* Decompress & unshuffle several blocks in a single thread */
static void* t_blosc(void* ctxt) {
  struct thread_context* thcontext = (struct thread_context*)ctxt;
  blosc2_context* context = thcontext->parent_context;
#ifdef BLOSC_POSIX_BARRIERS
  int rc;
#endif

  while (1) {
    /* Synchronization point for all threads (wait for initialization) */
    WAIT_INIT(NULL, context);

    if (context->end_threads) {
      break;
    }

    t_blosc_do_job(ctxt);

    /* Meeting point for all threads (wait for finalization) */
    WAIT_FINISH(NULL, context);
  }

  /* Cleanup our working space and context */
  free_thread_context(thcontext);

  return (NULL);
}


int init_threadpool(blosc2_context *context) {
  int32_t tid;
  int rc2;

  /* Initialize mutex and condition variable objects */
  pthread_mutex_init(&context->count_mutex, NULL);
  pthread_mutex_init(&context->delta_mutex, NULL);
  pthread_cond_init(&context->delta_cv, NULL);

  /* Set context thread sentinels */
  context->thread_giveup_code = 1;
  context->thread_nblock = -1;

  /* Barrier initialization */
#ifdef BLOSC_POSIX_BARRIERS
  pthread_barrier_init(&context->barr_init, NULL, context->nthreads + 1);
  pthread_barrier_init(&context->barr_finish, NULL, context->nthreads + 1);
#else
  pthread_mutex_init(&context->count_threads_mutex, NULL);
  pthread_cond_init(&context->count_threads_cv, NULL);
  context->count_threads = 0;      /* Reset threads counter */
#endif

  if (threads_callback) {
      /* Create thread contexts to store data for callback threads */
    context->thread_contexts = (struct thread_context *)my_malloc(
            context->nthreads * sizeof(struct thread_context));
    BLOSC_ERROR_NULL(context->thread_contexts, BLOSC2_ERROR_MEMORY_ALLOC);
    for (tid = 0; tid < context->nthreads; tid++)
      init_thread_context(context->thread_contexts + tid, context, tid);
  }
  else {
    #if !defined(_WIN32)
      /* Initialize and set thread detached attribute */
      pthread_attr_init(&context->ct_attr);
      pthread_attr_setdetachstate(&context->ct_attr, PTHREAD_CREATE_JOINABLE);
    #endif

    /* Make space for thread handlers */
    context->threads = (pthread_t*)my_malloc(
            context->nthreads * sizeof(pthread_t));
    BLOSC_ERROR_NULL(context->threads, BLOSC2_ERROR_MEMORY_ALLOC);
    /* Finally, create the threads */
    for (tid = 0; tid < context->nthreads; tid++) {
      /* Create a thread context (will destroy when finished) */
      struct thread_context *thread_context = create_thread_context(context, tid);
      BLOSC_ERROR_NULL(thread_context, BLOSC2_ERROR_THREAD_CREATE);
      #if !defined(_WIN32)
        rc2 = pthread_create(&context->threads[tid], &context->ct_attr, t_blosc,
                            (void*)thread_context);
      #else
        rc2 = pthread_create(&context->threads[tid], NULL, t_blosc,
                            (void *)thread_context);
      #endif
      if (rc2) {
        BLOSC_TRACE_ERROR("Return code from pthread_create() is %d.\n"
                          "\tError detail: %s\n", rc2, strerror(rc2));
        return BLOSC2_ERROR_THREAD_CREATE;
      }
    }
  }

  /* We have now started/initialized the threads */
  context->threads_started = context->nthreads;
  context->new_nthreads = context->nthreads;

  return 0;
}

int16_t blosc_get_nthreads(void)
{
  return g_nthreads;
}

int16_t blosc_set_nthreads(int16_t nthreads_new) {
  int16_t ret = g_nthreads;          /* the previous number of threads */

  /* Check whether the library should be initialized */
  if (!g_initlib) blosc_init();

 if (nthreads_new != ret) {
   g_nthreads = nthreads_new;
   g_global_context->new_nthreads = nthreads_new;
   check_nthreads(g_global_context);
 }

  return ret;
}


const char* blosc_get_compressor(void)
{
  const char* compname;
  blosc_compcode_to_compname(g_compressor, &compname);

  return compname;
}

int blosc_set_compressor(const char* compname) {
  int code = blosc_compname_to_compcode(compname);
  if (code >= BLOSC_LAST_CODEC) {
    BLOSC_TRACE_ERROR("User defined codecs cannot be set here. Use Blosc2 mechanism instead.");
    return -1;
  }
  g_compressor = code;

  /* Check whether the library should be initialized */
  if (!g_initlib) blosc_init();

  return code;
}

void blosc_set_delta(int dodelta) {

  g_delta = dodelta;

  /* Check whether the library should be initialized */
  if (!g_initlib) blosc_init();

}

const char* blosc_list_compressors(void) {
  static int compressors_list_done = 0;
  static char ret[256];

  if (compressors_list_done) return ret;
  ret[0] = '\0';
  strcat(ret, BLOSC_BLOSCLZ_COMPNAME);
  strcat(ret, ",");
  strcat(ret, BLOSC_LZ4_COMPNAME);
  strcat(ret, ",");
  strcat(ret, BLOSC_LZ4HC_COMPNAME);
#if defined(HAVE_ZLIB)
  strcat(ret, ",");
  strcat(ret, BLOSC_ZLIB_COMPNAME);
#endif /* HAVE_ZLIB */
#if defined(HAVE_ZSTD)
  strcat(ret, ",");
  strcat(ret, BLOSC_ZSTD_COMPNAME);
#endif /* HAVE_ZSTD */
  compressors_list_done = 1;
  return ret;
}


const char* blosc_get_version_string(void) {
  return BLOSC_VERSION_STRING;
}


int blosc_get_complib_info(const char* compname, char** complib, char** version) {
  int clibcode;
  const char* clibname;
  const char* clibversion = "unknown";
  char sbuffer[256];

  clibcode = compname_to_clibcode(compname);
  clibname = clibcode_to_clibname(clibcode);

  /* complib version */
  if (clibcode == BLOSC_BLOSCLZ_LIB) {
    clibversion = BLOSCLZ_VERSION_STRING;
  }
  else if (clibcode == BLOSC_LZ4_LIB) {
    sprintf(sbuffer, "%d.%d.%d",
            LZ4_VERSION_MAJOR, LZ4_VERSION_MINOR, LZ4_VERSION_RELEASE);
    clibversion = sbuffer;
  }
#if defined(HAVE_ZLIB)
  else if (clibcode == BLOSC_ZLIB_LIB) {
    clibversion = ZLIB_VERSION;
  }
#endif /* HAVE_ZLIB */
#if defined(HAVE_ZSTD)
  else if (clibcode == BLOSC_ZSTD_LIB) {
    sprintf(sbuffer, "%d.%d.%d",
            ZSTD_VERSION_MAJOR, ZSTD_VERSION_MINOR, ZSTD_VERSION_RELEASE);
    clibversion = sbuffer;
  }
#endif /* HAVE_ZSTD */

#ifdef _MSC_VER
  *complib = _strdup(clibname);
  *version = _strdup(clibversion);
#else
  *complib = strdup(clibname);
  *version = strdup(clibversion);
#endif
  return clibcode;
}

/* Return `nbytes`, `cbytes` and `blocksize` from a compressed buffer. */
void blosc_cbuffer_sizes(const void* cbuffer, size_t* nbytes, size_t* cbytes, size_t* blocksize) {
  int32_t nbytes32, cbytes32, blocksize32;
  blosc2_cbuffer_sizes(cbuffer, &nbytes32, &cbytes32, &blocksize32);
  *nbytes = nbytes32;
  *cbytes = cbytes32;
  *blocksize = blocksize32;
}

int blosc2_cbuffer_sizes(const void* cbuffer, int32_t* nbytes, int32_t* cbytes, int32_t* blocksize) {
  blosc_header header;
  int rc = read_chunk_header((uint8_t *) cbuffer, BLOSC_MIN_HEADER_LENGTH, false, &header);
  if (rc < 0) {
    /* Return zeros if error reading header */
    memset(&header, 0, sizeof(header));
  }

  /* Read the interesting values */
  if (nbytes != NULL)
    *nbytes = header.nbytes;
  if (cbytes != NULL)
    *cbytes = header.cbytes;
  if (blocksize != NULL)
    *blocksize = header.blocksize;
  return rc;
}

int blosc_cbuffer_validate(const void* cbuffer, size_t cbytes, size_t* nbytes) {
  int32_t header_cbytes;
  int32_t header_nbytes;
  if (cbytes < BLOSC_MIN_HEADER_LENGTH) {
    /* Compressed data should contain enough space for header */
    *nbytes = 0;
    return BLOSC2_ERROR_WRITE_BUFFER;
  }
  int rc = blosc2_cbuffer_sizes(cbuffer, &header_nbytes, &header_cbytes, NULL);
  if (rc < 0) {
    *nbytes = 0;
    return rc;
  }
  *nbytes = header_nbytes;
  if (header_cbytes != cbytes) {
    /* Compressed size from header does not match `cbytes` */
    *nbytes = 0;
    return BLOSC2_ERROR_INVALID_HEADER;
  }
  if (*nbytes > BLOSC_MAX_BUFFERSIZE) {
    /* Uncompressed size is larger than allowed */
    *nbytes = 0;
    return BLOSC2_ERROR_MEMORY_ALLOC;
  }
  return 0;
}

/* Return `typesize` and `flags` from a compressed buffer. */
void blosc_cbuffer_metainfo(const void* cbuffer, size_t* typesize, int* flags) {
  blosc_header header;
  int rc = read_chunk_header((uint8_t *) cbuffer, BLOSC_MIN_HEADER_LENGTH, false, &header);
  if (rc < 0) {
    *typesize = *flags = 0;
    return;
  }

  /* Read the interesting values */
  *flags = header.flags;
  *typesize = header.typesize;
}


/* Return version information from a compressed buffer. */
void blosc_cbuffer_versions(const void* cbuffer, int* version, int* versionlz) {
  blosc_header header;
  int rc = read_chunk_header((uint8_t *) cbuffer, BLOSC_MIN_HEADER_LENGTH, false, &header);
  if (rc < 0) {
    *version = *versionlz = 0;
    return;
  }

  /* Read the version info */
  *version = header.version;
  *versionlz = header.versionlz;
}


/* Return the compressor library/format used in a compressed buffer. */
const char* blosc_cbuffer_complib(const void* cbuffer) {
  blosc_header header;
  int clibcode;
  const char* complib;
  int rc = read_chunk_header((uint8_t *) cbuffer, BLOSC_MIN_HEADER_LENGTH, false, &header);
  if (rc < 0) {
    return NULL;
  }

  /* Read the compressor format/library info */
  clibcode = (header.flags & 0xe0) >> 5;
  complib = clibcode_to_clibname(clibcode);
  return complib;
}


/* Get the internal blocksize to be used during compression.  0 means
   that an automatic blocksize is computed internally. */
int blosc_get_blocksize(void)
{
  return (int)g_force_blocksize;
}


/* Force the use of a specific blocksize.  If 0, an automatic
   blocksize will be used (the default). */
void blosc_set_blocksize(size_t size) {
  g_force_blocksize = (int32_t)size;
}


/* Set pointer to super-chunk.  If NULL, no super-chunk will be
   reachable (the default). */
void blosc_set_schunk(blosc2_schunk* schunk) {
  g_schunk = schunk;
  g_global_context->schunk = schunk;
}

blosc2_io *blosc2_io_global = NULL;

void blosc_init(void) {
  /* Return if Blosc is already initialized */
  if (g_initlib) return;

  g_ncodecs = 0;
  g_nfilters = 0;

#if defined(HAVE_PLUGINS)
  #include "blosc2/blosc2-common.h"
  #include "blosc2/blosc2-stdio.h"
  register_codecs();
  register_filters();
#endif
  pthread_mutex_init(&global_comp_mutex, NULL);
  /* Create a global context */
  g_global_context = (blosc2_context*)my_malloc(sizeof(blosc2_context));
  memset(g_global_context, 0, sizeof(blosc2_context));
  g_global_context->nthreads = g_nthreads;
  g_global_context->new_nthreads = g_nthreads;
  g_initlib = 1;
}


void blosc_destroy(void) {
  /* Return if Blosc is not initialized */
  if (!g_initlib) return;

  g_initlib = 0;
  blosc2_free_ctx(g_global_context);

  pthread_mutex_destroy(&global_comp_mutex);

}


int release_threadpool(blosc2_context *context) {
  int32_t t;
  void* status;
  int rc;

  if (context->threads_started > 0) {
    if (threads_callback) {
      /* free context data for user-managed threads */
      for (t=0; t<context->threads_started; t++)
        destroy_thread_context(context->thread_contexts + t);
      my_free(context->thread_contexts);
    }
    else {
      /* Tell all existing threads to finish */
      context->end_threads = 1;
      WAIT_INIT(-1, context);

      /* Join exiting threads */
      for (t = 0; t < context->threads_started; t++) {
        rc = pthread_join(context->threads[t], &status);
        if (rc) {
          BLOSC_TRACE_ERROR("Return code from pthread_join() is %d\n"
                            "\tError detail: %s.", rc, strerror(rc));
        }
      }

      /* Thread attributes */
      #if !defined(_WIN32)
        pthread_attr_destroy(&context->ct_attr);
      #endif

      /* Release thread handlers */
      my_free(context->threads);
    }

    /* Release mutex and condition variable objects */
    pthread_mutex_destroy(&context->count_mutex);
    pthread_mutex_destroy(&context->delta_mutex);
    pthread_cond_destroy(&context->delta_cv);

    /* Barriers */
  #ifdef BLOSC_POSIX_BARRIERS
    pthread_barrier_destroy(&context->barr_init);
    pthread_barrier_destroy(&context->barr_finish);
  #else
    pthread_mutex_destroy(&context->count_threads_mutex);
    pthread_cond_destroy(&context->count_threads_cv);
    context->count_threads = 0;      /* Reset threads counter */
  #endif

    /* Reset flags and counters */
    context->end_threads = 0;
    context->threads_started = 0;
  }


  return 0;
}

int blosc_free_resources(void) {
  /* Return if Blosc is not initialized */
  if (!g_initlib) return BLOSC2_ERROR_FAILURE;

  return release_threadpool(g_global_context);
}


/* Contexts */

/* Create a context for compression */
blosc2_context* blosc2_create_cctx(blosc2_cparams cparams) {
  blosc2_context* context = (blosc2_context*)my_malloc(sizeof(blosc2_context));
  BLOSC_ERROR_NULL(context, NULL);

  /* Populate the context, using zeros as default values */
  memset(context, 0, sizeof(blosc2_context));
  context->do_compress = 1;   /* meant for compression */
  context->compcode = cparams.compcode;
  context->compcode_meta = cparams.compcode_meta;
  context->clevel = cparams.clevel;
  context->use_dict = cparams.use_dict;
  context->typesize = cparams.typesize;
  for (int i = 0; i < BLOSC2_MAX_FILTERS; i++) {
    context->filters[i] = cparams.filters[i];
    context->filters_meta[i] = cparams.filters_meta[i];

    if (context->filters[i] >= BLOSC_LAST_FILTER && context->filters[i] <= BLOSC2_DEFINED_FILTERS_STOP) {
      BLOSC_TRACE_ERROR("filter (%d) is not yet defined",
                        context->filters[i]);
      free(context);
      return NULL;
    }
    if (context->filters[i] > BLOSC_LAST_REGISTERED_FILTER && context->filters[i] <= BLOSC2_GLOBAL_REGISTERED_FILTERS_STOP) {
      BLOSC_TRACE_ERROR("filter (%d) is not yet defined",
                        context->filters[i]);
      free(context);
      return NULL;
    }
  }

  context->nthreads = cparams.nthreads;
  context->new_nthreads = context->nthreads;
  context->blocksize = cparams.blocksize;
  context->splitmode = cparams.splitmode;
  context->threads_started = 0;
  context->schunk = cparams.schunk;

  if (cparams.prefilter != NULL) {
    context->prefilter = cparams.prefilter;
    context->preparams = (blosc2_prefilter_params*)my_malloc(sizeof(blosc2_prefilter_params));
    BLOSC_ERROR_NULL(context->preparams, NULL);
    memcpy(context->preparams, cparams.preparams, sizeof(blosc2_prefilter_params));
  }

  if (cparams.udbtune == NULL) {
    context->udbtune = &BTUNE_DEFAULTS;
  } else {
    context->udbtune = cparams.udbtune;
  }

  return context;
}

/* Create a context for decompression */
blosc2_context* blosc2_create_dctx(blosc2_dparams dparams) {
  blosc2_context* context = (blosc2_context*)my_malloc(sizeof(blosc2_context));
  BLOSC_ERROR_NULL(context, NULL);

  /* Populate the context, using zeros as default values */
  memset(context, 0, sizeof(blosc2_context));
  context->do_compress = 0;   /* Meant for decompression */
  context->nthreads = dparams.nthreads;
  context->new_nthreads = context->nthreads;
  context->threads_started = 0;
  context->block_maskout = NULL;
  context->block_maskout_nitems = 0;
  context->schunk = dparams.schunk;

  if (dparams.postfilter != NULL) {
    context->postfilter = dparams.postfilter;
    context->postparams = (blosc2_postfilter_params*)my_malloc(sizeof(blosc2_postfilter_params));
    BLOSC_ERROR_NULL(context->postparams, NULL);
    memcpy(context->postparams, dparams.postparams, sizeof(blosc2_postfilter_params));
  }

  return context;
}


void blosc2_free_ctx(blosc2_context* context) {
  release_threadpool(context);
  if (context->serial_context != NULL) {
    free_thread_context(context->serial_context);
  }
  if (context->dict_cdict != NULL) {
#ifdef HAVE_ZSTD
    ZSTD_freeCDict(context->dict_cdict);
#endif
  }
  if (context->dict_ddict != NULL) {
#ifdef HAVE_ZSTD
    ZSTD_freeDDict(context->dict_ddict);
#endif
  }
  if (context->btune != NULL) {
    context->udbtune->btune_free(context);
  }
  if (context->prefilter != NULL) {
    my_free(context->preparams);
  }
  if (context->postfilter != NULL) {
    my_free(context->postparams);
  }

  if (context->block_maskout != NULL) {
    free(context->block_maskout);
  }
  my_free(context);
}


int blosc2_ctx_get_cparams(blosc2_context *ctx, blosc2_cparams *cparams) {
  cparams->compcode = ctx->compcode;
  cparams->compcode = ctx->compcode;
  cparams->clevel = ctx->clevel;
  cparams->use_dict = ctx->use_dict;
  cparams->typesize = ctx->typesize;
  cparams->nthreads = ctx->nthreads;
  cparams->blocksize = ctx->blocksize;
  cparams->splitmode = ctx->splitmode;
  cparams->schunk = ctx->schunk;
  for (int i = 0; i < BLOSC2_MAX_FILTERS; ++i) {
    cparams->filters[i] = ctx->filters[i];
    cparams->filters_meta[i] = ctx->filters_meta[i];
  }
  cparams->prefilter = ctx->prefilter;
  cparams->preparams = ctx->preparams;
  cparams->udbtune = ctx->udbtune;

  return BLOSC2_ERROR_SUCCESS;
}


int blosc2_ctx_get_dparams(blosc2_context *ctx, blosc2_dparams *dparams) {
  dparams->nthreads = ctx->nthreads;
  dparams->schunk = ctx->schunk;
  dparams->postfilter = ctx->postfilter;
  dparams->postparams = ctx->postparams;

  return BLOSC2_ERROR_SUCCESS;
}


/* Set a maskout in decompression context */
int blosc2_set_maskout(blosc2_context *ctx, bool *maskout, int nblocks) {

  if (ctx->block_maskout != NULL) {
    // Get rid of a possible mask here
    free(ctx->block_maskout);
  }

  bool *maskout_ = malloc(nblocks);
  BLOSC_ERROR_NULL(maskout_, BLOSC2_ERROR_MEMORY_ALLOC);
  memcpy(maskout_, maskout, nblocks);
  ctx->block_maskout = maskout_;
  ctx->block_maskout_nitems = nblocks;

  return 0;
}


/* Create a chunk made of zeros */
int blosc2_chunk_zeros(blosc2_cparams cparams, const size_t nbytes, void* dest, size_t destsize) {
  if (destsize < BLOSC_EXTENDED_HEADER_LENGTH) {
    BLOSC_TRACE_ERROR("dest buffer is not long enough");
    return BLOSC2_ERROR_DATA;
  }

  if (nbytes % cparams.typesize) {
    BLOSC_TRACE_ERROR("nbytes must be a multiple of typesize");
    return BLOSC2_ERROR_DATA;
  }

  blosc_header header;
  blosc2_context* context = blosc2_create_cctx(cparams);

  int error = initialize_context_compression(
          context, NULL, nbytes, dest, destsize,
          context->clevel, context->filters, context->filters_meta,
          context->typesize, context->compcode, context->blocksize,
          context->new_nthreads, context->nthreads,
          context->udbtune, context->btune, context->schunk);
  if (error <= 0) {
    blosc2_free_ctx(context);
    return error;
  }

  memset(&header, 0, sizeof(header));
  header.version = BLOSC_VERSION_FORMAT;
  header.versionlz = BLOSC_BLOSCLZ_VERSION_FORMAT;
  header.flags = BLOSC_DOSHUFFLE | BLOSC_DOBITSHUFFLE;  // extended header
  header.typesize = context->typesize;
  header.nbytes = (int32_t)nbytes;
  header.blocksize = context->blocksize;
  header.cbytes = BLOSC_EXTENDED_HEADER_LENGTH;
  header.blosc2_flags = BLOSC2_SPECIAL_ZERO << 4;  // mark chunk as all zeros
  memcpy((uint8_t *)dest, &header, sizeof(header));

  blosc2_free_ctx(context);

  return BLOSC_EXTENDED_HEADER_LENGTH;
}


/* Create a chunk made of uninitialized values */
int blosc2_chunk_uninit(blosc2_cparams cparams, const size_t nbytes, void* dest, size_t destsize) {
  if (destsize < BLOSC_EXTENDED_HEADER_LENGTH) {
    BLOSC_TRACE_ERROR("dest buffer is not long enough");
    return BLOSC2_ERROR_DATA;
  }

  if (nbytes % cparams.typesize) {
    BLOSC_TRACE_ERROR("nbytes must be a multiple of typesize");
    return BLOSC2_ERROR_DATA;
  }

  blosc_header header;
  blosc2_context* context = blosc2_create_cctx(cparams);
  int error = initialize_context_compression(
          context, NULL, nbytes, dest, destsize,
          context->clevel, context->filters, context->filters_meta,
          context->typesize, context->compcode, context->blocksize,
          context->new_nthreads, context->nthreads,
          context->udbtune, context->btune, context->schunk);
  if (error <= 0) {
    blosc2_free_ctx(context);
    return error;
  }

  memset(&header, 0, sizeof(header));
  header.version = BLOSC_VERSION_FORMAT;
  header.versionlz = BLOSC_BLOSCLZ_VERSION_FORMAT;
  header.flags = BLOSC_DOSHUFFLE | BLOSC_DOBITSHUFFLE;  // extended header
  header.typesize = context->typesize;
  header.nbytes = (int32_t)nbytes;
  header.blocksize = context->blocksize;
  header.cbytes = BLOSC_EXTENDED_HEADER_LENGTH;
  header.blosc2_flags = BLOSC2_SPECIAL_UNINIT << 4;  // mark chunk as unitialized
  memcpy((uint8_t *)dest, &header, sizeof(header));

  blosc2_free_ctx(context);

  return BLOSC_EXTENDED_HEADER_LENGTH;
}


/* Create a chunk made of nans */
int blosc2_chunk_nans(blosc2_cparams cparams, const size_t nbytes, void* dest, size_t destsize) {
  if (destsize < BLOSC_EXTENDED_HEADER_LENGTH) {
    BLOSC_TRACE_ERROR("dest buffer is not long enough");
    return BLOSC2_ERROR_DATA;
  }

  if (nbytes % cparams.typesize) {
    BLOSC_TRACE_ERROR("nbytes must be a multiple of typesize");
    return BLOSC2_ERROR_DATA;
  }

  blosc_header header;
  blosc2_context* context = blosc2_create_cctx(cparams);

  int error = initialize_context_compression(
          context, NULL, nbytes, dest, destsize,
          context->clevel, context->filters, context->filters_meta,
          context->typesize, context->compcode, context->blocksize,
          context->new_nthreads, context->nthreads,
          context->udbtune, context->btune, context->schunk);
  if (error <= 0) {
    blosc2_free_ctx(context);
    return error;
  }

  memset(&header, 0, sizeof(header));
  header.version = BLOSC_VERSION_FORMAT;
  header.versionlz = BLOSC_BLOSCLZ_VERSION_FORMAT;
  header.flags = BLOSC_DOSHUFFLE | BLOSC_DOBITSHUFFLE;  // extended header
  header.typesize = context->typesize;
  header.nbytes = (int32_t)nbytes;
  header.blocksize = context->blocksize;
  header.cbytes = BLOSC_EXTENDED_HEADER_LENGTH;
  header.blosc2_flags = BLOSC2_SPECIAL_NAN << 4;  // mark chunk as all NaNs
  memcpy((uint8_t *)dest, &header, sizeof(header));

  blosc2_free_ctx(context);

  return BLOSC_EXTENDED_HEADER_LENGTH;
}


/* Create a chunk made of repeated values */
int blosc2_chunk_repeatval(blosc2_cparams cparams, const size_t nbytes,
                           void* dest, size_t destsize, void* repeatval) {
  uint8_t typesize = cparams.typesize;
  if (destsize < BLOSC_EXTENDED_HEADER_LENGTH + typesize) {
    BLOSC_TRACE_ERROR("dest buffer is not long enough");
    return BLOSC2_ERROR_DATA;
  }

  if (nbytes % cparams.typesize) {
    BLOSC_TRACE_ERROR("nbytes must be a multiple of typesize");
    return BLOSC2_ERROR_DATA;
  }

  blosc_header header;
  blosc2_context* context = blosc2_create_cctx(cparams);

  int error = initialize_context_compression(
          context, NULL, nbytes, dest, destsize,
          context->clevel, context->filters, context->filters_meta,
          context->typesize, context->compcode, context->blocksize,
          context->new_nthreads, context->nthreads,
          context->udbtune, context->btune, context->schunk);
  if (error <= 0) {
    blosc2_free_ctx(context);
    return error;
  }

  memset(&header, 0, sizeof(header));
  header.version = BLOSC_VERSION_FORMAT;
  header.versionlz = BLOSC_BLOSCLZ_VERSION_FORMAT;
  header.flags = BLOSC_DOSHUFFLE | BLOSC_DOBITSHUFFLE;  // extended header
  header.typesize = (uint8_t)typesize;
  header.nbytes = (int32_t)nbytes;
  header.blocksize = context->blocksize;
  header.cbytes = BLOSC_EXTENDED_HEADER_LENGTH + (int32_t)typesize;
  header.blosc2_flags = BLOSC2_SPECIAL_VALUE << 4;  // mark chunk as all repeated value
  memcpy((uint8_t *)dest, &header, sizeof(header));
  memcpy((uint8_t *)dest + sizeof(header), repeatval, typesize);

  blosc2_free_ctx(context);

  return BLOSC_EXTENDED_HEADER_LENGTH + (uint8_t)typesize;
}


/* Register filters */

int register_filter_private(blosc2_filter *filter) {
    BLOSC_ERROR_NULL(filter, BLOSC2_ERROR_INVALID_PARAM);
    if (g_nfilters == UINT8_MAX) {
        BLOSC_TRACE_ERROR("Can not register more filters");
        return BLOSC2_ERROR_CODEC_SUPPORT;
    }
    if (filter->id < BLOSC2_GLOBAL_REGISTERED_FILTERS_START) {
        BLOSC_TRACE_ERROR("The id must be greater or equal than %d", BLOSC2_GLOBAL_REGISTERED_FILTERS_START);
        return BLOSC2_ERROR_FAILURE;
    }
    if (filter->id > BLOSC2_USER_REGISTERED_FILTERS_STOP) {
        BLOSC_TRACE_ERROR("The id must be leather or equal than %d", BLOSC2_USER_REGISTERED_FILTERS_STOP);
        return BLOSC2_ERROR_FAILURE;
    }

    // Check if the filter is already registered
    for (int i = 0; i < g_nfilters; ++i) {
        if (g_filters[i].id == filter->id) {
            BLOSC_TRACE_ERROR("The filter is already registered!");
            return BLOSC2_ERROR_FAILURE;
        }
    }

    blosc2_filter *filter_new = &g_filters[g_nfilters++];
    memcpy(filter_new, filter, sizeof(blosc2_filter));

    return BLOSC2_ERROR_SUCCESS;
}


int blosc2_register_filter(blosc2_filter *filter) {
  if (filter->id < BLOSC2_USER_REGISTERED_FILTERS_START) {
    BLOSC_TRACE_ERROR("The id must be greater or equal than %d", BLOSC2_USER_REGISTERED_FILTERS_START);
    return BLOSC2_ERROR_FAILURE;
  }

  return register_filter_private(filter);
}


/* Register codecs */

int register_codec_private(blosc2_codec *codec) {
    BLOSC_ERROR_NULL(codec, BLOSC2_ERROR_INVALID_PARAM);
    if (g_ncodecs == UINT8_MAX) {
        BLOSC_TRACE_ERROR("Can not register more codecs");
        return BLOSC2_ERROR_CODEC_SUPPORT;
    }
    if (codec->compcode < BLOSC2_GLOBAL_REGISTERED_CODECS_START) {
        BLOSC_TRACE_ERROR("The id must be greater or equal than %d", BLOSC2_GLOBAL_REGISTERED_CODECS_START);
        return BLOSC2_ERROR_FAILURE;
    }
    if (codec->compcode > BLOSC2_USER_REGISTERED_CODECS_STOP) {
        BLOSC_TRACE_ERROR("The id must be leather or equal than %d", BLOSC2_USER_REGISTERED_CODECS_STOP);
        return BLOSC2_ERROR_FAILURE;
    }

    // Check if the code is already registered
    for (int i = 0; i < g_ncodecs; ++i) {
        if (g_codecs[i].compcode == codec->compcode) {
            BLOSC_TRACE_ERROR("The codec is already registered!");
            return BLOSC2_ERROR_CODEC_PARAM;
        }
    }

    blosc2_codec *codec_new = &g_codecs[g_ncodecs++];
    memcpy(codec_new, codec, sizeof(blosc2_codec));

    return BLOSC2_ERROR_SUCCESS;
}


int blosc2_register_codec(blosc2_codec *codec) {
  if (codec->compcode < BLOSC2_USER_REGISTERED_CODECS_START) {
    BLOSC_TRACE_ERROR("The compcode must be greater or equal than %d", BLOSC2_USER_REGISTERED_CODECS_START);
    return BLOSC2_ERROR_CODEC_PARAM;
  }

  return register_codec_private(codec);
}


int _blosc2_register_io_cb(const blosc2_io_cb *io) {

  // Check if the io is already registered
  for (int i = 0; i < g_nio; ++i) {
    if (g_io[i].id == io->id) {
      BLOSC_TRACE_ERROR("The codec is already registered!");
      return BLOSC2_ERROR_PLUGIN_IO;
    }
  }

  blosc2_io_cb *io_new = &g_io[g_nio++];
  memcpy(io_new, io, sizeof(blosc2_io_cb));

  return BLOSC2_ERROR_SUCCESS;
}

int blosc2_register_io_cb(const blosc2_io_cb *io) {
  BLOSC_ERROR_NULL(io, BLOSC2_ERROR_INVALID_PARAM);
  if (g_nio == UINT8_MAX) {
    BLOSC_TRACE_ERROR("Can not register more codecs");
    return BLOSC2_ERROR_PLUGIN_IO;
  }

  if (io->id < BLOSC2_IO_REGISTERED) {
    BLOSC_TRACE_ERROR("The compcode must be greater or equal than %d", BLOSC2_IO_REGISTERED);
    return BLOSC2_ERROR_PLUGIN_IO;
  }

  return _blosc2_register_io_cb(io);
}

blosc2_io_cb *blosc2_get_io_cb(uint8_t id) {
  for (int i = 0; i < g_nio; ++i) {
    if (g_io[i].id == id) {
      return &g_io[i];
    }
  }
  if (id == BLOSC2_IO_FILESYSTEM) {
    if (_blosc2_register_io_cb(&BLOSC2_IO_CB_DEFAULTS) < 0) {
      BLOSC_TRACE_ERROR("Error registering the default IO API");
      return NULL;
    }
    return blosc2_get_io_cb(id);
  }
  return NULL;
}
