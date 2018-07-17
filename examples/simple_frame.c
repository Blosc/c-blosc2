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
  Compression time: 0.122 s, 3133.7 MB/s
  Time for schunk -> frame: 0.00644 s, 59242.6 MB/s
  Time for frame -> fileframe (simple_frame.b2frame): 0.00918 s, 41556.5 MB/s
  Time for schunk -> fileframe (simple_frame2.b2frame): 0.00888 s, 42962.7 MB/s
  fileframe -> schunk time: 0.00939 s, 40608.1 MB/s
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

  // super-chunk -> frame
  blosc_set_timestamp(&last);
  blosc2_frame* frame1 = &(blosc2_frame) {
    .sdata = NULL,
    .fname = NULL,
    .len = 0,
    .maxlen = 0,
  };
  int64_t frame_len = blosc2_new_frame(schunk, frame1);
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for schunk -> frame: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));
  printf("Frame length in memory: %lld bytes\n", frame_len);
  // frame1 -> fileframe
  blosc_set_timestamp(&last);
  frame_len = blosc2_frame_tofile(frame1, "simple_frame.b2frame");
  printf("Frame length on disk: %lld bytes\n", frame_len);
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for frame -> fileframe (simple_frame.b2frame): %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));

  // super-chunk -> fileframe (no intermediate frame buffer)
  blosc_set_timestamp(&last);
  blosc2_frame* frame2 = &(blosc2_frame) {
          .sdata = NULL,
          .fname = "simple_frame2.b2frame",
          .len = 0,
          .maxlen = 0,
  };
  blosc2_new_frame(schunk, frame2);
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for schunk -> fileframe (%s): %.3g s, %.1f MB/s\n",
         frame2->fname, ttotal, nbytes / (ttotal * MB));

  // fileframe -> schunk (no intermediate frame buffer)
  blosc_set_timestamp(&last);
  blosc2_schunk* schunk2 = blosc2_schunk_from_fileframe("simple_frame2.b2frame");
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("fileframe -> schunk time: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));

  /* Retrieve and decompress the chunks from the 2 frames */
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
    /* Check integrity of the last chunk */
    for (i = 0; i < CHUNKSIZE; i++) {
      assert (data_dest[i] == data_dest2[i]);
    }
  }
  printf("Successful roundtrip schunk <-> frame !\n");

  /* Free resources */
  blosc2_free_schunk(schunk);
  blosc2_free_schunk(schunk2);
  blosc2_free_frame(frame1);

  return 0;
}
