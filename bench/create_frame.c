/*
  Copyright (C) 2021  The Blosc Developers
  http://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Simple benchmark for frame creation.

  To run:

  $ ./create_frame

 *** Creating simple frame for blosclz
Compression ratio: 1.86 GB -> 0.05 GB (36.9x)
Compression time: 0.445 s, 4.2 GB/s
Decompression time: 0.0905 s, 20.6 GB/s

*** Creating simple frame for lz4
Compression ratio: 1.86 GB -> 0.08 GB (23.1x)
Compression time: 0.324 s, 5.8 GB/s
Decompression time: 0.135 s, 13.8 GB/s

*** Creating simple frame for lz4hc
Compression ratio: 1.86 GB -> 0.04 GB (50.0x)
Compression time: 0.96 s, 1.9 GB/s
Decompression time: 0.12 s, 15.5 GB/s

*** Creating simple frame for zlib
Compression ratio: 1.86 GB -> 0.04 GB (52.4x)
Compression time: 1.17 s, 1.6 GB/s
Decompression time: 0.32 s, 5.8 GB/s

*** Creating simple frame for zstd
Compression ratio: 1.86 GB -> 0.02 GB (98.6x)
Compression time: 0.773 s, 2.4 GB/s
Decompression time: 0.17 s, 11.0 GB/s

Process finished with exit code 0

 */

#include <stdio.h>
#include <blosc2.h>

#define KB  (1024.)
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE (500 * 1000)
#define NCHUNKS 1000
#define NTHREADS 8

// For exercising the optimized zero chunk creators uncomment the line below
#define CREATE_ZEROS


int create_cframe(const char* compname) {
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int32_t* data = malloc(isize);
  int32_t* data_dest = malloc(isize);
  int32_t* data_dest2 = malloc(isize);
  int64_t nbytes, cbytes;
  int nchunk;
  blosc_timestamp_t last, current;
  double ttotal;
  int compcode = blosc_compname_to_compcode(compname);
  printf("\n*** Creating simple frame for %s\n", compname);

  /* Create a super-chunk container */
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = compcode;
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  //cparams.blocksize = 1024;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = NTHREADS;
  char filename[64];
  sprintf(filename, "frame_simple-%s.b2frame", compname);
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams,
                            .urlpath=NULL, .contiguous=false};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);

#ifdef CREATE_ZEROS
  // Precompute chunk of zeros
  int ret = blosc2_chunk_zeros(cparams, isize, data_dest, isize);
  if (ret < 0) {
    printf("Compression error in chunk_zeros.  Error code: %d\n", ret);
    return ret;
  }
#endif

  // Add some data
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
#ifdef CREATE_ZEROS
    int nchunks = blosc2_schunk_append_chunk(schunk, (uint8_t *) data_dest, true);
    if (nchunks != nchunk + 1) {
      printf("Compression error in append chunk.  Error code: %d\n", nchunks);
      return nchunk;
    }
#else
    for (int i = 0; i < CHUNKSIZE; i++) {
      // Different data patterns
      data[i] = i * nchunk;
      // data[i] = nchunk;
      // data[i] = 0;
    }
    int nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    if (nchunks != nchunk + 1) {
      printf("Compression error appending in schunk.  Error code: %d\n", nchunks);
      return nchunk;
    }
#endif
  }
  blosc_set_timestamp(&current);

  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = blosc2_schunk_frame_len(schunk);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Compression ratio: %.2f GB -> %.2f GB (%.1fx)\n",
         nbytes / GB, cbytes / GB, (1. * nbytes) / (1. * cbytes));
  printf("Compression time: %.3g s, %.1f GB/s\n",
         ttotal, nbytes / (ttotal * GB));

  /* Retrieve and decompress the chunks from the super-chunks and compare values */
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    int32_t dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, data_dest, isize);
    if (dsize < 0) {
      printf("Decompression error in schunk.  Error code: %d\n", dsize);
      return dsize;
    }
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Decompression time: %.3g s, %.1f GB/s\n",
         ttotal, nbytes / (ttotal * GB));

  /* Free resources */
  blosc2_schunk_free(schunk);
  free(data);
  free(data_dest);
  free(data_dest2);

  return 0;
}


int main(void) {
#ifdef CREATE_ZEROS
  printf("\n   ***  Creating zeros!   ***\n");
#endif

  create_cframe("blosclz");
  create_cframe("lz4");
//  create_cframe("lz4hc");
//  create_cframe("zlib");
//  create_cframe("zstd");
}
