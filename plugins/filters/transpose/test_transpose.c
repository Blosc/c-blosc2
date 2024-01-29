/*
  Copyright (c) 2024  The Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

*/

#include <stdio.h>
#include <assert.h>
#include "blosc2.h"
#include "blosc2/filters-registry.h"


#define KB  1024
#define MB  (1024*KB)
#define GB  (1024*MB)

#define NCHUNKS 20
#define CHUNKSIZE (500 * 1000)
#define NTHREADS 8


// For 16-bit integers
void fill_buffer16(int16_t *buffer) {
  for (int32_t i = 0; i < CHUNKSIZE; i++) {
    buffer[i] = (int16_t)i;
  }
}

int main16(void) {
  blosc2_schunk *schunk;
  int32_t isize = CHUNKSIZE * sizeof(int16_t);
  int dsize;
  int64_t nbytes, cbytes;
  int nchunk;
  int64_t nchunks = 0;
  blosc_timestamp_t last, current;
  double totaltime;
  float totalsize = (float)(isize * NCHUNKS);
  int16_t *data_buffer = malloc(CHUNKSIZE * sizeof(int16_t));
  int16_t *rec_buffer = malloc(CHUNKSIZE * sizeof(int16_t));

  /* Create a super-chunk container */
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.filters[0] = BLOSC_FILTER_INT_TRUNC;
  int PRECISION_BITS = 10;  // remove 10 bits of precision
  cparams.filters_meta[0] = -PRECISION_BITS;
  cparams.typesize = sizeof(int16_t);
  // Good codec params for this dataset
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .contiguous=true};
  schunk = blosc2_schunk_new(&storage);

  /* Append the chunks */
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    fill_buffer16(data_buffer);
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
    fill_buffer16(data_buffer);
    for (int i = 0; i < CHUNKSIZE; i++) {
      // Check for precision
      if ((data_buffer[i] - rec_buffer[i]) > (1LL << PRECISION_BITS)) {
        printf("Value not in tolerance margin: ");
        printf("%d - %d: %d, (nchunk: %d, nelem: %d)\n",
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

  return isize * NCHUNKS;
}


int main(void) {
  int result;
  blosc2_init();

  result = main16();
  printf("main16: roundtrip for %d bytes successful \n \n", result);
  if (result < 0) {
    return result;
  }

  blosc2_destroy();
  return BLOSC2_ERROR_SUCCESS;
}
