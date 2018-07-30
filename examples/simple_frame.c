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
  Compression time: 0.705 s, 541.0 MB/s
  Time for schunk -> frame: 0.00796 s, 47905.3 MB/s
  Frame length in memory: 9940344 bytes
  Frame length on disk: 9940344 bytes
  Time for frame -> fileframe (simple_frame.b2frame): 0.0108 s, 35159.6 MB/s
  Time for fileframe (simple_frame.b2frame) -> frame : 0.000254 s, 1.5e+06 MB/s
  Time for frame -> schunk: 1.1e-05 s, 3.48e+07 MB/s
  Time for fileframe -> schunk: 1.25e-05 s, 3.05e+07 MB/s
  Successful roundtrip schunk <-> frame <-> fileframe !

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
  static int32_t data_dest1[CHUNKSIZE];
  static int32_t data_dest2[CHUNKSIZE];
  static int32_t data_dest3[CHUNKSIZE];
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  size_t nbytes, cbytes;
  int i, nchunk;
  int nchunks;
  blosc_timestamp_t last, current;
  double ttotal;

  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  /* Create a super-chunk container */
  blosc2_cparams cparams = BLOSC_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_LZ4;
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  blosc2_dparams dparams = BLOSC_DPARAMS_DEFAULTS;
  dparams.nthreads = NTHREADS;
  blosc2_schunk* schunk = blosc2_new_schunk(cparams, dparams, NULL);

  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      data[i] = i * nchunk;
    }
    nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    assert(nchunks == nchunk + 1);
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

  // Start different conversions between schunks, frames and fileframes

  // super-chunk -> frame1 (in-memory)
  blosc_set_timestamp(&last);
  blosc2_frame frame1 = BLOSC_EMPTY_FRAME;
  int64_t frame_len = blosc2_schunk_to_frame(schunk, &frame1);
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for schunk -> frame: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));
  printf("Frame length in memory: %lld bytes\n", frame_len);

  // frame1 (in-memory) -> fileframe (on-disk)
  blosc_set_timestamp(&last);
  frame_len = blosc2_frame_to_file(&frame1, "simple_frame.b2frame");
  printf("Frame length on disk: %lld bytes\n", frame_len);
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for frame -> fileframe (simple_frame.b2frame): %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));

  // fileframe (file) -> frame2 (on-disk frame)
  blosc_set_timestamp(&last);
  blosc2_frame* frame2 = blosc2_frame_from_file("simple_frame.b2frame");
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for fileframe (%s) -> frame : %.3g s, %.3g MB/s\n",
         frame2->fname, ttotal, nbytes / (ttotal * MB));

  // frame1 (in-memory) -> schunk
  blosc_set_timestamp(&last);
  // The next creates an schunk made of sparse chunks
  blosc2_schunk* schunk1 = blosc2_schunk_from_frame(&frame1);
  // The next creates a frame-backed schunk
  // blosc2_schunk* schunk1 = blosc2_new_schunk(cparams, dparams, &frame1);
  if (schunk1 == NULL) {
    printf("Bad conversion frame -> schunk!\n");
    return -1;
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for frame -> schunk: %.3g s, %.3g MB/s\n",
         ttotal, nbytes / (ttotal * MB));

  // frame2 (on-disk) -> schunk
  blosc_set_timestamp(&last);
  // The next creates an schunk made of sparse chunks
  // blosc2_schunk* schunk2 = blosc2_schunk_from_frame(frame2);
  // The next creates a frame-backed schunk
  blosc2_schunk* schunk2 = blosc2_new_schunk(cparams, dparams, frame2);
  if (schunk2 == NULL) {
    printf("Bad conversion frame -> schunk!\n");
    return -1;
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for fileframe -> schunk: %.3g s, %.3g MB/s\n",
         ttotal, nbytes / (ttotal * MB));

  /* Retrieve and decompress the chunks from the super-chunks and compare values */
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    int32_t dsize = blosc2_schunk_decompress_chunk(schunk1, nchunk, data_dest, isize);
    if (dsize < 0) {
      printf("Decompression error in schunk1.  Error code: %d\n", dsize);
      return dsize;
    }
    int32_t dsize1 = blosc2_frame_decompress_chunk(&frame1, nchunk, data_dest1, isize);
    if (dsize1 < 0) {
      printf("Decompression error in frame1.  Error code: %d\n", dsize1);
      return dsize1;
    }
    assert(dsize == dsize1);
    int32_t dsize2 = blosc2_schunk_decompress_chunk(schunk2, nchunk, data_dest2, isize);
    if (dsize2 < 0) {
      printf("Decompression error in schunk2.  Error code: %d\n", dsize2);
      return dsize2;
    }
    assert(dsize == dsize2);
    int32_t dsize3 = blosc2_frame_decompress_chunk(frame2, nchunk, data_dest3, isize);
    if (dsize3 < 0) {
      printf("Decompression error in frame2.  Error code: %d\n", dsize2);
      return dsize2;
    }
    assert(dsize == dsize2);
    /* Check integrity of the last chunk */
    for (i = 0; i < CHUNKSIZE; i++) {
      assert (data_dest[i] == i * nchunk);
      assert (data_dest1[i] == i * nchunk);
      assert (data_dest2[i] == i * nchunk);
      assert (data_dest3[i] == i * nchunk);
    }
  }
  printf("Successful roundtrip schunk <-> frame <-> fileframe !\n");

  /* Free resources */
  blosc2_free_schunk(schunk);
  blosc2_free_schunk(schunk1);
  blosc2_free_schunk(schunk2);
  blosc2_free_frame(frame2);

  return 0;
}
