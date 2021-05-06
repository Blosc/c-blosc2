/*
  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating frames going bigger than 2 GB.

  To compile this program:

  $ gcc -O frame_big.c -o frame_big -lblosc2

  To run:

  $ ./frame_big
  Blosc version info: 2.0.0-beta.4.dev ($Date:: 2019-09-02 #$)
  Compression ratio: 4577.6 MB -> 169.8 MB (27.0x)
  Time for append data to a schunk backed by a fileframe: 2.61 s, 1750.8 MB/s
  Successful roundtrip data <-> schunk (frame-backed) !

 */

#include <stdio.h>
#include <assert.h>
#include <blosc2.h>

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE (1000 * 1000)
#define NCHUNKS 1200   // > 4 GB int32 frame
#define NTHREADS 4


int main(void) {
  blosc_init();

  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  blosc_timestamp_t last, current;

  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  // Compression and decompression parameters
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = NTHREADS;

  /* Create a new super-chunk backed by a fileframe */
  char* urlpath = "frame_big.b2frame";
  blosc2_storage storage = {.contiguous=true, .urlpath=urlpath,
                            .cparams=&cparams, .dparams=&dparams};
  remove(urlpath);
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);

  blosc_set_timestamp(&last);
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i * nchunk;
    }
    int nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    assert(nchunks == nchunk + 1);
  }
  /* Gather some info */
  int64_t nbytes = schunk->nbytes;
  int64_t cbytes = schunk->cbytes;
  blosc_set_timestamp(&current);
  double ttotal = blosc_elapsed_secs(last, current);
  printf("Compression ratio: %.1f MB -> %.1f MB (%.1fx)\n",
         nbytes / MB, cbytes / MB, (1. * nbytes) / cbytes);
  printf("Time for append data to a schunk backed by a fileframe: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));

  /* Retrieve and decompress the chunks from the super-chunks and compare values */
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    int32_t dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, data_dest, isize);
    if (dsize < 0) {
      printf("Decompression error in schunk.  Error code: %d\n", dsize);
      return dsize;
    }
    /* Check integrity of the last chunk */
    for (int i = 0; i < CHUNKSIZE; i++) {
      assert (data_dest[i] == i * nchunk);
    }
  }

  printf("Successful roundtrip data <-> schunk (frame-backed) !\n");

  /* Free resources */
  blosc2_schunk_free(schunk);

  blosc_destroy();

  return 0;
}
