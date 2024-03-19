/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* Example program demonstrating use of the Blosc plugins from C code.
*
* To compile this program:
* $ gcc example_plugins_codecs.c -o example_plugins_codecs -lblosc2
*
* To run:
* $ ./example_plugins_codecs
*
* from_buffer: 0.0668 s
* to_buffer: 0.0068 s
* Process finished with exit code 0
*/



#include <b2nd.h>
#include <stdio.h>
#include <blosc2.h>
#include "../../plugins/codecs/codecs-registry.c"
#include <inttypes.h>

int main() {
  blosc_timestamp_t t0, t1;

  blosc2_init();
  int8_t ndim = 2;
  uint8_t typesize = sizeof(int64_t);

  int64_t shape[] = {745, 400};
  int32_t chunkshape[] = {150, 100};
  int32_t blockshape[] = {21, 30};

  int64_t nbytes = typesize;
  for (int i = 0; i < ndim; ++i) {
    nbytes *= shape[i];
  }

  int64_t *src = malloc((size_t) nbytes);
  for (int i = 0; i < nbytes / typesize; ++i) {
    src[i] = (int64_t) i;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 1;
  /*
   * Use the NDLZ codec through its plugin.
   * NDLZ metainformation: - it calls the 4x4 version if meta == 4
                           - it calls the 8x8 version if meta == 8
  */
  cparams.compcode = BLOSC_CODEC_NDLZ;
  cparams.splitmode = BLOSC_ALWAYS_SPLIT;
  cparams.compcode_meta = 4;
  cparams.clevel = 5;
  cparams.typesize = typesize;
  // We could use a filter plugin by setting cparams.filters[].

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_storage b2_storage = {.cparams=&cparams, .dparams=&dparams};

  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, ndim, shape, chunkshape, blockshape, NULL, 0,
                                        NULL, 0);
  BLOSC_ERROR_NULL(ctx, -1);
  b2nd_array_t *arr;
  blosc_set_timestamp(&t0);
  BLOSC_ERROR(b2nd_from_cbuffer(ctx, &arr, src, nbytes));
  blosc_set_timestamp(&t1);
  printf("from_buffer: %.4f s\n", blosc_elapsed_secs(t0, t1));

  int64_t *buffer = malloc(nbytes);
  int64_t buffer_size = nbytes;
  blosc_set_timestamp(&t0);
  BLOSC_ERROR(b2nd_to_cbuffer(arr, buffer, buffer_size));
  blosc_set_timestamp(&t1);
  printf("to_buffer: %.4f s\n", blosc_elapsed_secs(t0, t1));

  for (int i = 0; i < buffer_size / typesize; i++) {
    if (src[i] != buffer[i]) {
      printf("\n Decompressed data differs from original!\n");
      printf("i: %d, data %" PRId64 ", dest %" PRId64 "", i, src[i], buffer[i]);
      return -1;
    }
  }

  free(src);
  free(buffer);

  BLOSC_ERROR(b2nd_free(arr));
  BLOSC_ERROR(b2nd_free_ctx(ctx));

  blosc2_destroy();

  return 0;
}
