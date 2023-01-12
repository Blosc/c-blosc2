/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"


CUTEST_TEST_DATA(serialize) {
    caterva_ctx_t *ctx;
};


CUTEST_TEST_SETUP(serialize) {
    blosc2_init();
    caterva_config_t cfg = CATERVA_CONFIG_DEFAULTS;
    cfg.nthreads = 2;
    cfg.compcode = BLOSC_BLOSCLZ;
    caterva_ctx_new(&cfg, &data->ctx);

    // Add parametrizations
    CUTEST_PARAMETRIZE(itemsize, uint8_t, CUTEST_DATA(1, 2, 4, 8));
    CUTEST_PARAMETRIZE(shapes, _test_shapes, CUTEST_DATA(
            {0, {0}, {0}, {0}}, // 0-dim
            {1, {10}, {7}, {2}}, // 1-idim
            {2, {40, 40}, {20, 20}, {10, 10}},
            {3, {100, 55, 23}, {31, 5, 22}, {4, 4, 4}},
            {3, {100, 0, 12}, {31, 0, 12}, {10, 0, 12}},
            {4, {30, 26, 31, 12}, {25, 20, 20, 10}, {5, 5, 5, 10}},
            {5, {1, 1, 1024, 1, 1}, {1, 1, 500, 1, 1}, {1, 1, 200, 1, 1}},
            {6, {5, 1, 60, 3, 1, 2}, {5, 1, 50, 2, 1, 2}, {2, 1, 20, 2, 1, 2}}
    ));
    CUTEST_PARAMETRIZE(contiguous, bool, CUTEST_DATA(true, false));
}


CUTEST_TEST_TEST(serialize) {
    CUTEST_GET_PARAMETER(shapes, _test_shapes);
    CUTEST_GET_PARAMETER(itemsize, uint8_t);
    CUTEST_GET_PARAMETER(contiguous, bool);

    caterva_params_t params;
    params.itemsize = itemsize;
    params.ndim = shapes.ndim;
    for (int i = 0; i < params.ndim; ++i) {
        params.shape[i] = shapes.shape[i];
    }

    caterva_storage_t storage = {0};
    storage.urlpath = NULL;
    storage.contiguous = contiguous;
    for (int i = 0; i < params.ndim; ++i) {
        storage.chunkshape[i] = shapes.chunkshape[i];
        storage.blockshape[i] = shapes.blockshape[i];
    }

    /* Create original data */
    size_t buffersize = itemsize;
    for (int i = 0; i < params.ndim; ++i) {
        buffersize *= (size_t) params.shape[i];
    }


    uint8_t *buffer = malloc(buffersize);
    CUTEST_ASSERT("Buffer filled incorrectly", fill_buf(buffer, itemsize, buffersize / itemsize));

    /* Create caterva_array_t with original data */
    caterva_array_t *src;
    CATERVA_TEST_ASSERT(caterva_from_buffer(data->ctx, buffer, buffersize, &params, &storage,
                                            &src));

    uint8_t *cframe;
    int64_t cframe_len;
    bool needs_free;
    CATERVA_TEST_ASSERT(caterva_to_cframe(data->ctx, src, &cframe, &cframe_len, &needs_free));

    caterva_array_t *dest;
    CATERVA_TEST_ASSERT(caterva_from_cframe(data->ctx, cframe, cframe_len, true, &dest));

    /* Fill dest array with caterva_array_t data */
    uint8_t *buffer_dest = malloc(buffersize);
    CATERVA_TEST_ASSERT(caterva_to_buffer(data->ctx, dest, buffer_dest, buffersize));

    /* Testing */
    CATERVA_TEST_ASSERT_BUFFER(buffer, buffer_dest, (int) buffersize);

    /* Free mallocs */
    if (needs_free) {
        free(cframe);
    }

    free(buffer);
    free(buffer_dest);
    CATERVA_TEST_ASSERT(caterva_free(data->ctx, &src));
    CATERVA_TEST_ASSERT(caterva_free(data->ctx, &dest));

    return 0;
}


CUTEST_TEST_TEARDOWN(serialize) {
    caterva_ctx_free(&data->ctx);
    blosc2_destroy();
}

int main() {
    CUTEST_TEST_RUN(serialize);
}
