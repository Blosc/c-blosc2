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

#include <caterva.h>

int main() {

    int8_t ndim = 2;
    int64_t shape[] = {10, 10};
    int32_t chunkshape[] = {4, 4};
    int32_t blockshape[] = {2, 2};
    int8_t itemsize = 8;

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
    int64_t dataitems = 1;
    for (int i = 0; i < ndim; ++i) {
        dataitems *= shape[i];
    }
    int64_t datasize = dataitems * itemsize;
    double *data = malloc(datasize);
    for (int i = 0; i < dataitems; ++i) {
        data[i] = (double) i;
    }
    caterva_array_t *arr;
    CATERVA_ERROR(caterva_from_buffer(ctx, data, datasize, &params, &storage, &arr));
    free(data);

    int64_t sel0[] = {3, 1, 2};
    int64_t sel1[] = {2, 5};
    int64_t sel2[] = {3, 3, 3, 9,3, 1, 0};
    int64_t *selection[] = {sel0, sel1, sel2};
    int64_t selection_size[] = {sizeof(sel0)/sizeof(int64_t), sizeof(sel1)/(sizeof(int64_t)), sizeof(sel2)/(sizeof(int64_t))};
    int64_t *buffershape = selection_size;
    int64_t nitems = 1;
    for (int i = 0; i < ndim; ++i) {
        nitems *= buffershape[i];
    }
    int64_t buffersize = nitems * arr->itemsize;
    double *buffer = calloc(nitems, arr->itemsize);
    CATERVA_ERROR(caterva_set_orthogonal_selection(ctx, arr, selection, selection_size, buffer, buffershape, buffersize));
    CATERVA_ERROR(caterva_get_orthogonal_selection(ctx, arr, selection, selection_size, buffer, buffershape, buffersize));

    printf("Results: \n");
    for (int i = 0; i < nitems; ++i) {
        if (i % buffershape[1] == 0) {
            printf("\n");
        }
        printf(" %f ", buffer[i]);
    }
    printf("\n");
    free(buffer);
    CATERVA_ERROR(caterva_free(ctx, &arr));
    CATERVA_ERROR(caterva_ctx_free(&ctx));

    return 0;
}
