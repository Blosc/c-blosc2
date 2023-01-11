/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef CATERVA_TEST_COMMON_H
#define CATERVA_TEST_COMMON_H

#include <caterva.h>
#include "cutest.h"


#define CATERVA_TEST_ASSERT(rc) CUTEST_ASSERT(print_error(rc), (rc) == CATERVA_SUCCEED);

#ifdef __GNUC__
#define CATERVA_TEST_UNUSED __attribute__((unused))
#else
#define CATERVA_TEST_UNUSED
#endif

static bool fill_buf(void *buf, uint8_t itemsize, size_t buf_size) CATERVA_TEST_UNUSED;
        static bool fill_buf(void *buf, uint8_t itemsize, size_t buf_size) {
    switch (itemsize) {
        case 8:
            for (size_t i = 0; i < buf_size; ++i) {
                ((uint64_t *) buf)[i] = (uint64_t) i + 1;
            }
            break;
        case 4:
            for (size_t i = 0; i < buf_size; ++i) {
                ((uint32_t *) buf)[i] = (uint32_t) i + 1;
            }
            break;
        case 2:
            for (size_t i = 0; i < buf_size; ++i) {
                ((uint16_t *) buf)[i] = (uint16_t ) i + 1;
            }
            break;
        case 1:
            for (size_t i = 0; i < buf_size; ++i) {
                ((uint8_t *) buf)[i] = (uint8_t ) i + 1;
            }
            break;
        default:
            return false;
    }
    return true;
}


/* Tests data */

typedef struct {
    int8_t ndim;
    int64_t shape[CATERVA_MAX_DIM];
    int32_t chunkshape[CATERVA_MAX_DIM];
    int32_t blockshape[CATERVA_MAX_DIM];
} _test_shapes;


typedef struct {
    bool contiguous;
    bool persistent;
} _test_backend;


void caterva_default_parameters() {
    CUTEST_PARAMETRIZE(itemsize, uint8_t, CUTEST_DATA(1, 2, 4, 8));
    CUTEST_PARAMETRIZE(shapes, _test_shapes, CUTEST_DATA(
        {2, {40, 40}, {20, 20}, {10, 10}},
        {3, {40, 55, 23}, {31, 5, 22}, {4, 4, 4}},
        {3, {40, 0, 12}, {31, 0, 12}, {10, 0, 12}},
        {4, {50, 60, 31, 12}, {25, 20, 20, 10}, {5, 5, 5, 10}},
        {5, {1, 1, 1024, 1, 1}, {1, 1, 500, 1, 1}, {1, 1, 200, 1, 1}},
        {6, {5, 1, 50, 3, 1, 2}, {5, 1, 50, 2, 1, 2}, {2, 1, 20, 2, 1, 2}},
    ));
    CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
        {false, false},
        {true, false},
        {false, true},
        {true, true},
    ));
}


#define CATERVA_TEST_ASSERT_BUFFER(buffer1, buffer2, buffersize) \
    for (int i = 0; i < (buffersize); ++i) {                     \
        CUTEST_ASSERT("elements are not equals!", (buffer1)[i] == (buffer2)[i]); \
    }


#endif //CATERVA_TEST_COMMON_H
