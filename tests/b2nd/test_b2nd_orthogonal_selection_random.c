/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

// Orthogonal selections (get and set) with random index lists against an item-by-item reference, and a channels-last
// image.  The cases cover the per-block merging of trailing dimensions into longer copies (selections that are a
// block's whole extent in order and fill the buffer's extent) next to cases that must not merge (shuffled, reversed
// or repeated indices, partial blocks, partial buffer extents), so a wrong merge shows up as a mismatch.

#include "test_common.h"

static uint64_t lcg_state = 1;

static int64_t rand_range(int64_t lo, int64_t hi) {  // [lo, hi]
  return lo + test_lcg(&lcg_state, hi - lo + 1);
}

// Writes start, start + 1, ... (n items) to v; returns n
static int64_t iota(int64_t *v, int64_t start, int64_t n) {
  for (int64_t i = 0; i < n; ++i) {
    v[i] = start + i;
  }
  return n;
}

static void swap(int64_t *a, int64_t *b) {
  int64_t t = *a;
  *a = *b;
  *b = t;
}

static void shuffle(int64_t *v, int64_t n) {
  for (int64_t i = n - 1; i > 0; --i) {
    swap(&v[i], &v[rand_range(0, i)]);
  }
}

// A random selection of an axis of `extent` items in chunks of `chunk` and blocks of `block` items into `sel`
// (room for 2 * extent + 1 indices); returns its length.
static int64_t random_axis_selection(int64_t extent, int64_t chunk, int64_t block, int64_t *sel) {
  int64_t n;
  switch (rand_range(0, 9)) {
    case 0:
    case 1:
    case 2:
      // all items in order (whole blocks of an axis with one block merge)
      return iota(sel, 0, extent);
    case 3:
    case 4: {
      // one whole block in order (merges), if the axis has a whole one
      int64_t nchunks = (extent + chunk - 1) / chunk;
      int64_t start = rand_range(0, nchunks - 1) * chunk + rand_range(0, chunk / block - 1) * block;
      if (start + block > extent) {
        return iota(sel, 0, extent);
      }
      n = iota(sel, start, block);
      // sometimes the same items, but one repeated, reversed or shuffled (no merge)
      switch (rand_range(0, 11)) {
        case 0:
          sel[n++] = start + rand_range(0, block - 1);
          shuffle(sel, n);
          break;
        case 1:
          for (int64_t i = 0; i < n / 2; ++i) {
            swap(&sel[i], &sel[n - 1 - i]);
          }
          break;
        case 2:
          shuffle(sel, n);
          break;
        default:
          break;
      }
      return n;
    }
    case 5:
      // all items shuffled
      n = iota(sel, 0, extent);
      shuffle(sel, n);
      return n;
    case 6:
      // a sorted subset
      n = 0;
      for (int64_t i = 0; i < extent; ++i) {
        if (rand_range(0, 1) == 0) {
          sel[n++] = i;
        }
      }
      return n;
    case 7: {
      // all items in order but one, or with one repeated (no merge)
      n = iota(sel, 0, extent);
      int64_t k = rand_range(0, n - 1);
      if (rand_range(0, 1) == 0 && n > 1) {
        memmove(&sel[k], &sel[k + 1], (n - k - 1) * sizeof(int64_t));
        return n - 1;
      }
      memmove(&sel[k + 1], &sel[k], (n - k) * sizeof(int64_t));
      return n + 1;
    }
    case 8:
      // random indices, unsorted, with repeats
      n = rand_range(1, 2 * extent);
      for (int64_t i = 0; i < n; ++i) {
        sel[i] = rand_range(0, extent - 1);
      }
      return n;
    default:
      // a single item, rarely none
      if (rand_range(0, 4) == 0) {
        return 0;
      }
      sel[0] = rand_range(0, extent - 1);
      return 1;
  }
}

// Reads the selection of a random array with b2nd_get_orthogonal_selection() and writes random items to it with
// b2nd_set_orthogonal_selection(), both against references built item by item (the whole array is compared after
// the write, and the read checks a guard byte after its buffer); returns 0 when everything matches.
static int check_selection(int8_t ndim, int32_t typesize, const int64_t *shape, const int32_t *chunkshape,
                           const int32_t *blockshape, int64_t **selection, int64_t *selection_size) {
  int64_t nitems = 1;
  int64_t sel_nitems = 1;
  for (int i = 0; i < ndim; ++i) {
    nitems *= shape[i];
    sel_nitems *= selection_size[i];
  }
  uint8_t *src = malloc(nitems * typesize);
  uint8_t *out = malloc(sel_nitems * typesize + 1);
  uint8_t *expected_read = malloc(sel_nitems * typesize + 1);
  uint8_t *replacement = malloc(sel_nitems * typesize + 1);
  uint8_t *written = malloc(nitems * typesize);
  uint8_t *expected_array = malloc(nitems * typesize);
  for (int64_t i = 0; i < nitems * typesize; ++i) {
    src[i] = (uint8_t) test_lcg(&lcg_state, 256);
  }
  for (int64_t i = 0; i < sel_nitems * typesize; ++i) {
    replacement[i] = (uint8_t) test_lcg(&lcg_state, 256);
  }
  memset(out, 0xA5, sel_nitems * typesize + 1);
  expected_read[sel_nitems * typesize] = 0xA5;
  memcpy(expected_array, src, nitems * typesize);
  // References: the selection's items in C order of the buffer; for the set, later buffer items overwrite earlier
  // ones of the same array item (what the copy does with repeated indices)
  int64_t pos[B2ND_MAX_DIM];
  for (int64_t n = 0; n < sel_nitems; ++n) {
    int64_t rest = n;
    for (int i = ndim - 1; i >= 0; --i) {
      pos[i] = rest % selection_size[i];
      rest /= selection_size[i];
    }
    int64_t item = 0;
    for (int i = 0; i < ndim; ++i) {
      item = item * shape[i] + selection[i][pos[i]];
    }
    memcpy(expected_read + n * typesize, src + item * typesize, typesize);
    memcpy(expected_array + item * typesize, replacement + n * typesize, typesize);
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  cparams.nthreads = 1;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
  storage.cparams = &cparams;
  storage.dparams = &dparams;
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, ndim, shape, chunkshape, blockshape, NULL, 0, NULL, 0);
  b2nd_array_t *array = NULL;
  const char *failed = NULL;
  int rc = 0;
  if (ctx == NULL || (rc = b2nd_from_cbuffer(ctx, &array, src, nitems * typesize)) < 0) {
    failed = "creating the array";
  } else if ((rc = b2nd_get_orthogonal_selection(array, selection, selection_size, out, selection_size,
                                                 sel_nitems * typesize)) < 0) {
    failed = "b2nd_get_orthogonal_selection()";
  } else if (memcmp(out, expected_read, sel_nitems * typesize + 1) != 0) {
    failed = "the items read";
  } else if ((rc = b2nd_set_orthogonal_selection(array, selection, selection_size, replacement, selection_size,
                                                 sel_nitems * typesize)) < 0) {
    failed = "b2nd_set_orthogonal_selection()";
  } else if ((rc = b2nd_to_cbuffer(array, written, nitems * typesize)) < 0) {
    failed = "b2nd_to_cbuffer()";
  } else if (memcmp(written, expected_array, nitems * typesize) != 0) {
    failed = "the array after the write";
  }
  int bad = failed != NULL;
  if (bad) {
    fprintf(stderr, "Failed: %s (rc %d); typesize %d, shape/chunks/blocks/selection:", failed, rc, typesize);
    for (int i = 0; i < ndim; ++i) {
      fprintf(stderr, " (%lld %d %d [", (long long) shape[i], chunkshape[i], blockshape[i]);
      for (int64_t j = 0; j < selection_size[i]; ++j) {
        fprintf(stderr, "%s%lld", j ? " " : "", (long long) selection[i][j]);
      }
      fprintf(stderr, "])");
    }
    fprintf(stderr, "\n");
  }
  b2nd_free(array);
  b2nd_free_ctx(ctx);
  free(src);
  free(out);
  free(expected_read);
  free(replacement);
  free(written);
  free(expected_array);
  return bad;
}

// check_selection() of a random array of `ndim` dimensions with random selections
static int check_random_selection(int8_t ndim, int32_t typesize) {
  const int64_t max_extent[] = {0, 40, 24, 12, 8};
  int64_t shape[B2ND_MAX_DIM];
  int32_t chunkshape[B2ND_MAX_DIM];
  int32_t blockshape[B2ND_MAX_DIM];
  int64_t *selection[B2ND_MAX_DIM];
  int64_t selection_size[B2ND_MAX_DIM];
  for (int i = 0; i < ndim; ++i) {
    shape[i] = rand_range(1, max_extent[ndim]);
    // whole-axis chunks and blocks often, so that trailing axes can merge
    chunkshape[i] = (int32_t) (rand_range(0, 1) == 0 ? shape[i] : rand_range(1, shape[i]));
    blockshape[i] = (int32_t) (rand_range(0, 1) == 0 ? chunkshape[i] : rand_range(1, chunkshape[i]));
    selection[i] = malloc((2 * shape[i] + 1) * sizeof(int64_t));
    selection_size[i] = random_axis_selection(shape[i], chunkshape[i], blockshape[i], selection[i]);
  }
  int bad = check_selection(ndim, typesize, shape, chunkshape, blockshape, selection, selection_size);
  for (int i = 0; i < ndim; ++i) {
    free(selection[i]);
  }
  return bad;
}

// A full image of a channels-last (3, 16, 24, nchannels) uint8 array, one image per chunk, `nselected` channels:
// with all channels and blocks of 8x8 pixels, the channels merge into rows of 8 pixels; with blocks spanning the
// image, every axis merges (one copy per block); with the first 3 of 4 channels nothing merges.
static int check_image(int64_t nchannels, int32_t block_rows, int32_t block_cols, int64_t nselected) {
  int64_t shape[] = {3, 16, 24, nchannels};
  int32_t chunkshape[] = {1, 16, 24, (int32_t) nchannels};
  int32_t blockshape[] = {1, block_rows, block_cols, (int32_t) nchannels};
  int64_t image[] = {2};
  int64_t all[24];
  iota(all, 0, 24);
  int64_t *selection[] = {image, all, all, all};
  int64_t selection_size[] = {1, 16, 24, nselected};
  return check_selection(4, 1, shape, chunkshape, blockshape, selection, selection_size);
}


CUTEST_TEST_SETUP(orthogonal_selection_random) {
  blosc2_init();
}

CUTEST_TEST_TEST(orthogonal_selection_random) {
  CUTEST_ASSERT("Channels-last image, 8x8 pixel blocks", check_image(3, 8, 8, 3) == 0);
  CUTEST_ASSERT("Channels-last image, one block per image", check_image(3, 16, 24, 3) == 0);
  CUTEST_ASSERT("First 3 of 4 channels, 8x8 pixel blocks", check_image(4, 8, 8, 3) == 0);

  const int32_t typesizes[] = {1, 2, 3, 4, 8, 16};
  for (int n = 0; n < 5000; ++n) {
    int8_t ndim = (int8_t) rand_range(1, 4);
    int32_t typesize = typesizes[rand_range(0, 5)];
    CUTEST_ASSERT("Random orthogonal selection differs from the reference",
                  check_random_selection(ndim, typesize) == 0);
  }
  return 0;
}

CUTEST_TEST_TEARDOWN(orthogonal_selection_random) {
  blosc2_destroy();
}

int main(void) {
  CUTEST_TEST_RUN(orthogonal_selection_random);
}
