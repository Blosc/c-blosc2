/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "test_common.h"

int tests_run = 0;


static char* test_getitem(void) {
  blosc2_set_nthreads(1);

  size_t type_size = 131;
  size_t num_elements = 1;

  blosc1_set_compressor("blosclz");
  blosc2_set_delta(1);

  size_t buffer_size = type_size * num_elements;

  /* Allocate memory for the test. */
  void* original = malloc(buffer_size);
  void* intermediate = malloc(buffer_size + BLOSC2_MAX_OVERHEAD);
  void* items = malloc(buffer_size);
  void* result = malloc(buffer_size);

  /* The test data */
  memset(original, 0, buffer_size);
  ((char*)original)[128] = 1;

  /* Compress the input data and store it in an intermediate buffer.
     Decompress the data from the intermediate buffer into a result buffer. */
  blosc1_compress(1, 0, type_size, buffer_size,
                  original, intermediate, buffer_size + BLOSC2_MAX_OVERHEAD);

  int start_item = 0;
  int num_items = 1;
  blosc1_decompress(intermediate, result, buffer_size);
  assert(memcmp(original, result, buffer_size) == 0);
  mu_assert("ERROR: decompression with delta filter fails", memcmp(original, result, buffer_size) == 0);

  /* Now that we see the round-trip passed, check the getitem */
  int get_result = blosc1_getitem(intermediate, start_item, num_items, items);
  mu_assert("ERROR: the number of items in getitem is not correct", (uint32_t) get_result == (num_items * type_size));
  mu_assert("ERROR: getitem with delta filter fails", memcmp(original, items, get_result) == 0);

  /* Free allocated memory. */
  free(original);
  free(intermediate);
  free(items);
  free(result);
  return EXIT_SUCCESS;
}

static char *all_tests(void) {
  mu_run_test(test_getitem);
  return EXIT_SUCCESS;
}

int main(void) {
  char *result;
  blosc2_init();

  result = all_tests();

  blosc2_destroy();

  return result != EXIT_SUCCESS;
}
