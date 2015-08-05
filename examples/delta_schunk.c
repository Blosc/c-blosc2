/*
  Copyright (C) 2015  Francesc Alted
  http://blosc.org
  License: MIT (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  gcc delta_schunk.c -o delta_schunk -lblosc -lpthread

  or, if you don't have the blosc library installed:

  gcc -O3 -mavx2 delta_schunk.c ../blosc/*.c -I../blosc -o delta_schunk -lpthread

  Using MSVC on Windows:

  cl /arch:SSE2 /Ox /Fedelta_schunk.exe /Iblosc examples\delta_schunk.c blosc\blosc.c blosc\blosclz.c blosc\shuffle.c blosc\shuffle-sse2.c blosc\shuffle-generic.c blosc\bitshuffle-generic.c blosc\bitshuffle-sse2.c

  To run:

  $ ./delta_schunk
  Blosc version info: 2.0.0a1 ($Date:: 2015-07-30 #$)
  Compression: 4000000 -> 70841 (56.5x)
  Compression: 4000000 -> 23467 (170.5x)
  Decompression succesful!
  Succesful roundtrip!

*/

#include <stdio.h>
#include <assert.h>
#include "blosc.h"

#define SIZE 100*100*100
#define SHAPE {100,100,100}
#define CHUNKSHAPE {1,100,100}

int main(){
  static int32_t data[SIZE];
  int32_t* data_dest;
  int isize = SIZE * sizeof(int32_t), osize = SIZE * sizeof(int32_t);
  int dsize, csize;
  int32_t nbytes, cbytes;
  schunk_params* sc_params = calloc(1, sizeof(sc_params));
  schunk_header* sc_header;
  int i, nchunks;

  for(i=0; i<SIZE; i++){
    data[i] = i;
  }

  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  /* Initialize the Blosc compressor */
  blosc_init();

  /* Create a super-chunk container */
  sc_params->filters[0] = BLOSC_DELTA;
  sc_params->filters[1] = BLOSC_SHUFFLE;
  sc_params->compressor = BLOSC_BLOSCLZ;
  sc_params->clevel = 5;
  sc_header = blosc2_new_schunk(sc_params);

  /* Append the reference chunks first */
  nchunks = blosc2_append_buffer(sc_header, sizeof(int32_t), isize, data);
  assert(nchunks == 1);

  /* Now append another chunk (essentially the same as the reference) */
  nchunks = blosc2_append_buffer(sc_header, sizeof(int32_t), isize, data);
  assert(nchunks == 2);

  /* Gather some info */
  nbytes = sc_header->nbytes;
  cbytes = sc_header->cbytes;
  printf("Compression super-chunk: %d -> %d (%.1fx)\n",
         nbytes, cbytes, (1.*nbytes) / cbytes);

  /* Retrieve and decompress the chunks (0-based count) */
  dsize = blosc2_decompress_chunk(sc_header, 1, (void**)&data_dest);
  if (dsize < 0) {
    printf("Decompression error.  Error code: %d\n", dsize);
    return dsize;
  }

  printf("Decompression succesful!\n");

  for (i=0; i<SIZE; i++){
    if (data[i] != data_dest[i]) {
      printf("Decompressed data differs from original %d, %d, %d!\n", i, data[i], data_dest[i]);
      return -1;
    }
  }

  printf("Succesful roundtrip!\n");

  /* Free resources */
  free(data_dest);
  free(sc_params);
  /* Destroy the super-chunk */
  blosc2_destroy_schunk(sc_header);
  /* Destroy the Blosc environment */
  blosc_destroy();

  return 0;
}
