/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"
#include <limits.h>


CUTEST_TEST_SETUP(chunk_overflow) {
  blosc2_init();
}


CUTEST_TEST_TEST(chunk_overflow) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  blosc2_storage b2_storage = {.cparams = &cparams};
  b2_storage.contiguous = false;

  int64_t shape[1] = {(int64_t)INT_MAX};
  int32_t chunkshape[1] = {INT_MAX};
  int32_t blockshape[1] = {1};

  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, 1, shape, chunkshape, blockshape,
                                        NULL, 0, NULL, 0);
  CUTEST_ASSERT("ctx should not be NULL", ctx != NULL);

  b2nd_array_t *array = NULL;
  int rc = b2nd_empty(ctx, &array);
  CUTEST_ASSERT("expected max buffer size error", rc == BLOSC2_ERROR_MAX_BUFSIZE_EXCEEDED);

  if (array != NULL) {
    B2ND_TEST_ASSERT(b2nd_free(array));
  }
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));

  return 0;
}


CUTEST_TEST_TEARDOWN(chunk_overflow) {
  blosc2_destroy();
}


int main() {
  CUTEST_TEST_RUN(chunk_overflow);
}
