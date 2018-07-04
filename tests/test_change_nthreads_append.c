/* Copyright (C) 2017 Francesc Alted
 * http://blosc.org
 * License: BSD (see LICENSE.txt)
 *
*/

#include <stdio.h>
#include "test_common.h"
#include "../blosc/context.h"


#define CHUNKSIZE (200 * 1000)
#define NCHUNKS 500
//#define NCHUNKS 5
#define NTHREADS 4


int main() {
  static int64_t data[CHUNKSIZE];
  static int64_t data_dest[CHUNKSIZE];
  const size_t isize = CHUNKSIZE * sizeof(int64_t);
  int dsize = 0;
  int64_t nbytes, cbytes;
  blosc2_cparams cparams = BLOSC_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;
  int i;
  int nchunk;
  size_t nchunks;
  blosc_timestamp_t last, current;
  double ttotal;

  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

/* Initialize the Blosc compressor */
  blosc_init();

/* Create a super-chunk container */
  cparams.typesize = 8;
  cparams.filters[0] = BLOSC_DELTA;
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  schunk = blosc2_make_schunk(cparams, dparams);

  struct blosc2_context_s * cctx = schunk->cctx;
  blosc_set_timestamp(&last);
  for (nchunk = 1; nchunk <= NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      data[i] = i * (int64_t)nchunk;
    }
    // Alternate between 1 and NTHREADS
    cctx->new_nthreads = nchunk % NTHREADS + 1;
    nchunks = blosc2_append_buffer(schunk, isize, data);
    mu_assert("ERROR: nchunk is not correct", nchunks == nchunk);
  }
  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Compression ratio: %.1f MB -> %.1f MB (%.1fx)\n",
         (double)nbytes / MB, (double)cbytes / MB, (double)nbytes / cbytes);
  printf("Compression time: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));

  /* Retrieve and decompress the chunks (0-based count) */
  struct blosc2_context_s * dctx = schunk->dctx;
  blosc_set_timestamp(&last);
  for (nchunk = NCHUNKS-1; nchunk >= 0; nchunk--) {
    // Alternate between 1 and NTHREADS
    dctx->new_nthreads = nchunk % NTHREADS + 1;
    dsize = blosc2_decompress_chunk(schunk, (size_t)nchunk,
                                    (void *)data_dest, isize);
  }
  if (dsize < 0) {
    printf("Decompression error. Error code: %d\n", dsize);
    return dsize;
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Decompression time: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));

/* Check integrity of the first chunk */
  for (i = 0; i < CHUNKSIZE; i++) {
    if (data_dest[i] != (uint64_t)i) {
      printf("Decompressed data differs from original %d, %zd!\n",
             i, data_dest[i]);
      return -1;
    }
  }

  printf("Successful roundtrip!\n");

/* Free resources */
  blosc2_destroy_schunk(schunk);

  return 0;
}
