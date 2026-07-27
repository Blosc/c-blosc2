/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* Tests for blosc2_getitem_bytes_ctx(), the byte-counting counterpart of
   blosc2_getitem_ctx().  The unit of the latter is the typesize the *chunk*
   records, which is 1 for typesizes above BLOSC_MAX_TYPESIZE; bytes do not
   change meaning, so generic callers can use this one without knowing whether
   the cap kicked in. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "blosc2.h"
#include "cutest.h"

#define NITEMS 200

typedef struct {
  int64_t start;
  int64_t stop;
} test_range_t;

CUTEST_TEST_DATA(getitem_bytes) {
  int dummy;
};

CUTEST_TEST_SETUP(getitem_bytes) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_init();

  // 4 and 252 are stored as-is; 256 and 1024 are capped to 1 in the chunk
  CUTEST_PARAMETRIZE(typesize, int32_t, CUTEST_DATA(4, 252, 256, 1024));
  // clevel 0 produces a memcpyed chunk, which takes a different getitem path
  CUTEST_PARAMETRIZE(clevel, int32_t, CUTEST_DATA(0, 5));
  CUTEST_PARAMETRIZE(range, test_range_t, CUTEST_DATA(
      {0, 1},          // single item at the start
      {17, 18},        // single item in the middle
      {5, 10},         // a few items
      {199, NITEMS},   // last item
      {0, NITEMS},     // everything
  ));
}

CUTEST_TEST_TEST(getitem_bytes) {
  BLOSC_UNUSED_PARAM(data);
  CUTEST_GET_PARAMETER(typesize, int32_t);
  CUTEST_GET_PARAMETER(clevel, int32_t);
  CUTEST_GET_PARAMETER(range, test_range_t);

  int32_t srcsize = NITEMS * typesize;
  uint8_t *src = malloc((size_t) srcsize);
  CUTEST_ASSERT("src alloc failed", src != NULL);
  for (int32_t i = 0; i < srcsize; i++) {
    src[i] = (uint8_t) (i % 251);  // position-dependent
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  cparams.clevel = clevel;
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  CUTEST_ASSERT("cctx create failed", cctx != NULL);

  uint8_t *chunk = malloc((size_t) srcsize + BLOSC2_MAX_OVERHEAD);
  CUTEST_ASSERT("chunk alloc failed", chunk != NULL);
  int csize = blosc2_compress_ctx(cctx, src, srcsize, chunk, srcsize + BLOSC2_MAX_OVERHEAD);
  blosc2_free_ctx(cctx);
  CUTEST_ASSERT("compress failed", csize > 0);

  blosc2_context *dctx = blosc2_create_dctx(BLOSC2_DPARAMS_DEFAULTS);
  CUTEST_ASSERT("dctx create failed", dctx != NULL);

  // The typesize the chunk actually records, which is what getitem counts in
  int32_t stored_typesize = typesize <= BLOSC_MAX_TYPESIZE ? typesize : 1;

  int32_t offset = (int32_t) range.start * typesize;
  int32_t nbytes = (int32_t) (range.stop - range.start) * typesize;
  uint8_t *dest = malloc((size_t) nbytes);
  uint8_t *dest_items = malloc((size_t) nbytes);
  CUTEST_ASSERT("dest alloc failed", dest != NULL && dest_items != NULL);
  memset(dest, 0xAA, (size_t) nbytes);
  memset(dest_items, 0xAA, (size_t) nbytes);

  int rc = blosc2_getitem_bytes_ctx(dctx, chunk, csize, offset, nbytes, dest, nbytes);
  CUTEST_ASSERT("getitem_bytes returned wrong size", rc == nbytes);
  CUTEST_ASSERT("getitem_bytes returned wrong data",
                memcmp(dest, src + offset, (size_t) nbytes) == 0);

  // Must agree with the item API when the latter is given chunk-unit arguments
  rc = blosc2_getitem_ctx(dctx, chunk, csize, offset / stored_typesize,
                          nbytes / stored_typesize, dest_items, nbytes);
  CUTEST_ASSERT("getitem returned wrong size", rc == nbytes);
  CUTEST_ASSERT("getitem_bytes disagrees with getitem",
                memcmp(dest, dest_items, (size_t) nbytes) == 0);

  // Argument checking, exercised once per chunk rather than per range
  if (range.start == 0 && range.stop == 1) {
    uint8_t scratch[8];
    CUTEST_ASSERT("negative offset not rejected",
                  blosc2_getitem_bytes_ctx(dctx, chunk, csize, -1, typesize,
                                           scratch, sizeof(scratch)) < 0);
    CUTEST_ASSERT("negative nbytes not rejected",
                  blosc2_getitem_bytes_ctx(dctx, chunk, csize, 0, -1,
                                           scratch, sizeof(scratch)) < 0);
    CUTEST_ASSERT("undersized dest not rejected",
                  blosc2_getitem_bytes_ctx(dctx, chunk, csize, 0, srcsize,
                                           scratch, sizeof(scratch)) < 0);
    CUTEST_ASSERT("out of bounds read not rejected",
                  blosc2_getitem_bytes_ctx(dctx, chunk, csize, srcsize, typesize,
                                           dest, nbytes) < 0);
    // An empty request is a no-op, not an error
    CUTEST_ASSERT("empty request rejected",
                  blosc2_getitem_bytes_ctx(dctx, chunk, csize, 0, 0,
                                           scratch, sizeof(scratch)) == 0);
    // Ranges must align to the *stored* typesize.  Above the cap it is 1, so
    // nothing is ever misaligned there --- which is the point of the API.
    if (stored_typesize > 1) {
      CUTEST_ASSERT("misaligned offset not rejected",
                    blosc2_getitem_bytes_ctx(dctx, chunk, csize, 1, typesize,
                                             dest, nbytes) < 0);
      CUTEST_ASSERT("misaligned nbytes not rejected",
                    blosc2_getitem_bytes_ctx(dctx, chunk, csize, 0, typesize - 1,
                                             dest, nbytes) < 0);
    }
    else {
      CUTEST_ASSERT("byte-granular read rejected for a capped typesize",
                    blosc2_getitem_bytes_ctx(dctx, chunk, csize, 1, 3,
                                             scratch, sizeof(scratch)) == 3);
      CUTEST_ASSERT("byte-granular read returned wrong data",
                    memcmp(scratch, src + 1, 3) == 0);
    }
  }

  blosc2_free_ctx(dctx);
  free(dest_items);
  free(dest);
  free(chunk);
  free(src);

  return 0;
}

CUTEST_TEST_TEARDOWN(getitem_bytes) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_destroy();
}

int main(void) {
  CUTEST_TEST_RUN(getitem_bytes);
}
