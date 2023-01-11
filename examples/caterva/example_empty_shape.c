/*
 * Copyright (C) 2018 Francesc Alted, Aleix Alcacer.
 * Copyright (C) 2019-present Blosc Development team <blosc@blosc.org>
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

# include <caterva.h>

int main() {

    int8_t ndim = 2;
    int64_t shape[] = {10, 10};
    int32_t chunkshape[] = {4, 4};
    int32_t blockshape[] = {2, 2};
    int8_t itemsize = 8;

    int64_t slice_start[] = {2, 5};
    int64_t slice_stop[] = {2, 6};
    int32_t slice_chunkshape[] = {0, 1};
    int32_t slice_blockshape[] = {0, 1};

    int64_t nelem = 1;
    for (int i = 0; i < ndim; ++i) {
        nelem *= shape[i];
    }
    int64_t size = nelem * itemsize;
    int8_t *data = malloc(size);

    caterva_config_t cfg = CATERVA_CONFIG_DEFAULTS;

    caterva_ctx_t *ctx;
    caterva_ctx_new(&cfg, &ctx);

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
    slice_storage.urlpath = "example_hola.b2frame";
    for (int i = 0; i < ndim; ++i) {
        slice_storage.chunkshape[i] = slice_chunkshape[i];
        slice_storage.blockshape[i] = slice_blockshape[i];
    }

    caterva_array_t *slice;
    CATERVA_ERROR(caterva_get_slice(ctx, arr, slice_start, slice_stop, &slice_storage,
                                    &slice));


    uint8_t *buffer;
    uint64_t buffer_size = 1;
    for (int i = 0; i < slice->ndim; ++i) {
        buffer_size *= slice->shape[i];
    }
    buffer_size *= slice->itemsize;
    buffer = malloc(buffer_size);

    CATERVA_ERROR(caterva_to_buffer(ctx, slice, buffer, buffer_size));

    // printf("Elapsed seconds: %.5f\n", blosc_elapsed_secs(t0, t1));

    return 0;
}
