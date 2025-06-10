/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

// Benchmark for appending data to a b2nd array.  A new accelerated path has been
// added to b2nd_append() that allows for faster appending of data to a b2nd array
// when data to append is of the same size as the chunkshape.  This benchmark
// compares the performance of the new accelerated path with the old one.

#include <inttypes.h>
#include "blosc2.h"
#include "b2nd.h"


int main() {
  blosc2_init();
  const int width = 512;
  const int height = 256;
  const int nimages_inbuf = 10;
  const int64_t buffershape[] = {nimages_inbuf, height, width};
  int64_t N_images = 1000;

  // Shapes of the b2nd array
  int64_t shape[] = {N_images, height, width};
  int32_t chunkshape[] = {nimages_inbuf, height, width};
  int32_t blockshape[] = {1, height, width};

  // Determine the buffer size of the image (in bytes)
  const int64_t buffersize = nimages_inbuf * height * width * (int64_t)sizeof(uint16_t);
  uint16_t* image = malloc(buffersize);

  // Generate data
  for (int j = 0; j < nimages_inbuf * height * width; j++) {
    image[j] = j;
  }

  char *urlpath = "bench_stack_append.b2nd";
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(image[0]);
  blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
  //storage.contiguous = true;  // for a single file in output
  //storage.urlpath = urlpath;
  storage.cparams = &cparams;

  char *accel_str = "non-accel";
  for (int accel=0; accel < 2; accel++) {
    if (accel) {
      shape[0] = 0;
      accel_str = "accel";
    }

    blosc2_remove_urlpath(urlpath);

    b2nd_context_t *ctx = b2nd_create_ctx(&storage, 3,
                                          shape, chunkshape, blockshape,
                                          "|u2", DTYPE_NUMPY_FORMAT,
                                          NULL, 0);
    b2nd_array_t *src;
    if (b2nd_empty(ctx, &src) < 0) {
      printf("Error in b2nd_empty\n");
      return -1;
    }

    // loop through all images
    blosc_timestamp_t t0, t1;
    blosc_set_timestamp(&t0);
    for (int i = 0; i < N_images / nimages_inbuf; i++) {
      if (accel) {
        if (b2nd_append(src, image, buffersize, 0) < 0) {
          printf("Error in b2nd_append\n");
          return -1;
        }
      } else {
        int64_t start[] = {i, 0, 0};
        int64_t stop[] = {i + nimages_inbuf, height, width};
        if (b2nd_set_slice_cbuffer(image, buffershape, buffersize, start, stop, src) < 0) {
          printf("Error in b2nd_append\n");
          return -1;
        }
      }
    }
    blosc_set_timestamp(&t1);
    printf("Time to append (%s): %.4f s\n", accel_str, blosc_elapsed_secs(t0, t1));
    printf("Number of chunks: %" PRId64 "\n", src->sc->nchunks);
    printf("Shape of array: (%" PRId64 ", %" PRId64 ", %" PRId64 ")\n",
           src->shape[0], src->shape[1], src->shape[2]);

    b2nd_free(src);
    b2nd_free_ctx(ctx);
  }
  free(image);

  blosc2_destroy();
  return 0;
}
