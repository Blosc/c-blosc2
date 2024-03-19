/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Unit tests for basic features in Blosc.

  Copyright (c) 2010  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_TESTS_TEST_COMMON_H
#define BLOSC_TESTS_TEST_COMMON_H

#include "blosc2.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#if defined(_WIN32) && !defined(__MINGW32__)
  #include <time.h>
#else
  #include <sys/time.h>
#endif

#if defined(_WIN32)
  /* MSVC does not have setenv */
  #define setenv(name, value, overwrite) (_putenv_s(name, value))
#endif


/* This is MinUnit in action (https://jera.com/techinfo/jtns/jtn002) */
#define mu_assert(message, test) do { if (!(test)) return message; } while (0)
#define mu_run_test(test) do \
    { char *message = test(); tests_run++;                          \
      if (message) { printf("%c", 'F'); return message;}            \
      else printf("%c", '.'); } while (0)

extern int tests_run;

#define KB  1024
#define MB  (1024*KB)
#define GB  (1024*MB)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/*
  Memory functions.
*/

#define UNUSED(x) ((void)(x))

/** Allocates a block of memory with the specified size and alignment.
    The allocated memory is 'cleaned' before returning to avoid
    accidental reuse of data within or between tests.
 */
static inline void* blosc_test_malloc(const size_t alignment, const size_t size) {
  const int32_t clean_value = 0x99;
  void* block = NULL;
  int32_t res = 0;

#if defined(_ISOC11_SOURCE) || (defined(__FreeBSD__) && __STDC_VERSION__ >= 201112L)
  /* C11 aligned allocation. 'size' must be a multiple of the alignment. */
  block = aligned_alloc(alignment, size);
#elif defined(_WIN32)
  /* A (void *) cast needed for avoiding a warning with MINGW :-/ */
  block = (void *)_aligned_malloc(size, alignment);
#elif _POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600
  /* Platform does have an implementation of posix_memalign */
  res = posix_memalign(&block, alignment, size);
#elif defined(__APPLE__)
  /* Mac OS X guarantees 16-byte alignment in small allocs */
  UNUSED(alignment);
  block = malloc(size);
#else
  #error Cannot determine how to allocate aligned memory on the target platform.
#endif

  if (block == NULL || res != 0) {
    fprintf(stderr, "Error allocating memory!");
    return NULL;
  }

  /* Clean the allocated memory before returning. */
  memset(block, clean_value, size);

  return block;
}

/** Frees memory allocated by blosc_test_malloc. */
static inline void blosc_test_free(void* ptr) {
#if defined(_WIN32)
  _aligned_free(ptr);
#else
  free(ptr);
#endif  /* _WIN32 */
}

/** Fills a buffer with contiguous values. */
static inline void blosc_test_fill_seq(void* const ptr, const size_t size) {
  size_t k;
  uint8_t* const byte_ptr = (uint8_t*)ptr;
  for (k = 0; k < size; k++) {
    byte_ptr[k] = (uint8_t)k;
  }
}

/** Fills a buffer with random values. */
static inline void blosc_test_fill_random(void* const ptr, const size_t size) {
  size_t k;
  uint8_t* const byte_ptr = (uint8_t*)ptr;
  for (k = 0; k < size; k++) {
    byte_ptr[k] = (uint8_t)rand();
  }
}

/*
  Argument parsing.
*/

/** Parse a `int32_t` value from a string, checking for overflow. */
static inline bool blosc_test_parse_uint32_t(const char* const str, uint32_t* value) {
  char* str_end;
  long signed_value = strtol(str, &str_end, 10);
  if (signed_value < 0 || *str_end) {
    return false;
  }
  else {
    *value = (uint32_t)signed_value;
    return true;
  }
}

/*
  Error message functions.
*/

/** Print an error message when a test program has been invoked
    with an invalid number of arguments. */
static inline void blosc_test_print_bad_argcount_msg(
    const int32_t num_expected_args, const int32_t num_actual_args) {
  fprintf(stderr, "Invalid number of arguments specified.\nExpected %d arguments but was given %d.",
          num_expected_args, num_actual_args);
}

/** Print an error message when a test program has been invoked
    with an invalid argument value. */
static inline void blosc_test_print_bad_arg_msg(const int32_t arg_index) {
  fprintf(stderr, "Invalid value specified for argument at index %d.\n", arg_index);
}

/* dummy callback backend for testing purposes */
/* serial "threads" backend */
static void dummy_threads_callback(void *callback_data, void (*dojob)(void *), int numjobs, size_t jobdata_elsize, void *jobdata)
{
  int i;
  (void) callback_data; /* unused */
  for (i = 0; i < numjobs; ++i)
    dojob(((char *) jobdata) + ((unsigned) i)*jobdata_elsize);
}

/* install the callback if environment variable BLOSC_TEST_CALLBACK="yes" */
static inline void install_blosc_callback_test(void)
{
  char *callback_env;
  callback_env = getenv("BLOSC_TEST_CALLBACK");
  if (callback_env && !strcmp(callback_env, "yes"))
    blosc2_set_threads_callback(dummy_threads_callback, NULL);
}

#endif /* BLOSC_TESTS_TEST_COMMON_H */
