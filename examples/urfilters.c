/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

   Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  $ gcc urfilters.c -o urfilters -lblosc2

  To run:

  $ ./urfilters

 */


#include "stdio.h"
#include <blosc2.h>

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE (1000 * 1000)
#define NCHUNKS 100
#define NTHREADS 4


int filter_forward(const uint8_t* src, uint8_t* dest, int32_t size, uint8_t meta, blosc2_cparams *cparams,
                   uint8_t id) {
  BLOSC_UNUSED_PARAM(meta);
  BLOSC_UNUSED_PARAM(id);
  blosc2_schunk *schunk = cparams->schunk;

  for (int i = 0; i < size / schunk->typesize; ++i) {
    switch (schunk->typesize) {
      case 8:
        ((int64_t *) dest)[i] = ((int64_t *) src)[i] + 1;
        break;
      case 4:
        ((int32_t *) dest)[i] = ((int32_t *) src)[i] + 1;
        break;
      case 2:
        ((int16_t *) dest)[i] = (int16_t)(((int16_t *) src)[i] + 1);
        break;
      default:
        fprintf(stderr, "Item size %d not supported", schunk->typesize);
        return BLOSC2_ERROR_FAILURE;
    }
  }
  return BLOSC2_ERROR_SUCCESS;
}

int filter_backward(const uint8_t* src, uint8_t* dest, int32_t size, uint8_t meta, blosc2_dparams *dparams,
                    uint8_t id) {
  BLOSC_UNUSED_PARAM(meta);
  BLOSC_UNUSED_PARAM(id);
  blosc2_schunk *schunk = dparams->schunk;

  for (int i = 0; i < size / schunk->typesize; ++i) {
    switch (schunk->typesize) {
      case 8:
        ((int64_t *) dest)[i] = ((int64_t *) src)[i] - 1;
        break;
      case 4:
        ((int32_t *) dest)[i] = ((int32_t *) src)[i] - 1;
        break;
      case 2:
        ((int16_t *) dest)[i] = (int16_t)(((int16_t *) src)[i] - 1);
        break;
      default:
        fprintf(stderr, "Item size %d not supported", schunk->typesize);
        return BLOSC2_ERROR_FAILURE;
    }
  }
  return BLOSC2_ERROR_SUCCESS;
}

int main(void) {
  blosc2_init();

  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  int64_t nbytes, cbytes;

  blosc2_filter urfilter;
  urfilter.id = 250;
  urfilter.name = "urfilter_example";
  urfilter.version = 1;
  urfilter.forward = filter_forward;
  urfilter.backward = filter_backward;

  blosc2_register_filter(&urfilter);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.filters[4] = urfilter.id;
  cparams.filters_meta[4] = 0;

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;

  blosc2_schunk* schunk;
  int i, nchunk;
  blosc_timestamp_t last, current;
  double ttotal;

  printf("Blosc version info: %s (%s)\n", blosc2_get_version_string(), BLOSC2_VERSION_DATE);

  /* Create a super-chunk container */
  cparams.typesize = sizeof(int32_t);
  cparams.clevel = 9;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams};
  schunk = blosc2_schunk_new(&storage);

  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      data[i] = i * nchunk;
    }
    int64_t nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    if (nchunks != nchunk + 1) {
      printf("Unexpected nchunks!");
      return -1;
    }
  }
  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Compression ratio: %.1f MB -> %.1f MB (%.1fx)\n",
         (double)nbytes / MB, (double)cbytes / MB, (1. * (double)nbytes) / (double)cbytes);
  printf("Compression time: %.3g s, %.1f MB/s\n",
         ttotal, (double)nbytes / (ttotal * MB));

  /* Retrieve and decompress the chunks (0-based count) */
  blosc_set_timestamp(&last);
  for (nchunk = NCHUNKS-1; nchunk >= 0; nchunk--) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, data_dest, isize);
    if (dsize < 0) {
      printf("Decompression error.  Error code: %d\n", dsize);
      return dsize;
    }
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Decompression time: %.3g s, %.1f MB/s\n",
         ttotal, (double)nbytes / (ttotal * MB));

  /* Check integrity of the second chunk (made of non-zeros) */
  blosc2_schunk_decompress_chunk(schunk, 1, data_dest, isize);
  for (i = 0; i < CHUNKSIZE; i++) {
    if (data_dest[i] != i) {
      printf("Decompressed data differs from original %d, %d!\n",
             i, data_dest[i]);
      return -1;
    }
  }

  printf("Successful roundtrip data <-> schunk !\n");

  /* Free resources */
  /* Destroy the super-chunk */
  blosc2_schunk_free(schunk);

  blosc2_destroy();

  return 0;
}
