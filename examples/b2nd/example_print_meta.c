/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  The Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* Example program demonstrating how to print metainfo from a b2nd frame.
 * You can build frames with example_frame_generator.c
 *
 * Usage:
 * $ ./example_print_meta <urlpath>
 *
 * Example of output:
 * $ ./example_print_meta example_big_float_frame.b2nd
 * Blosc2 NDim metalayer parameters:
 * Ndim:       3
 * Shape:      200, 310, 214
 * Chunkshape: 110, 120, 76
 * Blockshape: 57, 52, 35
 *
*/

# include <b2nd.h>

int print_meta(char *urlpath) {
  blosc_timestamp_t last, current;
  blosc_timestamp_t last2, current2;
  double nsecs = 0;
  b2nd_array_t *arr;

  BLOSC_ERROR(b2nd_open(urlpath, &arr));
  BLOSC_ERROR(b2nd_print_meta(arr));

  int r2 = 100;
  int cube_size = 20 * 1000 / r2;
  int r3 = 10;
  blosc_set_timestamp(&current2);
  for (int i = 0; i < r3; i++) {
    for (int j = 0; j < r3; j++) {
      blosc_set_timestamp(&last2);
      nsecs = blosc_elapsed_secs(current2, last2);
      printf("i, j: %d, %d (%.4f s)\n", i, j, nsecs);
      blosc_set_timestamp(&current2);
      for (int k = 0; k < r3; k++) {
        int64_t slice_start[] = {i * cube_size, j * cube_size, k * cube_size};
        int64_t slice_stop[] = {(i + 1) * cube_size, (j + 1) * cube_size, (k + 1) * cube_size};
        int64_t buffer_shape[] = {cube_size, cube_size, cube_size};

        blosc_set_timestamp(&current);
        int64_t buffersize = cube_size * cube_size * cube_size * sizeof(float);
        float *buffer = malloc(buffersize);
        // float *buffer = calloc(buffersize, 1);
        BLOSC_ERROR(b2nd_get_slice_cbuffer(arr, slice_start, slice_stop, buffer, buffer_shape, buffersize));
        blosc_set_timestamp(&last);
        nsecs = blosc_elapsed_secs(current, last);
        //printf("i, j, k: %d, %d, %d; first buffer element (time): %.2f (%.4f s)\n", i, j, k, buffer[0], nsecs);
        free(buffer);
      }
    }
  }

  BLOSC_ERROR(b2nd_free(arr));

  return 0;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s urlpath", argv[0]);
    exit(-1);
  }

  blosc2_init();

  char *urlpath = argv[1];
  int rc = print_meta(urlpath);

  blosc2_destroy();

  return rc;
}
