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
    int64_t newshape[CATERVA_MAX_DIM];
    bool given_pos;
    int64_t start_resize[CATERVA_MAX_DIM];
} test_shapes_t;


CUTEST_TEST_DATA(resize_shape) {
    void *unused;
};


CUTEST_TEST_SETUP(resize_shape) {
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
            {1, {5}, {3}, {2}, {10}, false, {5}}, // extend only
            {2, {20, 5}, {7, 5}, {3, 3}, {27, 10}, true, {14, 5}}, // extend only - start
            {2, {20, 10}, {7, 5}, {3, 5}, {10, 10}, false, {10, 10}}, // shrink only
            {2, {30, 20}, {8, 5}, {2, 2}, {22, 10}, true, {8, 5}}, // shrink only - start
            {3, {12, 10, 14}, {3, 5, 9}, {3, 4, 4}, {10, 15, 14}, false, {10, 10, 14}}, // shrink and extend
            {3, {10, 21, 30}, {8, 7, 15}, {5, 5, 10}, {10, 13, 10}, false, {10, 13, 10}}, // shrink only
            {3, {10, 23, 30}, {8, 7, 15}, {5, 5, 10}, {10, 16, 45}, true, {0, 0, 0}}, // shrink and extend - start
            {2, {75, 50}, {25, 13}, {8, 8}, {50, 76}, true, {50, 13}}, // shrink and extend - start
            {2, {50, 50}, {25, 13}, {8, 8}, {49, 51}, false, {49, 50}}, // shrink and extend
            {2, {143, 41}, {18, 13}, {7, 7}, {50, 50}, false, {50, 41}}, // shrink and extend
            {4, {10, 10, 5, 5}, {5, 7, 3, 3}, {2, 2, 1, 1}, {11, 20, 2, 2}, false, {10, 10, 2, 2}}, // shrink and extend

    ));
}

CUTEST_TEST_TEST(resize_shape) {
    CUTEST_GET_PARAMETER(backend, _test_backend);
    CUTEST_GET_PARAMETER(shapes, test_shapes_t);
    CUTEST_GET_PARAMETER(typesize, uint8_t);

    char *urlpath = "test_resize_shape.b2frame";
    blosc2_remove_urlpath(urlpath);

    caterva_params_t params;
    params.ndim = shapes.ndim;
    for (int i = 0; i < params.ndim; ++i) {
        params.shape[i] = shapes.shape[i];
    }

    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
    cparams.nthreads = 2;
    cparams.typesize = typesize;
    blosc2_storage b_storage = {.cparams=&cparams, .dparams=&dparams};
    caterva_storage_t storage = {.b_storage=&b_storage};
    if (backend.persistent) {
        storage.b_storage->urlpath = urlpath;
    }
    storage.b_storage->contiguous = backend.contiguous;
    int32_t blocknitems = 1;
    for (int i = 0; i < params.ndim; ++i) {
        storage.chunkshape[i] = shapes.chunkshape[i];
        storage.blockshape[i] = shapes.blockshape[i];
        blocknitems *= storage.blockshape[i];
    }
    storage.b_storage->cparams->blocksize = blocknitems * storage.b_storage->cparams->typesize;

    blosc2_context *ctx = blosc2_create_cctx(*storage.b_storage->cparams);

    int64_t buffersize = typesize;
    bool only_shrink = true;
    for (int i = 0; i < params.ndim; ++i) {
        if (shapes.newshape[i] > shapes.shape[i]) {
            only_shrink = false;
        }
        buffersize *= shapes.newshape[i];
    }

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

    if (shapes.given_pos) {
        CATERVA_ERROR(caterva_resize(ctx, src, shapes.newshape, shapes.start_resize));
    }
    else {
        CATERVA_ERROR(caterva_resize(ctx, src, shapes.newshape, NULL));
    }

    // Create aux array to compare values
    caterva_array_t *aux;
    caterva_params_t aux_params;
    aux_params.ndim = shapes.ndim;
    for (int i = 0; i < aux_params.ndim; ++i) {
        aux_params.shape[i] = shapes.newshape[i];
    }
    blosc2_storage aux_b_storage = {.cparams=&cparams, .dparams=&dparams};
    caterva_storage_t aux_storage = {.b_storage=&aux_b_storage};
    aux_storage.b_storage->contiguous = backend.contiguous;
    blocknitems = 1;
    for (int i = 0; i < params.ndim; ++i) {
        aux_storage.chunkshape[i] = shapes.chunkshape[i];
        aux_storage.blockshape[i] = shapes.blockshape[i];
        blocknitems *= aux_storage.blockshape[i];
    }
    aux_storage.b_storage->cparams->blocksize = blocknitems * aux_storage.b_storage->cparams->typesize;

    blosc2_context *aux_ctx = blosc2_create_cctx(*aux_storage.b_storage->cparams);

    CATERVA_ERROR(caterva_full(&aux_params, &aux_storage, value, &aux));
    if (!only_shrink) {
        for (int i = 0; i < shapes.ndim; ++i) {
            if (shapes.newshape[i] <= shapes.shape[i]) {
                continue;
            }
            int64_t slice_start[CATERVA_MAX_DIM] = {0};
            int64_t slice_stop[CATERVA_MAX_DIM];
            int64_t slice_shape[CATERVA_MAX_DIM] = {0};
            int64_t buffer_len = 1;
            for (int j = 0; j < shapes.ndim; ++j) {
                if (j != i) {
                    slice_shape[j] = shapes.newshape[j];
                    buffer_len *= slice_shape[j];
                    slice_stop[j] = shapes.newshape[j];
                }
            }
            slice_start[i] = shapes.start_resize[i];
            slice_shape[i] = shapes.newshape[i] - shapes.shape[i];
            if (slice_start[i] % shapes.chunkshape[i] != 0) {
                // Old padding was filled with ones
                slice_shape[i] -= shapes.chunkshape[i] - slice_start[i] % shapes.chunkshape[i];
                slice_start[i] += shapes.chunkshape[i] - slice_start[i] % shapes.chunkshape[i];
            }
            if (slice_start[i] > shapes.newshape[i]) {
                continue;
            }
            slice_stop[i] = slice_start[i] + slice_shape[i];
            buffer_len *= slice_shape[i];
            uint8_t *buffer = calloc((size_t) buffer_len, (size_t) typesize);
            CATERVA_ERROR(caterva_set_slice_buffer(aux_ctx, buffer, slice_shape, buffer_len * typesize,
                                     slice_start, slice_stop, aux));
            free(buffer);
        }
    }

    /* Fill buffers with whole arrays */
    uint8_t *src_buffer = malloc((size_t) buffersize);
    uint8_t *aux_buffer = malloc((size_t) buffersize);
    CATERVA_TEST_ASSERT(caterva_to_buffer(ctx, src, src_buffer, buffersize));
    CATERVA_TEST_ASSERT(caterva_to_buffer(aux_ctx, aux, aux_buffer, buffersize));
    for (uint64_t i = 0; i < (uint64_t) buffersize / typesize; ++i) {
        switch (typesize) {
            case 8:
                CUTEST_ASSERT("Elements are not equal!",
                              ((uint64_t *) src_buffer)[i] == ((uint64_t *) aux_buffer)[i]);
                break;
            case 4:
                CUTEST_ASSERT("Elements are not equal!",
                              ((uint32_t *) src_buffer)[i] == ((uint32_t *) aux_buffer)[i]);
                break;
            case 2:
                CUTEST_ASSERT("Elements are not equal!",
                              ((uint16_t *) src_buffer)[i] == ((uint16_t *) aux_buffer)[i]);
                break;
            case 1:
                CUTEST_ASSERT("Elements are not equal!",
                              ((uint8_t *) src_buffer)[i] == ((uint8_t *) aux_buffer)[i]);
                break;
            default:
                CATERVA_TEST_ASSERT(CATERVA_ERR_INVALID_ARGUMENT);
        }
    }
    /* Free mallocs */
    free(value);
    free(src_buffer);
    free(aux_buffer);

    CATERVA_TEST_ASSERT(caterva_free(&src));
    CATERVA_TEST_ASSERT(caterva_free(&aux));
    blosc2_free_ctx(ctx);
    blosc2_free_ctx(aux_ctx);
    blosc2_remove_urlpath(urlpath);

    return 0;
}

CUTEST_TEST_TEARDOWN(resize_shape) {
    blosc2_destroy();
}

int main() {
    CUTEST_TEST_RUN(resize_shape);
}
