/*********************************************************************
    Blosc - Blocked Shuffling and Compression Library

    Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
    https://blosc.org
    License: BSD 3-Clause (see LICENSE.txt)

    See LICENSE.txt for details about copyright and rights to use.

    Regression test for the ZFP decompressor hardening in blosc2-zfp.c.

    The ZFP decoders take the block geometry (blockshape) from the "b2nd"
    metalayer, which is attacker-controlled in a crafted frame, while the real
    output buffer is sized from the chunk-header block size (output_len).
    A blockshape whose decompressed size exceeds output_len must be rejected
    BEFORE zfp_decompress() writes past the buffer (a heap overflow).

    These cases exercise the failure paths so the fix cannot regress silently:
      - blockshape larger than output_len
      - a zero block dimension (must be treated as invalid, not size 0)
      - a blockshape whose prod*typesize overflows int64
**********************************************************************/

#include "b2nd.h"
#include "blosc2.h"
#include "blosc2-zfp.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define TYPESIZE 4   /* float */

static blosc2_schunk *make_schunk(int8_t ndim, const int64_t *shape,
                                  const int32_t *chunkshape, const int32_t *blockshape) {
  blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = TYPESIZE;
  storage.cparams = &cparams;
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    return NULL;
  }
  uint8_t *smeta = NULL;
  int smeta_len = b2nd_serialize_meta(ndim, shape, chunkshape, blockshape, "<f4", 0, &smeta);
  if (smeta_len < 0) {
    blosc2_schunk_free(schunk);
    return NULL;
  }
  /* The metalayer MUST be attached, otherwise the decoders would fail for the
     wrong reason ("b2nd layer not found") and the test would pass vacuously. */
  int rc = blosc2_meta_add(schunk, "b2nd", smeta, smeta_len);
  free(smeta);
  if (rc < 0) {
    blosc2_schunk_free(schunk);
    return NULL;
  }
  return schunk;
}

/* Every ZFP decoder must reject this geometry (return < 0) without writing
   past `output_len`.  Returns 0 on success (rejected), -1 on failure. */
static int expect_rejected(const char *name, int32_t b0, int32_t b1, int32_t output_len) {
  int64_t shape[2]      = { b0, b1 };
  int32_t chunkshape[2] = { b0, b1 };
  int32_t blockshape[2] = { b0, b1 };
  blosc2_schunk *schunk = make_schunk(2, shape, chunkshape, blockshape);
  if (schunk == NULL) {
    printf("  %-28s: could not build schunk -- FAIL\n", name);
    return -1;
  }

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.schunk = schunk;

  /* A short, arbitrary input: the size guard must trigger before any read. */
  uint8_t input[64] = {0};
  uint8_t *output = malloc((size_t) (output_len > 0 ? output_len : 1));

  int rc_acc  = zfp_acc_decompress(input, (int32_t) sizeof(input), output, output_len, 4, &dparams, NULL);
  int rc_prec = zfp_prec_decompress(input, (int32_t) sizeof(input), output, output_len, 20, &dparams, NULL);
  int rc_rate = zfp_rate_decompress(input, (int32_t) sizeof(input), output, output_len, 25, &dparams, NULL);

  free(output);
  blosc2_schunk_free(schunk);

  if (rc_acc >= 0 || rc_prec >= 0 || rc_rate >= 0) {
    printf("  %-28s: NOT rejected (acc=%d prec=%d rate=%d) -- FAIL\n",
           name, rc_acc, rc_prec, rc_rate);
    return -1;
  }
  printf("  %-28s: rejected (acc=%d prec=%d rate=%d) -- OK\n",
         name, rc_acc, rc_prec, rc_rate);
  return 0;
}

int main(void) {
  int result = 0;
  blosc2_init();   // mandatory for initializing the plugin mechanism
  printf("ZFP decompress hardening regression tests:\n");

  /* 64*64*4 = 16384 bytes claimed vs a 256-byte output buffer */
  result |= expect_rejected("oversized blockshape", 64, 64, 256);
  /* a zero block dimension must be invalid input, not a size-0 shortcut */
  result |= expect_rejected("zero block dimension", 0, 64, 256);
  /* prod(blockshape)*typesize overflows int64 (must not wrap past the guard) */
  result |= expect_rejected("int64 overflow blockshape", 2000000000, 2000000000, 256);

  if (result == 0) {
    printf("All ZFP hardening checks passed.\n");
  }
  blosc2_destroy();
  return result;
}
