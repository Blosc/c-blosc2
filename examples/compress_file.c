/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  $ gcc compress_file.c -o compress_file -lblosc2

  To run:

  $ ./compress_file /usr/lib/libsqlite3.dylib libsqlite3.b2frame
  Blosc version info: 2.0.0a6.dev ($Date:: 2018-05-18 #$)
  Compression ratio: 5.1 MB -> 3.6 MB (1.4x)
  Compression time: 0.0185 s, 275.2 MB/s

 */

#include <stdio.h>
#include <blosc2.h>

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE (1000 * 1000)
#define NTHREADS 4


int main(int argc, char* argv[]) {
  blosc2_init();
  static int32_t data[CHUNKSIZE];
  int32_t isize;
  int64_t nbytes, cbytes;
  blosc_timestamp_t last, current;
  double ttotal;

  if (argc != 3) {
    fprintf(stderr, "Usage: compress_file input_file output_file.b2frame\n");
    return -1;
  }

  printf("Blosc version info: %s (%s)\n",
         BLOSC2_VERSION_STRING, BLOSC2_VERSION_DATE);

  /* Create a super-chunk container */
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  cparams.compcode = BLOSC_BLOSCLZ;
  //cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_BITSHUFFLE;
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = NTHREADS;

  /* Create a super-chunk backed by an in-memory frame */
  remove(argv[2]);
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams,
                            .contiguous=true, .urlpath=argv[2]};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);

  // Compress the file
  blosc_set_timestamp(&last);
  FILE* finput = fopen(argv[1], "rb");
  if (finput == NULL) {
    printf("Input file cannot be open.");
    exit(1);
  }
  while ((isize = (int32_t)fread(data, 1, CHUNKSIZE, finput)) == CHUNKSIZE) {
    if (blosc2_schunk_append_buffer(schunk, data, isize) < 0) {
      fprintf(stderr, "Error in appending data to destination file");
      return -1;
    }
  }
  if (blosc2_schunk_append_buffer(schunk, data, isize) < 0) {
    fprintf(stderr, "Error in appending data to destination file");
    return -1;
  }
  fclose(finput);

  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Compression ratio: %.1f MB -> %.1f MB (%.1fx)\n",
         (float)nbytes / MB, (float)cbytes / MB, (1. * (float)nbytes) / (float)cbytes);
  printf("Compression time: %.3g s, %.1f MB/s\n",
         ttotal, (float)nbytes / (ttotal * MB));

  /* Free resources */
  blosc2_schunk_free(schunk);
  blosc2_destroy();
  return 0;
}
