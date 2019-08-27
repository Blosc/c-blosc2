/*
  Copyright (C) 2018  Francesc Alted
  http://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating the use of the metalayers.

  To compile this program:

  $ gcc -O frame_metalayers.c -o frame_metalayers -lblosc2

  To run:

  $ ./frame_metalayers
  Blosc version info: 2.0.0a6.dev ($Date:: 2018-05-18 #$)
  Compression ratio: 3.8 MB -> 0.0 MB (234.4x)
  Compression time: 0.00218 s, 1747.2 MB/s
  Time for schunk -> frame: 1.19e-05 s, 313.5 GB/s
  Frame length in memory: 17247 bytes
  Frame length on disk: 17247 bytes
  Time for frame -> fileframe (simple_frame.b2frame): 0.000144 s, 25.9 GB/s
  Time for fileframe (frame_metalayers.b2frame) -> frame : 4.08e-05 s, 91.3 GB/s
  Time for fileframe -> schunk: 4.29e-07 s, 8683.7 GB/s

 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <blosc2.h>

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE (1000 * 1000)
#define NCHUNKS 1
#define NTHREADS 4


int main() {
  static int32_t data[CHUNKSIZE];
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
  blosc2_frame* frame1 = blosc2_new_frame(NULL);
  blosc2_schunk* schunk = blosc2_new_schunk(cparams, dparams, frame1);

  // Add some metalayers (one must add metalayers prior to actual data)
  blosc2_schunk_add_metalayer(schunk, "my_metalayer1", (uint8_t *) "my_content1",
                              (uint32_t) strlen("my_content1"));
  blosc2_schunk_add_metalayer(schunk, "my_metalayer2", (uint8_t *) "my_content1",
                              (uint32_t) strlen("my_content1"));
  blosc2_metalayer_flush(schunk);

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

  blosc_set_timestamp(&last);

  // Update a metalayer (this is fine as long as the new content does not exceed the size of the previous one)
  blosc2_schunk_update_metalayer(schunk, "my_metalayer2", (uint8_t *) "my_content2",
                                 (uint32_t) strlen("my_content2"));
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for schunk -> frame: %.3g s, %.1f GB/s\n",
         ttotal, nbytes / (ttotal * GB));
  printf("Frame length in memory: %ld bytes\n", (long)frame1->len);

  // frame1 (in-memory) -> fileframe (on-disk)
  blosc_set_timestamp(&last);
  int64_t frame_len = blosc2_frame_to_file(frame1, "frame_metalayers.b2frame");
  printf("Frame length on disk: %ld bytes\n", (long)frame_len);
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for frame -> fileframe (simple_frame.b2frame): %.3g s, %.1f GB/s\n",
         ttotal, nbytes / (ttotal * GB));

  // fileframe (file) -> schunk2 (schunk based on a on-disk frame)
  blosc_set_timestamp(&last);
  blosc2_frame* frame2 = blosc2_frame_from_file("frame_metalayers.b2frame");
  blosc2_schunk* schunk2 = blosc2_schunk_from_frame(frame2, false);
  if (schunk2 == NULL) {
    printf("Cannot get the schunk from frame2");
    return -1;
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for fileframe (%s) -> schunk : %.3g s, %.1f GB/s\n",
         frame2->fname, ttotal, nbytes / (ttotal * GB));

  // Check that the metalayers had a good roundtrip
  if (schunk2->nmetalayers != 2) {
      printf("nclients not retrieved correctly!\n");
      return -1;
  }
  uint8_t* content;
  uint32_t content_len;
  if (blosc2_schunk_get_metalayer(schunk2, "my_metalayer1", &content, &content_len) < 0) {
      printf("metalayer not found");
      return -1;
  }
  if (memcmp(content, "my_content1", content_len) != 0) {
      printf("serialized content for metalayer not retrieved correctly!\n");
      return -1;
  }
  free(content);

  /* Free resources */
  blosc2_free_schunk(schunk);
  blosc2_free_schunk(schunk2);
  blosc2_free_frame(frame1);
  blosc2_free_frame(frame2);

  return 0;
}
