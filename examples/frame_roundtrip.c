/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating a schunk roundtrip via a frame.

  To compile this program:

  $ gcc frame_rountrip.c -o frame_roundtrip -lblosc2

  To run:

  $ ./frame_rountrip

*/

#include <stdio.h>
#include <blosc2.h>

#define CHUNKSIZE 100
#define NCHUNKS 1000


int main(void) {
  blosc2_init();

  int32_t total_bytes = CHUNKSIZE * sizeof(int32_t);

  int32_t *buf = calloc(CHUNKSIZE, sizeof(int32_t));
  // You can initialize data, but zeros compress better ;-)
  //  for (int i = 0; i < CHUNKSIZE; i++) {
  //    buf[i] = i;
  //  }

  // Create the original schunk
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_BITSHUFFLE;
  cparams.clevel = 9;
  //  blosc2_remove_dir("/tmp/test.frame");
  //  blosc2_storage storage = {.cparams=&cparams, .contiguous=false, .urlpath="/tmp/test.frame"};
  blosc2_storage storage = {.cparams=&cparams, .contiguous=false};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    printf("Error in creating schunk\n");
    goto failed;
  }

  // Append some chunks
  for (int i = 0; i < NCHUNKS; i++) {
    int64_t status = blosc2_schunk_append_buffer(schunk, buf, total_bytes);
    if (status < 0) {
      printf("Error in appending to schunk\n");
      goto failed;
    }
  }
  printf("nbytes, cbytes for schunk: %lld, %lld\n",
         (long long int)schunk->nbytes, (long long int)schunk->cbytes);

  // Check contents
  uint8_t *chunk;
  bool needs_free;
  int32_t *dest = malloc(CHUNKSIZE * sizeof(int32_t));
  for (int i = 0; i < NCHUNKS; i++) {
    int cbytes = blosc2_schunk_get_chunk(schunk, i, &chunk, &needs_free);
    if (cbytes < 0) {
      printf("Error in getting chunk %d from schunk\n", i);
      goto failed;
    }
    int nbytes = blosc2_decompress(chunk, cbytes, dest, total_bytes);
    if (nbytes != total_bytes) {
      printf("Error in schunk: nbytes differs (%d != %d)\n", nbytes, total_bytes);
      goto failed;
    }
    if (needs_free) {
      free(chunk);
    }
    for (int j = 0; j < CHUNKSIZE; j++) {
      if (buf[j] != dest[j]) {
        printf("Error in schunk: data differs in index %d (%d != %d)!\n", j, buf[i], dest[i]);
        goto failed;
      }
    }
  }

  // Convert into a cframe (contiguous buffer)
  bool cframe_needs_free;
  uint8_t* cframe;
  int64_t cframe_len = blosc2_schunk_to_buffer(schunk, &cframe, &cframe_needs_free);
  if (cframe_len < 0 || !cframe_needs_free) {
    goto failed;
  }
  printf("converted into a cframe of %lld bytes\n", (long long int)cframe_len);

  // Convert back into a different schunk
  blosc2_schunk* schunk2 = blosc2_schunk_from_buffer(cframe, cframe_len, true);
  if (schunk2 == NULL) {
    goto failed;
  }
  printf("nbytes, cbytes for schunk2: %lld, %lld\n",
         (long long int)schunk2->nbytes, (long long int)schunk2->cbytes);

  // Check contents
  for (int i = 0; i < NCHUNKS; i++) {
    int cbytes = blosc2_schunk_get_chunk(schunk2, i, &chunk, &needs_free);
    if (cbytes < 0) {
      printf("Error in getting chunk %d from schunk2\n", i);
      goto failed;
    }
    int nbytes = blosc2_decompress(chunk, cbytes, dest, total_bytes);
    if (nbytes != total_bytes) {
      printf("Error in schunk2: nbytes differs (%d != %d)\n", nbytes, total_bytes);
      goto failed;
    }
    if (needs_free) {
      free(chunk);
    }
    for (int j = 0; j < CHUNKSIZE; j++) {
      if (buf[j] != dest[j]) {
        printf("Error in schunk2: data differs in index %d (%d != %d)!\n", j, buf[i], dest[i]);
        goto failed;
      }
    }
  }
  free(dest);

  blosc2_schunk_free(schunk2);
  blosc2_schunk_free(schunk);

  if (cframe_needs_free) {
    free(cframe);
  }
  free(buf);

  printf("All good!\n");
  blosc2_destroy();

  return 0;

  failed:
  return -1;
}
