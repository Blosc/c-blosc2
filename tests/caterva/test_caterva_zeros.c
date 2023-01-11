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


CUTEST_TEST_DATA(zeros) {
    caterva_ctx_t *ctx;
};


CUTEST_TEST_SETUP(zeros) {
    caterva_config_t cfg = CATERVA_CONFIG_DEFAULTS;
    cfg.nthreads = 2;
    cfg.compcodec = BLOSC_BLOSCLZ;
    caterva_ctx_new(&cfg, &data->ctx);

    // Add parametrizations
    CUTEST_PARAMETRIZE(itemsize, uint8_t, CUTEST_DATA(
            1, 2, 4, 7
    ));
    CUTEST_PARAMETRIZE(shapes, _test_shapes, CUTEST_DATA(
            {0, {0}, {0}, {0}}, // 0-dim
            {1, {5}, {3}, {2}}, // 1-idim
            {2, {20, 0}, {7, 0}, {3, 0}}, // 0-shape
            {2, {20, 10}, {7, 5}, {3, 5}}, // 0-shape
            {2, {14, 10}, {8, 5}, {2, 2}}, // general,
            {3, {12, 10, 14}, {3, 5, 9}, {3, 4, 4}}, // general
            {3, {10, 21, 30, 55}, {8, 7, 15, 3}, {5, 5, 10, 1}}, // general,
    ));
    CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
            {false, false},
            {true, false},
            {true, true},
            {false, true},
    ));
}


CUTEST_TEST_TEST(zeros) {
    CUTEST_GET_PARAMETER(backend, _test_backend);
    CUTEST_GET_PARAMETER(shapes, _test_shapes);
    CUTEST_GET_PARAMETER(itemsize, uint8_t);

    char *urlpath = "test_zeros.b2frame";
    caterva_remove(data->ctx, urlpath);

    caterva_params_t params;
    params.itemsize = itemsize;
    params.ndim = shapes.ndim;
    for (int i = 0; i < shapes.ndim; ++i) {
        params.shape[i] = shapes.shape[i];
    }

    caterva_storage_t storage = {0};
    if (backend.persistent) {
        storage.urlpath = urlpath;
    }
    storage.contiguous = backend.contiguous;
    for (int i = 0; i < shapes.ndim; ++i) {
        storage.chunkshape[i] = shapes.chunkshape[i];
        storage.blockshape[i] = shapes.blockshape[i];
    }

    /* Create original data */
    int64_t buffersize = itemsize;
    for (int i = 0; i < shapes.ndim; ++i) {
        buffersize *= shapes.shape[i];
    }

    /* Create caterva_array_t with original data */
    caterva_array_t *src;
    CATERVA_TEST_ASSERT(caterva_zeros(data->ctx, &params, &storage, &src));

    /* Fill dest array with caterva_array_t data */
    uint8_t *buffer_dest = malloc( buffersize);
    CATERVA_TEST_ASSERT(caterva_to_buffer(data->ctx, src, buffer_dest, buffersize));

    /* Testing */
    for (int i = 0; i < buffersize; ++i) {
        CUTEST_ASSERT("Elements are not equals", buffer_dest[i] == 0);
    }

    /* Free mallocs */
    free(buffer_dest);
    CATERVA_TEST_ASSERT(caterva_free(data->ctx, &src));
    caterva_remove(data->ctx, urlpath);

    return CATERVA_SUCCEED;
}


CUTEST_TEST_TEARDOWN(zeros) {
    caterva_ctx_free(&data->ctx);
}

int main() {
    CUTEST_TEST_RUN(zeros);
}
