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
    Blosc version info: 1.4.2.dev ($Date:: 2014-07-08 #$)
    Compression: 4000000 -> 158494 (25.2x)
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
  static float data_out1[SIZE];
  static float data_out2[SIZE];
  void* chunk1 = data_out1;
  void* chunk2 = data_out2;
  float* data_dest;
  int isize = SIZE * sizeof(float), osize = SIZE * sizeof(float);
  int dsize, csize;
  schunk_params sc_params;
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
  csize = blosc_compress(5, BLOSC_SHUFFLE, sizeof(float), isize, data, chunk1, osize);
  csize = blosc_compress(5, BLOSC_SHUFFLE, sizeof(float), isize, data, chunk2, osize);
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
  sc_params.filters = BLOSC_SHUFFLE;
  sc_params.compressor = BLOSC_BLOSCLZ;
  sc_params.clevel = 5;
  sc_header = blosc2_new_schunk(sc_params);

  /* Append a couple of chunks there */
  nchunks = blosc2_append_chunk(sc_header, chunk1);
  assert(nchunks == 1);
  nchunks = blosc2_append_chunk(sc_header, chunk2);
  assert(nchunks == 2);
  printf("Hola!\n");

  /* Retrieve and decompress the second chunk (0-based count) */
  dsize = blosc2_decompress_chunk(sc_header, 0, &data_dest);
  if (dsize < 0) {
    printf("Decompression error.  Error code: %d\n", dsize);
    return dsize;
  }

  printf("Decompression succesful!\n");

  /* Destroy the super-chunk */
  blosc2_destroy_schunk(sc_header);

  /* Destroy the Blosc environment */
  blosc_destroy();

  for(i=0;i<SIZE;i++){
    if(data[i] != data_dest[i]) {
      printf("Decompressed data differs from original!\n");
      return -1;
    }
  }

  printf("Succesful roundtrip!\n");
  free(data_dest);
  return 0;
}
