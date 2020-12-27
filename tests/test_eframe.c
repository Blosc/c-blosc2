/*
  Copyright (C) 2020 The Blosc Developers
  http://blosc.org
  License: BSD (see LICENSE.txt)

  Creation date: 2020-12-02

  See LICENSE.txt for details about copyright and rights to use.
*/

#include <stdio.h>
#include "test_common.h"

#define CHUNKSIZE (200 * 1000)
#define NTHREADS (2)

/* Global vars */
int tests_run = 0;
int nchunks;
char* directory;



static char* test_eframe(void) {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;

  /* Initialize the Blosc compressor */
  blosc_init();

  /* Create a super-chunk container */
  cparams.typesize = sizeof(int32_t);
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {false, directory, .cparams=&cparams, .dparams=&dparams};
  schunk = blosc2_schunk_new(storage);

  // Feed it with data
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk;
    }
    int nchunks_ = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: bad append in eframe", nchunks_ > 0);
  }

  /* Retrieve and decompress the chunks (0-based count) */
  for (int nchunk = nchunks-1; nchunk >= 0; nchunk--) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, data_dest, isize);
    mu_assert("Decompression error", dsize>=0);
  }

  if (nchunks >= 2) {
    /* Check integrity of the second chunk (made of non-zeros) */
    blosc2_schunk_decompress_chunk(schunk, 1, data_dest, isize);
    for (int i = 0; i < CHUNKSIZE; i++) {
      mu_assert("Decompressed data differs from original",data_dest[i]==(i+1));
    }
  }

  /* Remove directory */
  remove_dir(storage.path);
  /* Free resources */
  blosc2_schunk_free(schunk);
  /* Destroy the Blosc environment */
  blosc_destroy();
  return EXIT_SUCCESS;
}


static char* test_metalayers(void) {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_schunk* schunk;

  /* Initialize the Blosc compressor */
  blosc_init();

  /* Create a super-chunk container */
  cparams.typesize = sizeof(int32_t);
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {false, directory, .cparams=&cparams, .dparams=&dparams};
  schunk = blosc2_schunk_new(storage);

  // Add some metalayers (one must add metalayers prior to actual data)
  blosc2_add_metalayer(schunk, "my_metalayer1", (uint8_t *) "my_content1",
                       (uint32_t) strlen("my_content1"));
  blosc2_add_metalayer(schunk, "my_metalayer2", (uint8_t *) "my_content1",
                       (uint32_t) strlen("my_content1"));

  // Feed it with data
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk;
    }
    int nchunks_ = blosc2_schunk_append_buffer(schunk, data, isize);
    mu_assert("ERROR: bad append in eframe", nchunks_ > 0);
  }

  // Update a metalayer (this is fine as long as the new content does not exceed the size of the previous one)
  blosc2_update_metalayer(schunk, "my_metalayer2", (uint8_t *) "my_content2",
                          (uint32_t) strlen("my_content2"));

  blosc2_storage storage2 = {false, directory, .cparams=&cparams, .dparams=&dparams};
  blosc2_schunk* schunk2 = blosc2_schunk_open(storage2);
  mu_assert("ERROR: Cannot get the schunk from eframe", schunk2 != NULL);

  // Check that the metalayers had a good roundtrip
  mu_assert("ERROR: nclients not retrieved correctly", schunk2->nmetalayers == 2);

  uint8_t* content;
  uint32_t content_len;

  int nmetalayer = blosc2_get_metalayer(schunk2, "my_metalayer1", &content, &content_len);
  mu_assert("ERROR: metalayer not found", nmetalayer >= 0);

  mu_assert("ERROR: serialized content for metalayer not retrieved correctly", memcmp(content, "my_content1", content_len) == 0);

  free(content);


  /* Remove directory */
  remove_dir(storage.path);
  /* Free resources */
  blosc2_schunk_free(schunk);
  blosc2_schunk_free(schunk2);
  /* Destroy the Blosc environment */
  blosc_destroy();
  return EXIT_SUCCESS;
}


static char *all_tests(void) {
  directory = "dir1";

  nchunks = 0;
  mu_run_test(test_eframe);

  nchunks = 1;
  mu_run_test(test_eframe);

  nchunks = 10;
  mu_run_test(test_eframe);

  nchunks = 100;
  mu_run_test(test_eframe);

  mu_run_test(test_metalayers);

  directory = "dir1/";
  nchunks = 0;
  mu_run_test(test_eframe);

  nchunks = 1;
  mu_run_test(test_eframe);

  nchunks = 10;
  mu_run_test(test_eframe);

  nchunks = 100;
  mu_run_test(test_eframe);

  mu_run_test(test_metalayers);

  return EXIT_SUCCESS;
}


int main(void) {
  char *result;

  install_blosc_callback_test(); /* optionally install callback test */
  blosc_init();

  /* Run all the suite */
  result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  blosc_destroy();

  return result != EXIT_SUCCESS;
}
