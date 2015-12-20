/*
  Copyright (C) 2015  Francesc Alted
  http://blosc.org
  License: MIT (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  gcc delta_packed_schunk.c -o delta_packed_schunk -lblosc

  To run:

  $ ./delta_packed_schunk
  Blosc version info: 2.0.0a2 ($Date:: 2015-12-17 #$)
  Compression super-chunk: 60000112 -> 20234528 (3.0x)
  Decompression successful!
  Successful roundtrip!

*/

#include <stdio.h>
#include <assert.h>
#include "../blosc/blosc.h"

#define SIZE 500*100*100


int main() {
  static int32_t data[SIZE];
  static schunk_params sc_params;
  int32_t* data_dest;
  int isize = SIZE * sizeof(int32_t), osize = SIZE * sizeof(int32_t);
  int dsize, csize;
  int64_t nbytes, cbytes;
  schunk_header* sc_header;
  int i, nchunks;
  void* packed;

  for (i = 0; i < SIZE; i++) {
    data[i] = i;
  }

  printf("Blosc version info: %s (%s)\n", BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  /* Initialize the Blosc compressor */
  blosc_init();

  /* Create a super-chunk container */
  sc_params.filters[0] = BLOSC_DELTA;
  sc_params.filters[1] = BLOSC_SHUFFLE;
  sc_params.compressor = BLOSC_BLOSCLZ;
  sc_params.clevel = 5;
  sc_header = blosc2_new_schunk(&sc_params);

  /* Append the reference chunks first */
  nchunks = blosc2_append_buffer(sc_header, sizeof(int32_t), isize, data);
  assert(nchunks == 1);
  nbytes = sc_header->nbytes;
  cbytes = sc_header->cbytes;
  printf("Compression super-chunk (native) #%d: %lld -> %lld (%.1fx)\n",
         0, nbytes, cbytes, (1. * nbytes) / cbytes);

  /* Pack the super-chunk */
  packed = blosc2_pack_schunk(sc_header);

  /* Now append another chunk (essentially the same as the reference) */
  packed = blosc2_packed_append_buffer(packed, sizeof(int32_t), isize, data);

  /* Gather some info */
  assert(*(int64_t*)(packed + 16) == 2);
  nbytes = *(int64_t*)(packed + 24);
  cbytes = *(int64_t*)(packed + 32);
  printf("Compression super-chunk: %lld -> %lld (%.1fx)\n",
         nbytes, cbytes, (1. * nbytes) / cbytes);

  /* Retrieve and decompress the chunks (0-based count) */
  dsize = blosc2_packed_decompress_chunk(packed, 1, (void**)&data_dest);
  if (dsize < 0) {
    printf("Decompression error.  Error code: %d\n", dsize);
    return dsize;
  }

  printf("Decompression successful!\n");

  for (i = 0; i < SIZE; i++) {
    if (data[i] != data_dest[i]) {
      printf("Decompressed data differs from original %d, %d, %d!\n", i, data[i], data_dest[i]);
      return -1;
    }
  }

  printf("Successful roundtrip!\n");

  /* Free resources */
  free(data_dest);
  /* Destroy the super-chunk */
  blosc2_destroy_schunk(sc_header);
  /* Destroy the packed super-chunk */
  free(packed);
  /* Destroy the Blosc environment */
  blosc_destroy();

  return 0;
}
