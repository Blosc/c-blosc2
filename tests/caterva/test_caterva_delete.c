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
    int8_t axis;
    int64_t start;
    int64_t delete_len;
} test_shapes_t;


CUTEST_TEST_DATA(delete) {
    blosc2_storage *b_storage;
};


CUTEST_TEST_SETUP(delete) {
    blosc2_init();

    // Add parametrizations
    CUTEST_PARAMETRIZE(typesize, uint8_t, CUTEST_DATA(
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
    CUTEST_GET_PARAMETER(typesize, uint8_t);

    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
    cparams.nthreads = 2;
    cparams.compcode = BLOSC_LZ4;
    cparams.typesize = typesize;
    blosc2_storage b_storage = {.cparams=&cparams, .dparams=&dparams};
    data->b_storage = &b_storage;

    char *urlpath = "test_delete.b2frame";
    blosc2_remove_urlpath(urlpath);

    caterva_params_t params;
    params.ndim = shapes.ndim;
    for (int i = 0; i < params.ndim; ++i) {
        params.shape[i] = shapes.shape[i];
    }

    caterva_storage_t storage = {.b_storage=data->b_storage};
    if (backend.persistent) {
        storage.b_storage->urlpath = urlpath;
    }
    storage.b_storage->contiguous = backend.contiguous;
    for (int i = 0; i < params.ndim; ++i) {
        storage.chunkshape[i] = shapes.chunkshape[i];
        storage.blockshape[i] = shapes.blockshape[i];
    }
    int32_t blocknitems = 1;
    for (int i = 0; i < params.ndim; ++i) {
      blocknitems *= storage.blockshape[i];
    }
    storage.b_storage->cparams->blocksize = blocknitems * storage.b_storage->cparams->typesize;

    blosc2_context *ctx = blosc2_create_cctx(*storage.b_storage->cparams);

    /* Create caterva_array_t with original data */
    caterva_array_t *src;
    uint8_t *value = malloc(typesize);
    int8_t fill_value = 1;
    switch (typesize) {
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
    CATERVA_ERROR(caterva_full(&params, &storage, value, &src));

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
    uint8_t *buffer = calloc((size_t) bufferlen, (size_t) typesize);
    CATERVA_ERROR(caterva_set_slice_buffer(ctx, buffer, buffer_shape, bufferlen * typesize, start, stop, src));

    CATERVA_ERROR(caterva_delete(ctx, src, shapes.axis, shapes.start, shapes.delete_len));

    int64_t newshape[CATERVA_MAX_DIM] = {0};
    for (int i = 0; i < shapes.ndim; ++i) {
        newshape[i] = shapes.shape[i];
    }
    newshape[shapes.axis] -= shapes.delete_len;

    // Create aux array to compare values
    caterva_array_t *aux;
    caterva_params_t aux_params;
    aux_params.ndim = shapes.ndim;
    for (int i = 0; i < aux_params.ndim; ++i) {
        aux_params.shape[i] = newshape[i];
    }
    caterva_storage_t aux_storage = {.b_storage=data->b_storage};
    aux_storage.b_storage->cparams->typesize = typesize;
    aux_storage.b_storage->urlpath = NULL;
    aux_storage.b_storage->contiguous = backend.contiguous;
    for (int i = 0; i < params.ndim; ++i) {
        aux_storage.chunkshape[i] = shapes.chunkshape[i];
        aux_storage.blockshape[i] = shapes.blockshape[i];
    }
    blocknitems = 1;
    for (int i = 0; i < params.ndim; ++i) {
      blocknitems *= aux_storage.blockshape[i];
    }
    aux_storage.b_storage->cparams->blocksize = blocknitems * aux_storage.b_storage->cparams->typesize;
    blosc2_context *aux_ctx = blosc2_create_cctx(*aux_storage.b_storage->cparams);

    CATERVA_ERROR(caterva_full(&aux_params, &aux_storage, value, &aux));


    /* Fill buffer with whole array data */
    uint8_t *src_buffer = malloc((size_t) (src->nitems * typesize));
    CATERVA_TEST_ASSERT(caterva_to_buffer(aux_ctx, src, src_buffer, src->nitems * typesize));

    for (uint64_t i = 0; i < (uint64_t) src->nitems; ++i) {
        switch (typesize) {
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
    free(src_buffer);

    CATERVA_TEST_ASSERT(caterva_free(&src));
    CATERVA_TEST_ASSERT(caterva_free(&aux));
    blosc2_free_ctx(ctx);
    blosc2_free_ctx(aux_ctx);

    blosc2_remove_urlpath(urlpath);

    return 0;
}

CUTEST_TEST_TEARDOWN(delete) {
    blosc2_destroy();
}

int main() {
    CUTEST_TEST_RUN(delete);
}
