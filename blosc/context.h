/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef CONTEXT_H
#define CONTEXT_H

#if defined(_WIN32) && !defined(__GNUC__)
  #include "win32/pthread.h"
#else
  #include <pthread.h>
#endif

/* Have problems using posix barriers when symbol value is 200112L */
/* Requires more investigation, but this will work for the moment */
#if defined(_POSIX_BARRIERS) && ( (_POSIX_BARRIERS - 20012L) >= 0 && _POSIX_BARRIERS != 200112L)
#define BLOSC_POSIX_BARRIERS
#endif

#include "blosc2.h"

#if defined(HAVE_ZSTD)
  #include "zstd.h"
#endif /*  HAVE_ZSTD */

#ifdef HAVE_IPP
  #include <ipps.h>
#endif /* HAVE_IPP */

struct blosc2_context_s {
  const uint8_t* src;
  /* The source buffer */
  uint8_t* dest;
  /* The destination buffer */
  uint8_t header_flags;
  /* Flags for header */
  uint8_t blosc2_flags;
  /* Flags specific for blosc2 */
  int32_t sourcesize;
  /* Number of bytes in source buffer */
  int32_t header_overhead;
  /* The number of bytes in chunk header */
  int32_t nblocks;
  /* Number of total blocks in buffer */
  int32_t leftover;
  /* Extra bytes at end of buffer */
  int32_t blocksize;
  /* Length of the block in bytes */
  int32_t splitmode;
  /* Whether the blocks should be split or not */
  int32_t output_bytes;
  /* Counter for the number of input bytes */
  int32_t srcsize;
  /* Counter for the number of output bytes */
  int32_t destsize;
  /* Maximum size for destination buffer */
  int32_t typesize;
  /* Type size */
  int32_t* bstarts;
  /* Starts for every block inside the compressed buffer */
  int32_t special_type;
  /* Special type for chunk.  0 if not special. */
  int compcode;
  /* Compressor code to use */
  uint8_t compcode_meta;
  /* The metainfo for the compressor code */
  int clevel;
  /* Compression level (1-9) */
  int use_dict;
  /* Whether to use dicts or not */
  void* dict_buffer;
  /* The buffer to keep the trained dictionary */
  int32_t dict_size;
  /* The size of the trained dictionary */
  void* dict_cdict;
  /* The dictionary in digested form for compression */
  void* dict_ddict;
  /* The dictionary in digested form for decompression */
  uint8_t filter_flags;
  /* The filter flags in the filter pipeline */
  uint8_t filters[BLOSC2_MAX_FILTERS];
  /* the (sequence of) filters */
  uint8_t filters_meta[BLOSC2_MAX_FILTERS];
  /* the metainfo for filters */
  blosc2_filter urfilters[BLOSC2_MAX_UDFILTERS];
  /* The user-defined filters */
  blosc2_prefilter_fn prefilter;
  /* prefilter function */
  blosc2_postfilter_fn postfilter;
  /* postfilter function */
  blosc2_prefilter_params *preparams;
  /* prefilter params */
  blosc2_postfilter_params *postparams;
  /* postfilter params */
  bool* block_maskout;
  /* The blocks that are not meant to be decompressed.
   * If NULL (default), all blocks in a chunk should be read. */
  int block_maskout_nitems;
  /* The number of items in block_maskout array (must match
   * the number of blocks in chunk) */
  blosc2_schunk* schunk;
  /* Associated super-chunk (if available) */
  struct thread_context* serial_context;
  /* Cache for temporaries for serial operation */
  int do_compress;
  /* 1 if we are compressing, 0 if decompressing */
  void *btune;
  /* Entry point for BTune persistence between runs */
  blosc2_btune *udbtune;
  /* User-defined BTune parameters */
  /* Threading */
  int16_t nthreads;
  int16_t new_nthreads;
  int16_t threads_started;
  int16_t end_threads;
  pthread_t *threads;
  struct thread_context *thread_contexts; /* only for user-managed threads */
  pthread_mutex_t count_mutex;
#ifdef BLOSC_POSIX_BARRIERS
  pthread_barrier_t barr_init;
  pthread_barrier_t barr_finish;
#else
  int count_threads;
  pthread_mutex_t count_threads_mutex;
  pthread_cond_t count_threads_cv;
#endif
#if !defined(_WIN32)
  pthread_attr_t ct_attr;      /* creation time attrs for threads */
#endif
  int thread_giveup_code;
  /* error code when give up */
  int thread_nblock;       /* block counter */
  int dref_not_init;       /* data ref in delta not initialized */
  pthread_mutex_t delta_mutex;
  pthread_cond_t delta_cv;
};

struct thread_context {
  blosc2_context* parent_context;
  int tid;
  uint8_t* tmp;
  uint8_t* tmp2;
  uint8_t* tmp3;
  uint8_t* tmp4;
  int32_t tmp_blocksize; /* the blocksize for different temporaries */
  size_t tmp_nbytes;   /* keep track of how big the temporary buffers are */
#if defined(HAVE_ZSTD)
  /* The contexts for ZSTD */
  ZSTD_CCtx* zstd_cctx;
  ZSTD_DCtx* zstd_dctx;
#endif /* HAVE_ZSTD */
#ifdef HAVE_IPP
  Ipp8u* lz4_hash_table;
#endif
};


#endif  /* CONTEXT_H */
