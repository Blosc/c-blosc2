/*
  Copyright (c) 2024  The Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

*/

#include <stdio.h>
#include "blosc2.h"
#include "b2nd.h"
#include "blosc2/filters-registry.h"


#define KB  1024
#define MB  (1024*KB)
#define GB  (1024*MB)

#define NCHUNKS 1
#define NDIM 3
#define XSIZE (32 * NCHUNKS)
#define YSIZE 100
#define ZSIZE 32
#define CHUNKSIZE (XSIZE * YSIZE * ZSIZE)
#define NTHREADS 8


// For 16-bit integers
void fill_buffer16(int16_t *buffer) {
  for (int32_t i = 0; i < XSIZE; i++) {
    for (int32_t j = 0; j < YSIZE; j++) {
      for (int32_t k = 0; k < ZSIZE; k++) {
        buffer[i * YSIZE * ZSIZE + j * ZSIZE + k] = (int16_t) (i * YSIZE * ZSIZE + j * ZSIZE + k);
      }
    }
  }
}

int main16(void) {
  int32_t isize = NCHUNKS * CHUNKSIZE * sizeof(int16_t);
  int64_t nbytes, cbytes;
  blosc_timestamp_t last, current;
  double totaltime;
  float totalsize = (float)isize;
  int16_t *data_buffer = malloc(isize);
  int16_t *rec_buffer = malloc(isize);

  /* Create a super-chunk container */
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.filters[0] = BLOSC_FILTER_TRANSPOSE;
  cparams.filters_meta[0] = 0;
  cparams.typesize = sizeof(int16_t);
  // Good codec params for this dataset
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .contiguous=true};

  int64_t shape[] = {XSIZE, YSIZE, ZSIZE};
  int32_t chunkshape[] = {XSIZE / NCHUNKS , YSIZE, ZSIZE};
  int32_t blockshape[] = {XSIZE / NCHUNKS / 2, YSIZE / 8, ZSIZE / 2};

  b2nd_context_t *ctx = b2nd_create_ctx(
      &storage, NDIM, shape, chunkshape, blockshape, NULL, 0,
      NULL, 0);

  // Fill the data buffer
  fill_buffer16(data_buffer);

  // Fill a b2nd array with data
  blosc_set_timestamp(&last);
  b2nd_array_t *arr;
  BLOSC_ERROR(b2nd_from_cbuffer(ctx, &arr, data_buffer, isize));
  blosc2_schunk *schunk = arr->sc;
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

  /* Retrieve and decompress the data */
  blosc_set_timestamp(&last);
  BLOSC_ERROR(b2nd_to_cbuffer(arr, (void *)rec_buffer, isize) < 0);
  blosc_set_timestamp(&current);
  totaltime = blosc_elapsed_secs(last, current);
  printf("[Decompr] Elapsed time:\t %6.3f s."
         "  Processed data: %.3f GB (%.3f GB/s)\n",
         totaltime, totalsize / GB, totalsize / (GB * totaltime));

  /* Check that all the values have had a good roundtrip */
  for (int i = 0; i < isize; i++) {
    // Check for precision
    if ((data_buffer[i] != rec_buffer[i])) {
      printf("Values are not equal: ");
      printf("%d - %d: %d, (nelem: %d)\n",
             data_buffer[i], rec_buffer[i],
             (data_buffer[i] - rec_buffer[i]), i);
      return -1;
    }
  }
  printf("All data did a good roundtrip!\n");

  /* Free resources */
  free(data_buffer);
  free(rec_buffer);
  /* Destroy the b2nd array */
  b2nd_free(arr);

  return isize;
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
