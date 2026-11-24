/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* b2nd_get_slice_cbuffer_into(): slices go to a box of a larger buffer, and no
   byte of the buffer outside that box changes.  Pseudo-random slices of arrays
   in each backend, with all-zero chunks and some padding around the box on
   every axis; plus empty slices and invalid arguments. */

#include "test_common.h"

#define SENTINEL 0xA5

typedef struct {
  int8_t ndim;
  int64_t shape[B2ND_MAX_DIM];
  int32_t chunkshape[B2ND_MAX_DIM];
  int32_t blockshape[B2ND_MAX_DIM];
  bool from_zeros;  // b2nd_zeros() and one box written: special zero chunks elsewhere
} into_shapes_t;


static uint64_t lcg_state;

static int64_t lcg(int64_t n) {
  return test_lcg(&lcg_state, n);
}

// The bytes of item `idx` of the source array (zero if `zero`)
static void item_bytes(int64_t idx, bool zero, uint8_t typesize, uint8_t *out) {
  uint64_t v = zero ? 0 : (uint64_t) idx * 2654435761ULL + 17;
  for (int b = 0; b < typesize; ++b) {
    out[b] = (uint8_t) (v >> (8 * (b % 8)));
  }
}


CUTEST_TEST_SETUP(get_slice_buffer_into) {
  blosc2_init();

  CUTEST_PARAMETRIZE(typesize, uint8_t, CUTEST_DATA(1, 2, 4, 8));
  CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
      {false, false},
      {true, false},
      {true, true},
      {false, true},
  ));
  CUTEST_PARAMETRIZE(shapes, into_shapes_t, CUTEST_DATA(
      {1, {50}, {12}, {5}, false},
      {2, {21, 17}, {8, 6}, {3, 4}, false},
      {3, {13, 11, 9}, {5, 4, 4}, {2, 2, 3}, false},
      {4, {3, 9, 8, 7}, {1, 4, 3, 4}, {1, 2, 3, 2}, false},
      {3, {16, 12, 10}, {4, 4, 4}, {2, 2, 2}, true},
  ));
}

CUTEST_TEST_TEST(get_slice_buffer_into) {
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(shapes, into_shapes_t);
  CUTEST_GET_PARAMETER(typesize, uint8_t);

  char *urlpath = "test_b2nd_get_slice_buffer_into.b2frame";
  blosc2_remove_urlpath(urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 2;
  cparams.typesize = typesize;
  blosc2_storage b2_storage = {.cparams=&cparams};
  if (backend.persistent) {
    b2_storage.urlpath = urlpath;
  }
  b2_storage.contiguous = backend.contiguous;
  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, shapes.ndim, shapes.shape,
                                        shapes.chunkshape, shapes.blockshape, NULL, 0, NULL, 0);
  int8_t ndim = shapes.ndim;

  // Source items, as the array holds them
  int64_t nitems = 1;
  for (int i = 0; i < ndim; ++i) {
    nitems *= shapes.shape[i];
  }
  uint8_t *src = malloc((size_t) (nitems * typesize));
  int64_t strides[B2ND_MAX_DIM];
  strides[ndim - 1] = 1;
  for (int i = ndim - 2; i >= 0; --i) {
    strides[i] = strides[i + 1] * shapes.shape[i + 1];
  }
  int64_t zbox_start[B2ND_MAX_DIM], zbox_stop[B2ND_MAX_DIM];
  for (int i = 0; i < ndim; ++i) {
    zbox_start[i] = shapes.shape[i] / 4;
    zbox_stop[i] = shapes.shape[i] / 2 + 1;
  }
  for (int64_t idx = 0; idx < nitems; ++idx) {
    int64_t coords[B2ND_MAX_DIM];
    bool in_zbox = true;
    for (int i = 0; i < ndim; ++i) {
      coords[i] = (idx / strides[i]) % shapes.shape[i];
      in_zbox &= coords[i] >= zbox_start[i] && coords[i] < zbox_stop[i];
    }
    // from_zeros: data only in the box written below; else zero in the first
    // half of the first axis, so that some chunks are all zeros
    bool zero = shapes.from_zeros ? !in_zbox : coords[0] < shapes.shape[0] / 2;
    item_bytes(idx, zero, typesize, src + idx * typesize);
  }

  b2nd_array_t *arr;
  if (shapes.from_zeros) {
    B2ND_TEST_ASSERT(b2nd_zeros(ctx, &arr));
    int64_t bshape[B2ND_MAX_DIM];
    int64_t bsize = typesize;
    for (int i = 0; i < ndim; ++i) {
      bshape[i] = zbox_stop[i] - zbox_start[i];
      bsize *= bshape[i];
    }
    uint8_t *box = malloc((size_t) bsize);
    for (int64_t j = 0; j < bsize / typesize; ++j) {
      int64_t idx = 0;
      int64_t rest = j;
      for (int i = ndim - 1; i >= 0; --i) {
        idx += (zbox_start[i] + rest % bshape[i]) * strides[i];
        rest /= bshape[i];
      }
      memcpy(box + j * typesize, src + idx * typesize, typesize);
    }
    B2ND_TEST_ASSERT(b2nd_set_slice_cbuffer(box, bshape, bsize, zbox_start, zbox_stop, arr));
    free(box);
  }
  else {
    B2ND_TEST_ASSERT(b2nd_from_cbuffer(ctx, &arr, src, nitems * typesize));
  }

  lcg_state = 12345u + (uint64_t) ndim * 7u + typesize;
  for (int round = 0; round < 25; ++round) {
    int64_t start[B2ND_MAX_DIM], stop[B2ND_MAX_DIM], bufshape[B2ND_MAX_DIM], pad[B2ND_MAX_DIM];
    int64_t bufitems = 1;
    for (int i = 0; i < ndim; ++i) {
      start[i] = lcg(shapes.shape[i]);
      stop[i] = start[i] + 1 + lcg(shapes.shape[i] - start[i]);
      pad[i] = lcg(3);
      bufshape[i] = stop[i] - start[i] + pad[i] + lcg(3);
      bufitems *= bufshape[i];
    }
    int64_t bufstrides[B2ND_MAX_DIM];
    bufstrides[ndim - 1] = 1;
    for (int i = ndim - 2; i >= 0; --i) {
      bufstrides[i] = bufstrides[i + 1] * bufshape[i + 1];
    }
    int64_t origin = 0;
    for (int i = 0; i < ndim; ++i) {
      origin += pad[i] * bufstrides[i];
    }
    uint8_t *buf = malloc((size_t) (bufitems * typesize));
    memset(buf, SENTINEL, (size_t) (bufitems * typesize));
    B2ND_TEST_ASSERT(b2nd_get_slice_cbuffer_into(arr, start, stop, buf + origin * typesize, bufshape,
                                                 (bufitems - origin) * typesize));
    for (int64_t j = 0; j < bufitems; ++j) {
      bool inside = true;
      int64_t idx = 0;
      for (int i = 0; i < ndim; ++i) {
        int64_t c = (j / bufstrides[i]) % bufshape[i] - pad[i];
        inside &= c >= 0 && c < stop[i] - start[i];
        idx += (start[i] + c) * strides[i];
      }
      for (int b = 0; b < typesize; ++b) {
        uint8_t got = buf[j * typesize + b];
        uint8_t want = inside ? src[idx * typesize + b] : SENTINEL;
        CUTEST_ASSERT(inside ? "Wrong item in the slice" : "Byte outside the slice changed", got == want);
      }
    }

    // An empty slice writes nothing
    int64_t empty_stop[B2ND_MAX_DIM];
    memcpy(empty_stop, stop, sizeof(empty_stop));
    empty_stop[ndim - 1] = start[ndim - 1];
    memset(buf, SENTINEL, (size_t) (bufitems * typesize));
    B2ND_TEST_ASSERT(b2nd_get_slice_cbuffer_into(arr, start, empty_stop, buf, bufshape,
                                                 bufitems * typesize));
    for (int64_t j = 0; j < bufitems * typesize; ++j) {
      CUTEST_ASSERT("An empty slice wrote to the buffer", buf[j] == SENTINEL);
    }

    // Once per parameter set: the exact size of the box is enough; too small a
    // buffer, a buffer shape smaller than the slice and a slice outside the
    // array are refused
    if (round == 0) {
      int64_t last = origin;
      for (int i = 0; i < ndim; ++i) {
        last += (stop[i] - start[i] - 1) * bufstrides[i];
      }
      CUTEST_ASSERT("The exact size of the box was not accepted",
                    b2nd_get_slice_cbuffer_into(arr, start, stop, buf + origin * typesize, bufshape,
                                                (last - origin + 1) * typesize) >= 0);
      CUTEST_ASSERT("Too small a buffer was accepted",
                    b2nd_get_slice_cbuffer_into(arr, start, stop, buf + origin * typesize, bufshape,
                                                (last - origin + 1) * typesize - 1) < 0);
      int64_t small_shape[B2ND_MAX_DIM];
      memcpy(small_shape, bufshape, sizeof(small_shape));
      small_shape[ndim - 1] = stop[ndim - 1] - start[ndim - 1] - 1;
      CUTEST_ASSERT("A buffer shape smaller than the slice was accepted",
                    b2nd_get_slice_cbuffer_into(arr, start, stop, buf, small_shape, bufitems * typesize) < 0);
      int64_t outside_stop[B2ND_MAX_DIM];
      memcpy(outside_stop, stop, sizeof(outside_stop));
      outside_stop[0] = shapes.shape[0] + 1;
      CUTEST_ASSERT("A slice outside the array was accepted",
                    b2nd_get_slice_cbuffer_into(arr, start, outside_stop, buf, bufshape, bufitems * typesize) < 0);
    }
    free(buf);
  }

  free(src);
  B2ND_TEST_ASSERT(b2nd_free(arr));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));
  blosc2_remove_urlpath(urlpath);

  // A 0-d uninitialized array reads as zero, and the bytes after its item stay
  blosc2_storage storage0 = {.cparams=&cparams};
  b2nd_context_t *ctx0 = b2nd_create_ctx(&storage0, 0, NULL, NULL, NULL, NULL, 0, NULL, 0);
  b2nd_array_t *arr0;
  B2ND_TEST_ASSERT(b2nd_uninit(ctx0, &arr0));
  uint8_t buf0[16];
  memset(buf0, SENTINEL, sizeof(buf0));
  int64_t none[1] = {0};
  B2ND_TEST_ASSERT(b2nd_get_slice_cbuffer_into(arr0, none, none, buf0, none, typesize));
  for (int b = 0; b < (int) sizeof(buf0); ++b) {
    CUTEST_ASSERT("Wrong 0-d read", buf0[b] == (b < typesize ? 0 : SENTINEL));
  }
  B2ND_TEST_ASSERT(b2nd_free(arr0));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx0));
  return 0;
}

CUTEST_TEST_TEARDOWN(get_slice_buffer_into) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(get_slice_buffer_into);
}
