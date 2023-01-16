/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <caterva.h>

int main() {

    int8_t ndim = 2;
    int64_t shape[] = {10, 10};
    int32_t chunkshape[] = {4, 4};
    int32_t blockshape[] = {2, 2};
    int8_t itemsize = 8;

    int64_t slice_start[] = {2, 5};
    int64_t slice_stop[] = {3, 6};
    int32_t slice_chunkshape[] = {1, 1};
    int32_t slice_blockshape[] = {1, 1};

    int64_t nelem = 1;
    for (int i = 0; i < ndim; ++i) {
        nelem *= shape[i];
    }
    int64_t size = nelem * itemsize;
    int8_t *data = malloc(size);

    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    blosc2_context *ctx = blosc2_create_cctx(cparams);

    caterva_params_t params = {0};
    params.ndim = ndim;
    params.itemsize = itemsize;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    for (int i = 0; i < ndim; ++i) {
        storage.chunkshape[i] = chunkshape[i];
        storage.blockshape[i] = blockshape[i];
    }

    caterva_array_t *arr;
    CATERVA_ERROR(caterva_from_buffer(ctx, data, size, &params, &storage, &arr));


    caterva_storage_t slice_storage = {0};
    for (int i = 0; i < ndim; ++i) {
        slice_storage.chunkshape[i] = slice_chunkshape[i];
        slice_storage.blockshape[i] = slice_blockshape[i];
    }

    caterva_array_t *slice;
    CATERVA_ERROR(caterva_get_slice(ctx, arr, slice_start, slice_stop, &slice_storage,
                                    &slice));

    CATERVA_ERROR(caterva_squeeze(ctx, slice));

    uint8_t *buffer;
    uint64_t buffer_size = 1;
    for (int i = 0; i < slice->ndim; ++i) {
        buffer_size *= slice->shape[i];
    }
    buffer_size *= slice->sc->typesize;
    buffer = malloc(buffer_size);

    CATERVA_ERROR(caterva_to_buffer(ctx, slice, buffer, buffer_size));

    // printf("Elapsed seconds: %.5f\n", blosc_elapsed_secs(t0, t1));
    blosc2_free_ctx(ctx);

    return 0;
}
