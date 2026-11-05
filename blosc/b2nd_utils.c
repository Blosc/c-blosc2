/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "b2nd.h"

#include <stdint.h>

// Copies `nrows` rows of `row_nbytes` bytes each; consecutive rows are `src_stride` and `dst_stride` bytes apart.
// Rows of up to 64 bytes (e.g. 16 float32 or 32 int16 items: the rows of 16^3 float32 and 32x64x32 int16 blocks)
// are copied with fixed-size moves: a memcpy() call per row costs more than such a copy itself.  Two moves of n
// bytes, the second one ending at the end of the row, cover any length from n to 2 * n (they may overlap).
// Longer rows go to memcpy(), which is as fast or faster for them.
static inline void copy_rows(uint8_t *dst, int64_t dst_stride, const uint8_t *src, int64_t src_stride,
                             int64_t nrows, int64_t row_nbytes) {
#define COPY_ROWS(stmt)                     \
  for (int64_t r = 0; r < nrows; ++r) {     \
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


// Copies a box of `ndim` >= 2 dimensions (all of `copy_shape` > 0; strides in items): the two innermost
// dimensions go to copy_rows(), the outer ones are walked once per plane of rows.
static void copy_box(int8_t ndim, int32_t itemsize, const int64_t *copy_shape,
                     const uint8_t *bsrc, const int64_t *src_strides,
                     uint8_t *bdst, const int64_t *dst_strides) {
  const int64_t row_nbytes = copy_shape[ndim - 1] * itemsize;
  const int64_t nrows = copy_shape[ndim - 2];
  const int64_t src_row_stride = src_strides[ndim - 2] * itemsize;
  const int64_t dst_row_stride = dst_strides[ndim - 2] * itemsize;
  const int nouter = ndim - 2;
  int64_t index[B2ND_MAX_DIM] = {0};
  for (;;) {
    int64_t src_offset = 0;
    int64_t dst_offset = 0;
    for (int i = 0; i < nouter; ++i) {
      src_offset += index[i] * src_strides[i];
      dst_offset += index[i] * dst_strides[i];
    }
    copy_rows(bdst + dst_offset * itemsize, dst_row_stride, bsrc + src_offset * itemsize, src_row_stride,
              nrows, row_nbytes);
    int i = nouter - 1;
    while (i >= 0 && ++index[i] == copy_shape[i]) {
      index[i] = 0;
      --i;
    }
    if (i < 0) {
      break;
    }
  }
}

int b2nd_copy_buffer2(int8_t ndim,
                      int32_t itemsize,
                      const void *src, const int64_t *src_pad_shape,
                      const int64_t *src_start, const int64_t *src_stop,
                      void *dst, const int64_t *dst_pad_shape,
                      const int64_t *dst_start) {
  // Compute the shape of the copy
  int64_t copy_shape[B2ND_MAX_DIM] = {0};
  for (int i = 0; i < ndim; ++i) {
    copy_shape[i] = src_stop[i] - src_start[i];
    if (copy_shape[i] == 0) {
      return BLOSC2_ERROR_SUCCESS;
    }
  }

  // Compute the strides
  int64_t src_strides[B2ND_MAX_DIM] = {0};
  src_strides[ndim - 1] = 1;
  for (int i = ndim - 2; i >= 0; --i) {
    src_strides[i] = src_strides[i + 1] * src_pad_shape[i + 1];
  }

  int64_t dst_strides[B2ND_MAX_DIM] = {0};
  dst_strides[ndim - 1] = 1;
  for (int i = ndim - 2; i >= 0; --i) {
    dst_strides[i] = dst_strides[i + 1] * dst_pad_shape[i + 1];
  }

  // Align the buffers removing unnecessary data
  int64_t src_start_n;
  blosc2_multidim_to_unidim(src_start, ndim, src_strides, &src_start_n);
  uint8_t *bsrc = (uint8_t *) src;
  bsrc = &bsrc[src_start_n * itemsize];

  int64_t dst_start_n;
  blosc2_multidim_to_unidim(dst_start, ndim, dst_strides, &dst_start_n);
  uint8_t *bdst = (uint8_t *) dst;
  bdst = &bdst[dst_start_n * itemsize];

  if (ndim == 1) {
    memcpy(bdst, bsrc, copy_shape[0] * itemsize);
  } else {
    copy_box(ndim, itemsize, copy_shape, bsrc, src_strides, bdst, dst_strides);
  }

  return BLOSC2_ERROR_SUCCESS;
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
