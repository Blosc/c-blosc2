/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
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

  b2nd_array_t *arr;
  BLOSC_ERROR(b2nd_open(urlpath, &arr));
  BLOSC_ERROR(b2nd_print_meta(arr));
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
  print_meta(urlpath);

  blosc2_destroy();

  return 0;
}
