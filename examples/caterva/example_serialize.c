/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

# include <caterva.h>

int main() {

  int8_t ndim = 2;
  int64_t shape[] = {10, 10};
  int32_t chunkshape[] = {4, 4};
  int32_t blockshape[] = {2, 2};
  int32_t typesize = 8;

  int64_t nelem = 1;
  for (int i = 0; i < ndim; ++i) {
    nelem *= shape[i];
  }
  int64_t size = nelem * typesize;
  double *data = malloc(size);

  for (int i = 0; i < nelem; ++i) {
    data[i] = i;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = typesize;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_storage b2_storage = {.cparams=&cparams, .dparams=&dparams};
  b2_storage.contiguous = false;

  caterva_context_t *ctx = caterva_create_ctx(&b2_storage, ndim, shape, chunkshape, blockshape,
                                              NULL, 0);

  caterva_array_t *arr;
  CATERVA_ERROR(caterva_from_buffer(ctx, &arr, data, size));

  uint8_t *cframe;
  int64_t cframe_len;
  bool needs_free;
  CATERVA_ERROR(caterva_to_cframe(arr, &cframe, &cframe_len, &needs_free));

  caterva_array_t *dest;
  CATERVA_ERROR(caterva_from_cframe(cframe, cframe_len, true, &dest));

  /* Fill dest array with caterva_array_t data */
  uint8_t *data_dest = malloc(size);
  CATERVA_ERROR(caterva_to_buffer(dest, data_dest, size));

  for (int i = 0; i < nelem; ++i) {
    if (data[i] != data_dest[i] && data[i] != i) {
      return -1;
    }
  }

  /* Free mallocs */
  free(data);
  free(data_dest);

  CATERVA_ERROR(caterva_free(&arr));
  CATERVA_ERROR(caterva_free(&dest));
  CATERVA_ERROR(caterva_free_ctx(ctx));

  return 0;
}
