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
    int64_t start[CATERVA_MAX_DIM];
    int64_t stop[CATERVA_MAX_DIM];
    bool squeeze_indexes[CATERVA_MAX_DIM];
} test_squeeze_index_shapes_t;


CUTEST_TEST_DATA(squeeze_index) {
    void *unused;
};


CUTEST_TEST_SETUP(squeeze_index) {
    blosc2_init();

    // Add parametrizations
    CUTEST_PARAMETRIZE(typesize, uint8_t, CUTEST_DATA(
            1,
            2,
            4,
            8
    ));
    CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
            {false, false},
            {true, false},
            {true, true},
            {false, true},
    ));
    CUTEST_PARAMETRIZE(backend2, _test_backend, CUTEST_DATA(
            {false, false},
            {true, false},
            {true, true},
            {false, true},
    ));


    CUTEST_PARAMETRIZE(shapes, test_squeeze_index_shapes_t, CUTEST_DATA(
            {0, {0}, {0}, {0}, {0}, {0},
             {0}, {0}, {0}}, // 0-dim
            {1, {10}, {7}, {2}, {1}, {1},
             {2}, {3}, {0}}, // 1-idim
            {2, {14, 10}, {8, 5}, {2, 2}, {4, 1}, {2, 1},
             {5, 3}, {9, 4}, {0, 1}}, // general,
            {3, {10, 10, 10}, {3, 5, 9}, {3, 4, 4}, {1, 7, 1}, {1, 5, 1},
             {3, 0, 9}, {4, 7, 10}, {1, 0, 0}},
            {2, {20, 0}, {7, 0}, {3, 0}, {1, 0}, {1, 0},
             {1, 0}, {2, 0}, {1, 0}}, // 0-shape
            {2, {20, 10}, {7, 5}, {3, 5}, {1, 0}, {1, 0},
             {17, 0}, {18, 0}, {1, 0}}, // 0-shape
            {4, {10, 7, 6, 4}, {7, 5, 1, 4}, {2, 2, 1, 2}, {1, 1, 5, 1}, {1, 1, 2, 1},
             {4, 4, 0, 4}, {5, 5, 10, 5}, {1, 0, 0, 1}} // general
    ));
}


CUTEST_TEST_TEST(squeeze_index) {
    CUTEST_GET_PARAMETER(backend, _test_backend);
    CUTEST_GET_PARAMETER(shapes, test_squeeze_index_shapes_t);
    CUTEST_GET_PARAMETER(backend2, _test_backend);
    CUTEST_GET_PARAMETER(typesize, uint8_t);

    char *urlpath = "test_squeeze_index.b2frame";
    char *urlpath2 = "test_squezze_index2.b2frame";

    blosc2_remove_urlpath(urlpath);
    blosc2_remove_urlpath(urlpath2);

    caterva_params_t params;
    params.ndim = shapes.ndim;
    for (int i = 0; i < params.ndim; ++i) {
        params.shape[i] = shapes.shape[i];
    }

    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.nthreads = 2;
    cparams.compcode = BLOSC_BLOSCLZ;
    cparams.typesize = typesize;
    blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
    blosc2_storage b_storage = {.cparams=&cparams, .dparams=&dparams};
    caterva_storage_t storage = {.b_storage=&b_storage};
    if (backend.persistent) {
        storage.b_storage->urlpath = urlpath;
    }
    storage.b_storage->contiguous = backend.persistent;
    int32_t blocknitems = 1;
    for (int i = 0; i < params.ndim; ++i) {
        storage.chunkshape[i] = shapes.chunkshape[i];
        storage.blockshape[i] = shapes.blockshape[i];
      blocknitems *= storage.blockshape[i];
    }
    storage.b_storage->cparams->blocksize = blocknitems * storage.b_storage->cparams->typesize;

    /* Create original data */
    size_t buffersize = typesize;
    for (int i = 0; i < params.ndim; ++i) {
        buffersize *= (size_t) params.shape[i];
    }
    uint8_t *buffer = malloc(buffersize);
    CUTEST_ASSERT("Buffer filled incorrectly", fill_buf(buffer, typesize, buffersize / typesize));

    /* Create caterva_array_t with original data */
    caterva_array_t *src;
    CATERVA_TEST_ASSERT(caterva_from_buffer(buffer, buffersize, &params, &storage,
                                            &src));


    /* Create storage for dest container */

    blosc2_storage b_storage2 = {.cparams=&cparams, .dparams=&dparams};
    caterva_storage_t storage2 = {.b_storage=&b_storage2};
    if (backend2.persistent) {
        storage2.b_storage->urlpath = urlpath2;
    }
    storage2.b_storage->contiguous = backend2.contiguous;
    blocknitems = 1;
    for (int i = 0; i < params.ndim; ++i) {
        storage2.chunkshape[i] = shapes.chunkshape2[i];
        storage2.blockshape[i] = shapes.blockshape2[i];
        blocknitems *= storage2.blockshape[i];
    }
    storage2.b_storage->cparams->blocksize = blocknitems * storage2.b_storage->cparams->typesize;

    blosc2_context *ctx2 = blosc2_create_cctx(*storage2.b_storage->cparams);

    caterva_array_t *dest;
    CATERVA_TEST_ASSERT(caterva_get_slice(src, shapes.start, shapes.stop,
                                          &storage2, &dest));

    CATERVA_TEST_ASSERT(caterva_squeeze_index(ctx2, dest, shapes.squeeze_indexes));

    int8_t nsq = 0;
    for (int i = 0; i < params.ndim; ++i) {
        if (shapes.squeeze_indexes[i] == true) {
            nsq++;
        }
    }
    CUTEST_ASSERT("dims are not correct", src->ndim == dest->ndim + nsq);

    free(buffer);
    CATERVA_TEST_ASSERT(caterva_free(&src));
    CATERVA_TEST_ASSERT(caterva_free(&dest));
    blosc2_free_ctx(ctx2);

    blosc2_remove_urlpath(urlpath);
    blosc2_remove_urlpath(urlpath2);

    return 0;
}


CUTEST_TEST_TEARDOWN(squeeze_index) {
    blosc2_destroy();
}

int main() {
    CUTEST_TEST_RUN(squeeze_index);
}
