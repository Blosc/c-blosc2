/*
  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Benchmark showing Blosc filter from C code.

  To compile this program:

  $ gcc -O3 delta_schunk.c -o delta_schunk -lblosc2

*/

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <blosc2.h>

#define KB  1024
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE (50 * 1000)
#define NCHUNKS 100
// Setting NTHREADS > 1 increases the likelihood of a crash.  See #112.
#define NTHREADS 1


int main(void) {
  int32_t *data, *data_dest;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk *schunk;
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  int64_t nbytes, cbytes;
  int nchunk;
  int nchunks = 0;
  blosc_timestamp_t last, current;
  double totaltime;
  float totalsize = (float)(isize * NCHUNKS);

  data = malloc(CHUNKSIZE * sizeof(int32_t));
  data_dest = malloc(CHUNKSIZE * sizeof(int32_t));
  for (int i = 0; i < CHUNKSIZE; i++) {
    data[i] = i;
  }

  printf("Blosc version info: %s (%s)\n", BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  /* Initialize the Blosc compressor */
  blosc_init();

  /* Create a super-chunk container */
  cparams.filters[0] = BLOSC_DELTA;
  //cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_BITSHUFFLE;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 1;
  cparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams};
  schunk = blosc2_schunk_new(&storage);

  /* Append chunks (the first will be taken as reference for delta) */
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
  }
  blosc_set_timestamp(&current);
  totaltime = blosc_elapsed_secs(last, current);
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
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, data_dest, isize);
    if (dsize < 0) {
      printf("Decompression error.  Error code: %d\n", dsize);
      return dsize;
    }
    assert (dsize == (int)isize);
  }
  blosc_set_timestamp(&current);
  totaltime = blosc_elapsed_secs(last, current);
  totalsize = (float)(isize * nchunks);
  printf("[Decompr] Elapsed time:\t %6.3f s.  Processed data: %.3f GB (%.3f GB/s)\n",
         totaltime, totalsize / GB, totalsize / (GB * totaltime));

  printf("Decompression successful!\n");

  for (int i = 0; i < CHUNKSIZE; i++) {
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
  blosc2_schunk_free(schunk);
  /* Destroy the Blosc environment */
  blosc_destroy();

  return 0;
}
