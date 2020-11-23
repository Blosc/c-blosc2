#include <stdint.h>
#include <stdlib.h>

#include <blosc2.h>

#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  size_t nbytes = 0, cbytes = 0, blocksize = 0;
  void *output = NULL;

  if (size < BLOSC_MIN_HEADER_LENGTH) {
    return 0;
  }

  blosc_init();
  blosc_set_nthreads(1);
  blosc_cbuffer_sizes(data, &nbytes, &cbytes, &blocksize);

  if (cbytes != size || nbytes == 0) {
    blosc_destroy();
    return 0;
  }
  if (blosc_cbuffer_validate(data, size, &nbytes) != 0) {
    /* Unexpected `nbytes` specified in blosc header */
    blosc_destroy();
    return 0;
  }

  output = malloc(cbytes);
  if (output != NULL) {
    blosc2_decompress(data, size, output, cbytes);
    free(output);
  }

  blosc_destroy();
  return 0;
}

#ifdef __cplusplus
}
#endif
