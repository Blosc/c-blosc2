#include <stdio.h>
#include <blosc2.h>
#include <stdlib.h>

#define NCHUNKS 1
#define TYPESIZE 2
#define LEN 39
#define CHUNKSIZE (TYPESIZE * LEN)

int main(void) {
  blosc2_init();
  srand(0);
  if (blosc2_compname_to_compcode("zstd") < 0) {
    // We need ZSTD for the test here...
    return 0;
  }

  uint16_t *ref_data = (uint16_t *)malloc(CHUNKSIZE);
  uint16_t *data_dest = (uint16_t *)malloc(CHUNKSIZE);
  for (int i = 0; i < LEN; i++) {
    ref_data[i] = rand() % 118;
  }
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.compcode = BLOSC_ZSTD;
  cparams.filters[BLOSC2_MAX_FILTERS - 2] = BLOSC_BITSHUFFLE;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_SHUFFLE;

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;

  cparams.typesize = TYPESIZE;
  blosc2_storage storage = {.contiguous=false, .urlpath=NULL, .cparams=&cparams, .dparams=&dparams};

  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  blosc2_schunk_append_buffer(schunk, ref_data, CHUNKSIZE);

  blosc2_schunk_decompress_chunk(schunk, 0, data_dest, CHUNKSIZE);
  for (int i = 0; i < LEN; i++) {
    if (data_dest[i] != ref_data[i]) {
      printf("Decompressed data differs from original %d, %d, %d!\n",
          i, ref_data[i], data_dest[i]);
      return -1;
    }
  }

  printf("Successful roundtrip data <-> schunk !\n");

  blosc2_schunk_free(schunk);
  blosc2_destroy();
  return 0;
}
