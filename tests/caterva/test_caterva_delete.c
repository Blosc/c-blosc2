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
    int8_t axis;
    int64_t start;
    int64_t delete_len;
} test_shapes_t;


CUTEST_TEST_DATA(delete) {
    caterva_ctx_t *ctx;
};


CUTEST_TEST_SETUP(delete) {
    caterva_config_t cfg = CATERVA_CONFIG_DEFAULTS;
    cfg.nthreads = 2;
    cfg.compcodec = BLOSC_LZ4;
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
            {1, {10}, {3}, {2}, 0, 5, 5}, // delete the end
            {2, {18, 12}, {6, 6}, {3, 3}, 1, 0, 6}, // delete at the beginning
            {3, {12, 10, 27}, {3, 5, 9}, {3, 4, 4}, 2, 9, 9}, // delete in the middle
            {4, {10, 10, 5, 30}, {5, 7, 3, 3}, {2, 2, 1, 1}, 3, 12, 9}, // delete in the middle

    ));
}

CUTEST_TEST_TEST(delete) {
    CUTEST_GET_PARAMETER(backend, _test_backend);
    CUTEST_GET_PARAMETER(shapes, test_shapes_t);
    CUTEST_GET_PARAMETER(itemsize, uint8_t);

    char *urlpath = "test_delete.b2frame";
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

    /* Create caterva_array_t with original data */
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

    int64_t bufferlen = 1;
    int64_t stop[CATERVA_MAX_DIM];
    int64_t buffer_shape[CATERVA_MAX_DIM];
    for (int i = 0; i < params.ndim; ++i) {
        if (i != shapes.axis) {
            bufferlen *= shapes.shape[i];
            stop[i] = shapes.shape[i];
            buffer_shape[i] = shapes.shape[i];
        }
        else {
            bufferlen *= shapes.delete_len;
            stop[i] = shapes.start + shapes.delete_len;
            buffer_shape[i] = shapes.delete_len;
        }
    }
    // Set future deleted values to 0
    int64_t start[CATERVA_MAX_DIM] = {0};
    start[shapes.axis] = shapes.start;
    uint8_t *buffer = calloc((size_t) bufferlen, (size_t) itemsize);
    CATERVA_ERROR(caterva_set_slice_buffer(data->ctx, buffer, buffer_shape, bufferlen * itemsize, start, stop, src));

    CATERVA_ERROR(caterva_delete(data->ctx, src, shapes.axis, shapes.start, shapes.delete_len));

    int64_t newshape[CATERVA_MAX_DIM] = {0};
    for (int i = 0; i < shapes.ndim; ++i) {
        newshape[i] = shapes.shape[i];
    }
    newshape[shapes.axis] -= shapes.delete_len;

    // Create aux array to compare values
    caterva_array_t *aux;
    caterva_params_t aux_params;
    aux_params.itemsize = itemsize;
    aux_params.ndim = shapes.ndim;
    for (int i = 0; i < aux_params.ndim; ++i) {
        aux_params.shape[i] = newshape[i];
    }
    caterva_storage_t aux_storage = {0};
    aux_storage.contiguous = backend.contiguous;
    for (int i = 0; i < params.ndim; ++i) {
        aux_storage.chunkshape[i] = shapes.chunkshape[i];
        aux_storage.blockshape[i] = shapes.blockshape[i];
    }
    CATERVA_ERROR(caterva_full(data->ctx, &aux_params, &aux_storage, value, &aux));


    /* Fill buffer with whole array data */
    uint8_t *src_buffer = data->ctx->cfg->alloc((size_t) (src->nitems * itemsize));
    CATERVA_TEST_ASSERT(caterva_to_buffer(data->ctx, src, src_buffer, src->nitems * itemsize));

    for (uint64_t i = 0; i < (uint64_t) src->nitems; ++i) {
        switch (itemsize) {
            case 8:
                CUTEST_ASSERT("Elements are not equal!",
                              ((uint64_t *) src_buffer)[i] == (uint64_t) fill_value);
                break;
            case 4:
                CUTEST_ASSERT("Elements are not equal!",
                              ((uint32_t *) src_buffer)[i] == (uint32_t) fill_value);
                break;
            case 2:
                CUTEST_ASSERT("Elements are not equal!",
                              ((uint16_t *) src_buffer)[i] == (uint16_t) fill_value);
                break;
            case 1:
                CUTEST_ASSERT("Elements are not equal!",
                              ((uint8_t *) src_buffer)[i] == (uint8_t) fill_value);
                break;
            default:
                CATERVA_TEST_ASSERT(CATERVA_ERR_INVALID_ARGUMENT);
        }
    }
    /* Free mallocs */
    free(value);
    free(buffer);
    data->ctx->cfg->free(src_buffer);

    CATERVA_TEST_ASSERT(caterva_free(data->ctx, &src));
    CATERVA_TEST_ASSERT(caterva_free(data->ctx, &aux));

    caterva_remove(data->ctx, urlpath);

    return 0;
}

CUTEST_TEST_TEARDOWN(delete) {
    caterva_ctx_free(&data->ctx);
}

int main() {
    CUTEST_TEST_RUN(delete);
}
