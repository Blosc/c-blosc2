/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/*
 * Overflow-checked integer helpers shared by the library sources.
 *
 * All of them follow the same convention:
 *   - return true on success and false if the result does not fit;
 *   - write *out only on success, and accept out == NULL when only the
 *     check is wanted;
 *   - the _nonneg variants also fail when an operand is negative.
 *
 * Plain C on purpose (no __builtin_*_overflow), so they behave the same
 * with every compiler in the CI matrix.
 */

#ifndef BLOSC_BLOSC_CHECKED_H
#define BLOSC_BLOSC_CHECKED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static inline bool blosc2_checked_add_size(size_t a, size_t b, size_t *out) {
  if (a > SIZE_MAX - b) {
    return false;
  }
  if (out != NULL) {
    *out = a + b;
  }
  return true;
}

static inline bool blosc2_checked_mul_size(size_t a, size_t b, size_t *out) {
  if (a != 0 && b > SIZE_MAX / a) {
    return false;
  }
  if (out != NULL) {
    *out = a * b;
  }
  return true;
}

static inline bool blosc2_checked_add_i64(int64_t a, int64_t b, int64_t *out) {
  if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) {
    return false;
  }
  if (out != NULL) {
    *out = a + b;
  }
  return true;
}

static inline bool blosc2_checked_add_i64_nonneg(int64_t a, int64_t b, int64_t *out) {
  if (a < 0 || b < 0 || a > INT64_MAX - b) {
    return false;
  }
  if (out != NULL) {
    *out = a + b;
  }
  return true;
}

static inline bool blosc2_checked_mul_i64_nonneg(int64_t a, int64_t b, int64_t *out) {
  if (a < 0 || b < 0 || (a != 0 && b > INT64_MAX / a)) {
    return false;
  }
  if (out != NULL) {
    *out = a * b;
  }
  return true;
}

static inline bool blosc2_checked_mul_i32_nonneg(int32_t a, int32_t b, int32_t *out) {
  if (a < 0 || b < 0 || (a != 0 && b > INT32_MAX / a)) {
    return false;
  }
  if (out != NULL) {
    *out = a * b;
  }
  return true;
}

static inline bool blosc2_checked_size_to_i64(size_t value, int64_t *out) {
  if (value > (size_t)INT64_MAX) {
    return false;
  }
  if (out != NULL) {
    *out = (int64_t)value;
  }
  return true;
}

#endif /* BLOSC_BLOSC_CHECKED_H */
