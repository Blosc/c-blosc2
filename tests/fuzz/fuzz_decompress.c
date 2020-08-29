#include <stdint.h>
#include <stdlib.h>

#include "blosc2.h"

#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  size_t nbytes, cbytes, blocksize;
  void *output;

  if (size < BLOSC_MIN_HEADER_LENGTH) {
    return 0;
  }

  blosc_cbuffer_sizes(data, &nbytes, &cbytes, &blocksize);
  if (cbytes != size) {
    return 0;
  }
  if (nbytes == 0) {
    return 0;
  }

  if (blosc_cbuffer_validate(data, size, &nbytes) != 0) {
    /* Unexpected `nbytes` specified in blosc header */
    return 0;
  }

  output = malloc(cbytes);
  if (output != NULL) {
    blosc2_context *dctx;
    blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
    dctx = blosc2_create_dctx(dparams);
    blosc2_decompress_ctx(dctx, data, size, output, cbytes);
    blosc2_free_ctx(dctx);
    free(output);
  }
  return 0;
}

#ifdef __cplusplus
}
#endif
