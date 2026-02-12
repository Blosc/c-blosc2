/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"

bool fill_buf_string(void *buf, uint8_t str_len, size_t buf_size) {
    for (size_t i = 0; i < buf_size; ++i) {
        for (size_t j = 0; j < str_len; ++j) {
        ((uint32_t *) buf)[i + j] = (uint32_t) i + 1;
        }
    }
  return true;
}

CUTEST_TEST_SETUP(stringshuffle) {
  blosc2_init();

  // Add parametrizations
    CUTEST_PARAMETRIZE(shapes, _test_shapes, CUTEST_DATA(
      {2, {40, 40}, {20, 20}, {10, 10}},
      {3, {40, 55, 23}, {31, 5, 22}, {4, 4, 4}},
      {3, {40, 0, 12}, {31, 0, 12}, {10, 0, 12}},
      {4, {50, 60, 31, 12}, {25, 20, 20, 10}, {5, 5, 5, 10}},
      {5, {1, 1, 1024, 1, 1}, {1, 1, 500, 1, 1}, {1, 1, 200, 1, 1}},
      {6, {5, 1, 50, 3, 1, 2}, {5, 1, 50, 2, 1, 2}, {2, 1, 20, 2, 1, 2}},
      {B2ND_MAX_DIM,  {2, 3, 1, 1, 1, 1, 8, 1, 2, 2, 1, 1, 1, 1, 1, 2},
        {1, 2, 1, 1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
  ));
}


CUTEST_TEST_TEST(stringshuffle) {
  CUTEST_GET_PARAMETER(shapes, _test_shapes);
  uint8_t str_len = 10;
  uint8_t charsize = sizeof(uint32_t);
  uint8_t typesize = charsize * str_len;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 2;
  cparams.typesize = typesize;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_SHUFFLE;
  cparams.filters_meta[BLOSC2_MAX_FILTERS - 1] = charsize;

  /* Create original data */
  size_t buffersize = (size_t) typesize;
  for (int i = 0; i < shapes.ndim; ++i) {
    buffersize *= (size_t) shapes.shape[i];
  }
  uint8_t *buffer = malloc(buffersize);
  CUTEST_ASSERT("Buffer filled incorrectly", fill_buf_string(buffer, str_len, buffersize / typesize));

  /* Compress with filters_meta*/
  blosc2_context* cctx = blosc2_create_cctx(cparams);
  size_t dest_buffersize = buffersize + BLOSC2_MAX_OVERHEAD;
  uint8_t *dest = malloc(dest_buffersize);
  int cbytes = blosc2_compress_ctx(cctx, buffer, buffersize, dest, dest_buffersize);
  B2ND_TEST_ASSERT(cbytes);

  uint8_t *dest2 = malloc(dest_buffersize);
  cctx->filters_meta[-1] = 0; // now will use typesize by default for shuffle
  int cbytes2 = blosc2_compress_ctx(cctx, buffer, buffersize, dest2, dest_buffersize);
  B2ND_TEST_ASSERT(cbytes2);
  if (cbytes2 < cbytes){
    printf("Shuffle works better using stringsize not charsize!");
    return BLOSC2_ERROR_FAILURE;
  }
  /* Do b2nd_array roundtrip */
  blosc2_storage b2_storage = {.cparams=&cparams};
  b2nd_context_t *b2nd_ctx = b2nd_create_ctx(&b2_storage, shapes.ndim, shapes.shape,
                                        shapes.chunkshape, shapes.blockshape, NULL, 0, NULL, 0);
  b2nd_array_t *src;
  B2ND_TEST_ASSERT(b2nd_from_cbuffer(b2nd_ctx, &src, buffer, buffersize));
  uint8_t *buffer_dest = malloc(buffersize);
  B2ND_TEST_ASSERT(b2nd_to_cbuffer(src, buffer_dest, buffersize));

  /* Testing */
  B2ND_TEST_ASSERT_BUFFER(buffer, buffer_dest, (int) buffersize);

  /* Free mallocs */
  free(buffer);
  free(buffer_dest);
  free(dest);
  free(dest2);
  blosc2_free_ctx(cctx);
  B2ND_TEST_ASSERT(b2nd_free(src));
  B2ND_TEST_ASSERT(b2nd_free_ctx(b2nd_ctx));

  return BLOSC2_ERROR_SUCCESS;
}


CUTEST_TEST_TEARDOWN(stringshuffle) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(stringshuffle);
}
