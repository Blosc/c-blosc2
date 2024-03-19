/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "b2nd.h"

#include <stdint.h>
#include <string.h>

// copyNdim where N = {2-8} - specializations of copy loops to be used by b2nd_copy_buffer
// since we don't have c++ templates, substitute manual specializations for up to known B2ND_MAX_DIM (8)
// it's not pretty, but it substantially reduces overhead vs. the generic method
void copy8dim(const uint8_t itemsize,
              const int64_t *copy_shape,
              const uint8_t *bsrc, const int64_t *src_strides,
              uint8_t *bdst, const int64_t *dst_strides) {
  int64_t copy_nbytes = copy_shape[7] * itemsize;
  int64_t copy_start[7] = {0};
  do {
    do {
      do {
        do {
          do {
            do {
              do {
                int64_t src_copy_start = 0;
                int64_t dst_copy_start = 0;
                for (int j = 0; j < 7; ++j) {
                  src_copy_start += copy_start[j] * src_strides[j];
                  dst_copy_start += copy_start[j] * dst_strides[j];
                }
                memcpy(&bdst[dst_copy_start * itemsize], &bsrc[src_copy_start * itemsize], copy_nbytes);
                ++copy_start[6];
              } while (copy_start[6] < copy_shape[6]);
              ++copy_start[5];
              copy_start[6] = 0;
            } while (copy_start[5] < copy_shape[5]);
            ++copy_start[4];
            copy_start[5] = 0;
          } while (copy_start[4] < copy_shape[4]);
          ++copy_start[3];
          copy_start[4] = 0;
        } while (copy_start[3] < copy_shape[3]);
        ++copy_start[2];
        copy_start[3] = 0;
      } while (copy_start[2] < copy_shape[2]);
      ++copy_start[1];
      copy_start[2] = 0;
    } while (copy_start[1] < copy_shape[1]);
    ++copy_start[0];
    copy_start[1] = 0;
  } while (copy_start[0] < copy_shape[0]);
}

void copy7dim(const uint8_t itemsize,
              const int64_t *copy_shape,
              const uint8_t *bsrc, const int64_t *src_strides,
              uint8_t *bdst, const int64_t *dst_strides) {
  int64_t copy_nbytes = copy_shape[6] * itemsize;
  int64_t copy_start[6] = {0};
  do {
    do {
      do {
        do {
          do {
            do {
              int64_t src_copy_start = 0;
              int64_t dst_copy_start = 0;
              for (int j = 0; j < 6; ++j) {
                src_copy_start += copy_start[j] * src_strides[j];
                dst_copy_start += copy_start[j] * dst_strides[j];
              }
              memcpy(&bdst[dst_copy_start * itemsize], &bsrc[src_copy_start * itemsize], copy_nbytes);
              ++copy_start[5];
            } while (copy_start[5] < copy_shape[5]);
            ++copy_start[4];
            copy_start[5] = 0;
          } while (copy_start[4] < copy_shape[4]);
          ++copy_start[3];
          copy_start[4] = 0;
        } while (copy_start[3] < copy_shape[3]);
        ++copy_start[2];
        copy_start[3] = 0;
      } while (copy_start[2] < copy_shape[2]);
      ++copy_start[1];
      copy_start[2] = 0;
    } while (copy_start[1] < copy_shape[1]);
    ++copy_start[0];
    copy_start[1] = 0;
  } while (copy_start[0] < copy_shape[0]);
}

void copy6dim(const uint8_t itemsize,
              const int64_t *copy_shape,
              const uint8_t *bsrc, const int64_t *src_strides,
              uint8_t *bdst, const int64_t *dst_strides) {
  int64_t copy_nbytes = copy_shape[5] * itemsize;
  int64_t copy_start[5] = {0};
  do {
    do {
      do {
        do {
          do {
            int64_t src_copy_start = 0;
            int64_t dst_copy_start = 0;
            for (int j = 0; j < 5; ++j) {
              src_copy_start += copy_start[j] * src_strides[j];
              dst_copy_start += copy_start[j] * dst_strides[j];
            }
            memcpy(&bdst[dst_copy_start * itemsize], &bsrc[src_copy_start * itemsize], copy_nbytes);
            ++copy_start[4];
          } while (copy_start[4] < copy_shape[4]);
          ++copy_start[3];
          copy_start[4] = 0;
        } while (copy_start[3] < copy_shape[3]);
        ++copy_start[2];
        copy_start[3] = 0;
      } while (copy_start[2] < copy_shape[2]);
      ++copy_start[1];
      copy_start[2] = 0;
    } while (copy_start[1] < copy_shape[1]);
    ++copy_start[0];
    copy_start[1] = 0;
  } while (copy_start[0] < copy_shape[0]);
}

void copy5dim(const uint8_t itemsize,
              const int64_t *copy_shape,
              const uint8_t *bsrc, const int64_t *src_strides,
              uint8_t *bdst, const int64_t *dst_strides) {
  int64_t copy_nbytes = copy_shape[4] * itemsize;
  int64_t copy_start[4] = {0};
  do {
    do {
      do {
        do {
          int64_t src_copy_start = 0;
          int64_t dst_copy_start = 0;
          for (int j = 0; j < 4; ++j) {
            src_copy_start += copy_start[j] * src_strides[j];
            dst_copy_start += copy_start[j] * dst_strides[j];
          }
          memcpy(&bdst[dst_copy_start * itemsize], &bsrc[src_copy_start * itemsize], copy_nbytes);
          ++copy_start[3];
        } while (copy_start[3] < copy_shape[3]);
        ++copy_start[2];
        copy_start[3] = 0;
      } while (copy_start[2] < copy_shape[2]);
      ++copy_start[1];
      copy_start[2] = 0;
    } while (copy_start[1] < copy_shape[1]);
    ++copy_start[0];
    copy_start[1] = 0;
  } while (copy_start[0] < copy_shape[0]);
}

void copy4dim(const uint8_t itemsize,
              const int64_t *copy_shape,
              const uint8_t *bsrc, const int64_t *src_strides,
              uint8_t *bdst, const int64_t *dst_strides) {
  int64_t copy_nbytes = copy_shape[3] * itemsize;
  int64_t copy_start[3] = {0};
  do {
    do {
      do {
        int64_t src_copy_start = 0;
        int64_t dst_copy_start = 0;
        for (int j = 0; j < 3; ++j) {
          src_copy_start += copy_start[j] * src_strides[j];
          dst_copy_start += copy_start[j] * dst_strides[j];
        }
        memcpy(&bdst[dst_copy_start * itemsize], &bsrc[src_copy_start * itemsize], copy_nbytes);
        ++copy_start[2];
      } while (copy_start[2] < copy_shape[2]);
      ++copy_start[1];
      copy_start[2] = 0;
    } while (copy_start[1] < copy_shape[1]);
    ++copy_start[0];
    copy_start[1] = 0;
  } while (copy_start[0] < copy_shape[0]);
}

void copy3dim(const uint8_t itemsize,
              const int64_t *copy_shape,
              const uint8_t *bsrc, const int64_t *src_strides,
              uint8_t *bdst, const int64_t *dst_strides) {
  int64_t copy_nbytes = copy_shape[2] * itemsize;
  int64_t copy_start[2] = {0};
  do {
    do {
      int64_t src_copy_start = 0;
      int64_t dst_copy_start = 0;
      for (int j = 0; j < 2; ++j) {
        src_copy_start += copy_start[j] * src_strides[j];
        dst_copy_start += copy_start[j] * dst_strides[j];
      }
      memcpy(&bdst[dst_copy_start * itemsize], &bsrc[src_copy_start * itemsize], copy_nbytes);
      ++copy_start[1];
    } while (copy_start[1] < copy_shape[1]);
    ++copy_start[0];
    copy_start[1] = 0;
  } while (copy_start[0] < copy_shape[0]);
}

void copy2dim(const uint8_t itemsize,
              const int64_t *copy_shape,
              const uint8_t *bsrc, const int64_t *src_strides,
              uint8_t *bdst, const int64_t *dst_strides) {
  int64_t copy_nbytes = copy_shape[1] * itemsize;
  int64_t copy_start = 0;
  do {
    int64_t src_copy_start = copy_start * src_strides[0];
    int64_t dst_copy_start = copy_start * dst_strides[0];
    memcpy(&bdst[dst_copy_start * itemsize], &bsrc[src_copy_start * itemsize], copy_nbytes);
    ++copy_start;
  } while (copy_start < copy_shape[0]);
}


void copy_ndim_fallback(const int8_t ndim,
                        const uint8_t itemsize,
                        int64_t *copy_shape,
                        const uint8_t *bsrc, int64_t *src_strides,
                        uint8_t *bdst, int64_t *dst_strides) {
  int64_t copy_nbytes = copy_shape[ndim - 1] * itemsize;
  int64_t number_of_copies = 1;
  for (int i = 0; i < ndim - 1; ++i) {
    number_of_copies *= copy_shape[i];
  }
  for (int ncopy = 0; ncopy < number_of_copies; ++ncopy) {
    // Compute the start of the copy
    int64_t copy_start[B2ND_MAX_DIM] = {0};
    blosc2_unidim_to_multidim((int8_t) (ndim - 1), copy_shape, ncopy, copy_start);

    // Translate this index to the src buffer
    int64_t src_copy_start;
    blosc2_multidim_to_unidim(copy_start, (int8_t) (ndim - 1), src_strides, &src_copy_start);

    // Translate this index to the dst buffer
    int64_t dst_copy_start;
    blosc2_multidim_to_unidim(copy_start, (int8_t) (ndim - 1), dst_strides, &dst_copy_start);

    // Perform the copy
    memcpy(&bdst[dst_copy_start * itemsize],
           &bsrc[src_copy_start * itemsize],
           copy_nbytes);
  }
}

int b2nd_copy_buffer(int8_t ndim,
                     uint8_t itemsize,
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
  int64_t src_strides[B2ND_MAX_DIM];
  src_strides[ndim - 1] = 1;
  for (int i = ndim - 2; i >= 0; --i) {
    src_strides[i] = src_strides[i + 1] * src_pad_shape[i + 1];
  }

  int64_t dst_strides[B2ND_MAX_DIM];
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

  switch (ndim) {
    case 1:
      memcpy(&bdst[0], &bsrc[0], copy_shape[0] * itemsize);
      break;
    case 2:
      copy2dim(itemsize, copy_shape, bsrc, src_strides, bdst, dst_strides);
      break;
    case 3:
      copy3dim(itemsize, copy_shape, bsrc, src_strides, bdst, dst_strides);
      break;
    case 4:
      copy4dim(itemsize, copy_shape, bsrc, src_strides, bdst, dst_strides);
      break;
    case 5:
      copy5dim(itemsize, copy_shape, bsrc, src_strides, bdst, dst_strides);
      break;
    case 6:
      copy6dim(itemsize, copy_shape, bsrc, src_strides, bdst, dst_strides);
      break;
    case 7:
      copy7dim(itemsize, copy_shape, bsrc, src_strides, bdst, dst_strides);
      break;
    case 8:
      copy8dim(itemsize, copy_shape, bsrc, src_strides, bdst, dst_strides);
      break;
    default:
      // guard against potential future increase to B2ND_MAX_DIM
      copy_ndim_fallback(ndim, itemsize, copy_shape, bsrc, src_strides, bdst, dst_strides);
      break;
  }

  return BLOSC2_ERROR_SUCCESS;
}
