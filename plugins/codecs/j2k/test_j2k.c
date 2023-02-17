/*********************************************************************
    Blosc - Blocked Shuffling and Compression Library

    Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
    https://blosc.org
    License: BSD 3-Clause (see LICENSE.txt)

    See LICENSE.txt for details about copyright and rights to use.

    Test program demonstrating use of the Blosc filter from C code.
    To compile this program:

    $ gcc -O test_j2k.c -o test_j2k -lblosc2

    To run:

    $ ./test_j2k
    Read    OK
    Write   OK

**********************************************************************/

#include <stdio.h>
#include "blosc2.h"
#include "blosc2/codecs-registry.h"
#include <inttypes.h>
#include "b2nd.h"
#include "blosc2_htj2k.h"
#include "math.h"


int teapot() {
  const char *ifname = "teapot.ppm";
  char *ofname = "teapot2.ppm";
  image_t image;

  // Read source file(s)
  printf("Read\t");
  if (htj2k_read_image(&image, ifname)) {
    return -1;
  }
  printf("OK\n");

  int8_t ndim = 3;
  int64_t shape[] = {3, image.width, image.height};
  int32_t chunkshape[] = {3, (int32_t) image.width, (int32_t) image.height};
  int32_t blockshape[] = {3, (int32_t) image.width, (int32_t) image.height};
  uint8_t itemsize = 4;

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.compcode = BLOSC_CODEC_J2K;
  cparams.typesize = itemsize;
  for (int i = 0; i < BLOSC2_MAX_FILTERS; i++) {
    cparams.filters[i] = 0;
  }

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_storage b2_storage = {.cparams=&cparams, .dparams=&dparams};

  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, ndim, shape, chunkshape, blockshape, NULL, 0, NULL, 0);

  b2nd_array_t *arr;
  BLOSC_ERROR(b2nd_from_cbuffer(ctx, &arr, image.buffer, image.buffer_len));
  if((arr->sc->nbytes <= 0) || (arr->sc->nbytes > image.buffer_len)) {
    printf("Compression error");
    return -1;
  }

  uint8_t *buffer;
  uint64_t buffer_size = itemsize;
  for (int i = 0; i < arr->ndim; ++i) {
    buffer_size *= arr->shape[i];
  }
  buffer = malloc(buffer_size);

  BLOSC_ERROR(b2nd_to_cbuffer(arr, buffer, buffer_size));
  double tolerance = 0.1;
  for (int i = 0; i < (buffer_size / itemsize); i++) {
    if ((image.buffer[i] == 0) || (buffer[i] == 0)) {
      if (abs(image.buffer[i] - buffer[i]) > tolerance) {
        printf("i: %d, data %d, dest %d", i, image.buffer[i], buffer[i]);
        printf("\n Decompressed data differs too much from original!\n");
        return -1;
      }
    } else if (abs(image.buffer[i] - buffer[i]) > tolerance * fmaxf(image.buffer[i], buffer[i])) {
      printf("i: %d, data %d, dest %d", i, image.buffer[i], buffer[i]);
      printf("\n Decompressed data differs too much from original!\n");
      return -1;
    }
  }

  // Write output file
  printf("Write\t");
  htj2k_write_ppm(buffer, (int64_t) buffer_size, &image, ofname);
  printf("OK\n");

  BLOSC_ERROR(b2nd_free_ctx(ctx));
  BLOSC_ERROR(b2nd_free(arr));
  free(buffer);
  htj2k_free_image(&image);
  return BLOSC2_ERROR_SUCCESS;
}


int main(void) {

  int result;
  blosc2_init();   // this is mandatory for initiallizing the plugin mechanism
  result = teapot();
  printf("teapot: %d obtained \n \n", result);
  if (result < 0)
    return result;
  blosc2_destroy();

  return BLOSC2_ERROR_SUCCESS;

}
