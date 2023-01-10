/*
* Copyright (C) 2018 Francesc Alted, Aleix Alcacer.
* Copyright (C) 2019-present Blosc Development team <blosc@blosc.org>
* All rights reserved.
*
* This source code is licensed under both the BSD-style license (found in the
* LICENSE file in the root directory of this source tree) and the GPLv2 (found
* in the COPYING file in the root directory of this source tree).
* You may select, at your option, one of the above-listed licenses.
*
* Example program demonstrating use of the Blosc plugins from C code.
*
* To compile this program:
* $ gcc example_plugins_filters.c -o example_plugins_filters -lblosc2
*
* To run:
* $ ./example_plugins_filters
*
* from_buffer: 0.0668 s
* to_buffer: 0.0068 s
* Process finished with exit code 0
*/



#include <caterva.h>
#include <stdio.h>
#include <blosc2.h>
#include "../../plugins/filters/filters-registry.c"
#include <inttypes.h>

int main() {
    blosc_timestamp_t t0, t1;

    blosc2_init();
    int8_t ndim = 3;
    uint8_t itemsize = sizeof(int64_t);

    int64_t shape[] = {745, 400, 350};
    int32_t chunkshape[] = {150, 100, 150};
    int32_t blockshape[] = {21, 30, 27};

    int64_t nbytes = itemsize;
    for (int i = 0; i < ndim; ++i) {
        nbytes *= shape[i];
    }

    int64_t *src = malloc((size_t) nbytes);
    for (int i = 0; i < nbytes / itemsize; ++i) {
        src[i] = (int64_t) i;
    }

    caterva_config_t cfg = CATERVA_CONFIG_DEFAULTS;
    cfg.nthreads = 1;
    /*
     * Use the NDCELL filter through its plugin.
     * NDCELL metainformation: user must specify the parameter meta as the cellshape, so
     * if in a 3-dim dataset user specifies meta = 4, then cellshape will be 4x4x4.
    */
    cfg.filters[4] = BLOSC_FILTER_NDCELL;
    cfg.filtersmeta[4] = 4;
    // We could use a codec plugin by setting cfg.compcodec.

    caterva_ctx_t *ctx;
    caterva_ctx_new(&cfg, &ctx);

    caterva_params_t params = {0};
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

    int64_t *buffer = malloc(nbytes);
    int64_t buffer_size = nbytes;
    blosc_set_timestamp(&t0);
    CATERVA_ERROR(caterva_to_buffer(ctx, arr, buffer, buffer_size));
    blosc_set_timestamp(&t1);
    printf("to_buffer: %.4f s\n", blosc_elapsed_secs(t0, t1));

    blosc2_destroy();

    for (int i = 0; i < buffer_size / itemsize; i++) {
        if (src[i] != buffer[i]) {
            printf("\n Decompressed data differs from original!\n");
            printf("i: %d, data %" PRId64 ", dest %" PRId64 "", i, src[i], buffer[i]);
            return -1;
        }
    }

    free(src);
    free(buffer);

    caterva_free(ctx, &arr);
    caterva_ctx_free(&ctx);

    return 0;
}
