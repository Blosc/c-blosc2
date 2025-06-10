/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Unit test for the bitshuffle with blocks that are not aligned.
  See https://github.com/Blosc/python-blosc/issues/220
  Probably related: https://github.com/Blosc/c-blosc/issues/240

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
 **********************************************************************/

#include "test_common.h"

/* Global vars */
int tests_run = 0;
int size;
int32_t *data;
int32_t *data_out;
int32_t *data_dest;


static char* test_roundtrip_bitshuffle8(void) {
  /* Compress with bitshuffle active  */
  int isize = size;
  int osize = size + BLOSC_MIN_HEADER_LENGTH;
  int csize = blosc1_compress(9, BLOSC_BITSHUFFLE, 8, isize, data, data_out, osize);
  mu_assert("ERROR: Compression error", csize > 0);
  printf("Compression: %d -> %d (%.1fx)\n", isize, csize, (1.*isize) / csize);

  FILE *fout = fopen("test-bitshuffle8-nomemcpy.cdata", "wb");
  fwrite(data_out, csize, 1, fout);
  fclose(fout);

  /* Decompress  */
  int dsize = blosc1_decompress(data_out, data_dest, isize);
  mu_assert("ERROR: Decompression error.", dsize > 0);

  printf("Decompression successful!\n");

  int exit_code = memcmp(data, data_dest, size) ? EXIT_FAILURE : EXIT_SUCCESS;

  mu_assert("Decompressed data differs from original!", exit_code == EXIT_SUCCESS);

  return EXIT_SUCCESS;
}

static char* test_roundtrip_bitshuffle4(void) {
  /* Compress with bitshuffle active  */
  int isize = size;
  int osize = size + BLOSC_MIN_HEADER_LENGTH;
  int csize = blosc1_compress(9, BLOSC_BITSHUFFLE, 4, isize, data, data_out, osize);
  mu_assert("ERROR: Buffer is incompressible.  Giving up.", csize != 0);
  mu_assert("ERROR: Compression error.", csize > 0);
  printf("Compression: %d -> %d (%.1fx)\n", isize, csize, (1.*isize) / csize);

  FILE *fout = fopen("test-bitshuffle4-memcpy.cdata", "wb");
  fwrite(data_out, csize, 1, fout);
  fclose(fout);

  /* Decompress  */
  int dsize = blosc1_decompress(data_out, data_dest, isize);
  mu_assert("ERROR: Decompression error.", dsize >= 0);

  printf("Decompression successful!\n");

  int exit_code = memcmp(data, data_dest, size) ? EXIT_FAILURE : EXIT_SUCCESS;

  mu_assert("ERROR: Decompressed data differs from original!", exit_code == EXIT_SUCCESS);
  return EXIT_SUCCESS;
}

static char *all_tests(void) {
  /* `size` below is chosen so that it is not divisible by 8
   * (not supported by bitshuffle) and in addition, it is not
   * divisible by 8 (typesize) again.
   */

  size = 641092;
  data = malloc(size);
  data_out = malloc(size + BLOSC_MIN_HEADER_LENGTH);
  data_dest = malloc(size);

  /* Initialize data */
  for (int i = 0; i < (int) (size / sizeof(int32_t)); i++) {
    ((uint32_t*)data)[i] = i;
  }
  /* leftovers */
  for (int i = size / (int)sizeof(int32_t) * (int)sizeof(int32_t); i < size; i++) {
    ((uint8_t*)data)[i] = (uint8_t) i;
  }

  mu_run_test(test_roundtrip_bitshuffle4);

  mu_run_test(test_roundtrip_bitshuffle8);

  free(data);
  free(data_out);
  free(data_dest);

  return EXIT_SUCCESS;
}

int main(void) {
  char* result;

  blosc2_init();
  blosc2_set_nthreads(1);
  blosc1_set_compressor("lz4");
  printf("Blosc version info: %s (%s)\n", BLOSC2_VERSION_STRING, BLOSC2_VERSION_DATE);

  /* Run all the suite */
  result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  blosc2_destroy();

  return result != EXIT_SUCCESS;
}
