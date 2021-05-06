/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Roundtrip tests for the ALTIVEC-accelerated shuffle/unshuffle.

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"
#include "../blosc/shuffle.h"
#include "../blosc/shuffle-generic.h"


/* Include ALTIVEC-accelerated shuffle implementation if supported by this compiler.
   TODO: Need to also do run-time CPU feature support here. */
#if defined(SHUFFLE_ALTIVEC_ENABLED)
  #include "../blosc/shuffle-altivec.h"
#else
  #if defined(_MSC_VER)
    #pragma message("ALTIVEC shuffle tests not enabled.")
  #else
    #warning ALTIVEC shuffle tests not enabled.
  #endif
#endif  /* defined(SHUFFLE_ALTIVEC_ENABLED) */


/** Roundtrip tests for the ALTIVEC-accelerated shuffle/unshuffle. */
static int test_shuffle_roundtrip_altivec(int32_t type_size, int32_t num_elements,
                                          size_t buffer_alignment, int test_type) {
#if defined(SHUFFLE_ALTIVEC_ENABLED)
  int32_t buffer_size = type_size * num_elements;

  /* Allocate memory for the test. */
  void* original = blosc_test_malloc(buffer_alignment, (size_t)buffer_size);
  void* shuffled = blosc_test_malloc(buffer_alignment, (size_t)buffer_size);
  void* unshuffled = blosc_test_malloc(buffer_alignment, (size_t)buffer_size);

  /* Fill the input data buffer with random values. */
  blosc_test_fill_seq(original, (size_t)buffer_size);

  /* Shuffle/unshuffle, selecting the implementations based on the test type. */
  switch (test_type) {
    case 0:
      /* altivec/altivec */
      shuffle_altivec(type_size, buffer_size, original, shuffled);
      unshuffle_altivec(type_size, buffer_size, shuffled, unshuffled);
      break;
    case 1:
      /* generic/altivec */
      shuffle_generic(type_size, buffer_size, original, shuffled);
      unshuffle_altivec(type_size, buffer_size, shuffled, unshuffled);
      break;
    case 2:
      /* altivec/generic */
      shuffle_altivec(type_size, buffer_size, original, shuffled);
      unshuffle_generic(type_size, buffer_size, shuffled, unshuffled);
      break;
    default:
      fprintf(stderr, "Invalid test type specified (%d).", test_type);
      return EXIT_FAILURE;
  }

  /* The round-tripped data matches the original data when the
     result of memcmp is 0. */
  int exit_code = memcmp(original, unshuffled, (size_t)buffer_size) ?
                  EXIT_FAILURE : EXIT_SUCCESS;

  /* Free allocated memory. */
  blosc_test_free(original);
  blosc_test_free(shuffled);
  blosc_test_free(unshuffled);

  return exit_code;
#else
  return EXIT_SUCCESS;
#endif /* defined(SHUFFLE_ALTIVEC_ENABLED) */
}


/** Required number of arguments to this test, including the executable name. */
#define TEST_ARG_COUNT  5

int main(int argc, char** argv) {
  /*  argv[1]: sizeof(element type)
      argv[2]: number of elements
      argv[3]: buffer alignment
      argv[4]: test type
  */

  /*  Verify the correct number of command-line args have been specified. */
  if (TEST_ARG_COUNT != argc) {
    blosc_test_print_bad_argcount_msg(TEST_ARG_COUNT, argc);
    return EXIT_FAILURE;
  }

  /* Parse arguments */
  uint32_t type_size;
  if (!blosc_test_parse_uint32_t(argv[1], &type_size) || (type_size < 1)) {
    blosc_test_print_bad_arg_msg(1);
    return EXIT_FAILURE;
  }

  uint32_t num_elements;
  if (!blosc_test_parse_uint32_t(argv[2], &num_elements) || (num_elements < 1)) {
    blosc_test_print_bad_arg_msg(2);
    return EXIT_FAILURE;
  }

  uint32_t buffer_align_size;
  if (!blosc_test_parse_uint32_t(argv[3], &buffer_align_size)
      || (buffer_align_size & (buffer_align_size - 1))
      || (buffer_align_size < sizeof(void*))) {
    blosc_test_print_bad_arg_msg(3);
    return EXIT_FAILURE;
  }

  uint32_t test_type;
  if (!blosc_test_parse_uint32_t(argv[4], &test_type) || (test_type > 2)) {
    blosc_test_print_bad_arg_msg(4);
    return EXIT_FAILURE;
  }

  /* Run the test. */
  return test_shuffle_roundtrip_altivec(type_size, num_elements, buffer_align_size, test_type);
}
