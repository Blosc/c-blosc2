/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
**********************************************************************/

#include "b2nd_exec.h"

#include <math.h>
#include <stdio.h>

#define NITEMS 8

int main(void) {
  blosc2_init();
  if (!b2nd_exec_available()) {
    printf("C-Blosc2 built without MiniExpr execution support; skipping example.\n");
    blosc2_destroy();
    return 0;
  }

  double x_buf[NITEMS], y_buf[NITEMS], out_buf[NITEMS];
  for (int i = 0; i < NITEMS; i++) {
    x_buf[i] = (double)i * 0.5;
    y_buf[i] = (double)(NITEMS - i);
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(double);
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_storage storage = {.cparams = &cparams, .dparams = &dparams};
  int64_t shape[1] = {NITEMS};
  int32_t chunkshape[1] = {4};
  int32_t blockshape[1] = {2};
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, 1, shape, chunkshape, blockshape,
                                        "<f8", DTYPE_NUMPY_FORMAT, NULL, 0);
  if (!ctx) {
    blosc2_destroy();
    return 1;
  }

  b2nd_array_t *x = NULL, *y = NULL, *out = NULL;
  if (b2nd_from_cbuffer(ctx, &x, x_buf, sizeof(x_buf)) < 0 ||
      b2nd_from_cbuffer(ctx, &y, y_buf, sizeof(y_buf)) < 0) {
    b2nd_free_ctx(ctx);
    b2nd_free(x);
    blosc2_destroy();
    return 1;
  }
  b2nd_free_ctx(ctx);

  const char *names[2] = {"x", "y"};
  const b2nd_array_t *inputs[2] = {x, y};
  b2nd_exec_error_t err = B2ND_EXEC_ERROR_INITIALIZER;
  b2nd_exec_status_t st = b2nd_eval("sin(x) * 2.5 + y", names, inputs, 2,
                                    NULL, NULL, NULL, NULL, NULL, &out, NULL, &err);
  if (st != B2ND_EXEC_SUCCESS) {
    fprintf(stderr, "b2nd_eval failed: %s\n", err.message);
    b2nd_free(x);
    b2nd_free(y);
    blosc2_destroy();
    return 1;
  }

  if (b2nd_to_cbuffer(out, out_buf, sizeof(out_buf)) < 0) {
    b2nd_free(out);
    b2nd_free(x);
    b2nd_free(y);
    blosc2_destroy();
    return 1;
  }
  printf("sin(x[3]) * 2.5 + y[3] = %.6f (expected %.6f)\n",
         out_buf[3], sin(x_buf[3]) * 2.5 + y_buf[3]);

  b2nd_free(out);
  b2nd_free(x);
  b2nd_free(y);
  blosc2_destroy();
  return 0;
}
