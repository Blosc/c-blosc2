/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Edge cases for the overflow-checked helpers in blosc/blosc-checked.h.
  They all return true on success and leave *out untouched on failure.
*/

#include "test_common.h"
#include "cutest.h"
#include "blosc-checked.h"

#include <limits.h>
#include <stdint.h>
#include <stddef.h>


CUTEST_TEST_DATA(checked_arith) {
  bool placeholder;
};

CUTEST_TEST_SETUP(checked_arith) {
  BLOSC_UNUSED_PARAM(data);
}

CUTEST_TEST_TEST(checked_arith) {
  BLOSC_UNUSED_PARAM(data);
  size_t zs = 7;
  int64_t z64 = 7;
  int32_t z32 = 7;

  /* size_t add */
  CUTEST_ASSERT("add_size 0+0", blosc2_checked_add_size(0, 0, &zs) && zs == 0);
  CUTEST_ASSERT("add_size MAX-1+1", blosc2_checked_add_size(SIZE_MAX - 1, 1, &zs) && zs == SIZE_MAX);
  CUTEST_ASSERT("add_size 0+MAX", blosc2_checked_add_size(0, SIZE_MAX, &zs) && zs == SIZE_MAX);
  zs = 7;
  CUTEST_ASSERT("add_size MAX+1 fails", !blosc2_checked_add_size(SIZE_MAX, 1, &zs));
  CUTEST_ASSERT("add_size 1+MAX fails", !blosc2_checked_add_size(1, SIZE_MAX, &zs));
  CUTEST_ASSERT("add_size failure leaves out", zs == 7);
  CUTEST_ASSERT("add_size NULL out", blosc2_checked_add_size(1, 2, NULL));
  CUTEST_ASSERT("add_size NULL out fails", !blosc2_checked_add_size(SIZE_MAX, SIZE_MAX, NULL));

  /* size_t mul */
  CUTEST_ASSERT("mul_size 0*MAX", blosc2_checked_mul_size(0, SIZE_MAX, &zs) && zs == 0);
  CUTEST_ASSERT("mul_size MAX*0", blosc2_checked_mul_size(SIZE_MAX, 0, &zs) && zs == 0);
  CUTEST_ASSERT("mul_size MAX*1", blosc2_checked_mul_size(SIZE_MAX, 1, &zs) && zs == SIZE_MAX);
  CUTEST_ASSERT("mul_size MAX/2*2", blosc2_checked_mul_size(SIZE_MAX / 2, 2, &zs) && zs == SIZE_MAX - 1);
  zs = 7;
  CUTEST_ASSERT("mul_size (MAX/2+1)*2 fails", !blosc2_checked_mul_size(SIZE_MAX / 2 + 1, 2, &zs));
  CUTEST_ASSERT("mul_size 2*MAX fails", !blosc2_checked_mul_size(2, SIZE_MAX, &zs));
  CUTEST_ASSERT("mul_size failure leaves out", zs == 7);
  CUTEST_ASSERT("mul_size NULL out", blosc2_checked_mul_size(3, 4, NULL));

  /* signed int64 add, either sign */
  CUTEST_ASSERT("add_i64 MAX-1+1", blosc2_checked_add_i64(INT64_MAX - 1, 1, &z64) && z64 == INT64_MAX);
  CUTEST_ASSERT("add_i64 MIN+MAX", blosc2_checked_add_i64(INT64_MIN, INT64_MAX, &z64) && z64 == -1);
  CUTEST_ASSERT("add_i64 MIN+1-1", blosc2_checked_add_i64(INT64_MIN + 1, -1, &z64) && z64 == INT64_MIN);
  CUTEST_ASSERT("add_i64 -5+3", blosc2_checked_add_i64(-5, 3, &z64) && z64 == -2);
  z64 = 7;
  CUTEST_ASSERT("add_i64 MAX+1 fails", !blosc2_checked_add_i64(INT64_MAX, 1, &z64));
  CUTEST_ASSERT("add_i64 MIN-1 fails", !blosc2_checked_add_i64(INT64_MIN, -1, &z64));
  CUTEST_ASSERT("add_i64 failure leaves out", z64 == 7);
  CUTEST_ASSERT("add_i64 NULL out", blosc2_checked_add_i64(1, 2, NULL));

  /* non-negative int64 add */
  CUTEST_ASSERT("add_i64_nonneg 0+MAX", blosc2_checked_add_i64_nonneg(0, INT64_MAX, &z64) && z64 == INT64_MAX);
  z64 = 7;
  CUTEST_ASSERT("add_i64_nonneg MAX+1 fails", !blosc2_checked_add_i64_nonneg(INT64_MAX, 1, &z64));
  CUTEST_ASSERT("add_i64_nonneg -1+1 fails", !blosc2_checked_add_i64_nonneg(-1, 1, &z64));
  CUTEST_ASSERT("add_i64_nonneg 1+-1 fails", !blosc2_checked_add_i64_nonneg(1, -1, &z64));
  CUTEST_ASSERT("add_i64_nonneg failure leaves out", z64 == 7);
  CUTEST_ASSERT("add_i64_nonneg NULL out", blosc2_checked_add_i64_nonneg(1, 2, NULL));

  /* non-negative int64 mul */
  CUTEST_ASSERT("mul_i64_nonneg 0*MAX", blosc2_checked_mul_i64_nonneg(0, INT64_MAX, &z64) && z64 == 0);
  CUTEST_ASSERT("mul_i64_nonneg MAX*1", blosc2_checked_mul_i64_nonneg(INT64_MAX, 1, &z64) && z64 == INT64_MAX);
  CUTEST_ASSERT("mul_i64_nonneg MAX/2*2",
                blosc2_checked_mul_i64_nonneg(INT64_MAX / 2, 2, &z64) && z64 == INT64_MAX - 1);
  z64 = 7;
  CUTEST_ASSERT("mul_i64_nonneg (MAX/2+1)*2 fails", !blosc2_checked_mul_i64_nonneg(INT64_MAX / 2 + 1, 2, &z64));
  CUTEST_ASSERT("mul_i64_nonneg -1*0 fails", !blosc2_checked_mul_i64_nonneg(-1, 0, &z64));
  CUTEST_ASSERT("mul_i64_nonneg 0*-1 fails", !blosc2_checked_mul_i64_nonneg(0, -1, &z64));
  CUTEST_ASSERT("mul_i64_nonneg failure leaves out", z64 == 7);
  CUTEST_ASSERT("mul_i64_nonneg NULL out", blosc2_checked_mul_i64_nonneg(3, 4, NULL));

  /* non-negative int32 mul */
  CUTEST_ASSERT("mul_i32_nonneg 0*MAX", blosc2_checked_mul_i32_nonneg(0, INT32_MAX, &z32) && z32 == 0);
  CUTEST_ASSERT("mul_i32_nonneg MAX*1", blosc2_checked_mul_i32_nonneg(INT32_MAX, 1, &z32) && z32 == INT32_MAX);
  CUTEST_ASSERT("mul_i32_nonneg 46340^2",
                blosc2_checked_mul_i32_nonneg(46340, 46340, &z32) && z32 == 2147395600);
  z32 = 7;
  CUTEST_ASSERT("mul_i32_nonneg 46341^2 fails", !blosc2_checked_mul_i32_nonneg(46341, 46341, &z32));
  CUTEST_ASSERT("mul_i32_nonneg -1*1 fails", !blosc2_checked_mul_i32_nonneg(-1, 1, &z32));
  CUTEST_ASSERT("mul_i32_nonneg 1*-1 fails", !blosc2_checked_mul_i32_nonneg(1, -1, &z32));
  CUTEST_ASSERT("mul_i32_nonneg failure leaves out", z32 == 7);
  CUTEST_ASSERT("mul_i32_nonneg NULL out", blosc2_checked_mul_i32_nonneg(3, 4, NULL));

  /* size_t -> int64 */
  CUTEST_ASSERT("size_to_i64 0", blosc2_checked_size_to_i64(0, &z64) && z64 == 0);
  CUTEST_ASSERT("size_to_i64 NULL out", blosc2_checked_size_to_i64(1, NULL));
#if SIZE_MAX > INT64_MAX
  CUTEST_ASSERT("size_to_i64 INT64_MAX",
                blosc2_checked_size_to_i64((size_t)INT64_MAX, &z64) && z64 == INT64_MAX);
  z64 = 7;
  CUTEST_ASSERT("size_to_i64 INT64_MAX+1 fails", !blosc2_checked_size_to_i64((size_t)INT64_MAX + 1, &z64));
  CUTEST_ASSERT("size_to_i64 failure leaves out", z64 == 7);
#else
  CUTEST_ASSERT("size_to_i64 SIZE_MAX",
                blosc2_checked_size_to_i64(SIZE_MAX, &z64) && z64 == (int64_t)SIZE_MAX);
#endif

  return 0;
}

CUTEST_TEST_TEARDOWN(checked_arith) {
  BLOSC_UNUSED_PARAM(data);
}


int main(void) {
  CUTEST_TEST_RUN(checked_arith);
}
