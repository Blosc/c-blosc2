/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  The Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_BLOSC2_BLOSC2_COMMON_H
#define BLOSC_BLOSC2_BLOSC2_COMMON_H

#include "blosc2-export.h"

#include <stdint.h>
#include <string.h>

// For shutting up stupid compiler warning about some 'unused' variables in GCC
#ifdef __GNUC__
#define BLOSC_GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#define BLOSC_UNUSED_VAR __attribute__ ((unused))
#else
#define BLOSC_UNUSED_VAR
#endif  // __GNUC__

// For shutting up compiler warning about unused parameters
#define BLOSC_UNUSED_PARAM(x) ((void)(x))

/* Use inlined functions for supported systems */
#if defined(_MSC_VER) && !defined(__cplusplus)   /* Visual Studio */
  #define inline __inline  /* Visual C is not C99, but supports some kind of inline */
#endif


/* Define the __SSE2__ symbol if compiling with Visual C++ and
   targeting the minimum architecture level supporting SSE2.
   Other compilers define this as expected and emit warnings
   when it is re-defined. */
#if !defined(__SSE2__) && defined(_MSC_VER) && \
    (defined(_M_X64) || (defined(_M_IX86) && _M_IX86_FP >= 2))
  #define __SSE2__
#endif

#if defined(__SSE2__)
  #include <emmintrin.h>
#endif
#if defined(__AVX2__)
  #include <immintrin.h>
#endif

#endif  /* BLOSC_BLOSC2_BLOSC2_COMMON_H */
