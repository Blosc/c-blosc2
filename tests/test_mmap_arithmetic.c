/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include "test_common.h"
#include "cutest.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>


CUTEST_TEST_DATA(mmap_arithmetic) {
  bool placeholder;
};

CUTEST_TEST_SETUP(mmap_arithmetic) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_init();
}

CUTEST_TEST_TEST(mmap_arithmetic) {
  BLOSC_UNUSED_PARAM(data);
  uint8_t backing[32];
  memset(backing, 0, sizeof(backing));

  blosc2_stdio_mmap mmap_file = BLOSC2_STDIO_MMAP_DEFAULTS;
  mmap_file.addr = (char*)backing;
  mmap_file.file_size = sizeof(backing);
  mmap_file.mapping_size = sizeof(backing);

  uint8_t src_ok[4] = {1, 2, 3, 4};
  int64_t nwritten = blosc2_stdio_mmap_write(src_ok, 1, 4, 8, &mmap_file);
  CUTEST_ASSERT("Valid mmap write should succeed", nwritten == 4);
  CUTEST_ASSERT("Valid mmap write should copy bytes", memcmp(backing + 8, src_ok, 4) == 0);

  uint8_t src_guard[4] = {7, 7, 7, 7};
  nwritten = blosc2_stdio_mmap_write(src_guard, INT64_MAX, 2, 0, &mmap_file);
  CUTEST_ASSERT("mmap write must reject multiplication overflow", nwritten == 0);

  nwritten = blosc2_stdio_mmap_write(src_guard, 1, 1, INT64_MAX, &mmap_file);
  CUTEST_ASSERT("mmap write must reject addition overflow", nwritten == 0);

  nwritten = blosc2_stdio_mmap_write(src_guard, -1, 1, 0, &mmap_file);
  CUTEST_ASSERT("mmap write must reject negative size", nwritten == 0);

  void* read_ptr = NULL;
  int64_t nread = blosc2_stdio_mmap_read(&read_ptr, 1, 4, 8, &mmap_file);
  CUTEST_ASSERT("Valid mmap read should succeed", nread == 4);
  CUTEST_ASSERT("Valid mmap read should return mapped pointer", read_ptr == (void*)(backing + 8));

  nread = blosc2_stdio_mmap_read(NULL, 1, 1, 0, &mmap_file);
  CUTEST_ASSERT("mmap read must reject null ptr argument", nread == 0);

  read_ptr = (void*)0x1;
  nread = blosc2_stdio_mmap_read(&read_ptr, INT64_MAX, 2, 0, &mmap_file);
  CUTEST_ASSERT("mmap read must reject multiplication overflow", nread == 0);
  CUTEST_ASSERT("mmap read must null pointer on invalid args", read_ptr == NULL);

  read_ptr = (void*)0x1;
  nread = blosc2_stdio_mmap_read(&read_ptr, 1, 1, INT64_MAX, &mmap_file);
  CUTEST_ASSERT("mmap read must reject addition overflow", nread == 0);
  CUTEST_ASSERT("mmap read must null pointer on overflow", read_ptr == NULL);

  read_ptr = (void*)0x1;
  nread = blosc2_stdio_mmap_read(&read_ptr, -1, 1, 0, &mmap_file);
  CUTEST_ASSERT("mmap read must reject negative size", nread == 0);
  CUTEST_ASSERT("mmap read must null pointer on negative args", read_ptr == NULL);

  read_ptr = (void*)0x1;
  nread = blosc2_stdio_mmap_read(&read_ptr, 1, 8, 28, &mmap_file);
  CUTEST_ASSERT("mmap read must reject out-of-bounds access", nread == 0);
  CUTEST_ASSERT("mmap read must null pointer on out-of-bounds", read_ptr == NULL);

  return 0;
}

CUTEST_TEST_TEARDOWN(mmap_arithmetic) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_destroy();
}


int main(void) {
  CUTEST_TEST_RUN(mmap_arithmetic);
}
