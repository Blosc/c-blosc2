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


uint64_t result0[1024] = {0};
uint64_t result1[1024] = {2, 3, 4, 5, 6, 7, 8};
uint64_t result2[1024] = {53, 54, 55, 56, 57, 58, 59, 63, 64, 65, 66, 67, 68, 69, 73, 74, 75, 76,
                          77, 78, 79, 83, 84, 85, 86, 87, 88, 89};
uint64_t result3[1024] = {303, 304, 305, 306, 307, 308, 309, 313, 314, 315, 316, 317, 318, 319,
                        323, 324, 325, 326, 327, 328, 329, 333, 334, 335, 336, 337, 338, 339,
                        343, 344, 345, 346, 347, 348, 349, 353, 354, 355, 356, 357, 358, 359,
                        363, 364, 365, 366, 367, 368, 369, 403, 404, 405, 406, 407, 408, 409,
                        413, 414, 415, 416, 417, 418, 419, 423, 424, 425, 426, 427, 428, 429,
                        433, 434, 435, 436, 437, 438, 439, 443, 444, 445, 446, 447, 448, 449,
                        453, 454, 455, 456, 457, 458, 459, 463, 464, 465, 466, 467, 468, 469,
                        503, 504, 505, 506, 507, 508, 509, 513, 514, 515, 516, 517, 518, 519,
                        523, 524, 525, 526, 527, 528, 529, 533, 534, 535, 536, 537, 538, 539,
                        543, 544, 545, 546, 547, 548, 549, 553, 554, 555, 556, 557, 558, 559,
                        563, 564, 565, 566, 567, 568, 569};
uint64_t result4[1024] = {0};
uint64_t result5[1024] = {0};

typedef struct {
    int8_t ndim;
    int64_t shape[CATERVA_MAX_DIM];
    int32_t chunkshape[CATERVA_MAX_DIM];
    int32_t blockshape[CATERVA_MAX_DIM];
    int32_t chunkshape2[CATERVA_MAX_DIM];
    int32_t blockshape2[CATERVA_MAX_DIM];
    int64_t start[CATERVA_MAX_DIM];
    int64_t stop[CATERVA_MAX_DIM];
    uint64_t *result;
} test_shapes_t;


CUTEST_TEST_DATA(get_slice_buffer) {
    caterva_ctx_t *ctx;
};


CUTEST_TEST_SETUP(get_slice_buffer) {
    caterva_config_t cfg = CATERVA_CONFIG_DEFAULTS;
    cfg.nthreads = 1;
    cfg.compcodec = BLOSC_BLOSCLZ;
    caterva_ctx_new(&cfg, &data->ctx);

    // Add parametrizations
    CUTEST_PARAMETRIZE(itemsize, uint8_t, CUTEST_DATA(8));
    CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
            {false, false},
            {true, false},
            {true, true},
            {false, true},
    ));

    CUTEST_PARAMETRIZE(shapes, test_shapes_t, CUTEST_DATA(
            {0, {0}, {0}, {0}, {0}, {0}, {0}, {0}, result0}, // 0-dim
            {1, {10}, {7}, {2}, {6}, {2}, {2}, {9}, result1}, // 1-idim
            {2, {14, 10}, {8, 5}, {2, 2}, {4, 4}, {2, 3}, {5, 3}, {9, 10}, result2}, // general,
            {3, {10, 10, 10}, {3, 5, 9}, {3, 4, 4}, {3, 7, 7}, {2, 5, 5}, {3, 0, 3}, {6, 7, 10}, result3}, // general
            {2, {20, 0}, {7, 0}, {3, 0}, {5, 0}, {2, 0}, {2, 0}, {8, 0}, result4}, // 0-shape
            {2, {20, 10}, {7, 5}, {3, 5}, {5, 5}, {2, 2}, {2, 0}, {18, 0}, result5}, // 0-shape
    ));
}

CUTEST_TEST_TEST(get_slice_buffer) {
    CUTEST_GET_PARAMETER(backend, _test_backend);
    CUTEST_GET_PARAMETER(shapes, test_shapes_t);
    CUTEST_GET_PARAMETER(itemsize, uint8_t);

    char *urlpath = "test_get_slice_buffer.b2frame";
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

    /* Create original data */
    size_t buffersize = itemsize;
    for (int i = 0; i < params.ndim; ++i) {
        buffersize *= (size_t) shapes.shape[i];
    }
    uint8_t *buffer = data->ctx->cfg->alloc(buffersize);

    CUTEST_ASSERT("Buffer filled incorrectly", fill_buf(buffer, itemsize, buffersize / itemsize));

    /* Create caterva_array_t with original data */
    caterva_array_t *src;
    CATERVA_TEST_ASSERT(caterva_from_buffer(data->ctx, buffer, buffersize, &params, &storage,
                                            &src));

    /* Create dest buffer */
    int64_t destshape[CATERVA_MAX_DIM] = {0};
    int64_t destbuffersize = itemsize;
    for (int i = 0; i < params.ndim; ++i) {
        destshape[i] = shapes.stop[i] - shapes.start[i];
        destbuffersize *= destshape[i];
    }

    uint64_t *destbuffer = data->ctx->cfg->alloc((size_t) destbuffersize);

    /* Fill dest buffer with a slice*/
    CATERVA_TEST_ASSERT(caterva_get_slice_buffer(data->ctx, src, shapes.start, shapes.stop,
                                                 destbuffer,
                                                 destshape, destbuffersize));


    for (int i = 0; i < destbuffersize / itemsize; ++i) {
        uint64_t a = destbuffer[i];
        uint64_t b = shapes.result[i] + 1;
        CUTEST_ASSERT("Elements are not equals!", a == b);
    }

    /* Free mallocs */
    data->ctx->cfg->free(buffer);
    data->ctx->cfg->free(destbuffer);
    CATERVA_TEST_ASSERT(caterva_free(data->ctx, &src));
    caterva_remove(data->ctx, urlpath);

    return 0;
}

CUTEST_TEST_TEARDOWN(get_slice_buffer) {
    caterva_ctx_free(&data->ctx);
}

int main() {
    CUTEST_TEST_RUN(get_slice_buffer);
}
