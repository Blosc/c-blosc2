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

  int64_t slice_start[] = {2, 5};
  int64_t slice_stop[] = {3, 6};
  int32_t slice_chunkshape[] = {1, 1};
  int32_t slice_blockshape[] = {1, 1};

  int64_t nelem = 1;
  for (int i = 0; i < ndim; ++i) {
    nelem *= shape[i];
  }
  int64_t size = nelem * typesize;
  int8_t *data = malloc(size);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 2;
  blosc2_storage b2_storage = {.cparams=&cparams, .dparams=&dparams};

  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, ndim, shape, chunkshape, blockshape, NULL, 0,
                                        NULL, 0);

  b2nd_array_t *arr;
  BLOSC_ERROR(b2nd_from_cbuffer(ctx, &arr, data, size));


  blosc2_storage slice_b2_storage = {.cparams=&cparams, .dparams=&dparams};

  // shape will be overwritten by get_slice
  b2nd_context_t *slice_ctx = b2nd_create_ctx(&slice_b2_storage, ndim, shape, slice_chunkshape,
                                              slice_blockshape, NULL, 0,
                                              NULL, 0);

  b2nd_array_t *slice;
  BLOSC_ERROR(b2nd_get_slice(slice_ctx, &slice, arr, slice_start, slice_stop));

  BLOSC_ERROR(b2nd_squeeze(slice));

  uint8_t *buffer;
  uint64_t buffer_size = 1;
  for (int i = 0; i < slice->ndim; ++i) {
    buffer_size *= slice->shape[i];
  }
  buffer_size *= slice->sc->typesize;
  buffer = malloc(buffer_size);

  BLOSC_ERROR(b2nd_to_cbuffer(slice, buffer, buffer_size));

  BLOSC_ERROR(b2nd_free(arr));
  BLOSC_ERROR(b2nd_free(slice));
  BLOSC_ERROR(b2nd_free_ctx(ctx));
  BLOSC_ERROR(b2nd_free_ctx(slice_ctx));
  free(buffer);
  free(data);

  blosc2_destroy();
  // printf("Elapsed seconds: %.5f\n", blosc_elapsed_secs(t0, t1));

  return 0;
}
