/*
  Copyright (C) 2018  Francesc Alted
  http://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  $ gcc frame_simple.c -o frame_simple -lblosc2

  To run:

  $ ./frame_simple
  Blosc version info: 2.0.0a6.dev ($Date:: 2018-05-18 #$)
  Compression ratio: 381.5 MB -> 9.5 MB (40.2x)
  Compression time: 0.705 s, 541.0 MB/s
  Time for schunk -> frame: 0.00796 s, 47905.3 MB/s
  Frame length in memory: 9940344 bytes
  Frame length on disk: 9940344 bytes
  Time for frame -> fileframe (frame_simple.b2frame): 0.0108 s, 35159.6 MB/s
  Time for fileframe (frame_simple.b2frame) -> frame : 0.000254 s, 1.5e+06 MB/s
  Time for frame -> schunk: 1.1e-05 s, 3.48e+07 MB/s
  Time for fileframe -> schunk: 1.25e-05 s, 3.05e+07 MB/s
  Successful roundtrip schunk <-> frame <-> fileframe !

 */

#include <stdio.h>
#include <assert.h>
#include <blosc2.h>

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE (200 * 1000)
#define NCHUNKS 100
#define NTHREADS 4


int main(void) {
  size_t isize = CHUNKSIZE * sizeof(int32_t);

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
  blosc2_storage storage = {.contiguous=true, .urlpath="umeta.b2frame", .cparams=&cparams, .dparams=&dparams};
  blosc2_schunk* schunk = blosc2_schunk_new(storage);


  // Add a metalayer
  int umlen = blosc2_add_metalayer(schunk, "umetalayer", (uint8_t *) "This is a vlmetalayers content...", 10);
  if (umlen < 0) {
    printf("Cannot write vlmetalayers chunk");
    return umlen;
  }

  // Add some vlmetalayers data
  umlen = blosc2_add_vlmetalayer(schunk, "umetalayer", (uint8_t *) "This is a vlmetalayers content...", 32, NULL);
  if (umlen < 0) {
    printf("Cannot write vlmetalayers chunk");
    return umlen;
  }

  // Add some vlmetalayers data
  umlen = blosc2_add_vlmetalayer(schunk, "umetalayer2", (uint8_t *) "This is a content...", 10, NULL);
  if (umlen < 0) {
    printf("Cannot write vlmetalayers chunk");
    return umlen;
  }

  umlen = blosc2_update_vlmetalayer(schunk, "umetalayer", (uint8_t *) "This is a another umetalayer content...", 20,
                                    NULL);
  if (umlen < 0) {
    printf("Cannot write vlmetalayers chunk");
    return umlen;
  }

  blosc2_schunk *sc = blosc2_schunk_open("umeta.b2frame");

  /* Free resources */
  blosc2_schunk_free(schunk);
  blosc2_schunk_free(sc);

  return 0;
}
