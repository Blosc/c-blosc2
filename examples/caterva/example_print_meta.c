/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* Example program demonstrating how to print metainfo from a caterva frame.
 * You can build frames with example_frame_generator.c
 *
 * Usage:
 * $ ./example_print_meta <urlpath>
 *
 * Example of output:
 * $ ./caterva_example_print_meta example_big_float_frame.caterva
 * Caterva metalayer parameters:
 * Ndim:       3
 * Shape:      200, 310, 214
 * Chunkshape: 110, 120, 76
 * Blockshape: 57, 52, 35
 *
*/

# include <caterva.h>

int print_meta(char *urlpath) {

  caterva_array_t *arr;
  CATERVA_ERROR(caterva_open(urlpath, &arr));
  caterva_print_meta(arr);
  CATERVA_ERROR(caterva_free(&arr));

  return 0;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s urlpath", argv[0]);
    exit(-1);
  }

  char *urlpath = argv[1];
  print_meta(urlpath);

  return 0;
}
