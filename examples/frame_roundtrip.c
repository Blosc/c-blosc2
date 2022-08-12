/*
  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  $ gcc frame_rountrip.c -o frame_roundtrip -lblosc2

  To run:

  $ ./frame_rountrip

*/

#include <stdio.h>
#include <blosc2.h>

#define CHUNKSIZE (1000)
#define NCHUNKS 2


int main(void) {
  int32_t* buf;

  buf = calloc(CHUNKSIZE, sizeof(int32_t));
  for (int i = 0; i < CHUNKSIZE; i++) {
    buf[i] = i;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 4;
  cparams.filters[BLOSC_LAST_FILTER] = BLOSC_SHUFFLE;
  cparams.clevel = 9;
  cparams.nthreads = 1;
  blosc2_storage storage = {.cparams=&cparams, .contiguous=false};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    goto failed;
  }

  for (int i = 0; i < NCHUNKS; i++) {
    int64_t status = blosc2_schunk_append_buffer(schunk, buf, CHUNKSIZE * sizeof(int32_t));
    if (status < 0) {
      goto failed;
    }
  }

  printf("bytes, cbytes for schunk: %lld, %lld \n", schunk->nbytes, schunk->cbytes);

  bool cframe_needs_free;
  uint8_t* cframe;
  int64_t cframe_len = blosc2_schunk_to_buffer(schunk, &cframe, &cframe_needs_free);
  if (cframe_len < 0 || !cframe_needs_free) {
    goto failed;
  }

  printf("compressed into a cframe of %lld bytes\n", cframe_len);

  blosc2_schunk* schunk2 = blosc2_schunk_from_buffer(cframe, cframe_len, true);
  if (schunk2 == NULL) {
    goto failed;
  }

  printf("bytes, cbytes for schunk2: %lld, %lld \n", schunk2->nbytes, schunk2->cbytes);

  uint8_t *chunk;
  bool needs_free;
  int cbytes = blosc2_schunk_get_chunk(schunk2, 0, &chunk, &needs_free);
  if (cbytes < 0 || needs_free) {
    goto failed;
  }

  blosc2_schunk_free(schunk2);
  blosc2_schunk_free(schunk);

  if (cframe_needs_free) {
    free(cframe);
  }
  free(buf);

  return 0;

  failed:
  return -1;
}
