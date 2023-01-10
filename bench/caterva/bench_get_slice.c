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

    caterva_config_t cfg = CATERVA_CONFIG_DEFAULTS;
    cfg.nthreads = 4;

    caterva_ctx_t *ctx;
    caterva_ctx_new(&cfg, &ctx);

    caterva_params_t params;
    params.itemsize = itemsize;
    params.ndim = ndim;
    for (int i = 0; i < ndim; ++i) {
        params.shape[i] = shape[i];
    }

    caterva_storage_t storage = {0};
    for (int i = 0; i < ndim; ++i) {
        storage.chunkshape[i] = chunkshape[i];
        storage.blockshape[i] = blockshape[i];
    }

    caterva_array_t *arr;
    blosc_set_timestamp(&t0);
    CATERVA_ERROR(caterva_from_buffer(ctx, src, nbytes, &params, &storage, &arr));
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
            CATERVA_ERROR(caterva_get_slice_buffer(ctx, arr, slice_start, slice_stop, buffer, slice_shape, buffersize));
        }
        free(buffer);
    }

    blosc_set_timestamp(&t1);
    printf("get_slice: %.4f s\n", blosc_elapsed_secs(t0, t1));

    free(src);

    caterva_free(ctx, &arr);
    caterva_ctx_free(&ctx);

    return 0;
}
