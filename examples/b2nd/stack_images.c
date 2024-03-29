/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  The Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

// This is an example that saves a stack of images in a b2nd frame.

#include <blosc2.h>
#include <b2nd.h>


int main() {
  blosc2_init();
  const int32_t width = 4 * 512;
  const int32_t height = 4 * 272;
  const int64_t buffershape[] = {1, height, width};
  // Determine the buffer size of the image (in bytes)
  const int64_t buffersize = width * height * (int64_t)sizeof(uint16_t);
  uint16_t* image = malloc(buffersize);
  int64_t N_images = 10;

  char *urlpath = "test_image_dataset.b2nd";
  blosc2_remove_urlpath(urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(uint16_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 5;
  cparams.nthreads = 4;

  blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
  storage.contiguous = true;  // for a single file in output
  storage.cparams = &cparams;
  storage.urlpath = urlpath;

  // shape, chunkshape and blockshape of the ndarray
  int64_t shape[] = {N_images, height, width};
  int32_t chunkshape[] = {1, height, width};
  int32_t blockshape[] = {1, height, width};

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
  for (int64_t i = 0; i < N_images; i++) {
    printf("Saving image #: %lld\n", i);
    int64_t start[] = {i, 0, 0};
    int64_t stop[] = {i + 1, height, width};
    // Generate random image data
    for (int j = 0; j < width * height; j++) {
      image[j] = rand() % 65536; // generate random pixels (uncompressible data)
    }

    if (b2nd_set_slice_cbuffer(image, buffershape, buffersize, start, stop, src) < 0) {
      printf("Error in b2nd_append\n");
      return -1;
    }
  }

  // Clean resources
  b2nd_free(src);
  b2nd_free_ctx(ctx);
  blosc2_destroy();
  free(image);

  return 0;
}
