/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
*/

#include <stdio.h>
#include "test_common.h"

#define CHUNKSIZE (200 * 1000)
#define NTHREADS (2)

/* Global vars */
int tests_run = 0;
int nchunks;


static char* test_schunk(void) {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  int64_t nbytes, cbytes;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;

  /* Initialize the Blosc compressor */
  blosc2_init();

  /* Create a super-chunk container */
  cparams.typesize = sizeof(int32_t);
  cparams.clevel = 5;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .dparams=&dparams};
  schunk = blosc2_schunk_new(&storage);

  // Add a couple of metalayers
  blosc2_meta_add(schunk, "metalayer1", (uint8_t *) "my metalayer1", sizeof("my metalayer1"));
  blosc2_meta_add(schunk, "metalayer2", (uint8_t *) "my metalayer1", sizeof("my metalayer1"));

  // Feed it with data
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk * CHUNKSIZE;
    }
    int64_t nchunks_ = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: bad append in frame", nchunks_ > 0);
  }

  blosc2_meta_update(schunk, "metalayer2", (uint8_t *) "my metalayer2", sizeof("my metalayer2"));

  char names_[2][13] = {"vlmetalayer1", "vlmetalayer2"};
  char **names = malloc(schunk->nvlmetalayers * sizeof (char*));
  int nvlmetas = blosc2_vlmeta_get_names(schunk, names);
  mu_assert("ERROR: wrong number of vlmetalayers", nvlmetas == 0);

  // Attach some user metadata into it
  blosc2_vlmeta_add(schunk, "vlmetalayer1", (uint8_t *) "testing the vlmetalayers", 23, NULL);

  free(names);
  names = malloc(schunk->nvlmetalayers * sizeof (char*));
  nvlmetas = blosc2_vlmeta_get_names(schunk, names);
  mu_assert("ERROR: wrong number of vlmetalayers", nvlmetas == 1);
  for (int i = 0; i < nvlmetas; ++i) {
    mu_assert("ERROR: wrong vlmetalayer name", strcmp(names[i], names_[i]) == 0);
  }
  blosc2_vlmeta_add(schunk, "vlmetalayer2", (uint8_t *) "vlmetalayers", 11, NULL);

  free(names);
  names = malloc(schunk->nvlmetalayers * sizeof (char*));
  nvlmetas = blosc2_vlmeta_get_names(schunk, names);
  mu_assert("ERROR: wrong number of vlmetalayers", nvlmetas == 2);
  for (int i = 0; i < nvlmetas; ++i) {
    mu_assert("ERROR: wrong vlmetalayer name", strcmp(names[i], names_[i]) == 0);
  }

  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  if (nchunks > 0) {
    mu_assert("ERROR: bad compression ratio in frame", nbytes > 10 * cbytes);
  }

  // Exercise the metadata retrieval machinery
  bool needs_free;
  uint8_t* chunk;
  size_t nbytes_, cbytes_, blocksize;
  nbytes = 0;
  cbytes = 0;
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    dsize = blosc2_schunk_get_chunk(schunk, nchunk, &chunk, &needs_free);
    mu_assert("ERROR: chunk cannot be retrieved correctly.", dsize >= 0);
    blosc1_cbuffer_sizes(chunk, &nbytes_, &cbytes_, &blocksize);
    nbytes += (int64_t)nbytes_;
    cbytes += (int64_t)cbytes_;
    if (needs_free) {
      free(chunk);
    }
  }
  mu_assert("ERROR: nbytes is not correct", nbytes == schunk->nbytes);
  mu_assert("ERROR: cbytes is not correct", cbytes == schunk->cbytes);

  // Check that the chunks have been decompressed correctly
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) data_dest, isize);
    mu_assert("ERROR: chunk cannot be decompressed correctly.", dsize >= 0);
    for (int i = 0; i < CHUNKSIZE; i++) {
      mu_assert("ERROR: bad roundtrip",data_dest[i] == i + nchunk * CHUNKSIZE);
    }
  }
  // update metalayer
  blosc2_vlmeta_update(schunk, "vlmetalayer1", (uint8_t *) "testing the  vlmetalayers", 24, NULL);

  free(names);
  names = malloc(schunk->nvlmetalayers * sizeof (char*));
  nvlmetas = blosc2_vlmeta_get_names(schunk, names);
  mu_assert("ERROR: wrong number of vlmetalayers", nvlmetas == 2);
  for (int i = 0; i < nvlmetas; ++i) {
    mu_assert("ERROR: wrong vlmetalayer name", strcmp(names[i], names_[i]) == 0);
  }
  // metalayers
  uint8_t* content;
  int32_t content_len;
  blosc2_meta_get(schunk, "metalayer1", &content, &content_len);
  mu_assert("ERROR: bad metalayer content", strncmp((char*)content, "my metalayer1", content_len) == 0);
  free(content);
  blosc2_meta_get(schunk, "metalayer2", &content, &content_len);
  mu_assert("ERROR: bad metalayer content", strncmp((char*)content, "my metalayer2", content_len) == 0);
  free(content);

  // Check the vlmetalayers
  uint8_t* content2;
  int32_t content2_len;
  blosc2_vlmeta_get(schunk, "vlmetalayer1", &content2, &content2_len);
  mu_assert("ERROR: bad vlmetalayer content", strncmp((char*)content2, "testing the  vlmetalayers", content2_len) == 0);

  blosc2_vlmeta_get(schunk, "vlmetalayer2", &content2, &content2_len);
  mu_assert("ERROR: bad vlmetalayer content", strncmp((char*)content2, "vlmetalayers", content2_len) == 0);
  free(content2);

  // Delete the second vlmetalayer
  int nvlmeta = blosc2_vlmeta_delete(schunk, "vlmetalayer2");
  mu_assert("ERROR: error while deleting the vlmetalayer", nvlmeta == 1);

  free(names);
  names = malloc(schunk->nvlmetalayers * sizeof (char*));
  nvlmetas = blosc2_vlmeta_get_names(schunk, names);
  mu_assert("ERROR: wrong number of vlmetalayers", nvlmetas == 1);
  for (int i = 0; i < nvlmetas; ++i) {
    mu_assert("ERROR: wrong vlmetalayer name", strcmp(names[i], names_[i]) == 0);
  }
  free(names);
  int rc = blosc2_vlmeta_exists(schunk, "vlmetalayer2");
  mu_assert("ERROR: the vlmetalayer was not deleted correctly", rc < 0);

  /* Free resources */
  blosc2_schunk_free(schunk);
  /* Destroy the Blosc environment */
  blosc2_destroy();

  return EXIT_SUCCESS;
}

static char *all_tests(void) {
  nchunks = 0;
  mu_run_test(test_schunk);

  nchunks = 1;
  mu_run_test(test_schunk);

  nchunks = 10;
  mu_run_test(test_schunk);

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
