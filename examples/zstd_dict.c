/*
  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  $ gcc -O zstd_dict.c -o zstd_dict -lblosc2

  To run:

  $ ./zstd_dict
  TODO ...

*/

#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "blosc2.h"

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE (200 * 1000)
#define NCHUNKS 20
#define NTHREADS 4


int main(void) {
  static int64_t data[CHUNKSIZE];
  static int64_t data_dest[CHUNKSIZE];
  const int32_t isize = CHUNKSIZE * sizeof(int64_t);
  int dsize = 0;
  int64_t nbytes, cbytes;
  blosc2_schunk* schunk;
  int i;
  int32_t nchunk;
  int32_t nchunks;
  blosc_timestamp_t last, current;
  double ttotal;

  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  /* Initialize the Blosc compressor */
  blosc_init();

  /* Create a super-chunk container */
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 8;
  //cparams.filters[0] = BLOSC_DELTA;
  cparams.compcode = BLOSC_ZSTD;
  cparams.use_dict = 1;
  //cparams.clevel = 7;
  cparams.blocksize = 1024 * 4;  // a page size
  //cparams.blocksize = 1024 * 32;
  cparams.nthreads = NTHREADS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams};
  schunk = blosc2_schunk_new(&storage);

  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + (int64_t)nchunk * CHUNKSIZE;
    }
    nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    assert(nchunks == nchunk + 1);
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
  for (nchunk = NCHUNKS - 1; nchunk >= 0; nchunk--) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, data_dest, isize);
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
    if (data_dest[i] != (int64_t)i) {
      printf("Decompressed data differs from original %d, %ld!\n",
             i, (long)data_dest[i]);
      return -1;
    }
  }

  printf("Successful roundtrip!\n");

  /* Free resources */
  blosc2_schunk_free(schunk);
  blosc_destroy();

  return 0;
}
