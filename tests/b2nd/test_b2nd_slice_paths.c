/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* Slice reads and writes through each internal path of get_set_slice():
   whole chunks decompressed into (compressed from) the buffer, chunks
   scattered to (gathered from) the buffer by the worker threads, and the
   chunk-sized scratch fallback (DELTA).  Chunk shapes with edge padding,
   blocks that do not divide the chunks, multi-chunk and partial slices,
   buffers larger than the slice, one and several threads. */

#include "test_common.h"

#define NSLICES 6

typedef struct {
  int8_t ndim;
  int64_t shape[B2ND_MAX_DIM];
  int32_t chunkshape[B2ND_MAX_DIM];
  int32_t blockshape[B2ND_MAX_DIM];
  int64_t start[NSLICES][B2ND_MAX_DIM];
  int64_t stop[NSLICES][B2ND_MAX_DIM];
} slice_paths_t;

typedef struct {
  bool delta;
  int16_t nthreads;
} slice_params_t;

static int64_t nitems_of(int8_t ndim, const int64_t *shape) {
  int64_t n = 1;
  for (int i = 0; i < ndim; ++i) {
    n *= shape[i];
  }
  return n;
}

// Item index of `pos` in a C-order array of shape `shape`
static int64_t c_index(int8_t ndim, const int64_t *shape, const int64_t *pos) {
  int64_t idx = 0;
  for (int i = 0; i < ndim; ++i) {
    idx = idx * shape[i] + pos[i];
  }
  return idx;
}

// Check `out` (shape outshape, the slice at its origin) against `ref` (the
// whole array): items of the slice must match, the rest must be zero.
static bool check_slice(int8_t ndim, const int64_t *shape, const uint32_t *ref,
                        const int64_t *start, const int64_t *stop,
                        const int64_t *outshape, const uint32_t *out) {
  int64_t n = nitems_of(ndim, outshape);
  for (int64_t k = 0; k < n; ++k) {
    int64_t pos[B2ND_MAX_DIM];
    int64_t rem = k;
    bool inside = true;
    for (int i = ndim - 1; i >= 0; --i) {
      pos[i] = rem % outshape[i];
      rem /= outshape[i];
      inside &= pos[i] < stop[i] - start[i];
      pos[i] += start[i];
    }
    uint32_t expected = inside ? ref[c_index(ndim, shape, pos)] : 0;
    if (out[k] != expected) {
      return false;
    }
  }
  return true;
}

CUTEST_TEST_SETUP(slice_paths) {
  blosc2_init();

  CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
      {false, false},
      {true, false},
      {true, true},
      {false, true},
  ));
  CUTEST_PARAMETRIZE(params, slice_params_t, CUTEST_DATA(
      {false, 1},
      {false, 4},
      {true, 4},
  ));
  CUTEST_PARAMETRIZE(shapes, slice_paths_t, CUTEST_DATA(
      // Chunks of whole rows: direct path for chunk-aligned row ranges
      {3, {41, 20, 12}, {4, 20, 12}, {1, 10, 12},
       {{0, 0, 0}, {4, 0, 0}, {3, 0, 0}, {8, 5, 0}, {40, 0, 0}, {0, 0, 0}},
       {{41, 20, 12}, {36, 20, 12}, {9, 20, 12}, {33, 6, 12}, {41, 20, 12}, {16, 20, 12}}},
      // Padded edge chunks, blocks across rows: scatter/gather paths
      {3, {30, 25, 18}, {8, 10, 6}, {4, 5, 3},
       {{0, 0, 0}, {3, 2, 1}, {8, 10, 6}, {24, 20, 12}, {7, 0, 5}, {0, 9, 0}},
       {{30, 25, 18}, {29, 24, 17}, {16, 20, 12}, {30, 25, 18}, {9, 25, 7}, {30, 11, 18}}},
      // Blocks not dividing the chunks (padded blocks)
      {3, {23, 19, 11}, {7, 9, 5}, {3, 4, 2},
       {{0, 0, 0}, {6, 8, 4}, {0, 0, 0}, {21, 18, 10}, {7, 9, 5}, {2, 0, 3}},
       {{23, 19, 11}, {15, 12, 9}, {7, 9, 5}, {23, 19, 11}, {14, 18, 10}, {20, 19, 4}}},
      // 1-dim
      {1, {1000}, {100}, {25},
       {{0}, {100}, {37}, {950}, {0}, {499}},
       {{1000}, {300}, {963}, {1000}, {1}, {501}}},
      // 4-dim, unit leading dims in the chunks
      {4, {6, 10, 12, 8}, {1, 5, 12, 8}, {1, 1, 12, 8},
       {{0, 0, 0, 0}, {2, 5, 0, 0}, {1, 3, 2, 1}, {0, 0, 0, 0}, {5, 0, 0, 0}, {3, 2, 0, 0}},
       {{6, 10, 12, 8}, {4, 10, 12, 8}, {5, 9, 11, 7}, {1, 5, 12, 8}, {6, 10, 12, 8}, {4, 7, 12, 8}}},
  ));
}

CUTEST_TEST_TEST(slice_paths) {
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(params, slice_params_t);
  CUTEST_GET_PARAMETER(shapes, slice_paths_t);

  char *urlpath = "test_b2nd_slice_paths.b2frame";
  blosc2_remove_urlpath(urlpath);

  int8_t ndim = shapes.ndim;
  int32_t typesize = sizeof(uint32_t);
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = params.nthreads;
  cparams.typesize = typesize;
  if (params.delta) {
    cparams.filters[BLOSC2_MAX_FILTERS - 2] = BLOSC_DELTA;
  }
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = params.nthreads;
  blosc2_storage b2_storage = {.cparams=&cparams, .dparams=&dparams};
  if (backend.persistent) {
    b2_storage.urlpath = urlpath;
  }
  b2_storage.contiguous = backend.contiguous;

  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, ndim, shapes.shape, shapes.chunkshape,
                                        shapes.blockshape, NULL, 0, NULL, 0);
  int64_t nitems = nitems_of(ndim, shapes.shape);
  uint32_t *ref = malloc(nitems * typesize);
  for (int64_t i = 0; i < nitems; ++i) {
    ref[i] = (uint32_t) (i * 7 + 1);
  }
  b2nd_array_t *array;
  B2ND_TEST_ASSERT(b2nd_from_cbuffer(ctx, &array, ref, nitems * typesize));

  for (int s = 0; s < NSLICES; ++s) {
    const int64_t *start = shapes.start[s];
    const int64_t *stop = shapes.stop[s];
    int64_t sliceshape[B2ND_MAX_DIM];
    int64_t bigshape[B2ND_MAX_DIM];
    for (int i = 0; i < ndim; ++i) {
      sliceshape[i] = stop[i] - start[i];
      bigshape[i] = sliceshape[i] + 2;
    }

    // Get into an exact buffer (pre-filled, so unwritten items would show)
    int64_t n = nitems_of(ndim, sliceshape);
    uint32_t *out = malloc(n * typesize);
    memset(out, 0xa5, n * typesize);
    B2ND_TEST_ASSERT(b2nd_get_slice_cbuffer(array, start, stop, out, sliceshape, n * typesize));
    CUTEST_ASSERT("get_slice into an exact buffer is wrong",
                  check_slice(ndim, shapes.shape, ref, start, stop, sliceshape, out));
    free(out);

    // Get into a larger buffer: the items outside the slice must be zeroed
    int64_t nbig = nitems_of(ndim, bigshape);
    uint32_t *big = malloc(nbig * typesize);
    memset(big, 0xa5, nbig * typesize);
    B2ND_TEST_ASSERT(b2nd_get_slice_cbuffer(array, start, stop, big, bigshape, nbig * typesize));
    CUTEST_ASSERT("get_slice into a larger buffer is wrong",
                  check_slice(ndim, shapes.shape, ref, start, stop, bigshape, big));

    // Set from a larger buffer, then compare the whole array
    for (int64_t k = 0; k < nbig; ++k) {
      big[k] = (uint32_t) (k * 13 + s + 3);
    }
    B2ND_TEST_ASSERT(b2nd_set_slice_cbuffer(big, bigshape, nbig * typesize, start, stop, array));
    for (int64_t k = 0; k < n; ++k) {
      int64_t pos[B2ND_MAX_DIM];
      int64_t bpos[B2ND_MAX_DIM];
      int64_t rem = k;
      for (int i = ndim - 1; i >= 0; --i) {
        bpos[i] = rem % sliceshape[i];
        rem /= sliceshape[i];
        pos[i] = bpos[i] + start[i];
      }
      ref[c_index(ndim, shapes.shape, pos)] = big[c_index(ndim, bigshape, bpos)];
    }
    free(big);
    uint32_t *all = malloc(nitems * typesize);
    int64_t zeros[B2ND_MAX_DIM] = {0};
    B2ND_TEST_ASSERT(b2nd_to_cbuffer(array, all, nitems * typesize));
    CUTEST_ASSERT("set_slice is wrong",
                  check_slice(ndim, shapes.shape, ref, zeros, shapes.shape, shapes.shape, all));
    free(all);
  }

  // The same contents through a reopened handle
  if (backend.persistent) {
    B2ND_TEST_ASSERT(b2nd_free(array));
    B2ND_TEST_ASSERT(b2nd_open(urlpath, &array));
    uint32_t *all = malloc(nitems * typesize);
    int64_t zeros[B2ND_MAX_DIM] = {0};
    B2ND_TEST_ASSERT(b2nd_to_cbuffer(array, all, nitems * typesize));
    CUTEST_ASSERT("reopened contents are wrong",
                  check_slice(ndim, shapes.shape, ref, zeros, shapes.shape, shapes.shape, all));
    free(all);
  }

  free(ref);
  B2ND_TEST_ASSERT(b2nd_free(array));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));
  blosc2_remove_urlpath(urlpath);

  return 0;
}

CUTEST_TEST_TEARDOWN(slice_paths) {
  blosc2_destroy();
}


/* Uninitialized chunks read as zeros on the paths that decompress whole
   chunks or scatter them; a user postfilter keeps being applied on every
   path. */

static int add_one_postfilter(blosc2_postfilter_params *params) {
  const uint32_t *in = (const uint32_t *) params->input;
  uint32_t *out = (uint32_t *) params->output;
  for (int32_t i = 0; i < params->size / (int32_t) sizeof(uint32_t); ++i) {
    out[i] = in[i] + 1;
  }
  return 0;
}

CUTEST_TEST_SETUP(slice_special) {
  blosc2_init();
  CUTEST_PARAMETRIZE(nthreads, int16_t, CUTEST_DATA(1, 4));
}

CUTEST_TEST_TEST(slice_special) {
  CUTEST_GET_PARAMETER(nthreads, int16_t);
  int8_t ndim = 3;
  int64_t shape[] = {30, 25, 18};
  int32_t typesize = sizeof(uint32_t);
  int64_t nitems = nitems_of(ndim, shape);
  int64_t zeros[B2ND_MAX_DIM] = {0};
  // Rows (direct path) and small blocks (scatter path)
  int32_t chunkshapes[2][3] = {{6, 25, 18}, {8, 10, 6}};
  int32_t blockshapes[2][3] = {{2, 25, 18}, {4, 5, 3}};

  for (int c = 0; c < 2; ++c) {
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.nthreads = nthreads;
    cparams.typesize = typesize;
    blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
    dparams.nthreads = nthreads;
    blosc2_storage b2_storage = {.cparams=&cparams, .dparams=&dparams, .contiguous=true};
    b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, ndim, shape, chunkshapes[c], blockshapes[c],
                                          NULL, 0, NULL, 0);

    b2nd_array_t *array;
    B2ND_TEST_ASSERT(b2nd_uninit(ctx, &array));
    uint32_t *all = malloc(nitems * typesize);
    memset(all, 0xa5, nitems * typesize);
    B2ND_TEST_ASSERT(b2nd_to_cbuffer(array, all, nitems * typesize));
    for (int64_t i = 0; i < nitems; ++i) {
      CUTEST_ASSERT("uninitialized chunks must read as zeros", all[i] == 0);
    }
    B2ND_TEST_ASSERT(b2nd_free(array));
    B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));

    // With a postfilter on the decompression context
    blosc2_postfilter_params postparams = {0};
    dparams.postfilter = add_one_postfilter;
    dparams.postparams = &postparams;
    ctx = b2nd_create_ctx(&b2_storage, ndim, shape, chunkshapes[c], blockshapes[c],
                          NULL, 0, NULL, 0);
    uint32_t *ref = malloc(nitems * typesize);
    for (int64_t i = 0; i < nitems; ++i) {
      ref[i] = (uint32_t) (i * 3);
    }
    B2ND_TEST_ASSERT(b2nd_from_cbuffer(ctx, &array, ref, nitems * typesize));
    for (int64_t i = 0; i < nitems; ++i) {
      ref[i] += 1;
    }
    B2ND_TEST_ASSERT(b2nd_to_cbuffer(array, all, nitems * typesize));
    CUTEST_ASSERT("full read with a postfilter is wrong",
                  check_slice(ndim, shape, ref, zeros, shape, shape, all));
    int64_t start[] = {5, 3, 2};
    int64_t stop[] = {29, 24, 17};
    int64_t sliceshape[] = {24, 21, 15};
    B2ND_TEST_ASSERT(b2nd_get_slice_cbuffer(array, start, stop, all, sliceshape,
                                            nitems_of(ndim, sliceshape) * typesize));
    CUTEST_ASSERT("partial read with a postfilter is wrong",
                  check_slice(ndim, shape, ref, start, stop, sliceshape, all));
    free(ref);
    free(all);
    B2ND_TEST_ASSERT(b2nd_free(array));
    B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));
  }
  return 0;
}

CUTEST_TEST_TEARDOWN(slice_special) {
  blosc2_destroy();
}


/* Uninitialized chunks read as zeros on every path, also where they decode
   into scratch memory that is reused: each slice operation first runs on an
   initialized array, which leaves its items in the per-thread scratch.  A
   partial write must also store zeros around the slice. */

typedef struct {
  bool delta;
  bool set;
  int64_t start[3];
  int64_t stop[3];
} uninit_case_t;

CUTEST_TEST_SETUP(slice_uninit) {
  blosc2_init();
  CUTEST_PARAMETRIZE(nthreads, int16_t, CUTEST_DATA(1, 4));
  CUTEST_PARAMETRIZE(op, uninit_case_t, CUTEST_DATA(
      {false, false, {1, 2, 3}, {5, 6, 7}},      // compact get (one block)
      {false, false, {1, 2, 3}, {40, 40, 40}},   // scatter get
      {true, false, {1, 2, 3}, {40, 40, 40}},    // scratch get (DELTA)
      {false, true, {0, 0, 0}, {40, 64, 64}},    // gather set
      {true, true, {1, 2, 3}, {9, 10, 11}},      // scratch set (DELTA)
  ));
}

CUTEST_TEST_TEST(slice_uninit) {
  CUTEST_GET_PARAMETER(nthreads, int16_t);
  CUTEST_GET_PARAMETER(op, uninit_case_t);
  int8_t ndim = 3;
  int64_t shape[] = {64, 64, 64};
  // 64 KB blocks, 1 MB chunks: both kinds of scratch come from the cache
  int32_t chunkshape[] = {64, 64, 64};
  int32_t blockshape[] = {16, 32, 32};
  int32_t typesize = sizeof(uint32_t);
  int64_t nitems = nitems_of(ndim, shape);
  int64_t zeros[B2ND_MAX_DIM] = {0};
  int64_t sliceshape[3];
  for (int i = 0; i < ndim; ++i) {
    sliceshape[i] = op.stop[i] - op.start[i];
  }
  int64_t n = nitems_of(ndim, sliceshape);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = nthreads;
  cparams.typesize = typesize;
  if (op.delta) {
    cparams.filters[BLOSC2_MAX_FILTERS - 2] = BLOSC_DELTA;
  }
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = nthreads;
  blosc2_storage b2_storage = {.cparams=&cparams, .dparams=&dparams, .contiguous=true};
  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, ndim, shape, chunkshape, blockshape,
                                        NULL, 0, NULL, 0);
  uint32_t *ref = malloc(nitems * typesize);
  for (int64_t i = 0; i < nitems; ++i) {
    ref[i] = (uint32_t) (i * 7 + 1);
  }
  b2nd_array_t *dirty;
  B2ND_TEST_ASSERT(b2nd_from_cbuffer(ctx, &dirty, ref, nitems * typesize));
  b2nd_array_t *array;
  B2ND_TEST_ASSERT(b2nd_uninit(ctx, &array));

  uint32_t *out = malloc(n * typesize);
  if (op.set) {
    for (int64_t k = 0; k < n; ++k) {
      out[k] = (uint32_t) (k * 13 + 5);
    }
    B2ND_TEST_ASSERT(b2nd_set_slice_cbuffer(out, sliceshape, n * typesize, op.start, op.stop, dirty));
    B2ND_TEST_ASSERT(b2nd_set_slice_cbuffer(out, sliceshape, n * typesize, op.start, op.stop, array));
    // Expect the written items, and zeros everywhere else
    memset(ref, 0, nitems * typesize);
    for (int64_t k = 0; k < n; ++k) {
      int64_t pos[3];
      int64_t rem = k;
      for (int i = ndim - 1; i >= 0; --i) {
        pos[i] = rem % sliceshape[i] + op.start[i];
        rem /= sliceshape[i];
      }
      ref[c_index(ndim, shape, pos)] = out[k];
    }
    uint32_t *all = malloc(nitems * typesize);
    B2ND_TEST_ASSERT(b2nd_to_cbuffer(array, all, nitems * typesize));
    CUTEST_ASSERT("a partial write into an uninitialized chunk stored stale items",
                  check_slice(ndim, shape, ref, zeros, shape, shape, all));
    free(all);
  }
  else {
    B2ND_TEST_ASSERT(b2nd_get_slice_cbuffer(dirty, op.start, op.stop, out, sliceshape, n * typesize));
    memset(out, 0xa5, n * typesize);
    B2ND_TEST_ASSERT(b2nd_get_slice_cbuffer(array, op.start, op.stop, out, sliceshape, n * typesize));
    for (int64_t k = 0; k < n; ++k) {
      CUTEST_ASSERT("uninitialized chunks must read as zeros", out[k] == 0);
    }
  }
  free(out);
  free(ref);
  B2ND_TEST_ASSERT(b2nd_free(array));
  B2ND_TEST_ASSERT(b2nd_free(dirty));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));
  return 0;
}

CUTEST_TEST_TEARDOWN(slice_uninit) {
  blosc2_destroy();
}


// CUTEST_TEST_RUN() returns from the function it is in
static int run_slice_paths(void) {
  CUTEST_TEST_RUN(slice_paths);
}

static int run_slice_special(void) {
  CUTEST_TEST_RUN(slice_special);
}

static int run_slice_uninit(void) {
  CUTEST_TEST_RUN(slice_uninit);
}


int main(void) {
  int rc = run_slice_paths();
  rc |= run_slice_special();
  rc |= run_slice_uninit();
  return rc;
}
