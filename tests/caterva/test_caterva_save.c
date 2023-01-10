/*
 * Copyright (c) 2018 Francesc Alted, Aleix Alcacer.
 * Copyright (C) 2019-present Blosc Development team <blosc@blosc.org>
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include "test_common.h"
#ifdef __GNUC__
#include <unistd.h>
#define FILE_EXISTS(urlpath) access(urlpath, F_OK)
#else
#include <io.h>
#define FILE_EXISTS(urlpath) _access(urlpath, 0)
#endif


typedef struct {
    int8_t ndim;
    int64_t shape[CATERVA_MAX_DIM];
    int32_t chunkshape[CATERVA_MAX_DIM];
    int32_t blockshape[CATERVA_MAX_DIM];
} test_shapes_t;


CUTEST_TEST_DATA(save) {
    caterva_ctx_t *ctx;
};


CUTEST_TEST_SETUP(save) {
    caterva_config_t cfg = CATERVA_CONFIG_DEFAULTS;
    cfg.nthreads = 2;
    cfg.compcodec = BLOSC_BLOSCLZ;
    caterva_ctx_new(&cfg, &data->ctx);

    // Add parametrizations
    CUTEST_PARAMETRIZE(itemsize, uint8_t, CUTEST_DATA(1, 2, 4, 8));
    CUTEST_PARAMETRIZE(shapes, test_shapes_t, CUTEST_DATA(
            // {0, {0}, {0}, {0}}, // 0-dim
             {1, {10}, {7}, {2}}, // 1-idim
             {2, {100, 100}, {20, 20}, {10, 10}},
             {3, {100, 55, 123}, {31, 5, 22}, {4, 4, 4}},
             {3, {100, 0, 12}, {31, 0, 12}, {10, 0, 12}},
    ));
    CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
            {true, false},
            {false, false},
    ));
}

CUTEST_TEST_TEST(save) {
    CUTEST_GET_PARAMETER(backend, _test_backend);
    CUTEST_GET_PARAMETER(shapes, test_shapes_t);
    CUTEST_GET_PARAMETER(itemsize, uint8_t);

    char* urlpath = "test_save.b2frame";

    caterva_remove(data->ctx, urlpath);

    caterva_params_t params;
    params.itemsize = itemsize;
    params.ndim = shapes.ndim;
    for (int i = 0; i < params.ndim; ++i) {
        params.shape[i] = shapes.shape[i];
    }

    caterva_storage_t storage = {0};
    storage.urlpath = NULL;
    storage.contiguous = backend.contiguous;
    for (int i = 0; i < params.ndim; ++i) {
        storage.chunkshape[i] = shapes.chunkshape[i];
        storage.blockshape[i] = shapes.blockshape[i];
    }

    /* Create original data */
    int64_t buffersize = itemsize;
    for (int i = 0; i < params.ndim; ++i) {
        buffersize *= shapes.shape[i];
    }
    uint8_t *buffer = malloc(buffersize);
    CUTEST_ASSERT("Buffer filled incorrectly", fill_buf(buffer, itemsize, buffersize / itemsize));

    /* Create caterva_array_t with original data */
    caterva_array_t *src;
    CATERVA_TEST_ASSERT(caterva_from_buffer(data->ctx, buffer, buffersize, &params, &storage,
                                            &src));

    CATERVA_TEST_ASSERT(caterva_save(data->ctx, src, urlpath));
    caterva_array_t *dest;
    CATERVA_TEST_ASSERT(caterva_open(data->ctx, urlpath, &dest));

    /* Fill dest array with caterva_array_t data */
    uint8_t *buffer_dest = malloc(buffersize);
    CATERVA_TEST_ASSERT(caterva_to_buffer(data->ctx, dest, buffer_dest, buffersize));

    /* Testing */
    if (dest->nitems != 0) {
        for (int i = 0; i < buffersize / itemsize; ++i) {
            // printf("%d - %d\n", buffer[i], buffer_dest[i]);
            CUTEST_ASSERT("Elements are not equals!", buffer[i] == buffer_dest[i]);
        }
    }

    /* Free mallocs */
    free(buffer);
    free(buffer_dest);
    CATERVA_TEST_ASSERT(caterva_free(data->ctx, &src));
    CATERVA_TEST_ASSERT(caterva_free(data->ctx, &dest));

    caterva_remove(data->ctx, urlpath);

    return 0;
}


CUTEST_TEST_TEARDOWN(save) {
    caterva_ctx_free(&data->ctx);
}

int main() {
    CUTEST_TEST_RUN(save);
}
