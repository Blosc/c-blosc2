/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>
  Creation date: 2017-08-29

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/

#include "blosc.h"

/* The size of L1 cache.  32 KB is quite common nowadays. */
#define L1 (32 * 1024)


void btune_cparams(blosc2_context* context);
