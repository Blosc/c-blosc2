/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  $ gcc get_set_slice.c -o get_set_slice -lblosc2

  To run:

  $ ./get_set_slice
  Blosc version info: 2.3.2.dev ($Date:: 2022-08-24 #$)
  Compression ratio: 381.5 MB -> 3.1 MB (125.1x)
  Compression time: 2.84 s, 134.5 MB/s
  set_slice_buffer time: 0.0279 s, 13692.3 MB/s
  get_slice_buffer time: 0.00926 s, 41193.4 MB/s
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
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
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

  /* Set slice and get same slice */
  int64_t start = CHUNKSIZE + 3;
  int64_t stop = CHUNKSIZE * 2 + 7;
  int32_t *buffer = malloc((stop - start) * schunk->typesize);
  for (i = 0; i < (stop - start); ++i) {
    buffer[i] = i + NCHUNKS * CHUNKSIZE;
  }
  blosc_set_timestamp(&last);
  int rc = blosc2_schunk_set_slice_buffer(schunk, start, stop, buffer);
  if (rc < 0) {
    printf("ERROR: %d, cannot set slice correctly.\n", rc);
    return rc;
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("set_slice_buffer time: %.3g s, %.1f MB/s\n",
         ttotal, (double)nbytes / (ttotal * MB));

  int32_t *res = malloc((stop - start) * schunk->typesize);
  blosc_set_timestamp(&last);
  rc = blosc2_schunk_get_slice_buffer(schunk, start, stop, res);
  if (rc < 0) {
    printf("ERROR: %d, cannot get slice correctly.\n", rc);
    return rc;
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("get_slice_buffer time: %.3g s, %.1f MB/s\n",
         ttotal, (double)nbytes / (ttotal * MB));

  for (i = 0; i < (stop - start); ++i) {
    if(buffer[i] != res[i]) {
      printf("Bad roundtrip\n");
      return -1;
    }
  }

  printf("Successful roundtrip data <-> schunk !\n");

  /* Free resources */
  /* Destroy the super-chunk */
  blosc2_schunk_free(schunk);
  free(buffer);
  free(res);

  blosc2_destroy();

  return 0;
}
