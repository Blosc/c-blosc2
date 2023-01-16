/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"


CUTEST_TEST_DATA(roundtrip) {
    blosc2_context *ctx;
};


CUTEST_TEST_SETUP(roundtrip) {
    blosc2_init();
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.nthreads = 2;
    cparams.compcode = BLOSC_BLOSCLZ;
    data->ctx = blosc2_create_cctx(cparams);

    // Add parametrizations
    caterva_default_parameters();
}


CUTEST_TEST_TEST(roundtrip) {
    CUTEST_GET_PARAMETER(backend, _test_backend);
    CUTEST_GET_PARAMETER(shapes, _test_shapes);
    CUTEST_GET_PARAMETER(itemsize, uint8_t);

    char *urlpath = "test_roundtrip.b2frame";
    blosc2_remove_urlpath(urlpath);

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
    size_t buffersize = (size_t) itemsize;
    for (int i = 0; i < shapes.ndim; ++i) {
        buffersize *= (size_t) shapes.shape[i];
    }
    uint8_t *buffer = malloc(buffersize);
    CUTEST_ASSERT("Buffer filled incorrectly", fill_buf(buffer, itemsize, buffersize / itemsize));

    /* Create caterva_array_t with original data */
    caterva_array_t *src;
    CATERVA_TEST_ASSERT(caterva_from_buffer(data->ctx, buffer, buffersize, &params,
                                            &storage,
                                            &src));

    /* Fill dest array with caterva_array_t data */
    uint8_t *buffer_dest = malloc( buffersize);
    CATERVA_TEST_ASSERT(caterva_to_buffer(data->ctx, src, buffer_dest, buffersize));

    /* Testing */
    CATERVA_TEST_ASSERT_BUFFER(buffer, buffer_dest, (int) buffersize);

    /* Free mallocs */
    free(buffer);
    free(buffer_dest);
    CATERVA_TEST_ASSERT(caterva_free(data->ctx, &src));
    blosc2_remove_urlpath(urlpath);

    return CATERVA_SUCCEED;
}


CUTEST_TEST_TEARDOWN(roundtrip) {
    blosc2_free_ctx(data->ctx);
    blosc2_destroy();
}

int main() {
    CUTEST_TEST_RUN(roundtrip);
}
