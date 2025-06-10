/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  $ gcc schunk_simple.c -o schunk_simple -lblosc2

  To run:

  $ ./schunk_simple
  Blosc version info: 2.0.0-beta.1 ($Date:: 2019-08-09 #$)
  Compression ratio: 381.5 MB -> 12.2 MB (31.2x)
  Compression time: 0.119 s, 3192.9 MB/s
  Decompression time: 0.035 s, 10888.3 MB/s
  Successful roundtrip data <-> schunk !

*/

#include <stdio.h>
#include <blosc2.h>

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE (1000 * 1000)
#define NCHUNKS 100
#define NTHREADS 4

int main(void) {
  blosc2_init();

  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  int64_t nbytes, cbytes;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;
  int i, nchunk;
  blosc_timestamp_t last, current;
  double ttotal;

  printf("Blosc version info: %s (%s)\n", blosc2_get_version_string(), BLOSC2_VERSION_DATE);

  /* Create a super-chunk container */
  cparams.typesize = sizeof(int32_t);
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams};
  schunk = blosc2_schunk_new(&storage);

  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      data[i] = i * nchunk;
    }
    int64_t nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    if (nchunks != nchunk + 1) {
        printf("Unexpected nchunks!");
        return -1;
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
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, data_dest, isize);
    if (dsize < 0) {
      printf("Decompression error.  Error code: %d\n", dsize);
      return dsize;
    }
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Decompression time: %.3g s, %.1f MB/s\n",
         ttotal, (double)nbytes / (ttotal * MB));

  /* Check integrity of the second chunk (made of non-zeros) */
  blosc2_schunk_decompress_chunk(schunk, 1, data_dest, isize);
  for (i = 0; i < CHUNKSIZE; i++) {
    if (data_dest[i] != i) {
      printf("Decompressed data differs from original %d, %d!\n",
             i, data_dest[i]);
      return -1;
    }
  }

  printf("Successful roundtrip data <-> schunk !\n");

  /* Free resources */
  /* Destroy the super-chunk */
  blosc2_schunk_free(schunk);

  blosc2_destroy();

  return 0;
}
