/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

// Benchmark for concatenating b2nd arrays.  This is to check the fast path
// that has been added to b2nd_concat() that allows for faster concatenation of
// b2nd arrays when there are no partial (or zero-padded) chunks in the arrays being
// concatenated.

#include <inttypes.h>
#include "blosc2.h"
#include "b2nd.h"


int main() {
  blosc2_init();
  const int width = 1000;
  const int height = 1000;
  const int nimages_inbuf = 10;
  int64_t N_images = 1000;
  bool copy = false;  // whether to copy the data or expand src1

  // Shapes of the b2nd array
  // int64_t shape[] = {N_images, height, width};
  // The initial shape of the array before concatenation
  int64_t shape[] = {nimages_inbuf, height, width};
  int32_t chunkshape[] = {nimages_inbuf, height, width};
  int32_t blockshape[] = {1, height, width};

  // Determine the buffer size of the image (in bytes)
  const int64_t buffersize = nimages_inbuf * height * width * (int64_t)sizeof(uint16_t);
  uint16_t* image = malloc(buffersize);

  // Generate data
  for (int j = 0; j < nimages_inbuf * height * width; j++) {
    image[j] = j;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(image[0]);
  blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
  char *urlpath = "bench_concat.b2nd";
  //storage.urlpath = urlpath;  // for storing in a file
  storage.cparams = &cparams;

  char *accel_str;
  double t, t_accel;
  for (int accel=0; accel <= 1; accel++) {
    int32_t new_chunkshape[3] = {chunkshape[0], chunkshape[1], chunkshape[2]};
    if (!accel) {
      new_chunkshape[0] = chunkshape[0] + 1;  // to avoid the fast path
      accel_str = "non-fast path";
    }
    else {
      accel_str = "fast path";
    }

    blosc2_remove_urlpath(urlpath);

    b2nd_context_t *ctx = b2nd_create_ctx(&storage, 3,
                                          shape, new_chunkshape, blockshape,
                                          "|u2", DTYPE_NUMPY_FORMAT,
                                          NULL, 0);
    // The first array
    b2nd_array_t *src1;
    if (b2nd_empty(ctx, &src1) < 0) {
      printf("Error in b2nd_empty\n");
      return -1;
    }

    // The second array, with the data in buffersize
    b2nd_array_t *src2;
    int ret = b2nd_from_cbuffer(ctx, &src2, image, buffersize);
    if (ret < 0) {
      printf("Error in b2nd_from_cbuffer\n");
      return -1;
    }

    // Concatenate all images
    b2nd_array_t *array;
    blosc_timestamp_t t0, t1;
    blosc_set_timestamp(&t0);
    for (int i = 1; i < N_images / nimages_inbuf; i++) {
      if (b2nd_concatenate(ctx, src1, src2, 0, copy, &array) < 0) {
        printf("Error in b2nd_concatenate\n");
        return -1;
      }
      if (copy) {
        // If we copy, then we need to free the src1 array
        b2nd_free(src1);
      }
      src1 = array;
    }
    blosc_set_timestamp(&t1);
    if (!accel) {
      t = blosc_elapsed_secs(t0, t1);
      printf("Time to append (%s): %.4f s\n", accel_str, t);
    }
    else {
      t_accel = blosc_elapsed_secs(t0, t1);
      printf("Time to append (%s): %.4f s\n", accel_str, t_accel);
    }
    printf("Number of chunks: %" PRId64 "\n", array->sc->nchunks);
    // printf("Shape of array: (%" PRId64 ", %" PRId64 ", %" PRId64 ")\n",
    //        array->shape[0], array->shape[1], array->shape[2]);

    b2nd_free(src2);
    if (copy) {
      b2nd_free(array);
    }
    b2nd_free_ctx(ctx);
  }
  free(image);
  blosc2_remove_urlpath(urlpath);
  printf("Spedup: %.2fx\n", t / t_accel);

  blosc2_destroy();
  return 0;
}
