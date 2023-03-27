/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/


#include "test_common.h"
#include "blosc2/codecs-registry.h"
#include <math.h>

float result0[1024] = {0};
float result1[1024] = {2, 3, 4, 5, 6, 7, 8};
float result2[1024] = {53, 54, 55, 56, 57, 58, 59, 63, 64, 65, 66, 67, 68, 69, 73, 74, 75, 76,
                       77, 78, 79, 83, 84, 85, 86, 87, 88, 89};
float result3[1024] = {303, 304, 305, 306, 307, 308, 309, 313, 314, 315, 316, 317, 318, 319,
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
float result4[1024] = {0};
float result5[1024] = {0};

typedef struct {
  int8_t ndim;
  int64_t shape[B2ND_MAX_DIM];
  int32_t chunkshape[B2ND_MAX_DIM];
  int32_t blockshape[B2ND_MAX_DIM];
  int32_t chunkshape2[B2ND_MAX_DIM];
  int32_t blockshape2[B2ND_MAX_DIM];
  int64_t start[B2ND_MAX_DIM];
  int64_t stop[B2ND_MAX_DIM];
  float *result;
} test_shapes_t;


CUTEST_TEST_SETUP(get_slice_buffer) {
  blosc2_init();

  // Add parametrizations
  CUTEST_PARAMETRIZE(typesize, uint8_t, CUTEST_DATA(4));
  CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
      {false, false},
//      {true, false},
//      {true, true},
//      {false, true},
  ));

  CUTEST_PARAMETRIZE(shapes, test_shapes_t, CUTEST_DATA(
      {0, {0}, {0}, {0}, {0}, {0}, {0}, {0}, result0}, // 0-dim
      {1, {10}, {7}, {2}, {6}, {2}, {2}, {9}, result1}, // 1-idim
      {2, {16, 10}, {16, 10}, {8, 8}, {16, 16}, {8, 8}, {5, 3}, {9, 10}, result2}, // general,
      {3, {10, 10, 10}, {3, 5, 9}, {3, 4, 4}, {3, 7, 7}, {2, 5, 5}, {3, 0, 3}, {6, 7, 10}, result3}, // general
      {2, {20, 0}, {7, 0}, {3, 0}, {5, 0}, {2, 0}, {2, 0}, {8, 0}, result4}, // 0-shape
      {2, {20, 10}, {7, 5}, {4, 5}, {5, 5}, {2, 2}, {2, 0}, {18, 0}, result5}, // 0-shape
  ));
}

CUTEST_TEST_TEST(get_slice_buffer) {
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(shapes, test_shapes_t);
  CUTEST_GET_PARAMETER(typesize, uint8_t);

  char *urlpath = "test_get_slice_buffer.b2frame";
  blosc2_remove_urlpath(urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 1;
  cparams.typesize = typesize;
  cparams.compcode = BLOSC_CODEC_ZFP_FIXED_RATE;
  cparams.compcode_meta = 40;
  blosc2_storage b2_storage = {.cparams=&cparams};
  if (backend.persistent) {
    b2_storage.urlpath = urlpath;
  }
  b2_storage.contiguous = backend.contiguous;

  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, shapes.ndim, shapes.shape,
                                        shapes.chunkshape, shapes.blockshape, NULL, 0, NULL, 0);

  /* Create original data */
  size_t buffersize = typesize;
  for (int i = 0; i < ctx->ndim; ++i) {
    buffersize *= (size_t) shapes.shape[i];
  }
  float *buffer = malloc(buffersize);

  for (int i = 0; i < (int ) (buffersize / typesize); ++i) {
    buffer[i] = (float) i;
  }
  printf("\n Buffer: \n");
  for (int i = 0; i < (int ) (buffersize / typesize); ++i) {
    printf("%f, ", buffer[i]);
  }

  /* Create b2nd_array_t with original data */
  b2nd_array_t *src;
  B2ND_TEST_ASSERT(b2nd_from_cbuffer(ctx, &src, buffer, buffersize));

  float *cbuffer = malloc(buffersize);
  printf("\nTO_BUFFER");
  B2ND_TEST_ASSERT(b2nd_to_cbuffer(src, cbuffer, buffersize));
  printf("\n Cbuffer: \n");
  for (int i = 0; i < (int ) (buffersize / typesize); ++i) {
    printf("%g, ", cbuffer[i]);
  }
  /* Create dest buffer */
  int64_t destshape[B2ND_MAX_DIM] = {0};
  int64_t destbuffersize = typesize;
  for (int i = 0; i < ctx->ndim; ++i) {
    destshape[i] = shapes.stop[i] - shapes.start[i];
    destbuffersize *= destshape[i];
  }

  float *destbuffer = malloc((size_t) destbuffersize);

  /* Fill dest buffer with a slice*/
  printf("\n\nGET_SLICE");
  printf("\n MALLOC Destbuffer: \n");
  for (int i = 0; i < (int ) (destbuffersize / typesize); ++i) {
    printf("%f, ", destbuffer[i]);
  }
  B2ND_TEST_ASSERT(b2nd_get_slice_cbuffer(src, shapes.start, shapes.stop, destbuffer,
                                          destshape, destbuffersize));
  printf("\n Destbuffer: \n");
  for (int i = 0; i < (int ) (destbuffersize / typesize); ++i) {
    printf("%g, ", destbuffer[i]);
  }
  double tolerance = 0.4;
  for (int i = 0; i < destbuffersize / typesize; ++i) {
    float a = destbuffer[i];
    float b = shapes.result[i];
    if ((a == 0) || (b == 0)) {
      if (fabsf(a - b) > tolerance) {
        printf("i: %d, data %.8f, dest %.8f", i, a, b);
        printf("\n Decompressed data differs from original!\n");
        return -1;
      }
    } else if (fabsf(a - b) > tolerance * fmaxf(fabsf(a), fabsf(b))) {
      printf("i: %d, data %.8f, dest %.8f", i, a, b);
      printf("\n Decompressed data differs from original!\n");
      return -1;
    }
  }

  /* Free mallocs */
  free(buffer);
  free(destbuffer);
  B2ND_TEST_ASSERT(b2nd_free(src));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));

  blosc2_remove_urlpath(urlpath);

  return 0;
}

CUTEST_TEST_TEARDOWN(get_slice_buffer) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(get_slice_buffer);
}
