/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Generate inputs for fuzzer corpus

  To run:

  $ ./generate_inputs_corpus
Blosc version info: 2.0.0.beta.6.dev ($Date:: 2020-04-21 #$)

*** Creating simple frame for blosclz
Compression ratio: 1953.1 KB -> 32.1 KB (60.8x)
Compression time: 0.00402 s, 474.1 MB/s
Successfully created frame_simple-blosclz.b2frame

*** Creating simple frame for lz4
Compression ratio: 1953.1 KB -> 40.1 KB (48.7x)
Compression time: 0.00442 s, 431.7 MB/s
Successfully created frame_simple-lz4.b2frame

*** Creating simple frame for lz4hc
Compression ratio: 1953.1 KB -> 23.2 KB (84.1x)
Compression time: 0.00772 s, 246.9 MB/s
Successfully created frame_simple-lz4hc.b2frame

*** Creating simple frame for zlib
Compression ratio: 1953.1 KB -> 20.8 KB (94.1x)
Compression time: 0.00878 s, 217.2 MB/s
Successfully created frame_simple-zlib.b2frame

*** Creating simple frame for zstd
Compression ratio: 1953.1 KB -> 10.8 KB (180.2x)
Compression time: 0.00962 s, 198.2 MB/s
Successfully created frame_simple-zstd.b2frame

Process finished with exit code 0

 */

#include <stdio.h>
#include <assert.h>
#include <blosc2.h>

#define KB  (1024.)
#define MB  (1024*KB)
#define GB  (1024*KB)

#define CHUNKSIZE (50 * 1000)
#define NCHUNKS 10
#define NTHREADS 4


int create_cframe(const char* compname) {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  static int32_t data_dest2[CHUNKSIZE];
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int64_t nbytes, cbytes;
  int i, nchunk;
  blosc_timestamp_t last, current;
  double ttotal;
  int compcode = blosc2_compname_to_compcode(compname);
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
                            .urlpath=filename, .contiguous=true};
  remove(filename);
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);

  // Add some data
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      data[i] = i * nchunk;
    }
    BLOSC_ERROR(blosc2_schunk_append_buffer(schunk, data, (int32_t)isize));
  }
  blosc_set_timestamp(&current);

  // Add some vlmetalayers data
  int32_t content_len = 10;
  uint8_t *content = malloc(content_len);
  for (int32_t j = 0; j < content_len; ++j) {
    content[j] = (uint8_t) j;
  }
  int umlen = blosc2_vlmeta_add(schunk, "vlmetalayer", content, content_len, NULL);
  free(content);
  if (umlen < 0) {
    printf("Cannot write vlmetalayers chunk");
    return umlen;
  }

  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  ttotal = blosc_elapsed_secs(last, current);
  printf("Compression ratio: %.1f KB -> %.1f KB (%.1fx)\n",
         (double)nbytes / KB, (double)cbytes / KB, (1. * (double)nbytes) / (double)cbytes);
  printf("Compression time: %.3g s, %.1f MB/s\n",
         ttotal, (double)nbytes / (ttotal * MB));

  // Re-open the file and see if it is in a sane state
  blosc2_schunk* schunk2 = blosc2_schunk_open(filename);

  /* Retrieve and decompress the chunks from the super-chunks and compare values */
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    int32_t dsize = blosc2_schunk_decompress_chunk(schunk2, nchunk, data_dest, (int32_t)isize);
    if (dsize < 0) {
      printf("Decompression error in schunk2.  Error code: %d\n", dsize);
      return dsize;
    }
    dsize = blosc2_schunk_decompress_chunk(schunk2, nchunk, data_dest2, (int32_t)isize);
    if (dsize < 0) {
      printf("Decompression error in schunk2.  Error code: %d\n", dsize);
      return dsize;
    }
    /* Check integrity of this chunk */
    for (i = 0; i < CHUNKSIZE; i++) {
      assert (data_dest[i] == i * nchunk);
      assert (data_dest2[i] == i * nchunk);
    }
  }
  printf("Successfully created %s\n", filename);

  /* Free resources */
  blosc2_schunk_free(schunk);
  blosc2_schunk_free(schunk2);

  return 0;
}


int main(void) {
  blosc2_init();

  printf("Blosc version info: %s (%s)\n",
         BLOSC2_VERSION_STRING, BLOSC2_VERSION_DATE);

  create_cframe("blosclz");
  create_cframe("lz4");
  create_cframe("lz4hc");
  create_cframe("zlib");
  create_cframe("zstd");

  blosc2_destroy();
}
