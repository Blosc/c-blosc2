/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating use of the Blosc2 postfilters in schunks.

*/

#include <stdio.h>
#include <blosc2.h>

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE (20 * 1000)
#define NCHUNKS 1000
#define NTHREADS 8


typedef struct {
  int32_t mult;
  int32_t add;
} my_postparams;


int postfilter_func(blosc2_postfilter_params *postparams) {
  int nelems = postparams->size / postparams->typesize;
  int32_t *in = ((int32_t *)(postparams->input));
  int32_t *out = ((int32_t *)(postparams->output));
  my_postparams *user_data = postparams->user_data;
  for (int i = 0; i < nelems; i++) {
    out[i] = in[i] * user_data->mult + user_data->add ;
  }
  return 0;
}


int main(void) {

  blosc2_init();

  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int i, nchunk;
  int64_t nchunks;
  blosc_timestamp_t last;

  printf("Blosc version info: %s (%s)\n",
         BLOSC2_VERSION_STRING, BLOSC2_VERSION_DATE);

  /* Create a super-chunk container */
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_LZ4HC;
  cparams.clevel = 1;
  cparams.nthreads = NTHREADS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = NTHREADS;
  // Set some postfilter parameters and function
  dparams.postfilter = (blosc2_postfilter_fn)postfilter_func;
  // We need to zero the contents of the postparams
  blosc2_postfilter_params postparams = {0};
  // Additional user params
  my_postparams user_data = {2, 1};
  postparams.user_data = (void*)&user_data;
  dparams.postparams = &postparams;

  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);

  // Add some data
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk * CHUNKSIZE;
    }
    nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    if (nchunks != nchunk + 1) {
      printf("blosc2_schunk_append_buffer is not working correctly");
      return BLOSC2_ERROR_FAILURE;
    }
  }

  /* Retrieve and decompress the chunks from the super-chunks and compare values */
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    int32_t dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, data_dest, isize);
    if (dsize < 0) {
      printf("Decompression error in schunk.  Error code: %d\n", dsize);
      return dsize;
    }
    /* Check integrity of this chunk */
    for (i = 0; i < CHUNKSIZE; i++) {
      if (data_dest[i] != (i + nchunk * CHUNKSIZE) * user_data.mult + user_data.add) {
        printf("data mismatch!");
        return BLOSC2_ERROR_FAILURE;
      }
    }
  }
  printf("Postfilter is working correctly!\n");

  /* Free resources */
  blosc2_schunk_free(schunk);
  blosc2_destroy();

  return 0;
}
