/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "blosc2.h"

/* System-specific high-precision timing functions. */
#if defined(_WIN32)

#include <windows.h>

/* Set a timestamp value to the current time. */
void blosc_set_timestamp(blosc_timestamp_t* timestamp) {
  /* Ignore the return value, assume the call always succeeds. */
  QueryPerformanceCounter(timestamp);
}

/* Given two timestamp values, return the difference in nanoseconds. */
double blosc_elapsed_nsecs(blosc_timestamp_t start_time,
                           blosc_timestamp_t end_time) {
  LARGE_INTEGER CounterFreq;
  QueryPerformanceFrequency(&CounterFreq);

  return (double)(end_time.QuadPart - start_time.QuadPart) /
    ((double)CounterFreq.QuadPart / 1e9);
}

#else

#include <time.h>

#if defined(__MACH__) // OS X does not have clock_gettime, use clock_get_time

#include <mach/clock.h>

/* Set a timestamp value to the current time. */
void blosc_set_timestamp(blosc_timestamp_t* timestamp) {
  clock_serv_t cclock;
  mach_timespec_t mts;
  host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
  clock_get_time(cclock, &mts);
  mach_port_deallocate(mach_task_self(), cclock);
  timestamp->tv_sec = mts.tv_sec;
  timestamp->tv_nsec = mts.tv_nsec;
}

#else

/* Set a timestamp value to the current time. */
void blosc_set_timestamp(blosc_timestamp_t* timestamp) {
  clock_gettime(CLOCK_MONOTONIC, timestamp);
}

#endif

/* Given two timestamp values, return the difference in nanoseconds. */
double blosc_elapsed_nsecs(blosc_timestamp_t start_time,
                           blosc_timestamp_t end_time) {
  return (1e9 * (double)(end_time.tv_sec - start_time.tv_sec)) +
          (double)(end_time.tv_nsec - start_time.tv_nsec);
}

#endif

/* Given two timeval stamps, return the difference in seconds */
double blosc_elapsed_secs(blosc_timestamp_t last, blosc_timestamp_t current) {
  return 1e-9 * blosc_elapsed_nsecs(last, current);
}
