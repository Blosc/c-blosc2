/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"

uint64_t result0[1024] = {0};
uint64_t result1[1024] = {2, 3, 4, 5, 6, 7, 8};
uint64_t result2[1024] = {53, 54, 55, 56, 57, 58, 59, 63, 64, 65, 66, 67, 68, 69, 73, 74, 75, 76,
                          77, 78, 79, 83, 84, 85, 86, 87, 88, 89};
uint64_t result3[1024] = {303, 304, 305, 306, 307, 308, 309, 313, 314, 315, 316, 317, 318, 319,
                          323, 324, 325, 326, 327, 328, 329, 333, 334, 335, 336, 337, 338, 339,
                          343, 344, 345, 346, 347, 348, 349, 353, 354, 355, 356, 357, 358, 359,
                          363, 364, 365, 366, 367, 368, 369, 403, 404, 405, 406, 407, 408, 409,
                          413, 414, 415, 416, 417, 418, 419, 423, 424, 425, 426, 427, 428, 429,
                          433, 434, 435, 436, 437, 438, 439, 443, 444, 445, 446, 447, 448, 449,
                          453, 454, 455, 456, 457, 458, 459, 463, 464, 465, 466, 467, 468, 469,
                          503, 504, 505, 506, 507, 508, 509, 513, 514, 515, 516, 517, 518, 519,
                          523, 524, 525, 526, 527, 528, 529, 533, 534, 535, 536, 537, 538, 539,
                          543, 544, 545, 546, 547, 548, 549, 553, 554, 555, 556, 557, 558, 559,
                          563, 564, 565, 566, 567, 568, 569};
uint64_t result4[1024] = {0};
uint64_t result5[1024] = {0};

typedef struct {
    int8_t ndim;
    int64_t shape[CATERVA_MAX_DIM];
    int32_t chunkshape[CATERVA_MAX_DIM];
    int32_t blockshape[CATERVA_MAX_DIM];
    int32_t chunkshape2[CATERVA_MAX_DIM];
    int32_t blockshape2[CATERVA_MAX_DIM];
    int64_t start[CATERVA_MAX_DIM];
    int64_t stop[CATERVA_MAX_DIM];
    uint64_t *result;
} test_shapes_t;


CUTEST_TEST_DATA(get_slice) {
    void *unused;
};


CUTEST_TEST_SETUP(get_slice) {
  blosc2_init();

  // Add parametrizations
  CUTEST_PARAMETRIZE(typesize, uint8_t, CUTEST_DATA(8));
  CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
          {false, false},
          {true, false},
          {true, true},
          {false, true},
  ));
  CUTEST_PARAMETRIZE(backend2, _test_backend, CUTEST_DATA(
          {false, false},
          {true, false},
          {true, true},
          {false, true},
  ));

  CUTEST_PARAMETRIZE(shapes, test_shapes_t, CUTEST_DATA(
          {0, {0}, {0}, {0}, {0}, {0}, {0}, {0}, result0}, // 0-dim
          {1, {10}, {7}, {2}, {6}, {2}, {2}, {9}, result1}, // 1-idim
          {2, {14, 10}, {8, 5}, {2, 2}, {4, 4}, {2, 3}, {5, 3}, {9, 10}, result2}, // general,
          {3, {10, 10, 10}, {3, 5, 9}, {3, 4, 4}, {3, 7, 7}, {2, 5, 5}, {3, 0, 3}, {6, 7, 10}, result3}, // general
          {2, {20, 0}, {7, 0}, {3, 0}, {5, 0}, {2, 0}, {2, 0}, {8, 0}, result4}, // 0-shape
          {2, {20, 10}, {7, 5}, {3, 5}, {5, 5}, {2, 2}, {2, 0}, {18, 0}, result5}, // 0-shape
  ));
}

CUTEST_TEST_TEST(get_slice) {
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(shapes, test_shapes_t);
  CUTEST_GET_PARAMETER(backend2, _test_backend);
  CUTEST_GET_PARAMETER(typesize, uint8_t);

  char *urlpath = "test_get_slice.b2frame";
  char *urlpath2 = "test_get_slice2.b2frame";

  blosc2_remove_urlpath(urlpath);
  blosc2_remove_urlpath(urlpath2);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  cparams.nthreads = 2;
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.typesize = typesize;
  blosc2_storage b2_storage = {.cparams=&cparams, .dparams=&dparams};
  if (backend.persistent) {
    b2_storage.urlpath = urlpath;
  }
  b2_storage.contiguous = backend.contiguous;

  caterva_params_t *params = caterva_new_params(&b2_storage, shapes.ndim, shapes.shape,
                                                shapes.chunkshape, shapes.blockshape, NULL, 0);

  /* Create original data */
  size_t buffersize = typesize;
  for (int i = 0; i < params->ndim; ++i) {
    buffersize *= (size_t) shapes.shape[i];
  }
  uint8_t *buffer = malloc(buffersize);
  CUTEST_ASSERT("Buffer filled incorrectly", fill_buf(buffer, typesize, buffersize / typesize));

  /* Create caterva_array_t with original data */
  caterva_array_t *src;
  CATERVA_TEST_ASSERT(caterva_from_buffer(buffer, buffersize, params, &src));

  /* Add vlmeta */

  blosc2_metalayer vlmeta;
  vlmeta.name = "test_get_slice";
  double sdata = 2.3;
  vlmeta.content = (uint8_t *) &sdata;
  vlmeta.content_len = sizeof(double);

  CATERVA_TEST_ASSERT(blosc2_vlmeta_add(src->sc, vlmeta.name, vlmeta.content, vlmeta.content_len,
                                        src->sc->storage->cparams));

  /* Create storage for dest container */
  blosc2_storage b2_storage2 = {.cparams=&cparams, .dparams=&dparams};
  if (backend2.persistent) {
    b2_storage2.urlpath = urlpath2;
  }
  b2_storage.contiguous = backend2.contiguous;

  // shape param will then be ignored
  caterva_params_t *params2 = caterva_new_params(&b2_storage2, shapes.ndim, shapes.shape,
                                                shapes.chunkshape2, shapes.blockshape2, NULL, 0);

  blosc2_context *ctx = blosc2_create_cctx(*b2_storage2.cparams);

  caterva_array_t *dest;
  CATERVA_TEST_ASSERT(caterva_get_slice(src, shapes.start, shapes.stop,
                                            params2, &dest));

  /* Check metalayers */
  int rc = blosc2_meta_exists(dest->sc, "caterva");
  CUTEST_ASSERT("metalayer not exists", rc == 0);
  rc = blosc2_vlmeta_exists(dest->sc, vlmeta.name);
  CUTEST_ASSERT("vlmetalayer should not exist", rc < 0);

  int64_t destbuffersize = typesize;
  for (int i = 0; i < src->ndim; ++i) {
    destbuffersize *= (shapes.stop[i] - shapes.start[i]);
  }

  uint64_t *buffer_dest = malloc((size_t) destbuffersize);
  CATERVA_TEST_ASSERT(caterva_to_buffer(ctx, dest, buffer_dest, destbuffersize));

  for (int i = 0; i < destbuffersize / typesize; ++i) {
    uint64_t a = shapes.result[i] + 1;
    uint64_t b = buffer_dest[i];
    CUTEST_ASSERT("Elements are not equals!", a == b);
  }

  /* Free mallocs */
  free(buffer);
  free(buffer_dest);
  CATERVA_TEST_ASSERT(caterva_free(&src));
  CATERVA_TEST_ASSERT(caterva_free(&dest));
  CATERVA_TEST_ASSERT(caterva_free_params(params));
  CATERVA_TEST_ASSERT(caterva_free_params(params2));
  blosc2_free_ctx(ctx);
  blosc2_remove_urlpath(urlpath);
  blosc2_remove_urlpath(urlpath2);

  return 0;
}

CUTEST_TEST_TEARDOWN(get_slice) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(get_slice);
}
