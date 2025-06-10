/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_STUNE_H
#define BLOSC_STUNE_H

#include "context.h"

#include <stdint.h>

/* The size of L1 cache.  32 KB is quite common nowadays. */
#define L1 (32 * 1024)
/* The size of L2 cache.  256 KB is quite common nowadays. */
#define L2 (256 * 1024)

/* The maximum number of compressed data streams in a block for compression */
#define MAX_STREAMS 16 /* Cannot be larger than 128 */

#define BLOSC_STUNE 0

int blosc_stune_init(void * config, blosc2_context* cctx, blosc2_context* dctx);

int blosc_stune_next_blocksize(blosc2_context * context);

int blosc_stune_next_cparams(blosc2_context * context);

int blosc_stune_update(blosc2_context * context, double ctime);

int blosc_stune_free(blosc2_context * context);

/* Conditions for splitting a block before compressing with a codec. */
int split_block(blosc2_context *context, int32_t typesize, int32_t blocksize);

#endif  /* BLOSC_STUNE_H */
