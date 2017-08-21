/*
  Copyright (C) 2015  Francesc Alted
  http://blosc.org
  License: MIT (see LICENSE.txt)

  Benchmark showing Blosc filter from C code.

  To compile this program:

  $ gcc -O3 delta_schunk.c -o delta_schunk -lblosc

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
#include <blosc.h>

#elif defined(__unix__)
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
  double elapsed_usecs = blosc_elapsed_usecs(last, current);
  return elapsed_usecs / (double)(niter * nchunks);
}


#define CHUNKSIZE 5 * 1000 * 1000
#define NCHUNKS 100
#define NTHREADS 2


int main() {
  int32_t *data, *data_dest;
  blosc2_cparams cparams = BLOSC_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  int64_t nbytes, cbytes;
  int i, nchunk;
  size_t nchunks = 0;
  blosc_timestamp_t last, current;
  float totaltime;
  float totalsize = isize * NCHUNKS;

  data = malloc(CHUNKSIZE * sizeof(int32_t));
  data_dest = malloc(CHUNKSIZE * sizeof(int32_t));
  for (i = 0; i < CHUNKSIZE; i++) {
    data[i] = i;
  }

  printf("Blosc version info: %s (%s)\n", BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  /* Initialize the Blosc compressor */
  blosc_init();

  blosc_set_nthreads(NTHREADS);

  /* Create a super-chunk container */
  cparams.filters[0] = BLOSC_DELTA;
  //sparams.filters[7] = BLOSC_SHUFFLE;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 1;
  schunk = blosc2_new_schunk(cparams, dparams);

  /* Append chunks (the first will be taken as reference for delta) */
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    nchunks = blosc2_append_buffer(schunk, isize, data);
  }
  blosc_set_timestamp(&current);
  totaltime = (float)getseconds(last, current);
  printf("[Compr] Elapsed time:\t %6.3f s.  Processed data: %.3f GB (%.3f GB/s)\n",
         totaltime, totalsize / GB, totalsize / (GB * totaltime));

  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  printf("Compression super-chunk: %ld -> %ld (%.1fx)\n",
         (long)nbytes, (long)cbytes, (1. * nbytes) / cbytes);

  /* Retrieve and decompress the chunks */
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    dsize = blosc2_decompress_chunk(schunk, nchunk, (void*)data_dest,
                                    (int)isize);
    if (dsize < 0) {
      printf("Decompression error.  Error code: %d\n", dsize);
      return dsize;
    }
    assert (dsize == isize);
  }
  blosc_set_timestamp(&current);
  totaltime = (float)getseconds(last, current);
  totalsize = isize * nchunks;
  printf("[Decompr] Elapsed time:\t %6.3f s.  Processed data: %.3f GB (%.3f GB/s)\n",
         totaltime, totalsize / GB, totalsize / (GB * totaltime));

  printf("Decompression successful!\n");

  for (i = 0; i < CHUNKSIZE; i++) {
    if (data[i] != data_dest[i]) {
      printf("Decompressed data differs from original %d, %d, %d!\n",
             i, data[i], data_dest[i]);
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
