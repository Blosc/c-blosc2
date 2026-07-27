/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* Regression test for reads out of a schunk whose typesize exceeds
   BLOSC_MAX_TYPESIZE (255).  Such chunks are compressed with an internal
   typesize of 1, so blosc2_getitem_ctx() counts items in bytes for them.
   blosc2_schunk_get_slice_buffer() and the single-coordinate path of
   blosc2_schunk_get_sparse_buffer() used to pass item counts in *schunk*
   typesize units instead, which made most non chunk-aligned slices fail and
   some of them return wrong data with a success return code.  See #796. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "blosc2.h"
#include "cutest.h"

#define NITEMS 500
#define ITEMS_PER_CHUNK 100

typedef struct {
  int64_t start;
  int64_t stop;
} test_slice_t;

typedef struct {
  bool contiguous;
  char *urlpath;
} test_storage_t;

CUTEST_TEST_DATA(schunk_large_typesize) {
  int dummy;
};

CUTEST_TEST_SETUP(schunk_large_typesize) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_init();

  // 252 is a control below the cap; the rest are above it
  CUTEST_PARAMETRIZE(typesize, int32_t, CUTEST_DATA(252, 256, 300, 1024));
  // 0 means "let blosc choose"; an explicit one is needed by get_sparse_buffer()
  CUTEST_PARAMETRIZE(nitems_block, int32_t, CUTEST_DATA(0, 25));
  CUTEST_PARAMETRIZE(storage, test_storage_t, CUTEST_DATA(
      {false, NULL},
      {true, NULL},
      {true, "test_schunk_large_typesize.b2frame"},
      {false, "test_schunk_large_typesize.b2frame"},
  ));
  CUTEST_PARAMETRIZE(slice, test_slice_t, CUTEST_DATA(
      {0, 1},        // single item at the very beginning
      {68, 69},      // single item in the middle of a chunk
      {0, 50},       // start of a chunk, partial
      {50, 150},     // crosses a chunk boundary
      {99, 101},     // straddles a chunk boundary
      {100, 200},    // exactly one (non-first) chunk
      {7, 493},      // spans every chunk, both ends partial
      {0, NITEMS},   // the whole schunk
  ));
}

CUTEST_TEST_TEST(schunk_large_typesize) {
  BLOSC_UNUSED_PARAM(data);
  CUTEST_GET_PARAMETER(typesize, int32_t);
  CUTEST_GET_PARAMETER(nitems_block, int32_t);
  CUTEST_GET_PARAMETER(storage, test_storage_t);
  CUTEST_GET_PARAMETER(slice, test_slice_t);

  blosc2_remove_urlpath(storage.urlpath);

  // Position-dependent contents, so that an off-by-typesize read is visible
  size_t nbytes = (size_t) NITEMS * typesize;
  uint8_t *src = malloc(nbytes);
  CUTEST_ASSERT("src alloc failed", src != NULL);
  for (size_t i = 0; i < nbytes; i++) {
    src[i] = (uint8_t) (i % 251);
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  cparams.blocksize = nitems_block * typesize;
  cparams.nthreads = 2;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 2;
  blosc2_storage b2_storage = {.cparams=&cparams, .dparams=&dparams,
                              .urlpath=storage.urlpath, .contiguous=storage.contiguous};
  blosc2_schunk *schunk = blosc2_schunk_new(&b2_storage);
  CUTEST_ASSERT("schunk creation failed", schunk != NULL);

  for (int i = 0; i < NITEMS; i += ITEMS_PER_CHUNK) {
    int64_t nchunks = blosc2_schunk_append_buffer(schunk, src + (size_t) i * typesize,
                                                  ITEMS_PER_CHUNK * typesize);
    CUTEST_ASSERT("append failed", nchunks > 0);
  }

  // Slice read
  size_t slice_nbytes = (size_t) (slice.stop - slice.start) * typesize;
  uint8_t *dest = malloc(slice_nbytes);
  CUTEST_ASSERT("dest alloc failed", dest != NULL);
  memset(dest, 0xAA, slice_nbytes);  // poison, so partial fills stand out

  int rc = blosc2_schunk_get_slice_buffer(schunk, slice.start, slice.stop, dest);
  CUTEST_ASSERT("get_slice_buffer failed", rc >= 0);
  CUTEST_ASSERT("get_slice_buffer returned wrong data",
                memcmp(dest, src + (size_t) slice.start * typesize, slice_nbytes) == 0);

  // Sparse read of the slice bounds; only supported with an explicit blocksize
  if (nitems_block > 0) {
    int64_t coords[2] = {slice.start, slice.stop - 1};
    uint8_t *sparse = malloc((size_t) 2 * typesize);
    CUTEST_ASSERT("sparse alloc failed", sparse != NULL);
    for (int64_t ncoords = 1; ncoords <= 2; ncoords++) {
      // ncoords == 1 takes the getitem path, ncoords > 1 the block-decode one
      memset(sparse, 0xAA, (size_t) 2 * typesize);
      rc = blosc2_schunk_get_sparse_buffer(schunk, ncoords, coords, sparse);
      CUTEST_ASSERT("get_sparse_buffer failed", rc >= 0);
      for (int64_t i = 0; i < ncoords; i++) {
        CUTEST_ASSERT("get_sparse_buffer returned wrong data",
                      memcmp(sparse + i * typesize, src + coords[i] * typesize,
                             (size_t) typesize) == 0);
      }
    }
    free(sparse);
  }

  free(dest);
  free(src);
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(storage.urlpath);

  return 0;
}

CUTEST_TEST_TEARDOWN(schunk_large_typesize) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_destroy();
}

int main(void) {
  CUTEST_TEST_RUN(schunk_large_typesize);
}
