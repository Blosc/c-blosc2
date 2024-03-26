/*
  Copyright (c) 2024  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  $ gcc decompress_file.c -o decompress_file -lblosc2

  Example usage for compression/decompression verification:

  $ sha512sum compress_file
  385c93c..feaf38dbec  compress_file
  $ ./compress_file compress_file compress_file.bl2
  Blosc version info: 2.13.2.dev ($Date:: 2023-01-25 #$)
  Compression ratio: 5.1 MB -> 2.0 MB (2.5x)
  Compression time: 0.07 s, 72.8 MB/s
  $ ./decompress_file compress_file.bl2 compress_file.1
  Blosc version info: 2.13.2.dev ($Date:: 2023-01-25 #$)
  Decompression ratio: 2.0 MB -> 5.1 MB (0.4x)
  Decompression time: 0.0343 s, 148.5 MB/s
  $ sha512sum compress_file.1
  385c93c..feaf38dbec  compress_file.1

 */

#include <stdio.h>
#include <blosc2.h>

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

int main(int argc, char* argv[]) {
  blosc2_init();
  static char* data;
  int32_t dsize;
  int64_t nbytes, cbytes;
  blosc_timestamp_t last, current;
  double ttotal;

  if (argc != 3) {
    fprintf(stderr, "Usage: decompress_file input_file.b2frame output_file\n");
    return -1;
  }

  printf("Blosc version info: %s (%s)\n",
         BLOSC2_VERSION_STRING, BLOSC2_VERSION_DATE);

  /* Open an existing super-chunk that is on-disk (frame). */
  blosc2_schunk* schunk = blosc2_schunk_open(argv[1]);

  data = (char*)malloc(schunk->chunksize);

  // Decompress the file
  blosc_set_timestamp(&last);
  FILE* foutput = fopen(argv[2], "wb");
  if (foutput == NULL) {
    printf("Output file cannot be open.");
    exit(1);
  }
  for (int nchunk = 0; nchunk < schunk->nchunks; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, data, schunk->chunksize);
    if (dsize < 0) {
      fprintf(stderr, "Decompression error.  Error code: %d\n", dsize);
      return dsize;
    }
    fwrite(data, dsize, 1, foutput);
  }
  fclose(foutput);

  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Decompression ratio: %.1f MB -> %.1f MB (%.1fx)\n",
         (float)cbytes / MB, (float)nbytes / MB, (1. * (float)cbytes) / (float)nbytes);
  printf("Decompression time: %.3g s, %.1f MB/s\n",
         ttotal, (float)nbytes / (ttotal * MB));

  /* Free resources */
  free(data);
  blosc2_schunk_free(schunk);
  blosc2_destroy();
  return 0;
}
