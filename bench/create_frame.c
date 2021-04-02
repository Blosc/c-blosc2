/*
  Copyright (C) 2021  The Blosc Developers
  http://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Simple benchamrk for frame creation.

  To run:

  $ ./create_frame
Blosc version info: 2.0.0.beta.6.dev ($Date:: 2020-04-21 #$)

*** Creating simple frame for blosclz
Compression ratio: 1907.3 MB -> 51.6 MB (36.9x)
Compression time: 0.357 s, 5347.5 MB/s
Decompression time: 0.126 s, 15081.2 MB/s

*** Creating simple frame for lz4
Compression ratio: 1907.3 MB -> 82.7 MB (23.1x)
Compression time: 0.305 s, 6259.2 MB/s
Decompression time: 0.0979 s, 19475.7 MB/s

*** Creating simple frame for lz4hc
Compression ratio: 1907.3 MB -> 38.2 MB (50.0x)
Compression time: 0.947 s, 2013.8 MB/s
Decompression time: 0.0922 s, 20691.0 MB/s

*** Creating simple frame for zlib
Compression ratio: 1907.3 MB -> 36.4 MB (52.4x)
Compression time: 1.44 s, 1328.0 MB/s
Decompression time: 0.26 s, 7339.6 MB/s

*** Creating simple frame for zstd
Compression ratio: 1907.3 MB -> 19.3 MB (98.7x)
Compression time: 1.08 s, 1768.3 MB/s
Decompression time: 0.15 s, 12709.1 MB/s

Process finished with exit code 0

 */

#include <stdio.h>
#include <blosc2.h>

#define KB  (1024.)
#define MB  (1024*KB)
#define GB  (1024*KB)

#define CHUNKSIZE (500 * 1000)
#define NCHUNKS 1000
#define NTHREADS 8


int create_cframe(const char* compname) {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  static int32_t data_dest2[CHUNKSIZE];
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int64_t nbytes, cbytes;
  int i, nchunk;
  blosc_timestamp_t last, current;
  double ttotal;
  int compcode = blosc_compname_to_compcode(compname);
  printf("\n*** Creating simple frame for %s\n", compname);

  /* Create a super-chunk container */
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = compcode;
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = NTHREADS;
  char filename[64];
  sprintf(filename, "frame_simple-%s.b2frame", compname);
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams,
                            .urlpath=NULL, .contiguous=false};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);

  // Add some data
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      data[i] = i * nchunk;
    }
    int nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    if (nchunks != nchunk + 1) {
      printf("Compression error in schunk.  Error code: %d\n", nchunks);
      return nchunk;
    }
  }
  blosc_set_timestamp(&current);

  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  ttotal = blosc_elapsed_secs(last, current);
  printf("Compression ratio: %.1f MB -> %.1f MB (%.1fx)\n",
         nbytes / MB, cbytes / MB, (1. * nbytes) / cbytes);
  printf("Compression time: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));

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
  printf("Decompression time: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));

  /* Free resources */
  blosc2_schunk_free(schunk);

  return 0;
}


int main(void) {
  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  create_cframe("blosclz");
  create_cframe("lz4");
  create_cframe("lz4hc");
  create_cframe("zlib");
  create_cframe("zstd");
}
