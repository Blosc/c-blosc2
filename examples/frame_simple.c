/*
  Copyright (C) 2018  Francesc Alted
  http://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  $ gcc frame_simple.c -o frame_simple -lblosc2

  To run:

  $ ./frame_simple
  Blosc version info: 2.0.0a6.dev ($Date:: 2018-05-18 #$)
  Compression ratio: 381.5 MB -> 9.5 MB (40.2x)
  Compression time: 0.705 s, 541.0 MB/s
  Time for schunk -> frame: 0.00796 s, 47905.3 MB/s
  Frame length in memory: 9940344 bytes
  Frame length on disk: 9940344 bytes
  Time for frame -> fileframe (frame_simple.b2frame): 0.0108 s, 35159.6 MB/s
  Time for fileframe (frame_simple.b2frame) -> frame : 0.000254 s, 1.5e+06 MB/s
  Time for frame -> schunk: 1.1e-05 s, 3.48e+07 MB/s
  Time for fileframe -> schunk: 1.25e-05 s, 3.05e+07 MB/s
  Successful roundtrip schunk <-> frame <-> fileframe !

 */

#include <stdio.h>
#include <assert.h>
#include <blosc2.h>

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE (200 * 1000)
#define NCHUNKS 100
#define NTHREADS 4


int main(void) {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  static int32_t data_dest2[CHUNKSIZE];
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int64_t nbytes, cbytes;
  int i, nchunk;
  int nchunks;
  blosc_timestamp_t last, current;
  double ttotal;

  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  /* Create a super-chunk container */
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_LZ4;
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams};
  blosc2_schunk* schunk = blosc2_schunk_new(storage);

  // Add some data
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      data[i] = i * nchunk;
    }
    nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    assert(nchunks == nchunk + 1);
  }

  // Add some usermeta data
  int umlen = blosc2_update_usermeta(schunk, (uint8_t *) "This is a usermeta content.....", 32,
                                     BLOSC2_CPARAMS_DEFAULTS);
  if (umlen < 0) {
    printf("Cannot write usermeta chunk");
    return -1;
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
  uint8_t* usermeta;
  int content_len = blosc2_get_usermeta(schunk, &usermeta);
  printf("Usermeta in schunk: '%s' with length: %d\n", usermeta, content_len);
  free(usermeta);

  // Start different conversions between schunks, frames and fileframes

  // super-chunk -> frame1 (in-memory)
  blosc_set_timestamp(&last);
  blosc2_frame* frame1 = blosc2_frame_new(NULL);
  int64_t frame_len = blosc2_frame_from_schunk(schunk, frame1);
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for schunk -> frame: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));
  printf("Frame length in memory: %ld bytes\n", (long)frame_len);

  // super-chunk -> sframe (in-memory, sequential)
  blosc_set_timestamp(&last);
  uint8_t* sframe;
  int64_t sframe_len = blosc2_schunk_to_sframe(schunk, &sframe);
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for schunk -> sframe: %.3g s, %.1f MB/s\n",
         ttotal, sframe_len / (ttotal * MB));
  printf("sframe length in memory: %ld bytes\n", (long)sframe_len);
  free(sframe);

  // frame1 (in-memory) -> fileframe (on-disk)
  blosc_set_timestamp(&last);
  frame_len = blosc2_frame_to_file(frame1, "frame_simple.b2frame");
  if (frame_len < 0) {
    return frame_len;
  }
  printf("Frame length on disk: %ld bytes\n", (long)frame_len);
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for frame -> fileframe (frame_simple.b2frame): %.3g s, %.1f GB/s\n",
         ttotal, nbytes / (ttotal * GB));

  // fileframe (file) -> frame2 (on-disk frame)
  blosc_set_timestamp(&last);
  blosc2_frame* frame2 = blosc2_frame_from_file("frame_simple.b2frame");
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for fileframe (%s) -> frame : %.3g s, %.1f GB/s\n",
         frame2->urlpath, ttotal, nbytes / (ttotal * GB));

  // frame1 (in-memory) -> schunk
  blosc_set_timestamp(&last);
  // The next creates a schunk made of sequential chunks
  blosc2_schunk* schunk1 = blosc2_frame_to_schunk(frame1, true);
  // The next creates a frame-backed schunk
  // blosc2_schunk* schunk1 = blosc2_frame_to_schunk(frame1, false);
  if (schunk1 == NULL) {
    printf("Bad conversion frame1 -> schunk1!\n");
    return -1;
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for frame -> schunk: %.3g s, %.1f GB/s\n",
         ttotal, nbytes / (ttotal * GB));

  // frame2 (on-disk) -> schunk
  blosc_set_timestamp(&last);
  // The next creates an schunk made of sequential chunks
  // blosc2_schunk* schunk2 = blosc2_frame_to_schunk(frame2, true);
  // The next creates a frame-backed schunk
  blosc2_schunk* schunk2 = blosc2_frame_to_schunk(frame2, false);
  if (schunk2 == NULL) {
    printf("Bad conversion frame2 -> schunk2!\n");
    return -1;
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for fileframe -> schunk: %.3g s, %.1f GB/s\n",
         ttotal, nbytes / (ttotal * GB));

  /* Retrieve and decompress the chunks from the super-chunks and compare values */
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    int32_t dsize = blosc2_schunk_decompress_chunk(schunk1, nchunk, data_dest, isize);
    if (dsize < 0) {
      printf("Decompression error in schunk1.  Error code: %d\n", dsize);
      return dsize;
    }
    int32_t dsize2 = blosc2_schunk_decompress_chunk(schunk2, nchunk, data_dest2, isize);
    if (dsize2 < 0) {
      printf("Decompression error in schunk2.  Error code: %d\n", dsize2);
      return dsize2;
    }
    assert(dsize == dsize2);
    /* Check integrity of this chunk */
    for (i = 0; i < CHUNKSIZE; i++) {
      assert (data_dest[i] == i * nchunk);
      assert (data_dest2[i] == i * nchunk);
    }
  }
  printf("Successful roundtrip schunk <-> frame <-> fileframe !\n");

  content_len = blosc2_get_usermeta(schunk1, &usermeta);
  printf("Usermeta in schunk1: '%s' with length: %d\n", usermeta, content_len);
  free(usermeta);
  content_len = blosc2_get_usermeta(schunk2, &usermeta);
  printf("Usermeta in schunk2: '%s' with length: %d\n", usermeta, content_len);
  free(usermeta);

  /* Free resources */
  blosc2_schunk_free(schunk);
  blosc2_schunk_free(schunk1);
  blosc2_schunk_free(schunk2);
  blosc2_frame_free(frame1);
  //blosc2_frame_free(frame2);

  return 0;
}
