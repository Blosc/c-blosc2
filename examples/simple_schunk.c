/*
  Copyright (C) 2015  Francesc Alted
  http://blosc.org
  License: MIT (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  $ gcc simple_schunk.c -o schunk -lblosc

  To run:

  $ ./schunk
  Blosc version info: 2.0.0a1 ($Date:: 2015-07-30 #$)
  Compression: 4000000 -> 158788 (25.2x)
  destsize: 4000000
  Decompression succesful!
  Succesful roundtrip!

*/

#include <stdio.h>
#include <assert.h>
#include <blosc.h>

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE (200 * 1000)
#define NCHUNKS 500
#define NTHREADS 4

int main() {
  static float data[CHUNKSIZE];
  static float data_dest[CHUNKSIZE];
  size_t isize = CHUNKSIZE * sizeof(float);
  int dsize = 0;
  int64_t nbytes, cbytes;
  blosc2_cparams cparams = BLOSC_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;
  int i, nchunk;
  size_t nchunks;
  blosc_timestamp_t last, current;
  double ttotal;

  for (i = 0; i < CHUNKSIZE; i++) {
    data[i] = i;
  }

  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  /* Create a super-chunk container */
  cparams.typesize = 8;
  cparams.filters[0] = BLOSC_SHUFFLE;
  cparams.filters_meta[0] = 2;  // A number larger than 0 will execute additional shuffles
  cparams.compcode = BLOSC_LZ4;
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  schunk = blosc2_new_schunk(cparams, dparams);

  blosc_set_timestamp(&last);
  for (nchunk = 1; nchunk <= NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      data[i] = i * nchunk;
    }
    nchunks = blosc2_append_buffer(schunk, isize, data);
    assert(nchunks == nchunk);
  }
  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Compression ratio: %.1f MB -> %.1f MB (%.1fx)\n",
         nbytes / MB, cbytes / MB, (1. * nbytes) / cbytes);
  printf("Compression time: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));

  /* Retrieve and decompress the chunks (0-based count) */
  blosc_set_timestamp(&last);
  for (nchunk = NCHUNKS-1; nchunk >= 0; nchunk--) {
    dsize = blosc2_decompress_chunk(schunk, (size_t)nchunk,
                                    (void *)data_dest, isize);
  }
  if (dsize < 0) {
    printf("Decompression error.  Error code: %d\n", dsize);
    return dsize;
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Decompression time: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));

  /* Check integrity of the first chunk */
  for (i = 0; i < CHUNKSIZE; i++) {
    if (data_dest[i] != (float)i) {
      printf("Decompressed data differs from original %f, %f!\n",
             (float)i, data_dest[i]);
      return -1;
    }
  }

  printf("Successful roundtrip!\n");

  /* Free resources */
  /* Destroy the super-chunk */
  blosc2_free_schunk(schunk);

  return 0;
}
