/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_TIMESTAMP_H
#define BLOSC_TIMESTAMP_H

#if defined(_WIN32)
/* For QueryPerformanceCounter(), etc. */
  #include <windows.h>
#elif defined(__MACH__)
  #include <mach/clock.h>
  #include <mach/mach.h>
  #include <time.h>
#elif defined(__unix__)
  #if defined(__linux__)
    #include <time.h>
  #else
    #include <sys/time.h>
  #endif
#else
  #error Unable to detect platform.
#endif

#include "blosc-export.h"

/* The type of timestamp used on this system. */
#if defined(_WIN32)
  #define blosc_timestamp_t LARGE_INTEGER
#else
  #define blosc_timestamp_t struct timespec
#endif

BLOSC_EXPORT void blosc_set_timestamp(blosc_timestamp_t* timestamp);

BLOSC_EXPORT double blosc_elapsed_nsecs(blosc_timestamp_t start_time,
                                        blosc_timestamp_t end_time);

BLOSC_EXPORT double blosc_elapsed_secs(blosc_timestamp_t start_time,
                                       blosc_timestamp_t end_time);

#endif //BLOSC_TIMESTAMP_H
