/*********************************************************************
    Blosc - Blocked Shuffling and Compression Library

    Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
    https://blosc.org
    License: BSD 3-Clause (see LICENSE.txt)

    See LICENSE.txt for details about copyright and rights to use.

    Regression test for the zfp_getcell() hardening in blosc2-zfp.c.

    zfp_getcell() reads `ndim` straight from the "b2nd" metalayer, which is
    attacker-controlled in a crafted frame, and uses it to drive the writes
    into the fixed-size blockmeta[ZFP_MAX_DIM] array (and the matching reads
    from the metalayer content).  The only guard was an assert(), which is
    compiled out in release builds, so an ndim past ZFP_MAX_DIM (4) read and
    wrote out of bounds.  The item must be rejected (getitem returns < 0),
    not corrupt memory.
**********************************************************************/

#include "blosc-private.h"
#include "b2nd.h"
#include "blosc2/codecs-registry.h"
#include "blosc2.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ZFP supports at most 4 dimensions (ZFP_MAX_DIM); 5 is the smallest value
   that overflows blockmeta[ZFP_MAX_DIM] in zfp_getcell(). */
#define OVERSIZED_NDIM 5

int main(void) {
  blosc2_init();

  int8_t ndim = 2;
  int64_t shape[] = {40, 60};
  int32_t chunkshape[] = {20, 30};
  int32_t blockshape[] = {16, 16};
  int32_t typesize = sizeof(float);
  int64_t nelem = shape[0] * shape[1];
  int64_t size = nelem * typesize;

  float *data = malloc(size);
  for (int64_t i = 0; i < nelem; i++) {
    data[i] = (float) i;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  blosc2_storage b2_storage = {.cparams = &cparams};
  b2_storage.contiguous = true;
  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, ndim, shape, chunkshape, blockshape,
                                        "<f4", 0, NULL, 0);
  b2nd_array_t *arr;
  BLOSC_ERROR(b2nd_from_cbuffer(ctx, &arr, data, size));
  blosc2_schunk *schunk = arr->sc;
  int32_t chunksize = (int32_t) schunk->chunksize;

  /* Compress the first chunk with the ZFP fixed-rate codec. */
  float *data_in = malloc(chunksize);
  uint8_t *chunk_zfp = malloc(chunksize + BLOSC2_MAX_OVERHEAD);
  BLOSC_ERROR(blosc2_schunk_decompress_chunk(schunk, 0, data_in, chunksize));

  blosc2_cparams zcp = BLOSC2_CPARAMS_DEFAULTS;
  zcp.splitmode = BLOSC_NEVER_SPLIT;
  zcp.typesize = typesize;
  zcp.compcode = BLOSC_CODEC_ZFP_FIXED_RATE;
  zcp.compcode_meta = 37;
  zcp.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_NOFILTER;
  zcp.nthreads = 1;
  zcp.blocksize = schunk->blocksize;
  zcp.schunk = schunk;
  blosc2_context *cctx = blosc2_create_cctx(zcp);
  int64_t csize = blosc2_compress_ctx(cctx, data_in, chunksize, chunk_zfp,
                                      chunksize + BLOSC2_MAX_OVERHEAD);
  if (csize <= 0) {
    printf("Could not ZFP-compress the chunk (%" PRId64 ")\n", csize);
    return -1;
  }
  int32_t zfp_cbytes;
  blosc2_cbuffer_sizes(chunk_zfp, NULL, &zfp_cbytes, NULL);

  /* Poison the ndim byte of the b2nd metalayer, as a crafted frame could:
     get_meta_from_header() copies the metalayer verbatim and never checks it. */
  bool poisoned = false;
  for (int i = 0; i < schunk->nmetalayers; i++) {
    if (strcmp("b2nd", schunk->metalayers[i]->name) == 0) {
      schunk->metalayers[i]->content[2] = OVERSIZED_NDIM;
      poisoned = true;
    }
  }
  if (!poisoned) {
    printf("b2nd metalayer not found; cannot run the test\n");
    return -1;
  }

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  dparams.schunk = schunk;
  blosc2_context *dctx = blosc2_create_dctx(dparams);

  /* The getitem cell machinery calls zfp_getcell().  It must reject the bogus
     geometry with an error, not read/write out of bounds. */
  float item;
  int rc = blosc2_getitem_ctx(dctx, chunk_zfp, zfp_cbytes, 0, 1, &item, sizeof(item));
  if (rc >= 0) {
    printf("getitem accepted an out-of-range ndim (rc %d)\n", rc);
    return -1;
  }
  printf("getitem rejected out-of-range ndim as expected (rc %d)\n", rc);

  /* Test truncated metalayer length (e.g. content_len < 3) */
  int32_t orig_content_len = 0;
  for (int i = 0; i < schunk->nmetalayers; i++) {
    if (strcmp("b2nd", schunk->metalayers[i]->name) == 0) {
      schunk->metalayers[i]->content[2] = ndim;  /* restore valid ndim */
      orig_content_len = schunk->metalayers[i]->content_len;
      schunk->metalayers[i]->content_len = 2;  /* truncated */
    }
  }
  rc = blosc2_getitem_ctx(dctx, chunk_zfp, zfp_cbytes, 0, 1, &item, sizeof(item));
  if (rc >= 0) {
    printf("getitem accepted truncated metalayer (rc %d)\n", rc);
    return -1;
  }
  printf("getitem rejected truncated metalayer as expected (rc %d)\n", rc);

  /* Test partially truncated metalayer (truncated before blockshape) */
  for (int i = 0; i < schunk->nmetalayers; i++) {
    if (strcmp("b2nd", schunk->metalayers[i]->name) == 0) {
      schunk->metalayers[i]->content_len = 10;
    }
  }
  rc = blosc2_getitem_ctx(dctx, chunk_zfp, zfp_cbytes, 0, 1, &item, sizeof(item));
  if (rc >= 0) {
    printf("getitem accepted partially truncated metalayer (rc %d)\n", rc);
    return -1;
  }
  printf("getitem rejected partially truncated metalayer as expected (rc %d)\n", rc);

  /* Restore original content_len for clean teardown */
  for (int i = 0; i < schunk->nmetalayers; i++) {
    if (strcmp("b2nd", schunk->metalayers[i]->name) == 0) {
      schunk->metalayers[i]->content_len = orig_content_len;
    }
  }

  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  free(data_in);
  free(chunk_zfp);
  free(data);
  BLOSC_ERROR(b2nd_free(arr));
  BLOSC_ERROR(b2nd_free_ctx(ctx));
  blosc2_destroy();

  return BLOSC2_ERROR_SUCCESS;
}
