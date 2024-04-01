/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating use of the delta filter from C code.

  To compile this program:

  $ gcc -O delta_schunk_ex.c -o delta_schunk_ex -lblosc2

  To run:

  $ ./delta_schunk_ex
  Blosc version info: 2.0.0a4.dev ($Date:: 2016-08-04 #$)
  Compression ratio: 762.9 MB -> 7.6 MB (100.7x)
  Compression time: 0.222 s, 3437.4 MB/s
  Decompression time: 0.162 s, 4714.4 MB/s
  Successful roundtrip!

*/

#include <stdio.h>
#include <assert.h>
#include "blosc2.h"

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE (200 * 1000)
#define NCHUNKS 500
#define NTHREADS 4


int main(void) {
  static int64_t data[CHUNKSIZE];
  static int64_t data_dest[CHUNKSIZE];
  const int32_t isize = CHUNKSIZE * sizeof(int64_t);
  int dsize = 0;
  int64_t nbytes, cbytes;
  blosc2_schunk* schunk;
  int i;
  int nchunk;
  int64_t nchunks;
  blosc_timestamp_t last, current;
  double ttotal;

  printf("Blosc version info: %s (%s)\n",
         BLOSC2_VERSION_STRING, BLOSC2_VERSION_DATE);

  /* Initialize the Blosc compressor */
  blosc2_init();

  /* Create a super-chunk container */
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 8;
  cparams.filters[0] = BLOSC_DELTA;
  cparams.nthreads = NTHREADS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams};
  schunk = blosc2_schunk_new(&storage);

  blosc_set_timestamp(&last);
  for (nchunk = 1; nchunk <= NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      data[i] = i * (int64_t)nchunk;
    }
    nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    if (nchunks != nchunk) {
      printf("Unexpected nchunks!");
      return nchunks;
    }
  }
  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Compression ratio: %.1f MB -> %.1f MB (%.1fx)\n",
         (double)nbytes / MB, (double)cbytes / MB, (1. * (double)nbytes) / (double)cbytes);
  printf("Compression time: %.3g s, %.1f MB/s\n",
         ttotal, (double)nbytes / (ttotal * MB));

  /* Retrieve and decompress the chunks (0-based count) */
  blosc_set_timestamp(&last);
  for (nchunk = NCHUNKS-1; nchunk >= 0; nchunk--) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk,
                                           (void *) data_dest, isize);
  }
  if (dsize < 0) {
    printf("Decompression error.  Error code: %d\n", dsize);
    return dsize;
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Decompression time: %.3g s, %.1f MB/s\n",
         ttotal, (double)nbytes / (ttotal * MB));

  /* Check integrity of the first chunk */
  for (i = 0; i < CHUNKSIZE; i++) {
    if (data_dest[i] != i) {
      printf("Decompressed data differs from original %d, %ld!\n",
             i, (long)data_dest[i]);
      return -1;
    }
  }

  printf("Successful roundtrip!\n");

  /* Free resources */
  blosc2_schunk_free(schunk);
  blosc2_destroy();

  return 0;
}
