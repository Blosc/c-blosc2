/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

// This is an example that saves a stack of images in a b2nd frame.
// The images are generated randomly and saved in two different ways:
// 1) Using the b2nd_set_slice_cbuffer method.
// 2) Using the b2nd_append method.


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

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(uint16_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 5;
  cparams.nthreads = 4;

  blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
  storage.contiguous = true;  // for a single file in output
  char *urlpath = "example_stack_images_set_slice.b2nd";
  blosc2_remove_urlpath(urlpath);
  storage.urlpath = urlpath;
  storage.cparams = &cparams;

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

  // Loop through all images
  printf("Saving images (set_slice version)...\n");
  for (int64_t i = 0; i < N_images; i++) {
    int64_t start[] = {i, 0, 0};
    int64_t stop[] = {i + 1, height, width};
    // Generate random image data
    for (int j = 0; j < width * height; j++) {
      image[j] = rand() % 65536; // generate random pixels (uncompressible data)
    }

    if (b2nd_set_slice_cbuffer(image, buffershape, buffersize, start, stop, src) < 0) {
      printf("Error in b2nd_set_slice_cbuffer\n");
      return -1;
    }
  }
  printf("Adding vlmetalayer data\n");
  uint8_t msgpack[1024];
  // Pack the message using the recommended msgpack format
  // The Python wrapper can do this automatically
  char *content = "Using b2nd_set_slice_cbuffer()";
  msgpack[0] = 0xd9;
  msgpack[1] = strlen(content);
  memcpy(msgpack + 2, content, strlen(content) + 1);
  int metalen = blosc2_vlmeta_add(src->sc, "method",
                                 msgpack, strlen(content) + 2, NULL);
  if (metalen < 0) {
    printf("Cannot write vlmetalayer");
    return metalen;
  }
  b2nd_free_ctx(ctx);
  printf("Images saved successfully in %s\n", urlpath);

  // Use the append method to add more images
  urlpath = "example_stack_images_append.b2nd";
  blosc2_remove_urlpath(urlpath);
  storage.urlpath = urlpath;
  // shape can start with 0 now
  int64_t shape2[] = {0, height, width};

  ctx = b2nd_create_ctx(&storage, 3,
                         shape2, chunkshape, blockshape,
                         "|u2", DTYPE_NUMPY_FORMAT,
                         NULL, 0);
  b2nd_free(src);
  if (b2nd_empty(ctx, &src) < 0) {
    printf("Error in b2nd_empty\n");
    return -1;
  }

  // loop through all images
  printf("Saving images (append version)...\n");
  for (int64_t i = 0; i < N_images; i++) {
    // Generate random image data
    for (int j = 0; j < width * height; j++) {
      image[j] = rand() % 65536; // generate random pixels (uncompressible data)
    }

    if (b2nd_append(src, image, buffersize, 0) < 0) {
      printf("Error in b2nd_append\n");
      return -1;
    }
  }
  printf("Adding vlmetalayer data\n");
  // Pack the message using the recommended msgpack format
  // The Python wrapper can do this automatically
  content = "Using b2nd_append()";
  msgpack[0] = 0xd9;
  msgpack[1] = strlen(content);
  memcpy(msgpack + 2, content, strlen(content) + 1);
  metalen = blosc2_vlmeta_add(src->sc, "method",
                              msgpack, strlen(content) + 2, NULL);
  if (metalen < 0) {
    printf("Cannot write vlmetalayer");
    return metalen;
  }
  printf("Images saved successfully in %s\n", urlpath);

  // Clean resources
  b2nd_free(src);
  b2nd_free_ctx(ctx);
  blosc2_destroy();
  free(image);

  return 0;
}
