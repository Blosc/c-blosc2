/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>
  Creation date: 2015-12-18

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdio.h>
#include <string.h>
#include "blosc.h"
#include "delta.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))


/* Apply the delta filters to src.  This can never fail. */
void delta_encoder8(uint8_t* filters_chunk, int32_t offset, int32_t nbytes,
                    uint8_t* src, uint8_t* dest) {
  int i;
  uint8_t typesize = *(uint8_t*)(filters_chunk + 3);
  int32_t rbytes = *(int32_t*)(filters_chunk + 4);
  int32_t mbytes;
  int32_t cpy_bytes;
  uint8_t* dref;
  blosc2_context_dparams dparams = BLOSC_DPARAMS_DEFAULTS;
  blosc_context *dctx;

  mbytes = MIN(nbytes, rbytes - offset);
  if (mbytes > 0) {
    /* Fetch mbytes from reference frame */
    dref = malloc((size_t)mbytes);
    if ((mbytes % typesize) != 0) {
      printf("nbytes is not a multiple of typesize (delta_decoder8).  Please report this!\n");
      return;
    }
    /* Create a context for decompressing the interesting part of the reference */
    dparams.nthreads = 1;  /* we don't want to interfere with existing threads */
    dctx = blosc2_create_dctx(&dparams);
    cpy_bytes = blosc2_getitem_ctx(dctx, filters_chunk, offset / typesize, mbytes / typesize, dref);
    blosc2_free_ctx(dctx);
    if (cpy_bytes != mbytes) {
      printf("Error in getting items (delta_decoder8).  Please report this!\n");
      return;
    }

    /* Encode delta */
    for (i = 0; i < mbytes; i++) {
      dest[i] = src[i] - dref[i];
    }
    free(dref);
  }

  /* Copy the leftovers */
  if (nbytes > mbytes) {
    mbytes = MAX(0, mbytes); 	/* negative mbytes are not considered */
    memcpy(dest + mbytes, src + mbytes, nbytes - mbytes);
  }
}


/* Undo the delta filter in dest.  This can never fail. */
void delta_decoder8(uint8_t* filters_chunk, int32_t offset, int32_t nbytes, uint8_t* dest) {
  int i;
  uint8_t typesize = *(uint8_t*)(filters_chunk + 3);
  int32_t rbytes = *(int32_t*)(filters_chunk + 4);
  int32_t mbytes;
  int32_t cpy_bytes;
  uint8_t* dref;
  blosc2_context_dparams dparams = BLOSC_DPARAMS_DEFAULTS;
  blosc_context *dctx;

  mbytes = MIN(nbytes, rbytes - offset);
  if (mbytes > 0) {
    /* Fetch mbytes from reference frame */
    dref = malloc((size_t)mbytes);
    if ((mbytes % typesize) != 0) {
      printf("nbytes is not a multiple of typesize (delta_decoder8).  Please report this!\n");
      return;
    }
    /* Create a context for decompressing the interesting part of the reference */
    dparams.nthreads = 1;  /* we don't want to interfere with existing threads */
    dctx = blosc2_create_dctx(&dparams);
    cpy_bytes = blosc2_getitem_ctx(dctx, filters_chunk, offset / typesize, mbytes / typesize, dref);
    blosc2_free_ctx(dctx);
    if (cpy_bytes != mbytes) {
      printf("Error in getting items (delta_decoder8).  Please report this!\n");
      return;
    }

    /* Decode delta */
    for (i = 0; i < mbytes; i++) {
      dest[i] += dref[i];
    }
    free(dref);
  }

  /* The leftovers are in-place already */

}
