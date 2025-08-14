/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"

#ifdef __GNUC__

#include <unistd.h>

#define FILE_EXISTS(urlpath) access(urlpath, F_OK)
#else
#include <io.h>
#define FILE_EXISTS(urlpath) _access(urlpath, 0)
#endif


typedef struct {
  int8_t ndim;
  int64_t shape[B2ND_MAX_DIM];
  int32_t chunkshape[B2ND_MAX_DIM];
  int32_t blockshape[B2ND_MAX_DIM];
} test_shapes_t;


CUTEST_TEST_SETUP(save) {
  blosc2_init();

  // Add parametrizations
  CUTEST_PARAMETRIZE(typesize, uint8_t, CUTEST_DATA(1, 2, 4, 8));
  CUTEST_PARAMETRIZE(shapes, test_shapes_t, CUTEST_DATA(
      {0, {0}, {0}, {0}}, // 0-dim
      {1, {10}, {7}, {2}}, // 1-idim
      {2, {100, 100}, {20, 20}, {10, 10}},
      {3, {40, 55, 23}, {31, 5, 22}, {4, 4, 4}},
  ));
  CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
      {true, false},
      {false, false},
  ));
  CUTEST_PARAMETRIZE(padding, uint32_t, CUTEST_DATA(1, 57, 1024, 4031));
}

CUTEST_TEST_TEST(save) {
  CUTEST_GET_PARAMETER(backend, _test_backend);
  CUTEST_GET_PARAMETER(shapes, test_shapes_t);
  CUTEST_GET_PARAMETER(typesize, uint8_t);
  CUTEST_GET_PARAMETER(padding, uint32_t);

  char *urlpath = "test_save.b2frame";
  blosc2_remove_urlpath(urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 2;
  cparams.typesize = typesize;
  blosc2_storage b2_storage = {.cparams=&cparams};
  b2_storage.urlpath = NULL;
  b2_storage.contiguous = backend.contiguous;

  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, shapes.ndim, shapes.shape,
                                        shapes.chunkshape, shapes.blockshape, NULL, 0, NULL, 0);

  /* Create original data */
  int64_t buffersize = typesize;
  for (int i = 0; i < ctx->ndim; ++i) {
    buffersize *= shapes.shape[i];
  }
  uint8_t *buffer = malloc(buffersize);
  CUTEST_ASSERT("Buffer filled incorrectly", fill_buf(buffer, typesize, buffersize / typesize));

  /* Create b2nd_array_t with original data */
  b2nd_array_t *src;
  B2ND_TEST_ASSERT(b2nd_from_cbuffer(ctx, &src, buffer, buffersize));

  /* Write some padding to the file, so appending will not be degenerated */
  uint8_t *pad_buffer = calloc(padding, 1);
  FILE *fp = fopen(urlpath, "ab");
  fwrite(pad_buffer, 1, padding, fp);
  fclose(fp);
  free(pad_buffer);

  int64_t offset = b2nd_save_append(src, urlpath);
  B2ND_TEST_ASSERT(offset);
  CUTEST_ASSERT("Unexpected offset", offset == padding);
  b2nd_array_t *dest;
  B2ND_TEST_ASSERT(b2nd_open_offset(urlpath, &dest, offset));

  /* Fill dest array with b2nd_array_t data */
  uint8_t *buffer_dest = malloc(buffersize);
  B2ND_TEST_ASSERT(b2nd_to_cbuffer(dest, buffer_dest, buffersize));

  /* Testing */
  if (dest->nitems != 0) {
    for (int i = 0; i < buffersize / typesize; ++i) {
      CUTEST_ASSERT("Elements are not equals!", buffer[i] == buffer_dest[i]);
    }
  }

  /* Free mallocs */
  free(buffer);
  free(buffer_dest);
  B2ND_TEST_ASSERT(b2nd_free(src));
  B2ND_TEST_ASSERT(b2nd_free(dest));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));

  blosc2_remove_urlpath(urlpath);

  return 0;
}


CUTEST_TEST_TEARDOWN(save) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(save);

}
