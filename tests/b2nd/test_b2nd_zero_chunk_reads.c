/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* Slice reads over arrays whose chunks are partly special (zeros from
   b2nd_zeros(), uninitialized from b2nd_uninit(), which read as zeros):
   get_set_slice() zeroes those chunks' boxes without decompressing them, or
   the whole slice at once when they hold a good part of it.  Pseudo-random
   slices in every backend, into buffers of the slice's shape and into larger
   ones, and through b2nd_get_slice(), against a dense copy of the array. */

#include "test_common.h"

#define SENTINEL 0xA5

static uint64_t lcg_state;

static int64_t lcg(int64_t n) {
  return test_lcg(&lcg_state, n);
}

// Flat index of item j of a box of shape `bshape` starting at `bstart` in an array with item strides `strides`
static int64_t flat_index(int64_t j, const int64_t *bstart, const int64_t *bshape, const int64_t *strides) {
  int64_t idx = 0;
  for (int i = 2; i >= 0; --i) {
    idx += (bstart[i] + j % bshape[i]) * strides[i];
    j /= bshape[i];
  }
  return idx;
}


CUTEST_TEST_SETUP(zero_chunk_reads) {
  blosc2_init();
  CUTEST_PARAMETRIZE(typesize, uint8_t, CUTEST_DATA(1, 4, 8));
  CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
      {false, false},
      {true, false},
      {true, true},
      {false, true},
  ));
  CUTEST_PARAMETRIZE(uninit, bool, CUTEST_DATA(false, true));
}

CUTEST_TEST_TEST(zero_chunk_reads) {
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(typesize, uint8_t);
  CUTEST_GET_PARAMETER(uninit, bool);

  char *urlpath = "test_b2nd_zero_chunk_reads.b2frame";
  blosc2_remove_urlpath(urlpath);
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 2;
  cparams.typesize = typesize;
  blosc2_storage b2_storage = {.cparams=&cparams};
  if (backend.persistent) {
    b2_storage.urlpath = urlpath;
  }
  b2_storage.contiguous = backend.contiguous;
  int8_t ndim = 3;
  int64_t shape[] = {20, 18, 16};
  int32_t chunkshape[] = {5, 6, 4};
  int32_t blockshape[] = {2, 3, 2};
  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, ndim, shape, chunkshape, blockshape, NULL, 0, NULL, 0);
  // b2nd_get_slice() results live in memory (the source's urlpath is taken)
  blosc2_storage mem_storage = {.cparams=&cparams};
  b2nd_context_t *mem_ctx = b2nd_create_ctx(&mem_storage, ndim, shape, chunkshape, blockshape, NULL, 0, NULL, 0);
  b2nd_array_t *arr;
  if (uninit) {
    B2ND_TEST_ASSERT(b2nd_uninit(ctx, &arr));
  }
  else {
    B2ND_TEST_ASSERT(b2nd_zeros(ctx, &arr));
  }

  // A dense copy: zeros, then a few boxes of data written into both
  int64_t nitems = shape[0] * shape[1] * shape[2];
  int64_t strides[] = {shape[1] * shape[2], shape[2], 1};
  uint8_t *dense = calloc((size_t) nitems, typesize);
  lcg_state = 99u + typesize + (uninit ? 1000u : 0u);
  for (int b = 0; b < 3; ++b) {
    int64_t bstart[3], bstop[3], bshape[3];
    int64_t bitems = 1;
    for (int i = 0; i < ndim; ++i) {
      bstart[i] = lcg(shape[i] - 2);
      bstop[i] = bstart[i] + 1 + lcg(shape[i] / 3);
      bstop[i] = bstop[i] > shape[i] ? shape[i] : bstop[i];
      bshape[i] = bstop[i] - bstart[i];
      bitems *= bshape[i];
    }
    uint8_t *box = malloc((size_t) (bitems * typesize));
    for (int64_t j = 0; j < bitems; ++j) {
      int64_t idx = flat_index(j, bstart, bshape, strides);
      for (int k = 0; k < typesize; ++k) {
        box[j * typesize + k] = (uint8_t) (idx * 31 + k * 7 + b + 1);
        dense[idx * typesize + k] = box[j * typesize + k];
      }
    }
    B2ND_TEST_ASSERT(b2nd_set_slice_cbuffer(box, bshape, bitems * typesize, bstart, bstop, arr));
    free(box);
  }

  for (int round = 0; round < 40; ++round) {
    int64_t start[3], stop[3], sshape[3], bufshape[3];
    int64_t sitems = 1, bufitems = 1;
    for (int i = 0; i < ndim; ++i) {
      start[i] = lcg(shape[i]);
      stop[i] = start[i] + 1 + lcg(shape[i] - start[i]);
      sshape[i] = stop[i] - start[i];
      bufshape[i] = sshape[i] + (round % 2 ? lcg(3) : 0);  // odd rounds: a larger buffer
      sitems *= sshape[i];
      bufitems *= bufshape[i];
    }
    uint8_t *buf = malloc((size_t) (bufitems * typesize));
    memset(buf, SENTINEL, (size_t) (bufitems * typesize));
    B2ND_TEST_ASSERT(b2nd_get_slice_cbuffer(arr, start, stop, buf, bufshape, bufitems * typesize));
    for (int64_t j = 0; j < bufitems; ++j) {
      int64_t c0 = j / (bufshape[1] * bufshape[2]), c1 = (j / bufshape[2]) % bufshape[1], c2 = j % bufshape[2];
      bool inside = c0 < sshape[0] && c1 < sshape[1] && c2 < sshape[2];
      int64_t idx = (start[0] + c0) * strides[0] + (start[1] + c1) * strides[1] + start[2] + c2;
      for (int k = 0; k < typesize; ++k) {
        // b2nd_get_slice_cbuffer() zero-fills the rest of a larger buffer
        uint8_t want = inside ? dense[idx * typesize + k] : 0;
        CUTEST_ASSERT("Wrong byte in the buffer", buf[j * typesize + k] == want);
      }
    }
    free(buf);

    if (round % 8 == 0) {
      b2nd_array_t *sl;
      B2ND_TEST_ASSERT(b2nd_get_slice(mem_ctx, &sl, arr, start, stop));
      uint8_t *slbuf = malloc((size_t) (sitems * typesize));
      B2ND_TEST_ASSERT(b2nd_to_cbuffer(sl, slbuf, sitems * typesize));
      for (int64_t j = 0; j < sitems; ++j) {
        int64_t idx = flat_index(j, start, sshape, strides);
        CUTEST_ASSERT("Wrong item in b2nd_get_slice()",
                      memcmp(slbuf + j * typesize, dense + idx * typesize, typesize) == 0);
      }
      free(slbuf);
      B2ND_TEST_ASSERT(b2nd_free(sl));
    }
  }

  free(dense);
  B2ND_TEST_ASSERT(b2nd_free(arr));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));
  B2ND_TEST_ASSERT(b2nd_free_ctx(mem_ctx));
  blosc2_remove_urlpath(urlpath);
  return 0;
}

CUTEST_TEST_TEARDOWN(zero_chunk_reads) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(zero_chunk_reads);
}
