/*
  Copyright (C) 2015  Francesc Alted
  http://blosc.org
  License: MIT (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  $ gcc -O delta_schunk.c -o delta_schunk -lblosc

  To run:

  $ ./delta_schunk
  Blosc version info: 2.0.0a2 ($Date:: 2015-12-17 #$)
  Compression super-chunk: 200000000 -> 14081961 (14.2x)
  Decompression successful!
  Successful roundtrip!

*/

#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "blosc.h"

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE 200 * 1000
//#define NCHUNKS 500
#define NCHUNKS 1

/* The type of timestamp used on this system. */
#define blosc_timestamp_t struct timespec

/* Set a timestamp value to the current time. */
void blosc_set_timestamp(blosc_timestamp_t* timestamp) {
  clock_gettime(CLOCK_MONOTONIC, timestamp);
}

/* Given two timestamp values, return the difference in microseconds. */
double blosc_elapsed_usecs(blosc_timestamp_t start_time, blosc_timestamp_t end_time) {
  return (1e6 * (end_time.tv_sec - start_time.tv_sec))
         + (1e-3 * (end_time.tv_nsec - start_time.tv_nsec));
}

/* Given two timeval stamps, return the difference in seconds */
double getseconds(blosc_timestamp_t last, blosc_timestamp_t current) {
  return 1e-6 * blosc_elapsed_usecs(last, current);
}


int main() {
  static int64_t data[CHUNKSIZE];
  static int64_t data_dest[CHUNKSIZE];
  const int isize = CHUNKSIZE * sizeof(int64_t);
  int dsize;
  int32_t nbytes, cbytes;
  blosc2_sparams sparams = BLOSC_SPARAMS_DEFAULTS;
  blosc2_sheader* sheader;
  int i, nchunk, nchunks;
  blosc_timestamp_t last, current;
  double ttotal;

  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  /* Initialize the Blosc compressor */
  blosc_init();
  blosc_set_nthreads(4);

  /* Create a super-chunk container */
  //sparams.filters[0] = BLOSC_DELTA;
  //sparams.filters[1] = BLOSC_SHUFFLE;
  sparams.compressor = BLOSC_BLOSCLZ;
  sparams.clevel = 1;
  sheader = blosc2_new_schunk(&sparams);

  blosc_set_timestamp(&last);
  for (nchunk = 1; nchunk <= NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      data[i] = i * nchunk;
    }
    nchunks = blosc2_append_buffer(sheader, sizeof(int64_t), isize, data);
    assert(nchunks == nchunk);
  }
  /* Gather some info */
  nbytes = sheader->nbytes;
  cbytes = sheader->cbytes;
  blosc_set_timestamp(&current);
  ttotal = (double)getseconds(last, current);
  printf("Compression ratio: %.1f MB -> %.1f MB (%.1fx)\n",
         nbytes / MB, cbytes / MB, (1. * nbytes) / cbytes);
  printf("Compression time: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));

  /* Retrieve and decompress the chunks (0-based count) */
  blosc_set_timestamp(&last);
  for (nchunk = NCHUNKS-1; nchunk >= 0; nchunk--) {
    dsize = blosc2_decompress_chunk(sheader, nchunk, (void *)data_dest, isize);
  }
  if (dsize < 0) {
    printf("Decompression error.  Error code: %d\n", dsize);
    return dsize;
  }
  blosc_set_timestamp(&current);
  ttotal = (double)getseconds(last, current);
  printf("Decompression time: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));

  /* Check integrity of the first chunk */
  for (i = 0; i < CHUNKSIZE; i++) {
    if (data_dest[i] != (uint64_t)i) {
      printf("Decompressed data differs from original %d, %jd!\n",
             i, data_dest[i]);
      return -1;
    }
  }

  printf("Successful roundtrip!\n");

  /* Free resources */
  blosc2_destroy_schunk(sheader);
  blosc_destroy();

  return 0;
}
