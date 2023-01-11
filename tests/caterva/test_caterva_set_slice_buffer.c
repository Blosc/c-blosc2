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


#include "test_common.h"

typedef struct {
    int8_t ndim;
    int64_t shape[CATERVA_MAX_DIM];
    int32_t chunkshape[CATERVA_MAX_DIM];
    int32_t blockshape[CATERVA_MAX_DIM];
    int64_t start[CATERVA_MAX_DIM];
    int64_t stop[CATERVA_MAX_DIM];
} test_shapes_t;


CUTEST_TEST_DATA(set_slice_buffer) {
    caterva_ctx_t *ctx;
};


CUTEST_TEST_SETUP(set_slice_buffer) {
    caterva_config_t cfg = CATERVA_CONFIG_DEFAULTS;
    cfg.nthreads = 2;
    cfg.compcodec = BLOSC_ZSTD;
    caterva_ctx_new(&cfg, &data->ctx);

    // Add parametrizations
    CUTEST_PARAMETRIZE(itemsize, uint8_t, CUTEST_DATA(
            1,
            2,
            4,
            8,
    ));

    CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
            {false, false},
            {true, false},
            {true, true},
            {false, true},
    ));


    CUTEST_PARAMETRIZE(shapes, test_shapes_t, CUTEST_DATA(
            {0, {0}, {0}, {0}, {0}, {0}}, // 0-dim
            {1, {5}, {3}, {2}, {2}, {5}}, // 1-idim
            {2, {20, 0}, {7, 0}, {3, 0}, {2, 0}, {8, 0}}, // 0-shape
            {2, {20, 10}, {7, 5}, {3, 5}, {2, 0}, {18, 0}}, // 0-shape
            {2, {14, 10}, {8, 5}, {2, 2}, {5, 3}, {9, 10}}, // general,
            {3, {12, 10, 14}, {3, 5, 9}, {3, 4, 4}, {3, 0, 3}, {6, 7, 10}}, // general
            {3, {10, 21, 30, 55}, {8, 7, 15, 3}, {5, 5, 10, 1}, {5, 4, 3, 3}, {10, 8, 8, 34}}, // general,
            {2, {50, 50}, {25, 13}, {8, 8}, {0, 0}, {10, 10}}, // general,
            {2, {143, 41}, {18, 13}, {7, 7}, {4, 2}, {6, 5}}, // general,
            {2, {10, 10}, {5, 7}, {2, 2}, {0, 0}, {5, 5}},

    ));
}

CUTEST_TEST_TEST(set_slice_buffer) {
    CUTEST_GET_PARAMETER(backend, _test_backend);
    CUTEST_GET_PARAMETER(shapes, test_shapes_t);
    CUTEST_GET_PARAMETER(itemsize, uint8_t);

    char *urlpath = "test_set_slice_buffer.b2frame";
    caterva_remove(data->ctx, urlpath);

    caterva_params_t params;
    params.itemsize = itemsize;
    params.ndim = shapes.ndim;
    for (int i = 0; i < params.ndim; ++i) {
        params.shape[i] = shapes.shape[i];
    }

    caterva_storage_t storage = {0};
    if (backend.persistent) {
        storage.urlpath = urlpath;
    }
    storage.contiguous = backend.contiguous;
    for (int i = 0; i < params.ndim; ++i) {
        storage.chunkshape[i] = shapes.chunkshape[i];
        storage.blockshape[i] = shapes.blockshape[i];
    }

    /* Create dest buffer */
    int64_t shape[CATERVA_MAX_DIM] = {0};
    int64_t buffersize = itemsize;
    for (int i = 0; i < params.ndim; ++i) {
        shape[i] = shapes.stop[i] - shapes.start[i];
        buffersize *= shape[i];
    }

    uint8_t *buffer = data->ctx->cfg->alloc(buffersize);
    CUTEST_ASSERT("Buffer filled incorrectly", fill_buf(buffer, itemsize, buffersize / itemsize));

    /* Create caterva_array_t with original data */
    caterva_array_t *src;
    CATERVA_ERROR(caterva_zeros(data->ctx, &params, &storage, &src));


    CATERVA_ERROR(caterva_set_slice_buffer(data->ctx, buffer, shape, buffersize,
                                           shapes.start, shapes.stop, src));


    uint8_t *destbuffer = data->ctx->cfg->alloc((size_t) buffersize);

    /* Fill dest buffer with a slice*/
    CATERVA_TEST_ASSERT(caterva_get_slice_buffer(data->ctx, src, shapes.start, shapes.stop,
                                                 destbuffer,
                                                 shape, buffersize));

    for (uint64_t i = 0; i < (uint64_t) buffersize / itemsize; ++i) {
        uint64_t k = i + 1;
        switch (itemsize) {
            case 8:
                CUTEST_ASSERT("Elements are not equals!",
                              (uint64_t) k == ((uint64_t *) destbuffer)[i]);
                break;
            case 4:
                CUTEST_ASSERT("Elements are not equals!",
                              (uint32_t) k == ((uint32_t *) destbuffer)[i]);
                break;
            case 2:
                CUTEST_ASSERT("Elements are not equals!",
                              (uint16_t) k == ((uint16_t *) destbuffer)[i]);
                break;
            case 1:
                CUTEST_ASSERT("Elements are not equals!",
                              (uint8_t) k == ((uint8_t *) destbuffer)[i]);
                break;
            default:
                CATERVA_TEST_ASSERT(CATERVA_ERR_INVALID_ARGUMENT);
        }
    }

    /* Free mallocs */
    data->ctx->cfg->free(buffer);
    data->ctx->cfg->free(destbuffer);
    CATERVA_TEST_ASSERT(caterva_free(data->ctx, &src));
    caterva_remove(data->ctx, urlpath);

    return 0;
}

CUTEST_TEST_TEARDOWN(set_slice_buffer) {
    caterva_ctx_free(&data->ctx);
}

int main() {
    CUTEST_TEST_RUN(set_slice_buffer);
}
