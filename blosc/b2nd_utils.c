/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "b2nd.h"
#include "b2nd-private.h"

#include <stdint.h>

// Copies `nrows` rows of `row_nbytes` bytes each; consecutive rows are `src_stride` and `dst_stride` bytes apart.
// Rows of up to 64 bytes (e.g. 16 float32 or 32 int16 items: the rows of 16^3 float32 and 32x64x32 int16 blocks)
// are copied with fixed-size moves: a memcpy() call per row costs more than such a copy itself.  Two moves of n
// bytes, the second one ending at the end of the row, cover any length from n to 2 * n (they may overlap).
// Longer rows go to memcpy(), which is as fast or faster for them.  The loops count down: with one value less live
// across the memcpy() calls, gcc no longer spills the row counter around every call (8% faster for 128-byte rows).
static inline void copy_rows(uint8_t *dst, int64_t dst_stride, const uint8_t *src, int64_t src_stride,
                             int64_t nrows, int64_t row_nbytes) {
#define COPY_ROWS(stmt)                     \
  for (int64_t r = nrows; r > 0; --r) {     \
    stmt;                                   \
    dst += dst_stride;                      \
    src += src_stride;                      \
  }
#define MOVE_2X(n)                                                     \
  memcpy(dst, src, n);                                                 \
  memcpy(dst + row_nbytes - (n), src + row_nbytes - (n), n)

  if (row_nbytes > 64) {
    COPY_ROWS(memcpy(dst, src, (size_t) row_nbytes))
  } else if (row_nbytes > 32) {
    COPY_ROWS(MOVE_2X(32))
  } else if (row_nbytes == 32) {
    COPY_ROWS(memcpy(dst, src, 32))
  } else if (row_nbytes > 16) {
    COPY_ROWS(MOVE_2X(16))
  } else if (row_nbytes == 16) {
    COPY_ROWS(memcpy(dst, src, 16))
  } else if (row_nbytes > 8) {
    COPY_ROWS(MOVE_2X(8))
  } else if (row_nbytes == 8) {
    COPY_ROWS(memcpy(dst, src, 8))
  } else if (row_nbytes > 4) {
    COPY_ROWS(MOVE_2X(4))
  } else if (row_nbytes == 4) {
    COPY_ROWS(memcpy(dst, src, 4))
  } else if (row_nbytes > 2) {
    COPY_ROWS(MOVE_2X(2))
  } else if (row_nbytes == 2) {
    COPY_ROWS(memcpy(dst, src, 2))
  } else {
    COPY_ROWS(*dst = *src)
  }
#undef MOVE_2X
#undef COPY_ROWS
}


// Copies (or, with a NULL `src`, zeroes) a box of `ndim` >= 2 dimensions given in bytes, innermost first: shape[0]
// bytes per row, contiguous in both buffers; the rows of dimension 1 go to copy_rows() (or memset()), the outer
// dimensions are walked once per plane of rows.  Strides are in bytes.
static void copy_box(int ndim, const int64_t *shape,
                     const uint8_t *src, const int64_t *src_strides,
                     uint8_t *dst, const int64_t *dst_strides) {
  const int64_t row_nbytes = shape[0];
  const int64_t nrows = shape[1];
  const int64_t src_row_stride = src_strides[1];
  const int64_t dst_row_stride = dst_strides[1];
  int64_t index[B2ND_MAX_DIM];
  for (int i = 2; i < ndim; ++i) {
    index[i] = 0;
  }
  for (;;) {
    int64_t src_offset = 0;
    int64_t dst_offset = 0;
    for (int i = 2; i < ndim; ++i) {
      src_offset += index[i] * src_strides[i];
      dst_offset += index[i] * dst_strides[i];
    }
    if (src == NULL) {
      uint8_t *row = dst + dst_offset;
      for (int64_t r = 0; r < nrows; ++r) {
        memset(row, 0, (size_t) row_nbytes);
        row += dst_row_stride;
      }
    } else {
      copy_rows(dst + dst_offset, dst_row_stride, src + src_offset, src_row_stride, nrows, row_nbytes);
    }
    int i = 2;
    while (i < ndim && ++index[i] == shape[i]) {
      index[i] = 0;
      ++i;
    }
    if (i == ndim) {
      break;
    }
  }
}


// Describes a box of `box_shape` items (all > 0), at byte strides `src_strides` and `dst_strides` of two buffers,
// as few and as long rows as possible.  Going outwards from the bytes of an item, dimensions the box is one item
// wide in are dropped, and a dimension is merged into the one inside it wherever their items follow on contiguously
// in both buffers: e.g. a (1, 128, 128, 3) uint8 block of a (1, 512, 512, 3) image becomes 128 rows of 384 bytes.
// Writes the dimensions in bytes, innermost first, and returns their number: at most max(ndim, 1), as the innermost
// box dimension always merges with the item's bytes unless it is dropped.
static int simplify_box(int8_t ndim, int32_t itemsize, const int64_t *box_shape,
                        const int64_t *src_strides, const int64_t *dst_strides,
                        int64_t *shape, int64_t *src_bstrides, int64_t *dst_bstrides) {
  int n = 0;
  int64_t extent = itemsize;
  int64_t src_stride = 1;
  int64_t dst_stride = 1;
  for (int i = ndim - 1; i >= 0; --i) {
    if (box_shape[i] == 1) {
      continue;
    }
    if (src_strides[i] == src_stride * extent && dst_strides[i] == dst_stride * extent) {
      extent *= box_shape[i];
      continue;
    }
    shape[n] = extent;
    src_bstrides[n] = src_stride;
    dst_bstrides[n] = dst_stride;
    ++n;
    extent = box_shape[i];
    src_stride = src_strides[i];
    dst_stride = dst_strides[i];
  }
  shape[n] = extent;
  src_bstrides[n] = src_stride;
  dst_bstrides[n] = dst_stride;
  return n + 1;
}


// Byte strides of a C-order buffer of shape `pad_shape` and items of `itemsize` bytes
static void byte_strides(int8_t ndim, int32_t itemsize, const int64_t *pad_shape, int64_t *strides) {
  int64_t stride = itemsize;
  for (int i = ndim - 1; i >= 0; --i) {
    strides[i] = stride;
    stride *= pad_shape[i];
  }
}


// Copies the box of `box_shape` items (all > 0) at `src_start` of the C-order buffer `src` (of shape
// `src_pad_shape`) to `dst_start` of `dst` (of shape `dst_pad_shape`); with a NULL `src`, zeroes it in `dst`.
static void copy_or_zero_box(int8_t ndim, int32_t itemsize, const int64_t *box_shape,
                             const uint8_t *src, const int64_t *src_pad_shape, const int64_t *src_start,
                             uint8_t *dst, const int64_t *dst_pad_shape, const int64_t *dst_start) {
  int64_t src_strides[B2ND_MAX_DIM];
  int64_t dst_strides[B2ND_MAX_DIM];
  byte_strides(ndim, itemsize, src_pad_shape, src_strides);
  byte_strides(ndim, itemsize, dst_pad_shape, dst_strides);
  int64_t src_offset = 0;
  int64_t dst_offset = 0;
  for (int i = 0; i < ndim; ++i) {
    src_offset += src_start[i] * src_strides[i];
    dst_offset += dst_start[i] * dst_strides[i];
  }
  dst += dst_offset;
  if (src != NULL) {
    src += src_offset;
  }

  int64_t shape[B2ND_MAX_DIM];
  int64_t src_bstrides[B2ND_MAX_DIM];
  int64_t dst_bstrides[B2ND_MAX_DIM];
  int ndim_bytes = simplify_box(ndim, itemsize, box_shape, src_strides, dst_strides, shape, src_bstrides,
                                dst_bstrides);
  if (ndim_bytes > 1) {
    copy_box(ndim_bytes, shape, src, src_bstrides, dst, dst_bstrides);
  } else if (src == NULL) {
    memset(dst, 0, (size_t) shape[0]);
  } else {
    memcpy(dst, src, (size_t) shape[0]);
  }
}


int b2nd_copy_buffer2(int8_t ndim,
                      int32_t itemsize,
                      const void *src, const int64_t *src_pad_shape,
                      const int64_t *src_start, const int64_t *src_stop,
                      void *dst, const int64_t *dst_pad_shape,
                      const int64_t *dst_start) {
  int64_t copy_shape[B2ND_MAX_DIM];
  for (int i = 0; i < ndim; ++i) {
    copy_shape[i] = src_stop[i] - src_start[i];
    if (copy_shape[i] == 0) {
      return BLOSC2_ERROR_SUCCESS;
    }
  }
  copy_or_zero_box(ndim, itemsize, copy_shape, src, src_pad_shape, src_start, dst, dst_pad_shape, dst_start);

  return BLOSC2_ERROR_SUCCESS;
}


/* See b2nd-private.h */
void b2nd_zero_box(int8_t ndim, int32_t itemsize, void *dst, const int64_t *dst_pad_shape,
                   const int64_t *box_start, const int64_t *box_stop) {
  int64_t box_shape[B2ND_MAX_DIM];
  for (int i = 0; i < ndim; ++i) {
    box_shape[i] = box_stop[i] - box_start[i];
    if (box_shape[i] <= 0) {
      return;
    }
  }
  copy_or_zero_box(ndim, itemsize, box_shape, NULL, dst_pad_shape, box_start, dst, dst_pad_shape, box_start);
}


// Keep the old signature for API compatibility
int b2nd_copy_buffer(int8_t ndim,
                     uint8_t itemsize,
                     const void *src, const int64_t *src_pad_shape,
                     const int64_t *src_start, const int64_t *src_stop,
                     void *dst, const int64_t *dst_pad_shape,
                     const int64_t *dst_start) {
  // Simply cast itemsize to int32_t and delegate
  return b2nd_copy_buffer2(ndim, (int32_t)itemsize, src, src_pad_shape,
                          src_start, src_stop, dst, dst_pad_shape, dst_start);
}
