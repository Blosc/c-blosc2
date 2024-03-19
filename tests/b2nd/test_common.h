/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_TESTS_B2ND_TEST_COMMON_H
#define BLOSC_TESTS_B2ND_TEST_COMMON_H

#include <b2nd.h>
#include "context.h"
#include "cutest.h"


#define B2ND_TEST_ASSERT(rc) CUTEST_ASSERT(print_error(rc), (rc) >= 0);

#ifdef __GNUC__
#define B2ND_TEST_UNUSED __attribute__((unused))
#else
#define B2ND_TEST_UNUSED
#endif

static bool fill_buf(void *buf, uint8_t typesize, size_t buf_size) B2ND_TEST_UNUSED;

static bool fill_buf(void *buf, uint8_t typesize, size_t buf_size) {
  switch (typesize) {
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
        ((uint16_t *) buf)[i] = (uint16_t) i + 1;
      }
      break;
    case 1:
      for (size_t i = 0; i < buf_size; ++i) {
        ((uint8_t *) buf)[i] = (uint8_t) i + 1;
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
  int64_t shape[B2ND_MAX_DIM];
  int32_t chunkshape[B2ND_MAX_DIM];
  int32_t blockshape[B2ND_MAX_DIM];
} _test_shapes;


typedef struct {
  bool contiguous;
  bool persistent;
} _test_backend;


void b2nd_default_parameters() {
  CUTEST_PARAMETRIZE(typesize, uint8_t, CUTEST_DATA(1, 2, 4, 8));
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


#define B2ND_TEST_ASSERT_BUFFER(buffer1, buffer2, buffersize) \
    for (int i = 0; i < (buffersize); ++i) {                     \
        CUTEST_ASSERT("elements are not equals!", (buffer1)[i] == (buffer2)[i]); \
    }


#endif /* BLOSC_TESTS_B2ND_TEST_COMMON_H */
