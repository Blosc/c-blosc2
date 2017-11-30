/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>
  Creation date: 2017-08-29

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BTUNE_H
#define BTUNE_H

#include "context.h"

/* The size of L1 cache.  32 KB is quite common nowadays. */
#define L1 (32 * 1024)


BLOSC_EXPORT void btune_init(void * config, blosc2_context* cctx, blosc2_context* dctx);

void btune_next_cparams(blosc2_context* context);

void btune_update(blosc2_context* context, double ctime);

void btune_free(blosc2_context* context);

#endif  /* BTUNE_H */
