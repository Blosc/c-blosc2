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
    int64_t buffershape[CATERVA_MAX_DIM];
    int8_t axis;
    int64_t start;
} test_shapes_t;


CUTEST_TEST_DATA(insert) {
    caterva_ctx_t *ctx;
};


CUTEST_TEST_SETUP(insert) {
    caterva_config_t cfg = CATERVA_CONFIG_DEFAULTS;
    cfg.nthreads = 2;
    cfg.compcodec = BLOSC_BLOSCLZ;
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
            {1, {5}, {3}, {2}, {10}, 0, 5}, // insert at the end (old padding modified)
            {2, {18, 6}, {6, 6}, {3, 3}, {18, 12}, 1, 0}, // insert at the beginning
            {3, {12, 10, 14}, {3, 5, 9}, {3, 4, 4}, {12, 10, 18}, 2, 9}, // insert in the middle
            {4, {10, 10, 5, 5}, {5, 7, 3, 3}, {2, 2, 1, 1}, {10, 10, 5, 30}, 3, 3}, // insert in the middle

    ));
}

CUTEST_TEST_TEST(insert) {
    CUTEST_GET_PARAMETER(backend, _test_backend);
    CUTEST_GET_PARAMETER(shapes, test_shapes_t);
    CUTEST_GET_PARAMETER(itemsize, uint8_t);

    char *urlpath = "test_insert_shape.b2frame";
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

    int64_t buffersize = itemsize;
    for (int i = 0; i < params.ndim; ++i) {
        buffersize *= shapes.buffershape[i];
    }

    /* Create caterva_array_t filled with 1 */
    caterva_array_t *src;
    uint8_t *value = malloc(itemsize);
    int8_t fill_value = 1;
    switch (itemsize) {
        case 8:
            ((int64_t *) value)[0] = (int64_t) fill_value;
            break;
        case 4:
            ((int32_t *) value)[0] = (int32_t) fill_value;
            break;
        case 2:
            ((int16_t *) value)[0] = (int16_t) fill_value;
            break;
        case 1:
            ((int8_t *) value)[0] = fill_value;
            break;
        default:
            break;
    }
    CATERVA_ERROR(caterva_full(data->ctx, &params, &storage, value, &src));

    uint8_t *buffer = data->ctx->cfg->alloc(buffersize);
    fill_buf(buffer, itemsize, buffersize / itemsize);
    CATERVA_ERROR(caterva_insert(data->ctx, src, buffer, buffersize, shapes.axis, shapes.start));

    int64_t start[CATERVA_MAX_DIM] = {0};
    start[shapes.axis] = shapes.start;
    int64_t stop[CATERVA_MAX_DIM];
    for (int i = 0; i < shapes.ndim; ++i) {
        stop[i] = shapes.shape[i];
    }
    stop[shapes.axis] = shapes.start + shapes.buffershape[shapes.axis];

    /* Fill buffer with a slice from the new chunks */
    uint8_t *res_buffer = data->ctx->cfg->alloc(buffersize);
    CATERVA_ERROR(caterva_get_slice_buffer(data->ctx, src, start, stop, res_buffer,
                                           shapes.buffershape, buffersize));

    for (uint64_t i = 0; i < (uint64_t) buffersize / itemsize; ++i) {
        switch (itemsize) {
            case 8:
                CUTEST_ASSERT("Elements are not equal!",
                              ((uint64_t *) buffer)[i] == ((uint64_t *) res_buffer)[i]);
                break;
            case 4:
                CUTEST_ASSERT("Elements are not equal!",
                              ((uint32_t *) buffer)[i] == ((uint32_t *) res_buffer)[i]);
                break;
            case 2:
                CUTEST_ASSERT("Elements are not equal!",
                              ((uint16_t *) buffer)[i] == ((uint16_t *) res_buffer)[i]);
                break;
            case 1:
                CUTEST_ASSERT("Elements are not equal!",
                              ((uint8_t *) buffer)[i] == ((uint8_t *) res_buffer)[i]);
                break;
            default:
                CATERVA_TEST_ASSERT(CATERVA_ERR_INVALID_ARGUMENT);
        }
    }
    /* Free mallocs */
    free(value);
    data->ctx->cfg->free(buffer);
    data->ctx->cfg->free(res_buffer);

    CATERVA_TEST_ASSERT(caterva_free(data->ctx, &src));
    caterva_remove(data->ctx, urlpath);

    return 0;
}

CUTEST_TEST_TEARDOWN(insert) {
    caterva_ctx_free(&data->ctx);
}

int main() {
    CUTEST_TEST_RUN(insert);
}
