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

typedef struct {
    int8_t ndim;
    int64_t shape[CATERVA_MAX_DIM];
    int32_t chunkshape[CATERVA_MAX_DIM];
    int32_t blockshape[CATERVA_MAX_DIM];
    int32_t chunkshape2[CATERVA_MAX_DIM];
    int32_t blockshape2[CATERVA_MAX_DIM];
} test_shapes_t;


CUTEST_TEST_DATA(copy) {
    caterva_ctx_t *ctx;
};


CUTEST_TEST_SETUP(copy) {
    caterva_config_t cfg = CATERVA_CONFIG_DEFAULTS;
    cfg.nthreads = 2;
    cfg.compcodec = BLOSC_BLOSCLZ;
    caterva_ctx_new(&cfg, &data->ctx);

    // Add parametrizations
    CUTEST_PARAMETRIZE(itemsize, uint8_t, CUTEST_DATA(
            2,
            4,
    ));
    CUTEST_PARAMETRIZE(shapes, test_shapes_t, CUTEST_DATA(
            {2, {100, 100}, {20, 20}, {10, 10},
                {20, 20}, {10, 10}},
            {3, {100, 55, 123}, {31, 5, 22}, {4, 4, 4},
                {50, 15, 20}, {10, 4, 4}},
            {3, {100, 0, 12}, {31, 0, 12}, {10, 0, 12},
                {50, 0, 12}, {25, 0, 6}},
// The following cases are skipped to reduce the execution time of the test
//            {4, {25, 60, 31, 12}, {12, 20, 20, 10}, {5, 5, 5, 10},
//                {25, 20, 20, 10}, {5, 5, 5, 10}},
//            {5, {1, 1, 1024, 1, 1}, {1, 1, 500, 1, 1}, {1, 1, 200, 1, 1},
//                {1, 1, 300, 1, 1}, {1, 1, 50, 1, 1}},
//            {6, {5, 1, 100, 3, 1, 2}, {4, 1, 50, 2, 1, 2}, {2, 1, 20, 2, 1, 2},
//                {4, 1, 50, 2, 1, 1}, {2, 1, 20, 2, 1, 1}},
    ));
    CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
            {false, false},
            {true, false},
            {false, true},
            {true, true},
    ));

    CUTEST_PARAMETRIZE(backend2, _test_backend, CUTEST_DATA(
             {false, false},
             {false, false},
             {true, false},
             {false, true},
             {true, true},
    ));
}

CUTEST_TEST_TEST(copy) {
    CUTEST_GET_PARAMETER(backend, _test_backend);
    CUTEST_GET_PARAMETER(shapes, test_shapes_t);
    CUTEST_GET_PARAMETER(backend2, _test_backend);
    CUTEST_GET_PARAMETER(itemsize, uint8_t);

    char *urlpath = "test_copy.b2frame";
    char *urlpath2 = "test_copy2.b2frame";

    caterva_remove(data->ctx, urlpath);
    caterva_remove(data->ctx, urlpath2);

    caterva_params_t params;
    params.itemsize = itemsize;
    params.ndim = shapes.ndim;
    for (int i = 0; i < shapes.ndim; ++i) {
        params.shape[i] = shapes.shape[i];
    }

    double datatoserialize = 8.34;

    caterva_storage_t storage = {0};
    if (backend.persistent) {
        storage.urlpath = urlpath;
    } else {
        storage.urlpath = NULL;
    }
    storage.contiguous = backend.contiguous;
    for (int i = 0; i < params.ndim; ++i) {
        storage.chunkshape[i] = shapes.chunkshape[i];
        storage.blockshape[i] = shapes.blockshape[i];
    }
    storage.nmetalayers = 1;
    storage.metalayers[0].name = "random";
    storage.metalayers[0].sdata = (uint8_t *) &datatoserialize;
    storage.metalayers[0].size = 8;


    /* Create original data */
    size_t buffersize = itemsize;
    for (int i = 0; i < params.ndim; ++i) {
        buffersize *= (size_t) params.shape[i];
    }
    uint8_t *buffer = malloc(buffersize);
    CUTEST_ASSERT("Buffer filled incorrectly", fill_buf(buffer, itemsize, (buffersize / itemsize)));

    /* Create caterva_array_t with original data */
    caterva_array_t *src;
    CATERVA_TEST_ASSERT(caterva_from_buffer(data->ctx, buffer, buffersize, &params, &storage,
                                            &src));

    /* Assert the metalayers creation */
    bool exists;
    CATERVA_TEST_ASSERT(caterva_meta_exists(data->ctx, src, "random", &exists));
    if (!exists) {
        CATERVA_TEST_ASSERT(CATERVA_ERR_BLOSC_FAILED);
    }
    caterva_metalayer_t meta;
    CATERVA_TEST_ASSERT(caterva_meta_get(data->ctx, src, "random", &meta));
    double serializeddata = *((double *) meta.sdata);
    if (serializeddata != datatoserialize) {
        CATERVA_TEST_ASSERT(CATERVA_ERR_BLOSC_FAILED);
    }

    CATERVA_TEST_ASSERT(caterva_vlmeta_add(data->ctx, src, &meta));
    free(meta.sdata);
    free(meta.name);


    /* Create storage for dest container */
    caterva_storage_t storage2 = {0};

    if (backend2.persistent) {
        storage2.urlpath = urlpath2;
    } else {
        storage2.urlpath = NULL;
    }
    storage2.contiguous = backend2.contiguous;
    for (int i = 0; i < shapes.ndim; ++i) {
        storage2.chunkshape[i] = shapes.chunkshape2[i];
        storage2.blockshape[i] = shapes.blockshape2[i];
            }


    caterva_array_t *dest;
    CATERVA_TEST_ASSERT(caterva_copy(data->ctx, src, &storage2, &dest));

    /* Assert the metalayers creation */
    CATERVA_TEST_ASSERT(caterva_meta_get(data->ctx, dest, "random", &meta));
    serializeddata = *((double *) meta.sdata);
    if (serializeddata != datatoserialize) {
        CATERVA_TEST_ASSERT(CATERVA_ERR_BLOSC_FAILED);
    }
    free(meta.sdata);
    free(meta.name);

    caterva_metalayer_t vlmeta;
    CATERVA_TEST_ASSERT(caterva_vlmeta_get(data->ctx, dest, "random", &vlmeta));
    serializeddata = *((double *) vlmeta.sdata);
    if (serializeddata != datatoserialize) {
        CATERVA_TEST_ASSERT(CATERVA_ERR_BLOSC_FAILED);
    }
    free(vlmeta.sdata);
    free(vlmeta.name);


    uint8_t *buffer_dest = malloc(buffersize);
    CATERVA_TEST_ASSERT(caterva_to_buffer(data->ctx, dest, buffer_dest, buffersize));

    /* Testing */
    CATERVA_TEST_ASSERT_BUFFER(buffer, buffer_dest, (int) buffersize);

    /* Free mallocs */
    free(buffer);
    free(buffer_dest);
    CATERVA_TEST_ASSERT(caterva_free(data->ctx, &src));
    CATERVA_TEST_ASSERT(caterva_free(data->ctx, &dest));

    caterva_remove(data->ctx, urlpath);
    caterva_remove(data->ctx, urlpath2);

    return 0;
}

CUTEST_TEST_TEARDOWN(copy) {
    caterva_ctx_free(&data->ctx);
}

int main() {
    CUTEST_TEST_RUN(copy);
}
