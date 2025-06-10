/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Benchmark showing Blosc TRUNC_PREC filter from C code.

  To compile this program:

  $ gcc -O3 trunc_prec_schunk.c -o trunc_prec_schunk -lblosc2

*/

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include "blosc2.h"


#define KB  1024
#define MB  (1024*KB)
#define GB  (1024*MB)

#define NCHUNKS 200
#define CHUNKSIZE (500 * 1000)
#define NTHREADS 4


void fill_buffer(double *buffer, int nchunk) {
  double incx = 10. / (NCHUNKS * CHUNKSIZE);

  for (int i = 0; i < CHUNKSIZE; i++) {
    double x = incx * (nchunk * CHUNKSIZE + i);
    buffer[i] = (x - .25) * (x - 4.45) * (x - 8.95);
    //buffer[i] = x;
  }
}


int main(void) {
  blosc2_schunk *schunk;
  int32_t isize = CHUNKSIZE * sizeof(double);
  int dsize;
  int64_t nbytes, cbytes;
  int nchunk;
  int64_t nchunks = 0;
  blosc_timestamp_t last, current;
  double totaltime;
  float totalsize = (float)(isize * NCHUNKS);
  double *data_buffer = malloc(CHUNKSIZE * sizeof(double));
  double *rec_buffer = malloc(CHUNKSIZE * sizeof(double));

  printf("Blosc version info: %s (%s)\n",
         BLOSC2_VERSION_STRING, BLOSC2_VERSION_DATE);

  /* Initialize the Blosc compressor */
  blosc2_init();

  /* Create a super-chunk container */
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.filters[0] = BLOSC_TRUNC_PREC;
  cparams.filters_meta[0] = 23;  // treat doubles as floats
  cparams.typesize = sizeof(double);
  // DELTA makes compression ratio quite worse in this case
  //cparams.filters[1] = BLOSC_DELTA;
  // BLOSC_BITSHUFFLE is not compressing better and it quite slower here
  //cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_BITSHUFFLE;
  // Good codec params for this dataset
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .contiguous=true};
  schunk = blosc2_schunk_new(&storage);

  /* Append the chunks */
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    fill_buffer(data_buffer, nchunk);
    nchunks = blosc2_schunk_append_buffer(schunk, data_buffer, isize);
  }
  blosc_set_timestamp(&current);
  totaltime = blosc_elapsed_secs(last, current);
  printf("[Compr] Elapsed time:\t %6.3f s."
                 "  Processed data: %.3f GB (%.3f GB/s)\n",
         totaltime, totalsize / GB, totalsize / (GB * totaltime));

  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  printf("Compression super-chunk: %ld -> %ld (%.1fx)\n",
         (long)nbytes, (long)cbytes, (1. * (double)nbytes) / (double)cbytes);

  /* Retrieve and decompress the chunks */
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, rec_buffer, isize);
    if (dsize < 0) {
      printf("Decompression error.  Error code: %d\n", dsize);
      return dsize;
    }
    assert (dsize == (int)isize);
  }
  blosc_set_timestamp(&current);
  totaltime = blosc_elapsed_secs(last, current);
  totalsize = (float)(isize * nchunks);
  printf("[Decompr] Elapsed time:\t %6.3f s."
                 "  Processed data: %.3f GB (%.3f GB/s)\n",
         totaltime, totalsize / GB, totalsize / (GB * totaltime));

  /* Check that all the values are in the precision range */
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) rec_buffer, isize);
    if (dsize < 0) {
      printf("Decompression error.  Error code: %d\n", dsize);
      return dsize;
    }
    assert (dsize == (int)isize);
    fill_buffer(data_buffer, nchunk);
    for (int i = 0; i < CHUNKSIZE; i++) {
      if (fabs(data_buffer[i] - rec_buffer[i]) > 1e-5) {
        printf("Value not in tolerance margin: ");
        printf("%g - %g: %g, (nchunk: %d, nelem: %d)\n",
               data_buffer[i], rec_buffer[i],
               (data_buffer[i] - rec_buffer[i]), nchunk, i);
        return -1;
      }
    }
  }
  printf("All data did a good roundtrip!\n");

  /* Free resources */
  free(data_buffer);
  free(rec_buffer);
  /* Destroy the super-chunk */
  blosc2_schunk_free(schunk);
  /* Destroy the Blosc environment */
  blosc2_destroy();

  return 0;
}
