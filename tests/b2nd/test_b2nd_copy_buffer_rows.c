/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

// b2nd_copy_buffer2() against an item-by-item reference: every row length from 1 to 130 bytes (the fixed-size
// row copies up to 64 bytes and memcpy() beyond), random boxes of 1 to B2ND_MAX_DIM dimensions with several
// item sizes, and boxes with dimensions covered whole (merged into longer rows) or one item wide (dropped).  Also
// b2nd_zero_box(), which zeroes boxes the same way.
// The whole destination is compared, so writes outside the box are caught too.

#include "test_common.h"
#include "b2nd-private.h"

static uint64_t rng_state = 0x9E3779B97F4A7C15ULL;

static uint64_t next_rand(void) {
  // xorshift64*
  rng_state ^= rng_state >> 12;
  rng_state ^= rng_state << 25;
  rng_state ^= rng_state >> 27;
  return rng_state * 0x2545F4914F6CDD1DULL;
}

static int64_t rand_range(int64_t lo, int64_t hi) {  // [lo, hi]
  return lo + (int64_t) (next_rand() % (uint64_t) (hi - lo + 1));
}

static void reference_copy(int8_t ndim, int32_t itemsize,
                           const uint8_t *src, const int64_t *src_pad_shape,
                           const int64_t *src_start, const int64_t *src_stop,
                           uint8_t *dst, const int64_t *dst_pad_shape, const int64_t *dst_start) {
  int64_t copy_shape[B2ND_MAX_DIM];
  int64_t nitems = 1;
  for (int i = 0; i < ndim; ++i) {
    copy_shape[i] = src_stop[i] - src_start[i];
    nitems *= copy_shape[i];
  }
  int64_t index[B2ND_MAX_DIM];
  for (int64_t n = 0; n < nitems; ++n) {
    int64_t rest = n;
    for (int i = ndim - 1; i >= 0; --i) {
      index[i] = rest % copy_shape[i];
      rest /= copy_shape[i];
    }
    int64_t s = 0;
    int64_t d = 0;
    for (int i = 0; i < ndim; ++i) {
      s = s * src_pad_shape[i] + src_start[i] + index[i];
      d = d * dst_pad_shape[i] + dst_start[i] + index[i];
    }
    memcpy(dst + d * itemsize, src + s * itemsize, itemsize);
  }
}

// One copy through b2nd_copy_buffer2() and the reference; returns 0 when the destinations match.
static int check_one(int8_t ndim, int32_t itemsize, const int64_t *src_pad_shape, const int64_t *src_start,
                     const int64_t *src_stop, const int64_t *dst_pad_shape, const int64_t *dst_start) {
  int64_t src_nitems = 1;
  int64_t dst_nitems = 1;
  for (int i = 0; i < ndim; ++i) {
    src_nitems *= src_pad_shape[i];
    dst_nitems *= dst_pad_shape[i];
  }
  uint8_t *src = malloc(src_nitems * itemsize);
  uint8_t *dst = malloc(dst_nitems * itemsize);
  uint8_t *ref = malloc(dst_nitems * itemsize);
  for (int64_t i = 0; i < src_nitems * itemsize; ++i) {
    src[i] = (uint8_t) next_rand();
  }
  for (int64_t i = 0; i < dst_nitems * itemsize; ++i) {
    dst[i] = ref[i] = (uint8_t) (0xA5 ^ i);
  }
  int rc = b2nd_copy_buffer2(ndim, itemsize, src, src_pad_shape, src_start, src_stop, dst, dst_pad_shape,
                             dst_start);
  reference_copy(ndim, itemsize, src, src_pad_shape, src_start, src_stop, ref, dst_pad_shape, dst_start);
  int bad = rc != BLOSC2_ERROR_SUCCESS || memcmp(dst, ref, dst_nitems * itemsize) != 0;
  free(src);
  free(dst);
  free(ref);
  return bad;
}


// One b2nd_zero_box() against the reference copying zeros item by item; returns 0 when the buffers match.
static int check_zero_one(int8_t ndim, int32_t itemsize, const int64_t *pad_shape, const int64_t *box_start,
                          const int64_t *box_stop) {
  int64_t nbytes = itemsize;
  for (int i = 0; i < ndim; ++i) {
    nbytes *= pad_shape[i];
  }
  uint8_t *zeros = calloc(nbytes, 1);
  uint8_t *buf = malloc(nbytes);
  uint8_t *ref = malloc(nbytes);
  for (int64_t i = 0; i < nbytes; ++i) {
    buf[i] = ref[i] = (uint8_t) (0x5A ^ i) | 1;
  }
  b2nd_zero_box(ndim, itemsize, buf, pad_shape, box_start, box_stop);
  reference_copy(ndim, itemsize, zeros, pad_shape, box_start, box_stop, ref, pad_shape, box_start);
  int bad = memcmp(buf, ref, nbytes) != 0;
  free(zeros);
  free(buf);
  free(ref);
  return bad;
}


CUTEST_TEST_SETUP(copy_buffer_rows) {
  blosc2_init();
}

CUTEST_TEST_TEST(copy_buffer_rows) {
  // Every row length from 1 to 130 bytes, rows at odd offsets of buffers of other widths
  const int32_t itemsizes[] = {1, 2, 4};
  for (int k = 0; k < 3; ++k) {
    int32_t itemsize = itemsizes[k];
    for (int64_t row_nbytes = itemsize; row_nbytes <= 130; row_nbytes += itemsize) {
      int64_t row = row_nbytes / itemsize;
      for (int8_t ndim = 2; ndim <= 3; ++ndim) {
        int64_t src_pad_shape[3] = {3, 7, row + 5};
        int64_t src_start[3] = {1, 2, 3};
        int64_t src_stop[3] = {3, 7, row + 3};
        int64_t dst_pad_shape[3] = {4, 6, row + 9};
        int64_t dst_start[3] = {2, 1, 7};
        int off = 3 - ndim;  // ndim 2 uses the last two entries
        CUTEST_ASSERT("Rows differ from the reference",
                      check_one(ndim, itemsize, src_pad_shape + off, src_start + off, src_stop + off,
                                dst_pad_shape + off, dst_start + off) == 0);
      }
    }
  }

  // Random boxes: 1 to B2ND_MAX_DIM dimensions, several item sizes
  const int32_t rand_itemsizes[] = {1, 2, 3, 4, 8, 12, 16};
  for (int n = 0; n < 3000; ++n) {
    int8_t ndim = (int8_t) rand_range(1, B2ND_MAX_DIM);
    int32_t itemsize = rand_itemsizes[rand_range(0, 6)];
    int64_t src_pad_shape[B2ND_MAX_DIM];
    int64_t src_start[B2ND_MAX_DIM];
    int64_t src_stop[B2ND_MAX_DIM];
    int64_t dst_pad_shape[B2ND_MAX_DIM];
    int64_t dst_start[B2ND_MAX_DIM];
    // Last dimension first; every padded extent is capped by what is left of a 20000-item budget per buffer,
    // so dimensions late in the draw (of high-dimensional boxes) often have extent 1
    int64_t src_budget = 20000;
    int64_t dst_budget = 20000;
    for (int i = ndim - 1; i >= 0; --i) {
      int64_t max_extent = i == ndim - 1 ? 40 : 6;
      max_extent = max_extent < src_budget ? max_extent : src_budget;
      max_extent = max_extent < dst_budget ? max_extent : dst_budget;
      int64_t extent = rand_range(1, max_extent);
      src_pad_shape[i] = extent + rand_range(0, 3);
      src_pad_shape[i] = src_pad_shape[i] < src_budget ? src_pad_shape[i] : src_budget;
      dst_pad_shape[i] = extent + rand_range(0, 3);
      dst_pad_shape[i] = dst_pad_shape[i] < dst_budget ? dst_pad_shape[i] : dst_budget;
      src_budget /= src_pad_shape[i];
      dst_budget /= dst_pad_shape[i];
      src_start[i] = rand_range(0, src_pad_shape[i] - extent);
      src_stop[i] = src_start[i] + extent;
      dst_start[i] = rand_range(0, dst_pad_shape[i] - extent);
    }
    CUTEST_ASSERT("Random box differs from the reference",
                  check_one(ndim, itemsize, src_pad_shape, src_start, src_stop, dst_pad_shape, dst_start) == 0);
  }

  // Dimensions that the copy covers whole in one or both buffers (merged into the one inside it when whole in
  // both) and dimensions one item wide (dropped), in any position; up to 8 dimensions, which keeps buffers of
  // extents up to 7 small
  for (int n = 0; n < 3000; ++n) {
    int8_t ndim = (int8_t) rand_range(1, 8);
    int32_t itemsize = rand_itemsizes[rand_range(0, 6)];
    int64_t src_pad_shape[B2ND_MAX_DIM];
    int64_t src_start[B2ND_MAX_DIM];
    int64_t src_stop[B2ND_MAX_DIM];
    int64_t dst_pad_shape[B2ND_MAX_DIM];
    int64_t dst_start[B2ND_MAX_DIM];
    for (int i = 0; i < ndim; ++i) {
      int64_t extent = rand_range(0, 2) == 0 ? 1 : rand_range(1, 5);
      src_pad_shape[i] = extent + (rand_range(0, 2) == 0 ? rand_range(1, 2) : 0);
      dst_pad_shape[i] = extent + (rand_range(0, 2) == 0 ? rand_range(1, 2) : 0);
      src_start[i] = rand_range(0, src_pad_shape[i] - extent);
      src_stop[i] = src_start[i] + extent;
      dst_start[i] = rand_range(0, dst_pad_shape[i] - extent);
    }
    CUTEST_ASSERT("Box with whole or one-item dimensions differs from the reference",
                  check_one(ndim, itemsize, src_pad_shape, src_start, src_stop, dst_pad_shape, dst_start) == 0);
  }

  // A (1, 16, 16, 3) uint8 block into a (1, 40, 48, 3) image (rows of 48 bytes after merging), and the same block
  // into an image with one more channel (rows of 3 bytes, nothing to merge)
  {
    int64_t src_pad_shape[4] = {1, 16, 16, 3};
    int64_t src_start[4] = {0, 0, 0, 0};
    int64_t src_stop[4] = {1, 16, 16, 3};
    int64_t dst_pad_shape[4] = {1, 40, 48, 3};
    int64_t dst_start[4] = {0, 9, 32, 0};
    CUTEST_ASSERT("Block of an image differs from the reference",
                  check_one(4, 1, src_pad_shape, src_start, src_stop, dst_pad_shape, dst_start) == 0);
    dst_pad_shape[3] = 4;
    dst_start[3] = 1;
    CUTEST_ASSERT("Block of an image with 4 channels differs from the reference",
                  check_one(4, 1, src_pad_shape, src_start, src_stop, dst_pad_shape, dst_start) == 0);
  }

  // b2nd_zero_box(): boxes with dimensions covered whole or one item wide, the whole buffer and empty boxes (up
  // to 8 dimensions, as above)
  for (int n = 0; n < 3000; ++n) {
    int8_t ndim = (int8_t) rand_range(1, 8);
    int32_t itemsize = rand_itemsizes[rand_range(0, 6)];
    int64_t pad_shape[B2ND_MAX_DIM];
    int64_t box_start[B2ND_MAX_DIM];
    int64_t box_stop[B2ND_MAX_DIM];
    bool whole = rand_range(0, 9) == 0;
    int empty_dim = rand_range(0, 9) == 0 ? (int) rand_range(0, ndim - 1) : -1;
    for (int i = 0; i < ndim; ++i) {
      int64_t extent = rand_range(0, 2) == 0 ? 1 : rand_range(1, 5);
      pad_shape[i] = extent + (whole || rand_range(0, 1) == 0 ? 0 : rand_range(1, 2));
      box_start[i] = rand_range(0, pad_shape[i] - extent);
      box_stop[i] = box_start[i] + (i == empty_dim ? 0 : extent);
    }
    CUTEST_ASSERT("Zeroed box differs from the reference",
                  check_zero_one(ndim, itemsize, pad_shape, box_start, box_stop) == 0);
  }
  return 0;
}

CUTEST_TEST_TEARDOWN(copy_buffer_rows) {
  blosc2_destroy();
}

int main(void) {
  CUTEST_TEST_RUN(copy_buffer_rows);
}
