#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <blosc2.h>

#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  const char *compressors[] = { "blosclz", "lz4", "lz4hc", "zlib", "zstd" };
  int32_t i = 0, dsize = 0, filter = BLOSC_BITSHUFFLE;
  int32_t nchunk = 0,  nchunks = 0, max_chunksize = 512;

  blosc_init();

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  /* Find next available compressor */
  cparams.compcode = 0;
  while (blosc_set_compressor(compressors[cparams.compcode % 6]) == -1 && i < 6) {
    cparams.compcode++, i++;
  }
  if (i == 6) {
    /* No compressors available */
    blosc_destroy();
    return 0;
  }
  if (size > 0) {
    /* Variable size compression level and max chunksize */
    cparams.clevel = data[0] % (9 + 1);
    max_chunksize *= data[0];
  }
  if (size > 1) {
    filter = data[1];
  }
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = filter % (BLOSC_BITSHUFFLE + 1);
  cparams.filters_meta[BLOSC2_MAX_FILTERS - 1] = filter;
  cparams.nthreads = 1;

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;

  /* Create a super-chunk backed by an in-memory frame */
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    blosc_destroy();
    return 0;
  }

  /* Compress data */
  int32_t chunksize = max_chunksize;
  for (i = 0; chunksize > 0 && i < (int32_t)size; i += chunksize, nchunks++) {
    if (i + chunksize > (int32_t)size)
      chunksize = size - i;
    nchunks = blosc2_schunk_append_buffer(schunk, (uint8_t *)data + i, chunksize);
    if (nchunks < 0) {
      printf("Compression error.  Error code: %d\n", nchunks);
      break;
    }
  }

  /* Decompress data */
  uint8_t *uncompressed_data = (uint8_t *)malloc(size+1);
  if (uncompressed_data != NULL) {
    for (i = 0, nchunk = 0; nchunk < nchunks-1; nchunk++) {
      dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, uncompressed_data + i, chunksize);
      if (dsize < 0) {
        printf("Decompression error.  Error code: %d\n", dsize);
        break;
      }
      i += dsize;
    }

    /* Compare decompressed data with original */
    if (size > 0 && nchunks > 0) {
      if (dsize < 0 || memcmp(data, uncompressed_data, size) != 0) {
        printf("Decompression data does not match original %d\n", dsize);
      }
    }

    free(uncompressed_data);
  }

  /* Free resources */
  blosc2_schunk_free(schunk);

  blosc_destroy();
  return 0;
}

#ifdef __cplusplus
}
#endif
