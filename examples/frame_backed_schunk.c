/*
  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  $ gcc -O frame_backed_schunk.c -o frame_backed_schunk -lblosc2

  To run:

  $ ./frame_backed_schunk
  Blosc version info: 2.0.0a6.dev ($Date:: 2018-05-18 #$)
  Compression ratio: 381.5 MB -> 12.2 MB (31.2x)
  Time for append data to a schunk backed by an in-memory frame: 0.0892 s, 4278.1 MB/s
  Compression ratio: 381.5 MB -> 12.2 MB (31.2x)
  Time for append data to a schunk backed by a fileframe: 0.107 s, 3556.3 MB/s
  Successful roundtrip data <-> schunk (frame-backed) !

 */

#include <stdio.h>
#include <assert.h>
#include <blosc2.h>

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE (1000 * 1000)
#define NCHUNKS 100
#define NTHREADS 4


int main(void) {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest1[CHUNKSIZE];
  static int32_t data_dest2[CHUNKSIZE];
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int64_t nbytes, cbytes;
  int i, nchunk;
  int nchunks;
  blosc_timestamp_t last, current;
  double ttotal;

  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  // Compression and decompression parameters
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = NTHREADS;

  /* Create a new super-chunk backed by an in-memory frame */
  blosc2_storage storage = {.contiguous=true, .cparams=&cparams, .dparams=&dparams};
  blosc2_schunk* schunk1 = blosc2_schunk_new(&storage);

  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      data[i] = i * nchunk;
    }
    nchunks = blosc2_schunk_append_buffer(schunk1, data, isize);
    assert(nchunks == nchunk + 1);
  }
  /* Gather some info */
  nbytes = schunk1->nbytes;
  cbytes = schunk1->cbytes;
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Compression ratio: %.1f MB -> %.1f MB (%.1fx)\n",
         nbytes / MB, cbytes / MB, (1. * nbytes) / cbytes);
  printf("Time for append data to a schunk backed by an in-memory frame: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));

  /* Create a new super-chunk backed by an in-memory frame */
  storage = (blosc2_storage){.contiguous=true, .cparams=&cparams, .dparams=&dparams};
  blosc2_schunk* schunk2 = blosc2_schunk_new(&storage);

  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      data[i] = i * nchunk;
    }
    nchunks = blosc2_schunk_append_buffer(schunk2, data, isize);
    assert(nchunks == nchunk + 1);
  }
  /* Gather some info */
  nbytes = schunk2->nbytes;
  cbytes = schunk2->cbytes;
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Compression ratio: %.1f MB -> %.1f MB (%.1fx)\n",
         nbytes / MB, cbytes / MB, (1. * nbytes) / cbytes);
  printf("Time for append data to a schunk backed by a fileframe: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));

  /* Retrieve and decompress the chunks from the super-chunks and compare values */
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    int32_t dsize1 = blosc2_schunk_decompress_chunk(schunk1, nchunk, data_dest1, isize);
    if (dsize1 < 0) {
      printf("Decompression error in schunk1.  Error code: %d\n", dsize1);
      return dsize1;
    }
    int32_t dsize2 = blosc2_schunk_decompress_chunk(schunk2, nchunk, data_dest2, isize);
    if (dsize2 < 0) {
      printf("Decompression error in schunk2.  Error code: %d\n", dsize2);
      return dsize2;
    }
    assert(dsize1 == dsize2);
    /* Check integrity of the last chunk */
    for (i = 0; i < CHUNKSIZE; i++) {
      assert (data_dest1[i] == i * nchunk);
      assert (data_dest2[i] == i * nchunk);
    }
  }

  printf("Successful roundtrip data <-> schunk (frame-backed) !\n");

  /* Free resources */
  blosc2_schunk_free(schunk1);
  blosc2_schunk_free(schunk2);

  return 0;
}
