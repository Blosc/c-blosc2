/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
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


int main(void) {
  blosc2_init();

  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int32_t* data = malloc(isize);
  int64_t nbytes, cbytes;
  int i, nchunk;
  int64_t nchunks;
  blosc_timestamp_t last, current;
  double ttotal;

  printf("Blosc version info: %s (%s)\n",
         BLOSC2_VERSION_STRING, BLOSC2_VERSION_DATE);

  /* Create a super-chunk container */
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  //cparams.compcode = BLOSC_LZ4;
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams, .contiguous=true};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);

  // Add some metalayers (one must add metalayers prior to actual data)
  blosc2_meta_add(schunk, "my_metalayer1", (uint8_t *) "my_content1",
                  (uint32_t) strlen("my_content1"));
  blosc2_meta_add(schunk, "my_metalayer2", (uint8_t *) "my_content1",
                  (uint32_t) strlen("my_content1"));

  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      data[i] = i * nchunk + i;
    }
    nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    if (nchunks != nchunk + 1) {
      printf("Unexpected nchunks!");
      return nchunks;
    }
  }
  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Compression ratio: %.2f MB -> %.2f MB (%.1fx)\n",
         (double)nbytes / MB, (double)cbytes / MB, (1. * (double)nbytes) / (double)cbytes);
  printf("Compression time: %.3g s, %.1f MB/s\n",
         ttotal, (double)nbytes / (ttotal * MB));

  blosc_set_timestamp(&last);

  // Update a metalayer (this is fine as long as the new content does not exceed the size of the previous one)
  blosc2_meta_update(schunk, "my_metalayer2", (uint8_t *) "my_content2",
                     (uint32_t) strlen("my_content2"));
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for update metalayer in header: %.2g s\n", ttotal);
  printf("Frame length in memory: %ld bytes\n", (long)schunk->cbytes);

  // schunk (in-memory) -> fileframe (on-disk)
  blosc_set_timestamp(&last);
  int64_t frame_len = blosc2_schunk_to_file(schunk, "frame_metalayers.b2frame");
  printf("Frame length on disk: %ld bytes\n", (long)frame_len);
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for frame -> fileframe (simple_frame.b2frame): %.3g s, %.1f GB/s\n",
         ttotal, (double)nbytes / (ttotal * GB));

  // fileframe (file) -> schunk2 (schunk based on a on-disk frame)
  blosc_set_timestamp(&last);
  blosc2_schunk* schunk2 = blosc2_schunk_open("frame_metalayers.b2frame");
  if (schunk2 == NULL) {
    printf("Cannot get the schunk from frame2");
    return -1;
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for fileframe (%s) -> schunk : %.3g s, %.1f GB/s\n",
         schunk2->storage->urlpath, ttotal, (double)nbytes / (ttotal * GB));

  // Check that the metalayers had a good roundtrip
  if (schunk2->nmetalayers != 2) {
      printf("nclients not retrieved correctly!\n");
      return -1;
  }
  uint8_t* content;
  int32_t content_len;
  if (blosc2_meta_get(schunk2, "my_metalayer1", &content, &content_len) < 0) {
      printf("metalayer not found");
      return -1;
  }
  if (memcmp(content, "my_content1", content_len) != 0) {
      printf("serialized content for metalayer not retrieved correctly!\n");
      return -1;
  }
  free(content);

  /* Free resources */
  blosc2_schunk_free(schunk);
  blosc2_schunk_free(schunk2);
  free(data);

  blosc2_destroy();

  return 0;
}
