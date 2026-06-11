/*********************************************************************
    Blosc - Blocked Shuffling and Compression Library

    Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
    https://blosc.org
    License: BSD 3-Clause (see LICENSE.txt)

    See LICENSE.txt for details about copyright and rights to use.

    Regression test for the NDCELL filter hardening in ndcell.c.

    ndcell_backward() takes the block geometry (blockshape) from the "b2nd"
    metalayer, which is attacker-controlled in a crafted frame, while the real
    output block buffer is sized from the chunk-header block size (`length`).
    The old code computed   blocksize = typesize * prod(blockshape)   in 32-bit
    arithmetic: a crafted blockshape whose product wraps back onto `length`
    passed the `length == blocksize` check, after which the scatter-write
    index arithmetic used the *true* (huge) dimensions and wrote past the
    output buffer (heap overflow).

    These cases exercise the rejection paths so the fix cannot regress silently:
      - blockshape whose 32-bit product wraps back onto `length`
      - a zero block dimension (must be invalid, not a size-0 shortcut)
      - a negative block dimension
**********************************************************************/

#include "b2nd.h"
#include "blosc2.h"
#include "ndcell.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int ndcell_backward(const uint8_t *input, uint8_t *output, int32_t length, uint8_t meta,
                    blosc2_dparams *dparams, uint8_t id);

static blosc2_schunk *make_schunk(int8_t ndim, const int64_t *shape, const int32_t *chunkshape,
                                  const int32_t *blockshape, int32_t typesize) {
  blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  storage.cparams = &cparams;
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    return NULL;
  }
  uint8_t *smeta = NULL;
  int smeta_len = b2nd_serialize_meta(ndim, shape, chunkshape, blockshape, "|u1", 0, &smeta);
  if (smeta_len < 0) {
    blosc2_schunk_free(schunk);
    return NULL;
  }
  /* The metalayer MUST be attached, otherwise the decoder would fail for the
     wrong reason ("b2nd layer not found") and the test would pass vacuously. */
  int rc = blosc2_meta_add(schunk, "b2nd", smeta, smeta_len);
  free(smeta);
  if (rc < 0) {
    blosc2_schunk_free(schunk);
    return NULL;
  }
  return schunk;
}

/* ndcell_backward must reject this geometry (return < 0) without writing past
   `length`.  Returns 0 on success (rejected), -1 on failure. */
static int expect_rejected(const char *name, int32_t b0, int32_t b1,
                           int32_t length, int32_t typesize, uint8_t cell_shape) {
  int64_t shape[2]      = { b0, b1 };
  int32_t chunkshape[2] = { b0, b1 };
  int32_t blockshape[2] = { b0, b1 };
  blosc2_schunk *schunk = make_schunk(2, shape, chunkshape, blockshape, typesize);
  if (schunk == NULL) {
    printf("  %-30s: could not build schunk -- FAIL\n", name);
    return -1;
  }

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.schunk = schunk;

  uint8_t *input  = calloc(1, (size_t) (length > 0 ? length : 1));
  uint8_t *output = malloc((size_t) (length > 0 ? length : 1));

  int rc = ndcell_backward(input, output, length, cell_shape, &dparams, 0);

  free(input);
  free(output);
  blosc2_schunk_free(schunk);

  if (rc >= 0) {
    printf("  %-30s: NOT rejected (rc=%d) -- FAIL\n", name, rc);
    return -1;
  }
  printf("  %-30s: rejected (rc=%d) -- OK\n", name, rc);
  return 0;
}

int main(void) {
  int result = 0;
  blosc2_init();   // mandatory for initializing the plugin mechanism
  printf("NDCELL decompress hardening regression tests:\n");

  /* 1*65536*65537 = 0x100010000, low 32 bits = 0x10000 = 65536 == length */
  result |= expect_rejected("32-bit wrap onto length", 65536, 65537, 65536, 1, 2);
  /* a zero block dimension must be invalid input */
  result |= expect_rejected("zero block dimension", 0, 64, 256, 1, 2);
  /* a negative block dimension must be invalid input */
  result |= expect_rejected("negative block dimension", -1, 64, 256, 1, 2);

  if (result == 0) {
    printf("All NDCELL hardening checks passed.\n");
  }
  blosc2_destroy();
  return result;
}
