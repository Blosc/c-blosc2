/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <b2nd.h>

int main() {

  blosc2_init();

  int8_t ndim = 2;
  int64_t shape[] = {10, 10};
  int32_t chunkshape[] = {4, 4};
  int32_t blockshape[] = {2, 2};
  int32_t typesize = 8;

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  blosc2_storage b2_storage = {.cparams=&cparams};
  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, ndim, shape, chunkshape, blockshape, NULL, 0,
                                        NULL, 0);

  int64_t dataitems = 1;
  for (int i = 0; i < ndim; ++i) {
    dataitems *= shape[i];
  }
  int64_t datasize = dataitems * typesize;
  double *data = malloc(datasize);
  for (int i = 0; i < dataitems; ++i) {
    data[i] = (double) i;
  }
  b2nd_array_t *arr;
  BLOSC_ERROR(b2nd_from_cbuffer(ctx, &arr, data, datasize));
  free(data);

  int64_t sel0[] = {3, 1, 2};
  int64_t sel1[] = {2, 5};
  int64_t sel2[] = {3, 3, 3, 9, 3, 1, 0};
  int64_t *selection[] = {sel0, sel1, sel2};
  int64_t selection_size[] = {sizeof(sel0) / sizeof(int64_t), sizeof(sel1) / (sizeof(int64_t)),
                              sizeof(sel2) / (sizeof(int64_t))};
  int64_t *buffershape = selection_size;
  int64_t nitems = 1;
  for (int i = 0; i < ndim; ++i) {
    nitems *= buffershape[i];
  }
  int64_t buffersize = nitems * arr->sc->typesize;
  double *buffer = calloc(nitems, arr->sc->typesize);
  BLOSC_ERROR(b2nd_set_orthogonal_selection(arr, selection, selection_size, buffer, buffershape, buffersize));
  BLOSC_ERROR(b2nd_get_orthogonal_selection(arr, selection, selection_size, buffer, buffershape, buffersize));

  printf("Results: \n");
  for (int i = 0; i < nitems; ++i) {
    if (i % buffershape[1] == 0) {
      printf("\n");
    }
    printf(" %f ", buffer[i]);
  }
  printf("\n");
  free(buffer);
  BLOSC_ERROR(b2nd_free(arr));
  BLOSC_ERROR(b2nd_free_ctx(ctx));

  blosc2_destroy();

  return 0;
}
