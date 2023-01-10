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
    double *data = malloc(size);

    for (int i = 0; i < nelem; ++i) {
        data[i] = i;
    }

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
    storage.contiguous = false;

    caterva_array_t *arr;
    CATERVA_ERROR(caterva_from_buffer(ctx, data, size, &params, &storage, &arr));

    uint8_t *cframe;
    int64_t cframe_len;
    bool needs_free;
    CATERVA_ERROR(caterva_to_cframe(ctx, arr, &cframe, &cframe_len, &needs_free));

    caterva_array_t *dest;
    CATERVA_ERROR(caterva_from_cframe(ctx, cframe, cframe_len, true, &dest));

    /* Fill dest array with caterva_array_t data */
    uint8_t *data_dest = malloc(size);
    CATERVA_ERROR(caterva_to_buffer(ctx, dest, data_dest, size));

    for (int i = 0; i < nelem; ++i) {
        if (data[i] != data_dest[i] && data[i] != i) {
            return -1;
        }
    }

    /* Free mallocs */
    free(data);
    free(data_dest);

    caterva_free(ctx, &arr);
    caterva_free(ctx, &dest);

    return 0;
}
