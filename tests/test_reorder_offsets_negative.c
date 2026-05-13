
#include <stdio.h>
#include "test_common.h"

#define CHUNKSIZE (100)
#define NTHREADS (1)

int main(void) {
  blosc2_init();

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  
  blosc2_storage storage = {.contiguous=false, .urlpath=NULL, .cparams=&cparams, .dparams=&dparams};
  blosc2_schunk* schunk = blosc2_schunk_new(&storage);

  int32_t data[CHUNKSIZE];
  for (int i = 0; i < CHUNKSIZE; i++) data[i] = i;
  
  blosc2_schunk_append_buffer(schunk, data, sizeof(data));
  blosc2_schunk_append_buffer(schunk, data, sizeof(data));

  printf("nchunks: %lld\n", (long long)schunk->nchunks);

  int64_t offsets_order[2] = {0, -1};
  printf("Calling blosc2_schunk_reorder_offsets with negative index...\n");
  int err = blosc2_schunk_reorder_offsets(schunk, offsets_order);
  printf("Return code: %d\n", err);

  blosc2_schunk_free(schunk);
  blosc2_destroy();

  return 0;
}
