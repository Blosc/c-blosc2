/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>
  Creation date: 2009-05-20

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#if defined(USING_CMAKE)
  #include "config.h"
#endif /*  USING_CMAKE */
#include "blosc.h"
#include "shuffle.h"
#include "schunk.h"
#include "delta.h"
#include "trunc-prec.h"
#include "blosclz.h"
#if defined(HAVE_LZ4)
#include "lz4.h"
#include "lz4hc.h"
#endif /*  HAVE_LZ4 */
#if defined(HAVE_LIZARD)
#include "lizard_compress.h"
#include "lizard_decompress.h"
#endif /*  HAVE_LIZARD */
#if defined(HAVE_SNAPPY)
  #include "snappy-c.h"
#endif /*  HAVE_SNAPPY */
#if defined(HAVE_MINIZ)
  #include "miniz.c"
#elif defined(HAVE_ZLIB)
  #include "zlib.h"
#endif /*  HAVE_MINIZ */
#if defined(HAVE_ZSTD)
  #include "zstd.h"
  #include "zstd_errors.h"
#endif /*  HAVE_ZSTD */

#if defined(_WIN32) && !defined(__MINGW32__)
  #include <windows.h>
  #include <malloc.h>

/* stdint.h only available in VS2010 (VC++ 16.0) and newer */
  #if defined(_MSC_VER) && _MSC_VER < 1600
    #include "win32/stdint-windows.h"
  #else
    #include <stdint.h>
  #endif

  #include <process.h>
  #define getpid _getpid
#else
  #include <stdint.h>
  #include <unistd.h>
  #include <inttypes.h>
#endif  /* _WIN32 */

#if defined(_WIN32) && !defined(__GNUC__)
  #include "win32/pthread.h"
  #include "win32/pthread.c"
#else
  #include <pthread.h>
#endif

/* Some useful units */
#define KB 1024
#define MB (1024*KB)

/* Minimum buffer size to be compressed */
#define MIN_BUFFERSIZE 128       /* Cannot be smaller than 66 */

/* The maximum number of splits in a block for compression */
#define MAX_SPLITS 16            /* Cannot be larger than 128 */

/* The size of L1 cache.  32 KB is quite common nowadays. */
#define L1 (32*KB)

/* Have problems using posix barriers when symbol value is 200112L */
/* This requires more investigation, but will work for the moment */
#if defined(_POSIX_BARRIERS) && ((_POSIX_BARRIERS - 20012L) >= 0 && _POSIX_BARRIERS != 200112L)
#define _POSIX_BARRIERS_MINE
#endif
/* Synchronization variables */


struct blosc_context_s {
  const uint8_t* src;
  /* The source buffer */
  uint8_t* dest;
  /* The destination buffer */
  uint8_t* header_flags;
  /* Flags for header */
  int32_t sourcesize;
  /* Number of bytes in source buffer */
  int32_t nblocks;
  /* Number of total blocks in buffer */
  int32_t leftover;
  /* Extra bytes at end of buffer */
  int32_t blocksize;
  /* Length of the block in bytes */
  int32_t num_output_bytes;
  /* Counter for the number of output bytes */
  int32_t destsize;
  /* Maximum size for destination buffer */
  uint8_t* bstarts;
  /* Start of the buffer past header info */
  uint8_t typesize;
  /* Type size */
  uint8_t compcode;
  /* Compressor code to use */
  int8_t clevel;
  /* Compression level (1-9) */
  uint8_t filtercode;
  /* The code of the filter */
  blosc2_sheader* schunk;
  /* Associated super-chunk (if available) */
  struct thread_context* serial_context;
  /* Cache for temporaries for serial operation */
  uint8_t compress;
  /* 1 if we are doing compression 0 if decompress */

  /* Threading */
  int32_t nthreads;
  int32_t threads_started;
  int32_t end_threads;
  pthread_t *threads;
  pthread_mutex_t count_mutex;
  pthread_mutex_t delta_mutex;
  pthread_cond_t delta_cv;
#ifdef _POSIX_BARRIERS_MINE
  pthread_barrier_t barr_init;
  pthread_barrier_t barr_finish;
#else
  int32_t count_threads;
  pthread_mutex_t count_threads_mutex;
  pthread_cond_t count_threads_cv;
#endif
#if !defined(_WIN32)
  pthread_attr_t ct_attr;            /* creation time attrs for threads */
#endif
  int32_t thread_giveup_code;
  /* error code when give up */
  int32_t thread_nblock;                    /* block counter */
  int32_t dref_not_init;   /* data ref not initialized */
};

struct thread_context {
  blosc_context* parent_context;
  int32_t tid;
  uint8_t* tmp;
  uint8_t* tmp2;
  uint8_t* tmp3;
  int32_t tmpblocksize; /* keep track of how big the temporary buffers are */
#if defined(HAVE_ZSTD)
  /* The contexts for ZSTD */
  ZSTD_CCtx* zstd_cctx;
  ZSTD_DCtx* zstd_dctx;
#endif /* HAVE_ZSTD */
};

/* Global context for non-contextual API */
static blosc_context* g_global_context;
static pthread_mutex_t global_comp_mutex;
static int32_t g_compressor = BLOSC_BLOSCLZ;
/* the compressor to use by default */
static int32_t g_nthreads = 1;
static int32_t g_force_blocksize = 0;
static int32_t g_initlib = 0;
static blosc2_sheader* g_schunk = NULL;   /* the pointer to super-chunk */


/* Wrapped function to adjust the number of threads used by blosc */
int blosc_set_nthreads_(blosc_context*);

/* Releases the global threadpool */
int blosc_release_threadpool(blosc_context* context);

/* Macros for synchronization */

/* Wait until all threads are initialized */
#ifdef _POSIX_BARRIERS_MINE
#define WAIT_INIT(RET_VAL, CONTEXT_PTR)  \
  rc = pthread_barrier_wait(&CONTEXT_PTR->barr_init); \
  if (rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) { \
    printf("Could not wait on barrier (init): %d\n", rc); \
    return((RET_VAL));                            \
  }
#else
#define WAIT_INIT(RET_VAL, CONTEXT_PTR)   \
  pthread_mutex_lock(&CONTEXT_PTR->count_threads_mutex); \
  if (CONTEXT_PTR->count_threads < CONTEXT_PTR->nthreads) { \
    CONTEXT_PTR->count_threads++;  \
    pthread_cond_wait(&CONTEXT_PTR->count_threads_cv, &CONTEXT_PTR->count_threads_mutex); \
  } \
  else { \
    pthread_cond_broadcast(&CONTEXT_PTR->count_threads_cv); \
  } \
  pthread_mutex_unlock(&CONTEXT_PTR->count_threads_mutex);
#endif

/* Wait for all threads to finish */
#ifdef _POSIX_BARRIERS_MINE
#define WAIT_FINISH(RET_VAL, CONTEXT_PTR)   \
  rc = pthread_barrier_wait(&CONTEXT_PTR->barr_finish); \
  if (rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) { \
    printf("Could not wait on barrier (finish)\n"); \
    return((RET_VAL));                              \
  }
#else
#define WAIT_FINISH(RET_VAL, CONTEXT_PTR)                           \
  pthread_mutex_lock(&CONTEXT_PTR->count_threads_mutex); \
  if (CONTEXT_PTR->count_threads > 0) { \
    CONTEXT_PTR->count_threads--; \
    pthread_cond_wait(&CONTEXT_PTR->count_threads_cv, &CONTEXT_PTR->count_threads_mutex); \
  } \
  else { \
    pthread_cond_broadcast(&CONTEXT_PTR->count_threads_cv); \
  } \
  pthread_mutex_unlock(&CONTEXT_PTR->count_threads_mutex);
#endif


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
    printf("Error allocating memory!");
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


/* Copy 4 bytes from `*pa` to int32_t, changing endianness if necessary. */
static int32_t sw32_(const uint8_t* pa) {
  int32_t idest;
  uint8_t* dest = (uint8_t*)&idest;
  int i = 1;                    /* for big/little endian detection */
  char* p = (char*)&i;

  if (p[0] != 1) {
    /* big endian */
    dest[0] = pa[3];
    dest[1] = pa[2];
    dest[2] = pa[1];
    dest[3] = pa[0];
  }
  else {
    /* little endian */
    dest[0] = pa[0];
    dest[1] = pa[1];
    dest[2] = pa[2];
    dest[3] = pa[3];
  }
  return idest;
}


/* Copy 4 bytes from `*pa` to `*dest`, changing endianness if necessary. */
static void _sw32(uint8_t* dest, int32_t a) {
  uint8_t* pa = (uint8_t*)&a;
  int i = 1;                    /* for big/little endian detection */
  char* p = (char*)&i;

  if (p[0] != 1) {
    /* big endian */
    dest[0] = pa[3];
    dest[1] = pa[2];
    dest[2] = pa[1];
    dest[3] = pa[0];
  }
  else {
    /* little endian */
    dest[0] = pa[0];
    dest[1] = pa[1];
    dest[2] = pa[2];
    dest[3] = pa[3];
  }
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
  if (strcmp(compname, BLOSC_LIZARD_COMPNAME) == 0)
    return BLOSC_LIZARD_LIB;
  if (strcmp(compname, BLOSC_SNAPPY_COMPNAME) == 0)
    return BLOSC_SNAPPY_LIB;
  if (strcmp(compname, BLOSC_ZLIB_COMPNAME) == 0)
    return BLOSC_ZLIB_LIB;
  if (strcmp(compname, BLOSC_ZSTD_COMPNAME) == 0)
    return BLOSC_ZSTD_LIB;
  return -1;
}

/* Return the library name associated with the compressor code */
static char* clibcode_to_clibname(int clibcode) {
  if (clibcode == BLOSC_BLOSCLZ_LIB) return BLOSC_BLOSCLZ_LIBNAME;
  if (clibcode == BLOSC_LZ4_LIB) return BLOSC_LZ4_LIBNAME;
  if (clibcode == BLOSC_LIZARD_LIB) return BLOSC_LIZARD_LIBNAME;
  if (clibcode == BLOSC_SNAPPY_LIB) return BLOSC_SNAPPY_LIBNAME;
  if (clibcode == BLOSC_ZLIB_LIB) return BLOSC_ZLIB_LIBNAME;
  if (clibcode == BLOSC_ZSTD_LIB) return BLOSC_ZSTD_LIBNAME;
  return NULL;                  /* should never happen */
}


/*
 * Conversion routines between compressor names and compressor codes
 */

/* Get the compressor name associated with the compressor code */
int blosc_compcode_to_compname(int compcode, char** compname) {
  int code = -1;    /* -1 means non-existent compressor code */
  char* name = NULL;

  /* Map the compressor code */
  if (compcode == BLOSC_BLOSCLZ)
    name = BLOSC_BLOSCLZ_COMPNAME;
  else if (compcode == BLOSC_LZ4)
    name = BLOSC_LZ4_COMPNAME;
  else if (compcode == BLOSC_LZ4HC)
    name = BLOSC_LZ4HC_COMPNAME;
  else if (compcode == BLOSC_LIZARD)
    name = BLOSC_LIZARD_COMPNAME;
  else if (compcode == BLOSC_SNAPPY)
    name = BLOSC_SNAPPY_COMPNAME;
  else if (compcode == BLOSC_ZLIB)
    name = BLOSC_ZLIB_COMPNAME;
  else if (compcode == BLOSC_ZSTD)
    name = BLOSC_ZSTD_COMPNAME;

  *compname = name;

  /* Guess if there is support for this code */
  if (compcode == BLOSC_BLOSCLZ)
    code = BLOSC_BLOSCLZ;
#if defined(HAVE_LZ4)
  else if (compcode == BLOSC_LZ4)
    code = BLOSC_LZ4;
  else if (compcode == BLOSC_LZ4HC)
    code = BLOSC_LZ4HC;
#endif /* HAVE_LZ4 */
#if defined(HAVE_LIZARD)
  else if (compcode == BLOSC_LIZARD)
    code = BLOSC_LIZARD;
#endif /* HAVE_LIZARD */
#if defined(HAVE_SNAPPY)
  else if (compcode == BLOSC_SNAPPY)
    code = BLOSC_SNAPPY;
#endif /* HAVE_SNAPPY */
#if defined(HAVE_ZLIB)
  else if (compcode == BLOSC_ZLIB)
    code = BLOSC_ZLIB;
#endif /* HAVE_ZLIB */
#if defined(HAVE_ZSTD)
  else if (compcode == BLOSC_ZSTD)
    code = BLOSC_ZSTD;
#endif /* HAVE_ZSTD */

  return code;
}

/* Get the compressor code for the compressor name. -1 if it is not available */
int blosc_compname_to_compcode(const char* compname) {
  int code = -1;  /* -1 means non-existent compressor code */

  if (strcmp(compname, BLOSC_BLOSCLZ_COMPNAME) == 0) {
    code = BLOSC_BLOSCLZ;
  }
#if defined(HAVE_LZ4)
  else if (strcmp(compname, BLOSC_LZ4_COMPNAME) == 0) {
    code = BLOSC_LZ4;
  }
  else if (strcmp(compname, BLOSC_LZ4HC_COMPNAME) == 0) {
    code = BLOSC_LZ4HC;
  }
#endif /*  HAVE_LZ4 */
#if defined(HAVE_LIZARD)
  else if (strcmp(compname, BLOSC_LIZARD_COMPNAME) == 0) {
    code = BLOSC_LIZARD;
  }
#endif /*  HAVE_LIZARD */
#if defined(HAVE_SNAPPY)
  else if (strcmp(compname, BLOSC_SNAPPY_COMPNAME) == 0) {
    code = BLOSC_SNAPPY;
  }
#endif /*  HAVE_SNAPPY */
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

  return code;
}


#if defined(HAVE_LZ4)
static int lz4_wrap_compress(const char* input, size_t input_length,
                             char* output, size_t maxout, int accel) {
  int cbytes;
  cbytes = LZ4_compress_fast(input, output, (int)input_length, (int)maxout,
                             accel);
  return cbytes;
}

static int lz4hc_wrap_compress(const char* input, size_t input_length,
                               char* output, size_t maxout, int clevel) {
  int cbytes;
  if (input_length > (size_t)(2 << 30))
    return -1;   /* input larger than 1 GB is not supported */
  /* clevel for lz4hc goes up to 12, at least in LZ4 1.7.5
   * but levels larger than 9 does not buy much compression. */
  cbytes = LZ4_compress_HC(input, output, (int)input_length, (int)maxout,
                           clevel);
  return cbytes;
}

static int lz4_wrap_decompress(const char* input, size_t compressed_length,
                               char* output, size_t maxout) {
  size_t cbytes;
  cbytes = LZ4_decompress_fast(input, output, (int)maxout);
  if (cbytes != compressed_length) {
    return 0;
  }
  return (int)maxout;
}
#endif /* HAVE_LZ4 */

#if defined(HAVE_LIZARD)
static int lizard_wrap_compress(const char* input, size_t input_length,
                                char* output, size_t maxout, int clevel) {
  int cbytes;
  cbytes = Lizard_compress(input, output, (int)input_length, (int)maxout,
                           clevel);
  return cbytes;
}

static int lizard_wrap_decompress(const char* input, size_t compressed_length,
                                  char* output, size_t maxout) {
  int dbytes;
  dbytes = Lizard_decompress_safe(input, output, (int)compressed_length,
                                  (int)maxout);
  if (dbytes < 0) {
    return 0;
  }
  return dbytes;
}

#endif /* HAVE_LIZARD */

#if defined(HAVE_SNAPPY)
static int snappy_wrap_compress(const char* input, size_t input_length,
                                char* output, size_t maxout) {
  snappy_status status;
  size_t cl = maxout;
  status = snappy_compress(input, input_length, output, &cl);
  if (status != SNAPPY_OK) {
    return 0;
  }
  return (int)cl;
}

static int snappy_wrap_decompress(const char* input, size_t compressed_length,
                                  char* output, size_t maxout) {
  snappy_status status;
  size_t ul = maxout;
  status = snappy_uncompress(input, compressed_length, output, &ul);
  if (status != SNAPPY_OK) {
    return 0;
  }
  return (int)ul;
}
#endif /* HAVE_SNAPPY */

#if defined(HAVE_ZLIB)
/* zlib is not very respectful with sharing name space with others.
 Fortunately, its names do not collide with those already in blosc. */
static int zlib_wrap_compress(const char* input, size_t input_length,
                              char* output, size_t maxout, int clevel) {
  int status;
  uLongf cl = maxout;
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
  uLongf ul = maxout;
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
  clevel = (clevel < 9) ? clevel * 2 - 1 : ZSTD_maxCLevel();
  /* Make the level 8 close enough to maxCLevel */
  if (clevel == 8) clevel = ZSTD_maxCLevel() - 2;
  if (thread_context->zstd_cctx == NULL) {
    thread_context->zstd_cctx = ZSTD_createCCtx();
  }

  code = ZSTD_compressCCtx(thread_context->zstd_cctx,
      (void*)output, maxout, (void*)input, input_length, clevel);
  if (ZSTD_isError(code) != ZSTD_error_no_error) {
    return 0;
  }
  return (int)code;
}

static int zstd_wrap_decompress(struct thread_context* thread_context,
                                const char* input, size_t compressed_length,
                                char* output, size_t maxout) {
  size_t code;
  if (thread_context->zstd_dctx == NULL) {
    thread_context->zstd_dctx = ZSTD_createDCtx();
  }
  code = ZSTD_decompressDCtx(thread_context->zstd_dctx,
      (void*)output, maxout, (void*)input, compressed_length);
  if (ZSTD_isError(code) != ZSTD_error_no_error) {
    return 0;
  }
  return (int)code;
}
#endif /*  HAVE_ZSTD */

/* Compute acceleration for blosclz */
static int get_accel(const blosc_context* context) {
  int32_t clevel = context->clevel;
  int32_t typesize = context->typesize;

  if (context->compcode == BLOSC_BLOSCLZ) {
    /* Compute the power of 2. See:
     * http://www.exploringbinary.com/ten-ways-to-check-if-an-integer-is-a-power-of-two-in-c/
     */
    int32_t tspow2 = ((typesize != 0) && !(typesize & (typesize - 1)));
    if (tspow2 && typesize < 32) {
      return 32;
    }
  }
  else if (context->compcode == BLOSC_LZ4) {
    /* This acceleration setting based on discussions held in:
     * https://groups.google.com/forum/#!topic/lz4c/zosy90P8MQw
     */
    return (10 - clevel);
  }
  else if (context->compcode == BLOSC_LIZARD) {
    /* Lizard currently accepts clevels from 10 to 49 */
      switch (clevel) {
          case 1 :
              return 10;
          case 2 :
              return 10;
          case 3 :
              return 10;
          case 4 :
              return 10;
          case 5 :
              return 20;
          case 6 :
              return 20;
          case 7 :
              return 20;
          case 8 :
              return 41;
          case 9 :
              return 41;
      }
  }
  return 1;
}

/* Shuffle & compress a single block */
static int blosc_c(struct thread_context* thread_context, int32_t blocksize,
                   int32_t leftoverblock, int32_t ntbytes, int32_t maxbytes,
                   const uint8_t* src, int offset, uint8_t* dest,
                   uint8_t* tmp, uint8_t* tmp2) {
  blosc_context* context = thread_context->parent_context;
  int32_t compformat = (*(context->header_flags) & 0xe0) >> 5;
  int dont_split = (*(context->header_flags) & 0x10) >> 4;
  uint8_t blosc_version_format = BLOSC_VERSION_FORMAT;
  int32_t j, neblock, nsplits;
  int32_t cbytes;                   /* number of compressed bytes in split */
  int32_t ctbytes = 0;              /* number of compressed bytes in block */
  int32_t maxout;
  int32_t typesize = context->typesize;
  const uint8_t* _src = src + offset;
  char* compname;
  int accel;
  int bscount;

  /* Check if we have a delta in a super-chunk context */
  if (context->schunk != NULL) {
    uint8_t* filters;
    filters = decode_filters(context->schunk->filters);
    if (filters[0] == BLOSC_DELTA) {
      delta_encoder8((uint8_t*)src, offset, blocksize, (unsigned char*)_src,
                     tmp2);
      _src = tmp2;
    }
    else if (filters[0] == BLOSC_TRUNC_PREC) {
      if ((typesize != 4) && (typesize != 8)) {
        fprintf(stderr, "unsupported typesize for TRUNC_PREC filter\n");
        return -6;  // signals
      }
      truncate_precision(context->schunk->filters_meta[BLOSC_TRUNC_PREC_MSLOT],
                         typesize, blocksize, (unsigned char *)_src, tmp2);
      _src = tmp2;
    }
    free(filters);
  }

  /* Shuffle filters */
  if (context->filtercode == BLOSC_SHUFFLE) {
    shuffle(typesize, blocksize, _src, tmp);
    _src = tmp;
  }
  else if (context->filtercode == BLOSC_BITSHUFFLE) {
    bscount = bitshuffle(typesize, blocksize, _src, tmp, tmp2);
    if (bscount < 0)
      return bscount;
    _src = tmp;
  }

  /* Calculate acceleration for different compressors */
  accel = get_accel(context);

  /* The number of splits for this block */
  if (!dont_split && !leftoverblock) {
    nsplits = typesize;
  }
  else {
    nsplits = 1;
  }
  neblock = blocksize / nsplits;
  for (j = 0; j < nsplits; j++) {
    dest += sizeof(int32_t);
    ntbytes += (int32_t)sizeof(int32_t);
    ctbytes += (int32_t)sizeof(int32_t);
    maxout = neblock;
  #if defined(HAVE_SNAPPY)
    if (context->compcode == BLOSC_SNAPPY) {
      /* TODO perhaps refactor this to keep the value stashed somewhere */
      maxout = snappy_max_compressed_length(neblock);
    }
  #endif /*  HAVE_SNAPPY */
    if (ntbytes + maxout > maxbytes) {
      maxout = maxbytes - ntbytes;   /* avoid buffer overrun */
      if (maxout <= 0) {
        return 0;                  /* non-compressible block */
      }
    }
    if (context->compcode == BLOSC_BLOSCLZ) {
      cbytes = blosclz_compress(context->clevel, _src + j * neblock, neblock,
                                dest, maxout, accel);
    }
  #if defined(HAVE_LZ4)
    else if (context->compcode == BLOSC_LZ4) {
      cbytes = lz4_wrap_compress((char*)_src + j * neblock, (size_t)neblock,
                                 (char*)dest, (size_t)maxout, accel);
    }
    else if (context->compcode == BLOSC_LZ4HC) {
      cbytes = lz4hc_wrap_compress((char*)_src + j * neblock, (size_t)neblock,
                                   (char*)dest, (size_t)maxout, context->clevel);
    }
  #endif /* HAVE_LZ4 */
  #if defined(HAVE_LIZARD)
      else if (context->compcode == BLOSC_LIZARD) {
      cbytes = lizard_wrap_compress((char*)_src + j * neblock, (size_t)neblock,
                                    (char*)dest, (size_t)maxout, accel);
    }
  #endif /* HAVE_LIZARD */
  #if defined(HAVE_SNAPPY)
      else if (context->compcode == BLOSC_SNAPPY) {
      cbytes = snappy_wrap_compress((char*)_src + j * neblock, (size_t)neblock,
                                    (char*)dest, (size_t)maxout);
    }
  #endif /* HAVE_SNAPPY */
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

    else {
      blosc_compcode_to_compname(context->compcode, &compname);
      fprintf(stderr, "Blosc has not been compiled with '%s' ", compname);
      fprintf(stderr, "compression support.  Please use one having it.");
      return -5;    /* signals no compression support */
    }

    if (cbytes > maxout) {
      /* Buffer overrun caused by compression (should never happen) */
      return -1;
    }
    else if (cbytes < 0) {
      /* cbytes should never be negative */
      return -2;
    }
    else if (cbytes == 0 || cbytes == neblock) {
      /* The compressor has been unable to compress data at all. */
      /* Before doing the copy, check that we are not running into a
         buffer overflow. */
      if ((ntbytes + neblock) > maxbytes) {
        return 0;    /* Non-compressible data */
      }
      memcpy(dest, _src + j * neblock, neblock);
      cbytes = neblock;
    }
    _sw32(dest - 4, cbytes);
    dest += cbytes;
    ntbytes += cbytes;
    ctbytes += cbytes;
  }  /* Closes j < nsplits */

  return ctbytes;
}

/* Decompress & unshuffle a single block */
static int blosc_d(
    struct thread_context* thread_context, int32_t blocksize, int32_t leftoverblock,
    const uint8_t* src, uint8_t* dest, int offset, uint8_t* tmp, uint8_t* tmp2) {
  blosc_context* context = thread_context->parent_context;
  int32_t compformat = (*(context->header_flags) & 0xe0) >> 5;
  int dont_split = (*(context->header_flags) & 0x10) >> 4;
  uint8_t blosc_version_format = src[0];
  int32_t j, neblock, nsplits;
  int32_t nbytes;                /* number of decompressed bytes in split */
  int32_t cbytes;                /* number of compressed bytes in split */
  int32_t ctbytes = 0;           /* number of compressed bytes in block */
  int32_t ntbytes = 0;           /* number of uncompressed bytes in block */
  uint8_t* _dest = dest + offset;
  int32_t typesize = context->typesize;
  char* compname;
  int bscount;

  if ((context->filtercode == BLOSC_SHUFFLE) || \
      (context->filtercode == BLOSC_BITSHUFFLE)) {
    _dest = tmp;
  }

  /* The number of splits for this block */
  if (!dont_split && !leftoverblock) {
    nsplits = typesize;
  }
  else {
    nsplits = 1;
  }

  neblock = blocksize / nsplits;
  for (j = 0; j < nsplits; j++) {
    cbytes = sw32_(src);      /* amount of compressed bytes */
    src += sizeof(int32_t);
    ctbytes += (int32_t)sizeof(int32_t);
    /* Uncompress */
    if (cbytes == neblock) {
      memcpy(_dest, src, neblock);
      nbytes = neblock;
    }
    else {
      if (compformat == BLOSC_BLOSCLZ_FORMAT) {
        nbytes = blosclz_decompress(src, cbytes, _dest, neblock);
      }
  #if defined(HAVE_LZ4)
      else if (compformat == BLOSC_LZ4_FORMAT) {
        nbytes = lz4_wrap_decompress((char*)src, (size_t)cbytes,
                                     (char*)_dest, (size_t)neblock);
      }
  #endif /*  HAVE_LZ4 */
  #if defined(HAVE_LIZARD)
        else if (compformat == BLOSC_LIZARD_FORMAT) {
        nbytes = lizard_wrap_decompress((char*)src, (size_t)cbytes,
                                        (char*)_dest, (size_t)neblock);
      }
  #endif /*  HAVE_LIZARD */
  #if defined(HAVE_SNAPPY)
        else if (compformat == BLOSC_SNAPPY_FORMAT) {
        nbytes = snappy_wrap_decompress((char*)src, (size_t)cbytes,
                                        (char*)_dest, (size_t)neblock);
      }
  #endif /*  HAVE_SNAPPY */
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
      else {
        compname = clibcode_to_clibname(compformat);
        fprintf(stderr,
                "Blosc has not been compiled with decompression "
                    "support for '%s' format. ", compname);
        fprintf(stderr, "Please recompile for adding this support.\n");
        return -5;    /* signals no decompression support */
      }

      /* Check that decompressed bytes number is correct */
      if (nbytes != neblock) {
        return -2;
      }

    }
    src += cbytes;
    ctbytes += cbytes;
    _dest += nbytes;
    ntbytes += nbytes;
  } /* Closes j < nsplits */

  if (context->filtercode == BLOSC_SHUFFLE) {
    unshuffle(typesize, blocksize, tmp, dest + offset);
  }
  else if (context->filtercode == BLOSC_BITSHUFFLE) {
    bscount = bitunshuffle(typesize, blocksize, tmp, dest + offset, tmp2);
    if (bscount < 0)
      return bscount;
  }

  if (context->schunk != NULL) {
    uint8_t* filters;
    filters = decode_filters(context->schunk->filters);
    if (filters[0] == BLOSC_DELTA) {
      /* Force the thread in charge of the block 0 to go first */
      pthread_mutex_lock(&context->delta_mutex);
      if (context->dref_not_init) {
        if (offset != 0) {
          pthread_cond_wait(&context->delta_cv, &context->delta_mutex);
        } else {
          context->dref_not_init = 0;
          pthread_cond_broadcast(&context->delta_cv);
        }
      }
      pthread_mutex_unlock(&context->delta_mutex);
      delta_decoder8(dest, offset, blocksize, dest + offset);
    }
    free(filters);
  }

  /* Return the number of uncompressed bytes */
  return ntbytes;
}


/* Serial version for compression/decompression */
static int serial_blosc(struct thread_context* thread_context) {
  blosc_context* context = thread_context->parent_context;
  int32_t j, bsize, leftoverblock;
  int32_t cbytes;

  int32_t ebsize = context->blocksize + context->typesize * (int32_t)sizeof(int32_t);
  int32_t ntbytes = context->num_output_bytes;

  uint8_t* tmp = thread_context->tmp;
  uint8_t* tmp2 = thread_context->tmp2;

  for (j = 0; j < context->nblocks; j++) {
    if (context->compress && !(*(context->header_flags) & BLOSC_MEMCPYED)) {
      _sw32(context->bstarts + j * 4, ntbytes);
    }
    bsize = context->blocksize;
    leftoverblock = 0;
    if ((j == context->nblocks - 1) && (context->leftover > 0)) {
      bsize = context->leftover;
      leftoverblock = 1;
    }
    if (context->compress) {
      if (*(context->header_flags) & BLOSC_MEMCPYED) {
        /* We want to memcpy only */
        memcpy(context->dest + BLOSC_MAX_OVERHEAD + j * context->blocksize,
               context->src + j * context->blocksize,
               bsize);
        cbytes = bsize;
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
      if (*(context->header_flags) & BLOSC_MEMCPYED) {
        /* We want to memcpy only */
        memcpy(context->dest + j * context->blocksize,
               context->src + BLOSC_MAX_OVERHEAD + j * context->blocksize,
               bsize);
        cbytes = bsize;
      }
      else {
        /* Regular decompression */
        cbytes = blosc_d(thread_context, bsize, leftoverblock,
                         context->src + sw32_(context->bstarts + j * 4),
                         context->dest, j * context->blocksize, tmp, tmp2);
      }
    }
    if (cbytes < 0) {
      ntbytes = cbytes;         /* error in blosc_c or blosc_d */
      break;
    }
    ntbytes += cbytes;
  }

  return ntbytes;
}


/* Threaded version for compression/decompression */
static int parallel_blosc(blosc_context* context) {
  int rc;

  /* Set sentinels */
  context->thread_giveup_code = 1;
  context->thread_nblock = -1;
  context->dref_not_init = 1;

  /* Synchronization point for all threads (wait for initialization) */
  WAIT_INIT(-1, context);

  /* Synchronization point for all threads (wait for finalization) */
  WAIT_FINISH(-1, context);

  if (context->thread_giveup_code > 0) {
    /* Return the total bytes (de-)compressed in threads */
    return context->num_output_bytes;
  }
  else {
    /* Compression/decompression gave up.  Return error code. */
    return context->thread_giveup_code;
  }
}


static struct thread_context* create_thread_context(
                                blosc_context* context, int32_t tid) {
  struct thread_context* thread_context;
  int32_t ebsize;

  thread_context = (struct thread_context*)my_malloc(sizeof(struct thread_context));
  thread_context->parent_context = context;
  thread_context->tid = tid;

  ebsize = context->blocksize + context->typesize * (int32_t)sizeof(int32_t);
  thread_context->tmp = my_malloc(context->blocksize + ebsize + context->blocksize);
  thread_context->tmp2 = thread_context->tmp + context->blocksize;
  thread_context->tmp3 = thread_context->tmp + context->blocksize + ebsize;
  thread_context->tmpblocksize = context->blocksize;
  #if defined(HAVE_ZSTD)
  thread_context->zstd_cctx = NULL;
  thread_context->zstd_dctx = NULL;
  #endif

  return thread_context;
}

void free_thread_context(struct thread_context* thread_context) {
  my_free(thread_context->tmp);
  #if defined(HAVE_ZSTD)
  if (thread_context->zstd_cctx != NULL) {
    ZSTD_freeCCtx(thread_context->zstd_cctx);
  }
  if (thread_context->zstd_dctx != NULL) {
    ZSTD_freeDCtx(thread_context->zstd_dctx);
  }
  #endif
  my_free(thread_context);
}

/* Do the compression or decompression of the buffer depending on the
   global params. */
static int do_job(blosc_context* context) {
  int32_t ntbytes;

  /* Run the serial version when nthreads is 1 or when the buffers are
     not larger than blocksize */
  if (context->nthreads == 1 || (context->sourcesize / context->blocksize) <= 1) {
    /* The context for this 'thread' has no been initialized yet */
    if (context->serial_context == NULL) {
      context->serial_context = create_thread_context(context, 0);
    }
    else if (context->blocksize != context->serial_context->tmpblocksize) {
      free_thread_context(context->serial_context);
      context->serial_context = create_thread_context(context, 0);
    }
    ntbytes = serial_blosc(context->serial_context);
  }
  else {
    /* Check whether we need to restart threads */
    blosc_set_nthreads_(context);
    /* and run the job */
    ntbytes = parallel_blosc(context);
  }

  return ntbytes;
}


/* Whether a codec is meant for High Compression Ratios */
/* Include LZ4 + BITSHUFFLE here, but not BloscLZ + BITSHUFFLE because,
   for some reason, the latter couple does not work too well */
#define HCR(codec, filter) ( \
             (((codec) == BLOSC_LZ4) && \
              ((filter) == BLOSC_BITSHUFFLE)) || \
             ((codec) == BLOSC_LZ4HC) || \
             ((codec) == BLOSC_LIZARD) || \
             ((codec) == BLOSC_ZLIB) ||  \
             ((codec) == BLOSC_ZSTD) ? 1 : 0 )

static int32_t compute_blocksize(
    blosc_context* context, int32_t clevel, int32_t typesize, int32_t nbytes,
    int32_t forced_blocksize) {
  int32_t blocksize = nbytes;

  /* Protection against very small buffers */
  if (nbytes < (int32_t)typesize) {
    return 1;
  }

  if (forced_blocksize) {
    blocksize = forced_blocksize;
    /* Check that forced blocksize is not too small */
    if (blocksize < MIN_BUFFERSIZE) {
      blocksize = MIN_BUFFERSIZE;
    }
  }
  else if (nbytes >= L1) {
    blocksize = L1;

    /* For HCR codecs, increase the block sizes by a factor of 2 because they
       are meant for compressing large blocks (i.e. they show a big overhead
       when compressing small ones). */
    if (HCR(context->compcode, context->filtercode)) {
      blocksize *= 2;
    }

    /* Choose a different blocksize depending on the compression level */
    switch (clevel) {
    case 0:
      /* Case of plain copy */
      blocksize /= 4;
      break;
    case 1:
    case 2:
    case 3:
    case 4:
      blocksize *= 1;
      break;
    case 5:
      blocksize *= 2;
      break;
    case 6:
      blocksize *= 4;
      break;
    case 7:
    case 8:
      blocksize *= 8;
      break;
    case 9:
      /* Do not exceed 256 KB for non HCR codecs */
      blocksize *= 8;
      if (HCR(context->compcode, context->filtercode)) {
        blocksize *= 2;
      }
      break;
    }
  }

  /* Check that blocksize is not too large */
  if (blocksize > (int32_t)nbytes) {
    blocksize = nbytes;
  }

  /* blocksize *must absolutely* be a multiple of the typesize */
  if (blocksize > typesize) {
    blocksize = blocksize / typesize * typesize;
  }

  return blocksize;
}

static int initialize_context_compression(
  blosc_context* context,
  size_t sourcesize, const void* src, void* dest, size_t destsize, int clevel,
  int filtercode, size_t typesize, int32_t compressor, int32_t blocksize,
  int32_t nthreads, blosc2_sheader* schunk) {

  /* Set parameters */
  context->compress = 1;
  context->src = (const uint8_t*)src;
  context->dest = (uint8_t*)(dest);
  context->num_output_bytes = 0;
  context->destsize = (int32_t)destsize;
  context->sourcesize = sourcesize;
  context->typesize = typesize;
  context->filtercode = filtercode;
  context->compcode = compressor;
  context->nthreads = nthreads;
  context->end_threads = 0;
  context->clevel = clevel;
  context->schunk = schunk;

  /* Check buffer size limits */
  if (sourcesize > BLOSC_MAX_BUFFERSIZE) {
    /* If buffer is too large, give up. */
    fprintf(stderr, "Input buffer size cannot exceed %d bytes\n",
            BLOSC_MAX_BUFFERSIZE);
    return -1;
  }

  /* Compression level */
  if (clevel < 0 || clevel > 9) {
    /* If clevel not in 0..9, print an error */
    fprintf(stderr, "`clevel` parameter must be between 0 and 9!\n");
    return -10;
  }

  /* Shuffle */
  if ((filtercode < 0) || (filtercode >= BLOSC_LAST_FILTER)) {
    fprintf(stderr, "`filtercode` parameter value `%d` not allowed!\n", filtercode);
    return -10;
  }

  /* Check typesize limits */
  if (context->typesize > BLOSC_MAX_TYPESIZE) {
    /* If typesize is too large, treat buffer as an 1-byte stream. */
    context->typesize = 1;
  }

  /* Get the blocksize */
  context->blocksize = compute_blocksize(
    context, clevel, (int32_t)context->typesize, context->sourcesize, blocksize);

  /* Compute number of blocks in buffer */
  context->nblocks = context->sourcesize / context->blocksize;
  context->leftover = context->sourcesize % context->blocksize;
  context->nblocks = (context->leftover > 0) ? \
   (context->nblocks + 1) : context->nblocks;

  return 1;
}

/* Get the filter code from header flags */
static uint8_t get_filtercode(const uint8_t header_flags, const int32_t typesize) {
  uint8_t filtercode = BLOSC_NOFILTER;

  if (header_flags & BLOSC_DOSHUFFLE & (typesize > 1)) {
    filtercode = BLOSC_SHUFFLE;
  } else if (header_flags & BLOSC_DOBITSHUFFLE) {
    filtercode = BLOSC_BITSHUFFLE;
  }
  return filtercode;
}

static int initialize_context_decompression(
    blosc_context* context, const void* src, void* dest, size_t destsize) {

  int i;

  context->compress = 0;
  context->src = (const uint8_t*)src;
  context->dest = (uint8_t*)dest;
  context->destsize = destsize;
  context->num_output_bytes = 0;
  context->end_threads = 0;

  context->header_flags = (uint8_t*)(context->src + 2); /* flags */
  context->typesize = (int32_t)context->src[3];      /* typesize */
  context->sourcesize = sw32_(context->src + 4);     /* buffer size */
  context->blocksize = sw32_(context->src + 8);      /* block size */
  context->filtercode = get_filtercode(*(context->header_flags), context->typesize);

  /* Check that we have enough space to decompress */
  if (context->sourcesize > (int32_t)destsize) {
    return -1;
  }

  context->bstarts = (uint8_t*)(context->src + 16);
  /* Compute some params */
  /* Total blocks */
  context->nblocks = context->sourcesize / context->blocksize;
  context->leftover = context->sourcesize % context->blocksize;
  context->nblocks = (context->leftover > 0) ? context->nblocks + 1 : context->nblocks;

  return 0;
}

/* Conditions for splitting a block before compressing with a codec. */
static int split_block(int compcode, int typesize, int blocksize) {
  /* Normally all the compressors designed for speed benefit from a
     split.  However, in conducted benchmarks LZ4 seems that it runs
     faster if we don't split, which is quite surprising. */
  return (((compcode == BLOSC_BLOSCLZ) ||
          // (compcode == BLOSC_LZ4) ||
          // (compcode == BLOSC_LIZARD)) ||
          (compcode == BLOSC_SNAPPY)) &&
          (typesize <= MAX_SPLITS) &&
          (blocksize / typesize) >= MIN_BUFFERSIZE);
}

static int write_compression_header(blosc_context* context) {
  int32_t compformat;
  int dont_split;

  /* Write version header for this block */
  context->dest[0] = BLOSC_VERSION_FORMAT;          /* blosc format version */

  /* Write compressor format */
  compformat = -1;
  switch (context->compcode) {
    case BLOSC_BLOSCLZ:
      compformat = BLOSC_BLOSCLZ_FORMAT;
      context->dest[1] = BLOSC_BLOSCLZ_VERSION_FORMAT; /* blosclz format version */
      break;

#if defined(HAVE_LZ4)
    case BLOSC_LZ4:
      compformat = BLOSC_LZ4_FORMAT;
      context->dest[1] = BLOSC_LZ4_VERSION_FORMAT;  /* lz4 format version */
      break;
    case BLOSC_LZ4HC:
      compformat = BLOSC_LZ4HC_FORMAT;
      context->dest[1] = BLOSC_LZ4HC_VERSION_FORMAT; /* lz4hc is the same as lz4 */
      break;
#endif /*  HAVE_LZ4 */

#if defined(HAVE_LIZARD)
    case BLOSC_LIZARD:
      compformat = BLOSC_LIZARD_FORMAT;
      context->dest[1] = BLOSC_LIZARD_VERSION_FORMAT;  /* lizard format version */
      break;
#endif /*  HAVE_LIZARD */

#if defined(HAVE_SNAPPY)
    case BLOSC_SNAPPY:
      compformat = BLOSC_SNAPPY_FORMAT;
      context->dest[1] = BLOSC_SNAPPY_VERSION_FORMAT;  /* snappy format version */
      break;
#endif /*  HAVE_SNAPPY */

#if defined(HAVE_ZLIB)
    case BLOSC_ZLIB:
      compformat = BLOSC_ZLIB_FORMAT;
      context->dest[1] = BLOSC_ZLIB_VERSION_FORMAT;  /* zlib format version */
      break;
#endif /*  HAVE_ZLIB */

#if defined(HAVE_ZSTD)
    case BLOSC_ZSTD:
      compformat = BLOSC_ZSTD_FORMAT;
      context->dest[1] = BLOSC_ZSTD_VERSION_FORMAT;  /* zstd format version */
      break;
#endif /*  HAVE_ZSTD */

    default: {
      char* compname;
      compname = clibcode_to_clibname(compformat);
      fprintf(stderr, "Blosc has not been compiled with '%s' ", compname);
      fprintf(stderr, "compression support.  Please use one having it.");
      return -5;    /* signals no compression support */
      break;
    }
  }

  context->header_flags = context->dest + 2;                       /* flags */
  context->dest[2] = 0;                                          /* zeroes flags */
  context->dest[3] = (uint8_t)context->typesize;                 /* type size */
  _sw32(context->dest + 4, context->sourcesize);                 /* size of the buffer */
  _sw32(context->dest + 8, context->blocksize);                  /* block size */
  context->bstarts = context->dest + 16;                         /* starts for every block */
  context->num_output_bytes = 16 + sizeof(int32_t) * context->nblocks;  /* space for header and pointers */

  if (context->clevel == 0) {
    /* Compression level 0 means buffer to be memcpy'ed */
    *(context->header_flags) |= BLOSC_MEMCPYED;
  }

  if (context->sourcesize < MIN_BUFFERSIZE) {
    /* Buffer is too small.  Try memcpy'ing. */
    *(context->header_flags) |= BLOSC_MEMCPYED;
  }

  if (context->filtercode == BLOSC_SHUFFLE) {
    /* Byte-shuffle is active */
    *(context->header_flags) |= BLOSC_DOSHUFFLE;     /* bit 0 set to one in flags */
  }

  if (context->filtercode == BLOSC_BITSHUFFLE) {
    /* Bit-shuffle is active */
    *(context->header_flags) |= BLOSC_DOBITSHUFFLE;  /* bit 2 set to one in flags */
  }

  dont_split = !split_block(context->compcode, context->typesize, context->blocksize);
  *(context->header_flags) |= dont_split << 4;  /* dont_split is in bit 4 */
  *(context->header_flags) |= compformat << 5;  /* compressor format starts at bit 5 */

  return 1;
}

int blosc_compress_context(blosc_context* context) {
  int32_t ntbytes = 0;

  if (!(*(context->header_flags) & BLOSC_MEMCPYED)) {
    /* Do the actual compression */
    ntbytes = do_job(context);
    if (ntbytes < 0) {
      return -1;
    }
    if ((ntbytes == 0) && (context->sourcesize + BLOSC_MAX_OVERHEAD <= context->destsize)) {
      /* Last chance for fitting `src` buffer in `dest`.  Update flags
       and do a memcpy later on. */
      *(context->header_flags) |= BLOSC_MEMCPYED;
    }
  }

  if (*(context->header_flags) & BLOSC_MEMCPYED) {
    if (context->sourcesize + BLOSC_MAX_OVERHEAD > context->destsize) {
      /* We are exceeding maximum output size */
      ntbytes = 0;
    }
    else if (((context->sourcesize % L1) == 0) || (context->nthreads > 1)) {
      /* More effective with large buffers that are multiples of the
       cache size or multi-cores */
      context->num_output_bytes = BLOSC_MAX_OVERHEAD;
      ntbytes = do_job(context);
      if (ntbytes < 0) {
        return -1;
      }
    }
    else {
      memcpy(context->dest + BLOSC_MAX_OVERHEAD, context->src, context->sourcesize);
      ntbytes = context->sourcesize + BLOSC_MAX_OVERHEAD;
    }
  }

  /* Set the number of compressed bytes in header */
  _sw32(context->dest + 12, ntbytes);

  assert(ntbytes <= context->destsize);
  return ntbytes;
}

/* The public routine for compression with context. */
int blosc2_compress_ctx(
    blosc_context* context, size_t nbytes, const void* src, void* dest,
    size_t destsize) {
  int error, result;

  if (context->compress != 1) {
    fprintf(stderr, "Context is not meant for compression.  Giving up.\n");
    return -10;
  }

  error = initialize_context_compression(
    context, nbytes, src, dest, destsize,
    context->clevel, context->filtercode, context->typesize,
    context->compcode, context->blocksize, context->nthreads,
    context->schunk);
  if (error < 0) { return error; }

  error = write_compression_header(context);
  if (error < 0) { return error; }

  result = blosc_compress_context(context);

  return result;
}

/* The public routine for compression.  See blosc.h for docstrings. */
int blosc_compress(int clevel, int doshuffle, size_t typesize, size_t nbytes,
                   const void* src, void* dest, size_t destsize) {
  int error;
  int result;
  char* envvar;
  blosc_context *cctx;
  blosc2_context_cparams cparams = BLOSC_CPARAMS_DEFAULTS;

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

  /* Check for a BLOSC_TYPESIZE environment variable */
  envvar = getenv("BLOSC_TYPESIZE");
  if (envvar != NULL) {
    long value;
    value = strtol(envvar, NULL, 10);
    if ((value != EINVAL) && (value > 0)) {
      typesize = (int)value;
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
    char *compname;
    blosc_compcode_to_compname(g_compressor, &compname);
    /* Create a context for compression */
    cparams.typesize = typesize;
    cparams.compcode = g_compressor;
    cparams.filtercode = doshuffle;
    cparams.clevel = clevel;
    cparams.nthreads = g_nthreads;
    cctx = blosc2_create_cctx(&cparams);
    /* Do the actual compression */
    result = blosc2_compress_ctx(cctx, nbytes, src, dest, destsize);
    /* Release context resources */
    blosc2_free_ctx(cctx);
    return result;
  }

  pthread_mutex_lock(&global_comp_mutex);

  error = initialize_context_compression(
    g_global_context, nbytes, src, dest, destsize, clevel, doshuffle, typesize,
    g_compressor, g_force_blocksize, g_nthreads, g_schunk);
  if (error < 0) { return error; }

  error = write_compression_header(g_global_context);
  if (error < 0) { return error; }

  result = blosc_compress_context(g_global_context);

  pthread_mutex_unlock(&global_comp_mutex);

  return result;
}

int blosc_run_decompression_with_context(
    blosc_context* context, const void* src, void* dest,
    size_t destsize) {
  uint8_t version;
  uint8_t versionlz;
  uint32_t ctbytes;
  int32_t ntbytes;
  int error;

  error = initialize_context_decompression(context, src, dest, destsize);
  if (error < 0) { return error; }

  /* Read the header block */
  version = context->src[0];                        /* blosc format version */
  versionlz = context->src[1];                      /* blosclz format version */
  ctbytes = sw32_(context->src + 12);               /* compressed buffer size */

  /* Unused values */
  version += 0;                             /* shut up compiler warning */
  versionlz += 0;                           /* shut up compiler warning */
  ctbytes += 0;                             /* shut up compiler warning */

  /* Check whether this buffer is memcpy'ed */
  if (*(context->header_flags) & BLOSC_MEMCPYED) {
    memcpy(dest, (uint8_t*)src + BLOSC_MAX_OVERHEAD, context->sourcesize);
    ntbytes = context->sourcesize;
  }
  else {
    /* Do the actual decompression */
    ntbytes = do_job(context);
    if (ntbytes < 0) {
      return -1;
    }
  }

  assert(ntbytes <= (int32_t)destsize);
  return ntbytes;
}

/* The public routine for decompression with context. */
int blosc2_decompress_ctx(
    blosc_context* context, const void* src, void* dest, size_t destsize) {
  int result;

  if (context->compress != 0) {
    fprintf(stderr, "Context is not meant for decompression.  Giving up.\n");
    return -10;
  }

  result = blosc_run_decompression_with_context(context, src, dest, destsize);

  return result;
}


/* The public routine for decompression.  See blosc.h for docstrings. */
int blosc_decompress(const void* src, void* dest, size_t destsize) {
  int result;
  char* envvar;
  long nthreads;
  blosc_context *dctx;
  blosc2_context_dparams dparams = BLOSC_DPARAMS_DEFAULTS;

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
    dctx = blosc2_create_dctx(&dparams);
    result = blosc2_decompress_ctx(dctx, src, dest, destsize);
    blosc2_free_ctx(dctx);
    return result;
  }

  pthread_mutex_lock(&global_comp_mutex);

  result = blosc_run_decompression_with_context(g_global_context, src, dest,
                                                destsize);

  pthread_mutex_unlock(&global_comp_mutex);

  return result;
}

/* Specific routine optimized for decompression a small number of
   items out of a compressed chunk.  This does not use threads because
   it would affect negatively to performance. */
int _blosc_getitem(const blosc_context* context, const void* src, int start,
    int nitems, void* dest) {
  uint8_t* _src = NULL;             /* current pos for source buffer */
  uint8_t version, versionlz;       /* versions for compressed header */
  uint8_t flags;                    /* flags for header */
  int32_t ntbytes = 0;              /* the number of uncompressed bytes */
  int32_t nblocks;                  /* number of total blocks in buffer */
  int32_t leftover;                 /* extra bytes at end of buffer */
  uint8_t* bstarts;                 /* start pointers for each block */
  int tmp_init = 0;
  int32_t typesize, blocksize, nbytes, ctbytes;
  int32_t j, bsize, bsize2, leftoverblock;
  int32_t cbytes, startb, stopb;
  int stop = start + nitems;
  int32_t ebsize;

  _src = (uint8_t*)(src);

  /* Read the header block */
  version = _src[0];                        /* blosc format version */
  versionlz = _src[1];                      /* blosclz format version */
  flags = _src[2];                          /* flags */
  typesize = (int32_t)_src[3];              /* typesize */
  nbytes = sw32_(_src + 4);                 /* buffer size */
  blocksize = sw32_(_src + 8);              /* block size */
  ctbytes = sw32_(_src + 12);               /* compressed buffer size */

  ebsize = blocksize + typesize * (int32_t)sizeof(int32_t);

  version += 0;                             /* shut up compiler warning */
  versionlz += 0;                           /* shut up compiler warning */
  ctbytes += 0;                             /* shut up compiler warning */

  _src += 16;
  bstarts = _src;
  /* Compute some params */
  /* Total blocks */
  nblocks = nbytes / blocksize;
  leftover = nbytes % blocksize;
  nblocks = (leftover > 0) ? nblocks + 1 : nblocks;
  _src += sizeof(int32_t) * nblocks;

  /* Check region boundaries */
  if ((start < 0) || (start * typesize > nbytes)) {
    fprintf(stderr, "`start` out of bounds");
    return -1;
  }

  if ((stop < 0) || (stop * typesize > nbytes)) {
    fprintf(stderr, "`start`+`nitems` out of bounds");
    return -1;
  }

  for (j = 0; j < nblocks; j++) {
    bsize = blocksize;
    leftoverblock = 0;
    if ((j == nblocks - 1) && (leftover > 0)) {
      bsize = leftover;
      leftoverblock = 1;
    }

    /* Compute start & stop for each block */
    startb = start * typesize - j * blocksize;
    stopb = stop * typesize - j * blocksize;
    if ((startb >= (int)blocksize) || (stopb <= 0)) {
      continue;
    }
    if (startb < 0) {
      startb = 0;
    }
    if (stopb > (int)blocksize) {
      stopb = blocksize;
    }
    bsize2 = stopb - startb;

    /* Do the actual data copy */
    if (flags & BLOSC_MEMCPYED) {
      /* We want to memcpy only */
      memcpy((uint8_t*)dest + ntbytes,
             (uint8_t*)src + BLOSC_MAX_OVERHEAD + j * blocksize + startb,
             bsize2);
      cbytes = bsize2;
    }
    else {
      struct thread_context* scontext = context->serial_context;

      /* Resize the temporaries in serial context if needed */
      if (blocksize != scontext->tmpblocksize) {
        my_free(scontext->tmp);
        scontext->tmp = my_malloc(blocksize + ebsize + blocksize);
        scontext->tmp2 = scontext->tmp + blocksize;
        scontext->tmp3 = scontext->tmp + blocksize + ebsize;
        scontext->tmpblocksize = blocksize;
      }

      /* Regular decompression.  Put results in tmp2. */
      cbytes = blosc_d(context->serial_context, bsize, leftoverblock,
                       (uint8_t*)src + sw32_(bstarts + j * 4),
                       scontext->tmp2, 0, scontext->tmp, scontext->tmp3);
      if (cbytes < 0) {
        ntbytes = cbytes;
        break;
      }
      /* Copy to destination */
      memcpy((uint8_t*)dest + ntbytes, scontext->tmp2 + startb, bsize2);
      cbytes = bsize2;
    }
    ntbytes += cbytes;
  }

  return ntbytes;
}


/* Specific routine optimized for decompression a small number of
   items out of a compressed chunk.  Public non-contextual API. */
int blosc_getitem(const void* src, int start, int nitems, void* dest) {
  uint8_t* _src = (uint8_t*)(src);
  blosc_context context;
  int result;

  /* Minimally populate the context */
  context.typesize = (int32_t)_src[3];
  context.blocksize = sw32_(_src + 8);
  context.header_flags = _src + 2;
  context.filtercode = get_filtercode(*(_src + 2), context.typesize);
  context.schunk = g_schunk;
  context.serial_context = create_thread_context(&context, 0);

  /* Call the actual getitem function */
  result = _blosc_getitem(&context, src, start, nitems, dest);

  /* Release resources */
  free_thread_context(context.serial_context);
  return result;
}

int blosc2_getitem_ctx(blosc_context* context, const void* src, int start,
    int nitems, void* dest) {
  uint8_t* _src = (uint8_t*)(src);
  int result;

  /* Minimally populate the context */
  context->typesize = (int32_t)_src[3];
  context->blocksize = sw32_(_src + 8);
  context->header_flags = _src + 2;
  context->filtercode = get_filtercode(*(_src + 2), context->typesize);
  if (context->serial_context == NULL) {
    context->serial_context = create_thread_context(context, 0);
  }

  /* Call the actual getitem function */
  result = _blosc_getitem(context, src, start, nitems, dest);

  return result;
}


/* Decompress & unshuffle several blocks in a single thread */
static void* t_blosc(void* ctxt) {
  struct thread_context* context = (struct thread_context*)ctxt;
  int32_t cbytes, ntdest;
  int32_t tblocks;              /* number of blocks per thread */
  int32_t leftover2;
  int32_t tblock;               /* limit block on a thread */
  int32_t nblock_;              /* private copy of nblock */
  int32_t bsize, leftoverblock;
  /* Parameters for threads */
  int32_t blocksize;
  int32_t ebsize;
  int32_t compress;
  int32_t maxbytes;
  int32_t ntbytes;
  int32_t flags;
  int32_t nblocks;
  int32_t leftover;
  uint8_t* bstarts;
  const uint8_t* src;
  uint8_t* dest;
  uint8_t* tmp;
  uint8_t* tmp2;
  uint8_t* tmp3;
  int rc;

  while (1) {
    /* Synchronization point for all threads (wait for initialization) */
    WAIT_INIT(NULL, context->parent_context);

    if (context->parent_context->end_threads) {
      break;
    }

    /* Get parameters for this thread before entering the main loop */
    blocksize = context->parent_context->blocksize;
    ebsize = blocksize + context->parent_context->typesize * (int32_t)sizeof(int32_t);
    compress = context->parent_context->compress;
    flags = *(context->parent_context->header_flags);
    maxbytes = context->parent_context->destsize;
    nblocks = context->parent_context->nblocks;
    leftover = context->parent_context->leftover;
    bstarts = context->parent_context->bstarts;
    src = context->parent_context->src;
    dest = context->parent_context->dest;

    /* Resize the temporaries if needed */
    if (blocksize != context->tmpblocksize) {
      my_free(context->tmp);
      context->tmp = my_malloc(blocksize + ebsize + blocksize);
      context->tmp2 = context->tmp + blocksize;
      context->tmp3 = context->tmp + blocksize + ebsize;
      context->tmpblocksize = blocksize;
    }

    tmp = context->tmp;
    tmp2 = context->tmp2;
    tmp3 = context->tmp3;

    ntbytes = 0;                /* only useful for decompression */

    if (compress && !(flags & BLOSC_MEMCPYED)) {
      /* Compression always has to follow the block order */
      pthread_mutex_lock(&context->parent_context->count_mutex);
      context->parent_context->thread_nblock++;
      nblock_ = context->parent_context->thread_nblock;
      pthread_mutex_unlock(&context->parent_context->count_mutex);
      tblock = nblocks;
    }
    else {
      /* Decompression can happen using any order.  We choose
       sequential block order on each thread */

      /* Blocks per thread */
      tblocks = nblocks / context->parent_context->nthreads;
      leftover2 = nblocks % context->parent_context->nthreads;
      tblocks = (leftover2 > 0) ? tblocks + 1 : tblocks;

      nblock_ = context->tid * tblocks;
      tblock = nblock_ + tblocks;
      if (tblock > nblocks) {
        tblock = nblocks;
      }
    }

    /* Loop over blocks */
    leftoverblock = 0;
    while ((nblock_ < tblock) && context->parent_context->thread_giveup_code > 0) {
      bsize = blocksize;
      if (nblock_ == (nblocks - 1) && (leftover > 0)) {
        bsize = leftover;
        leftoverblock = 1;
      }
      if (compress) {
        if (flags & BLOSC_MEMCPYED) {
          /* We want to memcpy only */
          memcpy(dest + BLOSC_MAX_OVERHEAD + nblock_ * blocksize,
                 src + nblock_ * blocksize, bsize);
          cbytes = bsize;
        }
        else {
          /* Regular compression */
          cbytes = blosc_c(context, bsize, leftoverblock, 0,
                           ebsize, src, nblock_ * blocksize, tmp2, tmp, tmp3);
        }
      }
      else {
        if (flags & BLOSC_MEMCPYED) {
          /* We want to memcpy only */
          memcpy(dest + nblock_ * blocksize,
                 src + BLOSC_MAX_OVERHEAD + nblock_ * blocksize, bsize);
          cbytes = bsize;
        }
        else {
          cbytes = blosc_d(context, bsize, leftoverblock,
                           src + sw32_(bstarts + nblock_ * 4),
                           dest, nblock_ * blocksize,
                           tmp, tmp2);
        }
      }

      /* Check whether current thread has to giveup */
      if (context->parent_context->thread_giveup_code <= 0) {
        break;
      }

      /* Check results for the compressed/decompressed block */
      if (cbytes < 0) {            /* compr/decompr failure */
        /* Set giveup_code error */
        pthread_mutex_lock(&context->parent_context->count_mutex);
        context->parent_context->thread_giveup_code = cbytes;
        pthread_mutex_unlock(&context->parent_context->count_mutex);
        break;
      }

      if (compress && !(flags & BLOSC_MEMCPYED)) {
        /* Start critical section */
        pthread_mutex_lock(&context->parent_context->count_mutex);
        ntdest = context->parent_context->num_output_bytes;
        _sw32(bstarts + nblock_ * 4, ntdest); /* update block start counter */
        if ((cbytes == 0) || (ntdest + cbytes > maxbytes)) {
          context->parent_context->thread_giveup_code = 0;  /* uncompressible buffer */
          pthread_mutex_unlock(&context->parent_context->count_mutex);
          break;
        }
        context->parent_context->thread_nblock++;
        nblock_ = context->parent_context->thread_nblock;
        context->parent_context->num_output_bytes += cbytes;           /* update return bytes counter */
        pthread_mutex_unlock(&context->parent_context->count_mutex);
        /* End of critical section */

        /* Copy the compressed buffer to destination */
        memcpy(dest + ntdest, tmp2, cbytes);
      }
      else {
        nblock_++;
        /* Update counter for this thread */
        ntbytes += cbytes;
      }

    } /* closes while (nblock_) */

    /* Sum up all the bytes decompressed */
    if ((!compress || (flags & BLOSC_MEMCPYED)) && context->parent_context->thread_giveup_code > 0) {
      /* Update global counter for all threads (decompression only) */
      pthread_mutex_lock(&context->parent_context->count_mutex);
      context->parent_context->num_output_bytes += ntbytes;
      pthread_mutex_unlock(&context->parent_context->count_mutex);
    }

    /* Meeting point for all threads (wait for finalization) */
    WAIT_FINISH(NULL, context->parent_context);
  }

  /* Cleanup our working space and context */
  free_thread_context(context);

  return (NULL);
}

static int init_threads(blosc_context* context) {
  int32_t tid;
  int rc2;
  struct thread_context* thread_context;

  /* Initialize mutex and condition variable objects */
  pthread_mutex_init(&context->count_mutex, NULL);
  pthread_mutex_init(&context->delta_mutex, NULL);
  pthread_cond_init(&context->delta_cv, NULL);

  /* Set context thread sentinels */
  context->thread_giveup_code = 1;
  context->thread_nblock = -1;

  /* Barrier initialization */
#ifdef _POSIX_BARRIERS_MINE
  pthread_barrier_init(&context->barr_init, NULL, context->nthreads + 1);
  pthread_barrier_init(&context->barr_finish, NULL, context->nthreads + 1);
#else
  pthread_mutex_init(&context->count_threads_mutex, NULL);
  pthread_cond_init(&context->count_threads_cv, NULL);
  context->count_threads = 0;      /* Reset threads counter */
#endif

#if !defined(_WIN32)
  /* Initialize and set thread detached attribute */
  pthread_attr_init(&context->ct_attr);
  pthread_attr_setdetachstate(&context->ct_attr, PTHREAD_CREATE_JOINABLE);
#endif

  /* Make space for thread handlers */
  context->threads = (pthread_t*)my_malloc(context->nthreads * sizeof(pthread_t));
  /* Finally, create the threads */
  for (tid = 0; tid < context->nthreads; tid++) {
    /* Create a thread context thread owns context (will destroy when finished) */
    thread_context = create_thread_context(context, tid);

#if !defined(_WIN32)
    rc2 = pthread_create(&context->threads[tid], &context->ct_attr, t_blosc,
                         (void*)thread_context);
#else
    rc2 = pthread_create(&context->threads[tid], NULL, t_blosc, (void *)thread_context);
#endif
    if (rc2) {
      fprintf(stderr, "ERROR; return code from pthread_create() is %d\n", rc2);
      fprintf(stderr, "\tError detail: %s\n", strerror(rc2));
      return (-1);
    }
  }

  return (0);
}

int blosc_get_nthreads(void)
{
  int ret = g_nthreads;

  return ret;
}

int blosc_set_nthreads(int nthreads_new) {
  int ret = g_nthreads;          /* the previous number of threads */

  /* Check whether the library should be initialized */
  if (!g_initlib) blosc_init();

 if (nthreads_new != ret) {
    /* Re-initialize Blosc */
    blosc_destroy();
    blosc_init();
    g_nthreads = nthreads_new;
    g_global_context->nthreads = nthreads_new;
  }

  return ret;
}

int blosc_set_nthreads_(blosc_context* context) {
  if (context->nthreads <= 0) {
    fprintf(stderr, "Error.  nthreads must be a positive integer");
    return -1;
  }

  /* Launch a new pool of threads */
  if (context->nthreads > 1 && context->nthreads != context->threads_started) {
    blosc_release_threadpool(context);
    init_threads(context);
  }

  /* We have now started the threads */
  context->threads_started = context->nthreads;

  return context->nthreads;
}

char* blosc_get_compressor(void)
{
  char* compname;
  blosc_compcode_to_compname(g_compressor, &compname);

  return compname;
}

int blosc_set_compressor(const char* compname) {
  int code = blosc_compname_to_compcode(compname);

  g_compressor = code;

  /* Check whether the library should be initialized */
  if (!g_initlib) blosc_init();

  return code;
}

char* blosc_list_compressors(void) {
  static int compressors_list_done = 0;
  static char ret[256];

  if (compressors_list_done) return ret;
  ret[0] = '\0';
  strcat(ret, BLOSC_BLOSCLZ_COMPNAME);
#if defined(HAVE_LZ4)
  strcat(ret, ",");
  strcat(ret, BLOSC_LZ4_COMPNAME);
  strcat(ret, ",");
  strcat(ret, BLOSC_LZ4HC_COMPNAME);
#endif /* HAVE_LZ4 */
#if defined(HAVE_LIZARD)
  strcat(ret, ",");
  strcat(ret, BLOSC_LIZARD_COMPNAME);
#endif /* HAVE_LIZARD */
#if defined(HAVE_SNAPPY)
  strcat(ret, ",");
  strcat(ret, BLOSC_SNAPPY_COMPNAME);
#endif /* HAVE_SNAPPY */
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

char* blosc_get_version_string(void) {
  static char ret[256];
  strcpy(ret, BLOSC_VERSION_STRING);
  return ret;
}

int blosc_get_complib_info(char* compname, char** complib, char** version) {
  int clibcode;
  char* clibname;
  char* clibversion = "unknown";

#if (defined(HAVE_LZ4) && defined(LZ4_VERSION_MAJOR)) || \
  (defined(HAVE_LIZARD) && defined(LIZARD_VERSION_MAJOR)) || \
  (defined(HAVE_SNAPPY) && defined(SNAPPY_VERSION)) || \
  (defined(HAVE_ZSTD) && defined(ZSTD_VERSION_MAJOR))
  char sbuffer[256];
#endif

  clibcode = compname_to_clibcode(compname);
  clibname = clibcode_to_clibname(clibcode);

  /* complib version */
  if (clibcode == BLOSC_BLOSCLZ_LIB) {
    clibversion = BLOSCLZ_VERSION_STRING;
  }
#if defined(HAVE_LZ4)
  else if (clibcode == BLOSC_LZ4_LIB) {
#if defined(LZ4_VERSION_MAJOR)
    sprintf(sbuffer, "%d.%d.%d",
            LZ4_VERSION_MAJOR, LZ4_VERSION_MINOR, LZ4_VERSION_RELEASE);
    clibversion = sbuffer;
#endif /* LZ4_VERSION_MAJOR */
  }
#endif /* HAVE_LZ4 */
#if defined(HAVE_LIZARD)
  else if (clibcode == BLOSC_LIZARD_LIB) {
    sprintf(sbuffer, "%d.%d.%d",
            LIZARD_VERSION_MAJOR, LIZARD_VERSION_MINOR, LIZARD_VERSION_RELEASE);
    clibversion = sbuffer;
  }
#endif /* HAVE_LIZARD */
#if defined(HAVE_SNAPPY)
  else if (clibcode == BLOSC_SNAPPY_LIB) {
#if defined(SNAPPY_VERSION)
    sprintf(sbuffer, "%d.%d.%d", SNAPPY_MAJOR, SNAPPY_MINOR, SNAPPY_PATCHLEVEL);
    clibversion = sbuffer;
#endif /* SNAPPY_VERSION */
  }
#endif /* HAVE_SNAPPY */
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

  *complib = strdup(clibname);
  *version = strdup(clibversion);
  return clibcode;
}

/* Return `nbytes`, `cbytes` and `blocksize` from a compressed buffer. */
void blosc_cbuffer_sizes(const void* cbuffer, size_t* nbytes,
                         size_t* cbytes, size_t* blocksize) {
  uint8_t* _src = (uint8_t*)(cbuffer);    /* current pos for source buffer */
  uint8_t version, versionlz;              /* versions for compressed header */

  /* Read the version info (could be useful in the future) */
  version = _src[0];                       /* blosc format version */
  versionlz = _src[1];                     /* blosclz format version */

  version += 0;                            /* shut up compiler warning */
  versionlz += 0;                          /* shut up compiler warning */

  /* Read the interesting values */
  *nbytes = (size_t)sw32_(_src + 4);       /* uncompressed buffer size */
  *blocksize = (size_t)sw32_(_src + 8);    /* block size */
  *cbytes = (size_t)sw32_(_src + 12);      /* compressed buffer size */
}

/* Return `typesize` and `flags` from a compressed buffer. */
void blosc_cbuffer_metainfo(const void* cbuffer, size_t* typesize,
                            int* flags) {
  uint8_t* _src = (uint8_t*)(cbuffer);  /* current pos for source buffer */
  uint8_t version, versionlz;            /* versions for compressed header */

  /* Read the version info (could be useful in the future) */
  version = _src[0];                     /* blosc format version */
  versionlz = _src[1];                   /* blosclz format version */

  version += 0;                             /* shut up compiler warning */
  versionlz += 0;                           /* shut up compiler warning */

  /* Read the interesting values */
  *flags = (int)_src[2];                 /* flags */
  *typesize = (size_t)_src[3];           /* typesize */
}

/* Return version information from a compressed buffer. */
void blosc_cbuffer_versions(const void* cbuffer, int* version,
                            int* versionlz) {
  uint8_t* _src = (uint8_t*)(cbuffer);  /* current pos for source buffer */

  /* Read the version info */
  *version = (int)_src[0];         /* blosc format version */
  *versionlz = (int)_src[1];       /* Lempel-Ziv compressor format version */
}

/* Return the compressor library/format used in a compressed buffer. */
char* blosc_cbuffer_complib(const void* cbuffer) {
  uint8_t* _src = (uint8_t*)(cbuffer);  /* current pos for source buffer */
  int clibcode;
  char* complib;

  /* Read the compressor format/library info */
  clibcode = (_src[2] & 0xe0) >> 5;
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
void blosc_set_schunk(blosc2_sheader* schunk) {
  g_schunk = schunk;
  g_global_context->schunk = schunk;
}

blosc_context* create_context(int nthreads) {
  blosc_context* context = (blosc_context*)my_malloc(sizeof(blosc_context));

  /* Initialize some struct components */
  context->serial_context = NULL;
  context->threads = NULL;

  return context;
}

void blosc_init(void) {
  /* Return if we are already initialized */
  if (g_initlib) return;

  pthread_mutex_init(&global_comp_mutex, NULL);
  g_global_context = create_context(g_nthreads);
  g_global_context->threads_started = 0;
  g_initlib = 1;
}

void blosc_destroy(void) {
  /* Return if Blosc is not initialized */
  if (!g_initlib) return;

  g_initlib = 0;
  blosc_release_threadpool(g_global_context);
  if (g_global_context->serial_context != NULL) {
    free_thread_context(g_global_context->serial_context);
  }
  my_free(g_global_context);
  pthread_mutex_destroy(&global_comp_mutex);
}

int blosc_release_threadpool(blosc_context* context) {
  int32_t t;
  void* status;
  int rc;
  int rc2;

  if (context->threads_started > 0) {
    /* Tell all existing threads to finish */
    context->end_threads = 1;

    /* Sync threads */
    WAIT_INIT(-1, context);

    /* Join exiting threads */
    for (t = 0; t < context->threads_started; t++) {
      rc2 = pthread_join(context->threads[t], &status);
      if (rc2) {
        fprintf(stderr, "ERROR; return code from pthread_join() is %d\n", rc2);
        fprintf(stderr, "\tError detail: %s\n", strerror(rc2));
      }
    }

    /* Release mutex and condition variable objects */
    pthread_mutex_destroy(&context->count_mutex);
    pthread_mutex_destroy(&context->delta_mutex);
    pthread_cond_destroy(&context->delta_cv);

    /* Barriers */
  #ifdef _POSIX_BARRIERS_MINE
    pthread_barrier_destroy(&context->barr_init);
    pthread_barrier_destroy(&context->barr_finish);
  #else
    pthread_mutex_destroy(&context->count_threads_mutex);
    pthread_cond_destroy(&context->count_threads_cv);
  #endif

    /* Thread attributes */
  #if !defined(_WIN32)
    pthread_attr_destroy(&context->ct_attr);
  #endif

    /* Release thread handlers */
    my_free(context->threads);
  }

  context->threads_started = 0;

  return 0;
}

int blosc_free_resources(void) {
  /* Return if Blosc is not initialized */
  if (!g_initlib) return -1;

  return blosc_release_threadpool(g_global_context);
}


/* Contexts */

/* Create a context for compression */
blosc_context* blosc2_create_cctx(blosc2_context_cparams* cparams) {
  int error;
  blosc_context* context = (blosc_context*)my_malloc(sizeof(blosc_context));
  memset(context, 0, sizeof(blosc_context));

  context->compress = 1;   /* meant for compression */
  /* Populate the context, using default values for zeroed values */
  context->typesize = cparams->typesize ? cparams->typesize : 8;
  context->compcode = cparams->compcode ? cparams->compcode : BLOSC_BLOSCLZ;
  context->clevel = cparams->clevel ? cparams->clevel : 5;
  context->filtercode = cparams->filtercode ? cparams->filtercode : BLOSC_SHUFFLE;
  context->blocksize = cparams->blocksize ? cparams->blocksize : 0;
  context->nthreads = cparams->nthreads ? cparams->nthreads : 1;
  context->schunk = cparams->schunk ? cparams->schunk : NULL;

  return context;
}



/* Create a context for decompression */
blosc_context* blosc2_create_dctx(blosc2_context_dparams* dparams) {
  int error;
  blosc_context* context = (blosc_context*)my_malloc(sizeof(blosc_context));
  memset(context, 0, sizeof(blosc_context));

  context->compress = 0;   /* Meant for decompression */
  /* Populate the context, using default values for zeroed values */
  context->nthreads = dparams->nthreads ? dparams->nthreads : 1;
  context->schunk = dparams->schunk ? dparams->schunk : NULL;

  return context;
}

void blosc2_free_ctx(blosc_context* context) {
  blosc_release_threadpool(context);
  if (context->serial_context != NULL) {
    free_thread_context(context->serial_context);
  }
  my_free(context);
}
