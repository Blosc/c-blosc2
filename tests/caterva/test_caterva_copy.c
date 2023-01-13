/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

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
    blosc2_init();
    caterva_config_t cfg = CATERVA_CONFIG_DEFAULTS;
    cfg.nthreads = 2;
    cfg.compcode = BLOSC_BLOSCLZ;
    caterva_ctx_new(&cfg, &data->ctx);

    // Add parametrizations
    CUTEST_PARAMETRIZE(itemsize, uint8_t, CUTEST_DATA(
            2,
            4,
    ));
    CUTEST_PARAMETRIZE(shapes, test_shapes_t, CUTEST_DATA(
            {2, {30, 30}, {20, 20}, {10, 10},
                {20, 20}, {10, 10}},
            {3, {40, 15, 23}, {31, 5, 22}, {4, 4, 4},
                {30, 5, 20}, {10, 4, 4}},
            {3, {40, 0, 12}, {31, 0, 12}, {10, 0, 12},
                {20, 0, 12}, {25, 0, 6}},
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

    blosc2_remove_urlpath(urlpath);
    blosc2_remove_urlpath(urlpath2);

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
    storage.metalayers[0].content = (uint8_t *) &datatoserialize;
    storage.metalayers[0].content_len = 8;


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
    int rc = blosc2_meta_exists(src->sc, "random");
    if (rc < 0) {
        CATERVA_TEST_ASSERT(CATERVA_ERR_BLOSC_FAILED);
    }
    uint8_t *content;
    int32_t content_len;
    CATERVA_TEST_ASSERT(blosc2_meta_get(src->sc, "random", &content, &content_len));
    double serializeddata = *((double *) content);
    if (serializeddata != datatoserialize) {
        CATERVA_TEST_ASSERT(CATERVA_ERR_BLOSC_FAILED);
    }

    CATERVA_TEST_ASSERT(blosc2_vlmeta_add(src->sc, "random", content, content_len,
                                          src->sc->storage->cparams));
    free(content);


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
    CATERVA_TEST_ASSERT(blosc2_meta_get(dest->sc, "random", &content, &content_len));
    serializeddata = *((double *) content);
    if (serializeddata != datatoserialize) {
        CATERVA_TEST_ASSERT(CATERVA_ERR_BLOSC_FAILED);
    }
    free(content);

    CATERVA_TEST_ASSERT(blosc2_vlmeta_get(dest->sc, "random", &content, &content_len));
    serializeddata = *((double *) content);
    if (serializeddata != datatoserialize) {
        CATERVA_TEST_ASSERT(CATERVA_ERR_BLOSC_FAILED);
    }
    free(content);


    uint8_t *buffer_dest = malloc(buffersize);
    CATERVA_TEST_ASSERT(caterva_to_buffer(data->ctx, dest, buffer_dest, buffersize));

    /* Testing */
    CATERVA_TEST_ASSERT_BUFFER(buffer, buffer_dest, (int) buffersize);

    /* Free mallocs */
    free(buffer);
    free(buffer_dest);
    CATERVA_TEST_ASSERT(caterva_free(data->ctx, &src));
    CATERVA_TEST_ASSERT(caterva_free(data->ctx, &dest));

    blosc2_remove_urlpath(urlpath);
    blosc2_remove_urlpath(urlpath2);

    return 0;
}

CUTEST_TEST_TEARDOWN(copy) {
    caterva_ctx_free(&data->ctx);
    blosc2_destroy();
}

int main() {
    CUTEST_TEST_RUN(copy);
}
