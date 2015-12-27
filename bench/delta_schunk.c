/*
  Copyright (C) 2015  Francesc Alted
  http://blosc.org
  License: MIT (see LICENSE.txt)

  Benchmark showing Blosc filter from C code.

  To compile this program:

  $ gcc delta_schunk.c -o delta_schunk -lblosc

*/

#include <stdio.h>
#include <assert.h>

#if defined(_WIN32)
/* For QueryPerformanceCounter(), etc. */
  #include <windows.h>
#elif defined(__MACH__)
  #include <mach/clock.h>
  #include <mach/mach.h>
  #include <time.h>
  #include <sys/time.h>
#elif defined(__unix__)
  #include <unistd.h>
  #if defined(__linux__)
    #include <time.h>
  #else
    #include <sys/time.h>
  #endif
#else
  #error Unable to detect platform.
#endif

#include "../blosc/blosc.h"

#define KB  1024
#define MB  (1024*KB)
#define GB  (1024*MB)


/* System-specific high-precision timing functions. */
#if defined(_WIN32)

/* The type of timestamp used on this system. */
#define blosc_timestamp_t LARGE_INTEGER

/* Set a timestamp value to the current time. */
void blosc_set_timestamp(blosc_timestamp_t* timestamp) {
  /* Ignore the return value, assume the call always succeeds. */
  QueryPerformanceCounter(timestamp);
}

/* Given two timestamp values, return the difference in microseconds. */
double blosc_elapsed_usecs(blosc_timestamp_t start_time, blosc_timestamp_t end_time) {
  LARGE_INTEGER CounterFreq;
  QueryPerformanceFrequency(&CounterFreq);

  return (double)(end_time.QuadPart - start_time.QuadPart) / ((double)CounterFreq.QuadPart / 1e6);
}

#else

/* The type of timestamp used on this system. */
#define blosc_timestamp_t struct timespec

/* Set a timestamp value to the current time. */
void blosc_set_timestamp(blosc_timestamp_t* timestamp) {
#ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
  clock_serv_t cclock;
  mach_timespec_t mts;
  host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
  clock_get_time(cclock, &mts);
  mach_port_deallocate(mach_task_self(), cclock);
  timestamp->tv_sec = mts.tv_sec;
  timestamp->tv_nsec = mts.tv_nsec;
#else
  clock_gettime(CLOCK_MONOTONIC, timestamp);
#endif
}

/* Given two timestamp values, return the difference in microseconds. */
double blosc_elapsed_usecs(blosc_timestamp_t start_time, blosc_timestamp_t end_time) {
  return (1e6 * (end_time.tv_sec - start_time.tv_sec))
      + (1e-3 * (end_time.tv_nsec - start_time.tv_nsec));
}

#endif

/* Given two timeval stamps, return the difference in seconds */
double getseconds(blosc_timestamp_t last, blosc_timestamp_t current) {
  return 1e-6 * blosc_elapsed_usecs(last, current);
}

/* Given two timeval stamps, return the time per chunk in usec */
double get_usec_chunk(blosc_timestamp_t last, blosc_timestamp_t current, int niter, size_t nchunks) {
  double elapsed_usecs = (double)blosc_elapsed_usecs(last, current);
  return elapsed_usecs / (double)(niter * nchunks);
}


#define CHUNKSIZE 5 * 100 * 1000
#define NCHUNKS 100
#define NTHREADS 4


int main() {
  int32_t *data, *data_dest;
  static schunk_params sc_params;
  schunk_header* schunk;
  int isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  int64_t nbytes, cbytes;
  int i, nchunk, nchunks;
  blosc_timestamp_t last, current;
  float totaltime;
  float totalsize = isize * NCHUNKS;

  data = malloc(CHUNKSIZE * sizeof(int32_t));
  for (i = 0; i < CHUNKSIZE; i++) {
    data[i] = i;
  }

  printf("Blosc version info: %s (%s)\n", BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  /* Initialize the Blosc compressor */
  blosc_init();

  blosc_set_nthreads(NTHREADS);

  /* Create a super-chunk container */
  sc_params.filters[0] = BLOSC_DELTA;
  sc_params.filters[1] = BLOSC_SHUFFLE;
  sc_params.compressor = BLOSC_BLOSCLZ;
  sc_params.clevel = 5;
  schunk = blosc2_new_schunk(&sc_params);

  /* Append the reference chunk first */
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    nchunks = blosc2_append_buffer(schunk, sizeof(int32_t), isize, data);
  }
  blosc_set_timestamp(&current);
  totaltime = (float)getseconds(last, current);
  printf("[Compr] Elapsed time:\t %6.3f s.  Processed data: %.3f GB\n",
         totaltime, totalsize / GB);

  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  printf("Compression super-chunk: %lld -> %lld (%.1fx)\n",
         nbytes, cbytes, (1. * nbytes) / cbytes);

  /* Retrieve and decompress the chunks */
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    dsize = blosc2_decompress_chunk(schunk, nchunk, (void**)&data_dest);
    if (dsize < 0) {
      printf("Decompression error.  Error code: %d\n", dsize);
      return dsize;
    }
    assert (dsize == isize);
  }
  blosc_set_timestamp(&current);
  totaltime = (float)getseconds(last, current);
  totalsize = isize * nchunks;
  printf("[Decompr] Elapsed time:\t %6.3f s.  Processed data: %.3f GB\n",
         totaltime, totalsize / GB);

  printf("Decompression successful!\n");

  for (i = 0; i < CHUNKSIZE; i++) {
    if (data[i] != data_dest[i]) {
      printf("Decompressed data differs from original %d, %d, %d!\n", i, data[i], data_dest[i]);
      return -1;
    }
  }

  printf("Successful roundtrip!\n");

  /* Free resources */
  free(data);
  free(data_dest);
  /* Destroy the super-chunk */
  blosc2_destroy_schunk(schunk);
  /* Destroy the Blosc environment */
  blosc_destroy();

  return 0;
}
