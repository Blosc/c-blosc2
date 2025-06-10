/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
*/

#include <stdio.h>
#include "test_common.h"

int tests_run = 0;

static char* test_lazy_chunk_memcpyed(void) {

  int cbytes, nbytes;

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;

  blosc2_storage storage = {.urlpath="update.b2frame", .contiguous=false, .cparams=&cparams};
  blosc2_remove_dir(storage.urlpath);

  blosc2_schunk *sc = blosc2_schunk_new(&storage);

  uint8_t buffer_b[] = {1};

  int32_t chunk_size = sc->typesize + BLOSC2_MAX_OVERHEAD;
  uint8_t *chunk = malloc(chunk_size);
  cbytes = blosc2_compress_ctx(sc->cctx, buffer_b, sc->typesize, chunk, chunk_size);
  mu_assert("ERROR: cbytes are incorrect", cbytes == 33);

  nbytes = blosc2_decompress_ctx(sc->dctx, chunk, chunk_size, buffer_b, sc->typesize);
  mu_assert("ERROR: nbytes are incorrect", nbytes == 1);

  blosc2_schunk_append_chunk(sc, chunk, false);

  uint8_t *chunk2;
  bool needs_free;
  int32_t chunk2_size = blosc2_schunk_get_chunk(sc, 0, &chunk2, &needs_free);
  nbytes = blosc2_decompress_ctx(sc->dctx, chunk2, chunk2_size, buffer_b, sc->typesize);
  mu_assert("ERROR: nbytes are incorrect", nbytes == 1);
  if (needs_free) {
    free(chunk2);
  }

  nbytes = blosc2_schunk_decompress_chunk(sc, 0, buffer_b, sc->typesize);
  mu_assert("ERROR: nbytes are incorrect", nbytes == 1);

  blosc2_remove_dir(storage.urlpath);

  return 0;
}

static char* test_lazy_chunk_memcpyed_nofilter(void) {
  int chunk_nitems = 2000;
  int blocksize = 2000 - sizeof(int32_t) * 4;  // make it not a multiple of chunk_nitems
  int cbytes, nbytes;

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_NOFILTER;
  cparams.blocksize = blocksize;
  cparams.typesize = sizeof(int32_t);

  blosc2_storage storage = {.urlpath="memcpyed_nofilter.b2frame", .contiguous=false, .cparams=&cparams};
  blosc2_remove_dir(storage.urlpath);

  blosc2_schunk *sc = blosc2_schunk_new(&storage);

  int32_t chunk_size = chunk_nitems * sc->typesize;
  int32_t *buffer_b = malloc(chunk_size);
  for (int i = 0; i < chunk_nitems; i++) {
    buffer_b[i] = i;
  }

  int dest_size = chunk_size + BLOSC2_MAX_OVERHEAD;
  uint8_t *chunk = malloc(dest_size);
  cbytes = blosc2_compress_ctx(sc->cctx, buffer_b, chunk_size, chunk, dest_size);
  mu_assert("ERROR: cbytes are incorrect", cbytes == dest_size);  // incompressible data

  nbytes = blosc2_decompress_ctx(sc->dctx, chunk, dest_size, buffer_b, chunk_size);
  mu_assert("ERROR: nbytes are incorrect", nbytes == chunk_size);

  blosc2_schunk_append_chunk(sc, chunk, false);

  uint8_t *chunk2;
  bool needs_free;
  int32_t chunk2_size = blosc2_schunk_get_chunk(sc, 0, &chunk2, &needs_free);
  nbytes = blosc2_decompress_ctx(sc->dctx, chunk2, chunk2_size, buffer_b, chunk_size);
  mu_assert("ERROR: nbytes are incorrect", nbytes == chunk_size);
  if (needs_free) {
    free(chunk2);
  }

  nbytes = blosc2_schunk_decompress_chunk(sc, 0, buffer_b, chunk_size);
  mu_assert("ERROR: nbytes are incorrect", nbytes == chunk_size);

  // Retrieve the last item using the lazy_chunk mechanism
  int32_t last_item;
  blosc2_schunk_get_slice_buffer(sc, chunk_nitems - 1, chunk_nitems, &last_item);
  mu_assert("ERROR: last value is incorrect", last_item == chunk_nitems - 1);

  blosc2_remove_dir(storage.urlpath);

  return 0;
}

static char *all_tests(void) {
  mu_run_test(test_lazy_chunk_memcpyed);
  mu_run_test(test_lazy_chunk_memcpyed_nofilter);

  return EXIT_SUCCESS;
}


int main(void) {
  char *result;

  install_blosc_callback_test(); /* optionally install callback test */
  blosc2_init();

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
