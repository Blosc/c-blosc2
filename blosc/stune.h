/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef STUNE_H
#define STUNE_H

#include "context.h"

/* The size of L1 cache.  32 KB is quite common nowadays. */
#define L1 (32 * 1024)
/* The size of L2 cache.  256 KB is quite common nowadays. */
#define L2 (256 * 1024)

/* The maximum number of compressed data streams in a block for compression */
#define MAX_STREAMS 16 /* Cannot be larger than 128 */


void blosc_stune_init(void * config, blosc2_context* cctx, blosc2_context* dctx);

void blosc_stune_next_blocksize(blosc2_context * context);

void blosc_stune_next_cparams(blosc2_context * context);

void blosc_stune_update(blosc2_context * context, double ctime);

void blosc_stune_free(blosc2_context * context);

static blosc2_btune BTUNE_DEFAULTS = {
    .btune_init = blosc_stune_init,
    .btune_free = blosc_stune_free,
    .btune_update = blosc_stune_update,
    .btune_next_cparams = blosc_stune_next_cparams,
    .btune_next_blocksize = blosc_stune_next_blocksize,
    .btune_config = NULL,
};


/* Conditions for splitting a block before compressing with a codec. */
static int split_block(blosc2_context* context, int32_t typesize,
                       int32_t blocksize, bool extended_header) {
  switch (context->splitmode) {
    case BLOSC_ALWAYS_SPLIT:
      return 1;
    case BLOSC_NEVER_SPLIT:
      return 0;
    case BLOSC_FORWARD_COMPAT_SPLIT:
    case BLOSC_AUTO_SPLIT:
      // These cases will be handled later
      break;
    default:
      BLOSC_TRACE_WARNING("Unrecognized split mode.  Default to BLOSC_FORWARD_COMPAT_SPLIT");
  }

  int compcode = context->compcode;
  return (
          // fast codecs like blosclz and lz4 prefer to split always
          ((compcode == BLOSC_BLOSCLZ) || (compcode == BLOSC_LZ4)) &&
          (typesize <= MAX_STREAMS) &&
          (blocksize / typesize) >= BLOSC_MIN_BUFFERSIZE);
}

#endif  /* STUNE_H */
