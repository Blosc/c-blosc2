/*
  Copyright (C) 2015  Francesc Alted
  http://blosc.org
  License: MIT (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  gcc simple_schunk.c -o schunk -lblosc -lpthread

  or, if you don't have the blosc library installed:

  gcc -O3 -mavx2 simple_schunk.c ../blosc/*.c -I../blosc -o schunk -lpthread

  Using MSVC on Windows:

  cl /arch:SSE2 /Ox /Feschunk.exe /Iblosc examples\simple_schunk.c blosc\blosc.c blosc\blosclz.c blosc\shuffle.c blosc\shuffle-sse2.c blosc\shuffle-generic.c blosc\bitshuffle-generic.c blosc\bitshuffle-sse2.c

  To run:

  $ ./schunk
  Blosc version info: 2.0.0a1 ($Date:: 2015-07-30 #$)
  Compression: 4000000 -> 158788 (25.2x)
  destsize: 4000000
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
  static float data[SIZE];
  static float data2[SIZE];
  void* chunk = data2;
  float* data_dest;
  int isize = SIZE * sizeof(float), osize = SIZE * sizeof(float);
  int dsize, csize;
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

  /* Compress with clevel=5 and shuffle active  */
  csize = blosc_compress(5, BLOSC_SHUFFLE, sizeof(float), isize, data, chunk, osize);
  if (csize == 0) {
    printf("Buffer is uncompressible.  Giving up.\n");
    return 1;
  }
  else if (csize < 0) {
    printf("Compression error.  Error code: %d\n", csize);
    return csize;
  }

  printf("Compression: %d -> %d (%.1fx)\n", isize, csize, (1.*isize) / csize);

  /* Create a super-chunk container */
  sc_params->filters[0] = BLOSC_SHUFFLE;
  sc_params->compressor = BLOSC_BLOSCLZ;
  sc_params->clevel = 5;
  sc_header = blosc2_new_schunk(sc_params);

  /* Append a couple of chunks there */
  nchunks = blosc2_append_chunk(sc_header, chunk, 1);
  assert(nchunks == 1);

  /* Now append another chunk coming from the initial buffer */
  nchunks = blosc2_append_buffer(sc_header, sizeof(float), isize, data);
  assert(nchunks == 2);

  /* Retrieve and decompress the chunks (0-based count) */
  dsize = blosc2_decompress_chunk(sc_header, 1, &data_dest);
  if (dsize < 0) {
    printf("Decompression error.  Error code: %d\n", dsize);
    return dsize;
  }
  dsize = blosc2_decompress_chunk(sc_header, 0, &data_dest);
  if (dsize < 0) {
    printf("Decompression error.  Error code: %d\n", dsize);
    return dsize;
  }

  printf("Decompression succesful!\n");

  for(i=0;i<SIZE;i++){
    if(data[i] != data_dest[i]) {
      printf("Decompressed data differs from original!\n");
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
