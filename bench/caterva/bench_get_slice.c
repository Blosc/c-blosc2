/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#define DATA_TYPE int64_t

# include <caterva.h>

int main() {
  blosc_timestamp_t t0, t1;

  int nslices = 10;

  int8_t ndim = 3;
  uint8_t itemsize = sizeof(DATA_TYPE);

  int64_t shape[] = {1250, 745, 400};

  int32_t chunkshape[] = {50, 150, 100};
  int32_t blockshape[] = {13, 21, 30};

  int64_t nbytes = itemsize;
  for (int i = 0; i < ndim; ++i) {
    nbytes *= shape[i];
  }

  DATA_TYPE *src = malloc(nbytes);
  for (int i = 0; i < nbytes / itemsize; ++i) {
    src[i] = i;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 4;
  cparams.typesize = itemsize;

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_storage b2_storage = {.cparams=&cparams, .dparams=&dparams};

  caterva_params_t *params = caterva_new_params(&b2_storage, ndim, shape, chunkshape, blockshape,
                                                NULL, 0);

  caterva_array_t *arr;
  blosc_set_timestamp(&t0);
  CATERVA_ERROR(caterva_from_buffer(src, nbytes, params, &arr));
  blosc_set_timestamp(&t1);
  printf("from_buffer: %.4f s\n", blosc_elapsed_secs(t0, t1));

  blosc_set_timestamp(&t0);

  for (int dim = 0; dim < ndim; ++dim) {
    int64_t slice_start[CATERVA_MAX_DIM], slice_stop[CATERVA_MAX_DIM], slice_shape[CATERVA_MAX_DIM];
    int64_t buffersize = itemsize;
    for (int j = 0; j < ndim; ++j) {
      slice_start[j] = 0;
      slice_stop[j] = j == dim ? 1 : shape[j];
      slice_shape[j] = slice_stop[j] - slice_start[j];
      buffersize *= slice_shape[j];
    }

    DATA_TYPE *buffer = malloc(buffersize);

    for (int slice = 0; slice < nslices; ++slice) {
      slice_start[dim] = rand() % shape[dim];
      slice_stop[dim] = slice_start[dim] + 1;
      CATERVA_ERROR(caterva_get_slice_buffer(arr, slice_start, slice_stop, buffer, slice_shape, buffersize));
    }
    free(buffer);
  }

  blosc_set_timestamp(&t1);
  printf("get_slice: %.4f s\n", blosc_elapsed_secs(t0, t1));

  free(src);

  CATERVA_ERROR(caterva_free(&arr));
  CATERVA_ERROR(caterva_free_params(params));

  return 0;
}
