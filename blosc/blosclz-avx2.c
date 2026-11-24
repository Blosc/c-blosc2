/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* Compile the BloscLZ compressor in its own AVX2 translation unit.
 * Callers must verify runtime CPUID/OSXSAVE support before invoking it. */
#define BLOSCLZ_BUILD_AVX2 1
#include "blosclz.c"
