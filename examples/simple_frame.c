/*
  Copyright (C) 2018  Francesc Alted
  http://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  $ gcc simple_frame.c -o simple_frame -lblosc

  To run:

  $ ./simple_frame
  Blosc version info: 2.0.0a6.dev ($Date:: 2018-05-18 #$)
  Compression ratio: 381.5 MB -> 9.5 MB (40.2x)
  Compression time: 0.613 s, 622.7 MB/s
  Frame output to simple_frame.b2frame with 9940344 bytes
  Same frame output to simple_frame2.b2frame with no intermediate buffers
  Decompression time: 0.713 s, 535.3 MB/s
  Successful roundtrip schunk <-> frame !

 */

#include <stdio.h>
#include <assert.h>
#include <blosc.h>

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE (1000 * 1000)
#define NCHUNKS 100
#define NTHREADS 4

int main() {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  static int32_t data_dest2[CHUNKSIZE];
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int64_t nbytes, cbytes;
  blosc2_schunk* schunk;
  int i, nchunk;
  size_t nchunks;
  blosc_timestamp_t last, current;
  double ttotal;

  for (i = 0; i < CHUNKSIZE; i++) {
    data[i] = i;
  }

  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  /* Create a super-chunk container */
  schunk = blosc2_new_schunk(
          (blosc2_cparams) {
                  .typesize = sizeof(int32_t),
                  .filters[BLOSC_MAX_FILTERS - 1] = BLOSC_SHUFFLE,
                  .compcode = BLOSC_LZ4,
                  .clevel = 9,
                  .nthreads = NTHREADS,
          },
          (blosc2_dparams) {
                  .nthreads = NTHREADS,
          });

  blosc_set_timestamp(&last);
  for (nchunk = 1; nchunk <= NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      data[i] = i * nchunk;
    }
    nchunks = blosc2_append_buffer(schunk, isize, data);
    assert(nchunks == nchunk);
  }
  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Compression ratio: %.1f MB -> %.1f MB (%.1fx)\n",
         nbytes / MB, cbytes / MB, (1. * nbytes) / cbytes);
  printf("Compression time: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));

  // Get an in-memory frame from the super-chunk
  void* frame = blosc2_new_frame(schunk, NULL);
  // Write the frame out to a file
  uint64_t frame_len = blosc2_frame_tofile(frame, "simple_frame.b2frame");
  printf("Frame output to simple_frame.b2frame with %lld bytes\n", frame_len);

  // Alternatively, you can create the frame directly on disk (both have to be equal)
  blosc2_new_frame(schunk, "simple_frame2.b2frame");
  printf("Same frame output to simple_frame2.b2frame with no intermediate buffers\n");

  // Get a new schunk from a frame
  blosc2_schunk* schunk2 = blosc2_schunk_from_frame(frame);

  /* Retrieve and decompress the chunks from the 2 frames */
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    int32_t dsize = blosc2_decompress_chunk(schunk, (size_t)nchunk, (void *)data_dest, isize);
    if (dsize < 0) {
      printf("Decompression error in schunk.  Error code: %d\n", dsize);
      return dsize;
    }
    int32_t dsize2 = blosc2_decompress_chunk(schunk2, (size_t)nchunk, (void *)data_dest2, isize);
    if (dsize2 < 0) {
      printf("Decompression error in schunk2.  Error code: %d\n", dsize2);
      return dsize2;
    }
    assert(dsize == dsize2);
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Decompression time: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));

  /* Check integrity of the last chunk */
  for (i = 0; i < CHUNKSIZE; i++) {
    if (data_dest[i] != data_dest2[i]) {
      printf("Decompressed data differs from original %d, %d!\n", data_dest[i], data_dest2[i]);
      return -1;
    }
  }

  printf("Successful roundtrip schunk <-> frame !\n");

  /* Free resources */
  blosc2_free_schunk(schunk);
  blosc2_free_schunk(schunk2);
  free(frame);

  return 0;
}
