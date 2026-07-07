/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  Test that a handle on a disk-based (super-chunk) frame stays coherent after
  another handle mutates a chunk: the stale handle must re-sync its cached view
  of the chunk offsets (instead of silently returning wrong values from
  blosc2_schunk_get_chunk and failing with an I/O error from
  blosc2_schunk_get_lazychunk).  See sframe_stale_handle_repro.c in examples.
*/

#include <stdio.h>
#include "test_common.h"

#define CHUNKSIZE (500)
#define NCHUNKS (10)

/* Global vars */
int tests_run = 0;
char* urlpath;
bool contiguous;


static void fill_schunk(void) {
  static int64_t data[CHUNKSIZE];
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int64_t);
  blosc2_storage storage = {.contiguous=contiguous, .urlpath=urlpath, .cparams=&cparams};
  blosc2_remove_urlpath(storage.urlpath);

  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = nchunk * CHUNKSIZE + i;
    }
    blosc2_schunk_append_buffer(schunk, data, CHUNKSIZE * sizeof(int64_t));
  }
  blosc2_schunk_free(schunk);
}


static int evict_chunk(blosc2_schunk* schunk, int64_t nchunk) {
  uint8_t uninit_chunk[BLOSC_EXTENDED_HEADER_LENGTH];
  int csize = blosc2_chunk_uninit(*schunk->storage->cparams, CHUNKSIZE * sizeof(int64_t),
                                  uninit_chunk, sizeof(uninit_chunk));
  if (csize < 0) {
    return csize;
  }
  int64_t nchunks_ = blosc2_schunk_update_chunk(schunk, nchunk, uninit_chunk, true);
  return nchunks_ < 0 ? (int)nchunks_ : 0;
}


/* Read one chunk through both entry points and check they agree. */
static char* check_chunk(blosc2_schunk* schunk, int64_t nchunk, int expected_size) {
  uint8_t* chunk;
  bool needs_free;

  int gsize = blosc2_schunk_get_chunk(schunk, nchunk, &chunk, &needs_free);
  mu_assert("ERROR: get_chunk does not return the expected size", gsize == expected_size);
  if (gsize >= 0 && needs_free) {
    free(chunk);
  }
  int lsize = blosc2_schunk_get_lazychunk(schunk, nchunk, &chunk, &needs_free);
  mu_assert("ERROR: get_lazychunk failed", lsize >= 0);
  if (needs_free) {
    free(chunk);
  }
  return EXIT_SUCCESS;
}


/* A stale reader handle must re-sync after another handle replaces a chunk
   with a special (UNINIT) one. */
static char* test_stale_reader(void) {
  static int64_t data_dest[CHUNKSIZE];
  fill_schunk();

  blosc2_schunk* h1 = blosc2_schunk_open(urlpath);
  blosc2_schunk* h2 = blosc2_schunk_open(urlpath);
  mu_assert("ERROR: cannot open the schunk twice", h1 != NULL && h2 != NULL);

  // Warm h2's cached view of the offsets
  uint8_t* chunk;
  bool needs_free;
  int old_size = blosc2_schunk_get_chunk(h2, 0, &chunk, &needs_free);
  mu_assert("ERROR: cannot read chunk 0 before the mutation", old_size > 0);
  if (needs_free) {
    free(chunk);
  }
  int old_size1 = blosc2_schunk_get_chunk(h2, 1, &chunk, &needs_free);
  mu_assert("ERROR: cannot read chunk 1 before the mutation", old_size1 > 0);
  if (needs_free) {
    free(chunk);
  }

  // h1 evicts chunk 0 behind h2's back
  mu_assert("ERROR: cannot evict chunk 0", evict_chunk(h1, 0) == 0);

  // h2 must now see the special chunk, identically on both entry points
  char* msg = check_chunk(h2, 0, BLOSC_EXTENDED_HEADER_LENGTH);
  if (msg != EXIT_SUCCESS) return msg;
  // ... still decompressible
  int dsize = blosc2_schunk_decompress_chunk(h2, 0, data_dest, sizeof(data_dest));
  mu_assert("ERROR: cannot decompress the special chunk on the stale handle", dsize >= 0);
  // ... and an untouched chunk keeps reading fine
  msg = check_chunk(h2, 1, old_size1);
  if (msg != EXIT_SUCCESS) return msg;

  // A fresh handle agrees with the stale one
  blosc2_schunk* h3 = blosc2_schunk_open(urlpath);
  mu_assert("ERROR: cannot re-open the schunk", h3 != NULL);
  msg = check_chunk(h3, 0, BLOSC_EXTENDED_HEADER_LENGTH);
  if (msg != EXIT_SUCCESS) return msg;

  blosc2_schunk_free(h1);
  blosc2_schunk_free(h2);
  blosc2_schunk_free(h3);
  blosc2_remove_urlpath(urlpath);
  return EXIT_SUCCESS;
}


/* A stale writer handle must re-sync before rewriting the offsets index;
   otherwise it would silently resurrect the chunk another handle replaced. */
static char* test_stale_writer(void) {
  fill_schunk();

  blosc2_schunk* h1 = blosc2_schunk_open(urlpath);
  blosc2_schunk* h2 = blosc2_schunk_open(urlpath);
  mu_assert("ERROR: cannot open the schunk twice", h1 != NULL && h2 != NULL);

  // Warm h2's cached view of the offsets
  uint8_t* chunk;
  bool needs_free;
  int old_size1 = blosc2_schunk_get_chunk(h2, 1, &chunk, &needs_free);
  mu_assert("ERROR: cannot read chunk 1 before the mutation", old_size1 > 0);
  if (needs_free) {
    free(chunk);
  }

  // h1 evicts chunk 0; then h2 (stale) evicts chunk 5
  mu_assert("ERROR: cannot evict chunk 0", evict_chunk(h1, 0) == 0);
  mu_assert("ERROR: cannot evict chunk 5", evict_chunk(h2, 5) == 0);

  // A fresh handle must see *both* evictions (h2 did not clobber h1's)
  blosc2_schunk* h3 = blosc2_schunk_open(urlpath);
  mu_assert("ERROR: cannot re-open the schunk", h3 != NULL);
  char* msg = check_chunk(h3, 0, BLOSC_EXTENDED_HEADER_LENGTH);
  if (msg != EXIT_SUCCESS) return msg;
  msg = check_chunk(h3, 5, BLOSC_EXTENDED_HEADER_LENGTH);
  if (msg != EXIT_SUCCESS) return msg;
  msg = check_chunk(h3, 1, old_size1);
  if (msg != EXIT_SUCCESS) return msg;

  blosc2_schunk_free(h1);
  blosc2_schunk_free(h2);
  blosc2_schunk_free(h3);
  blosc2_remove_urlpath(urlpath);
  return EXIT_SUCCESS;
}


/* If a sparse chunk file gets corrupted (truncated) without the offsets index
   changing, both entry points must fail the same way (no silent 0 from
   get_chunk while get_lazychunk errors out). */
static char* test_truncated_chunkfile(void) {
  fill_schunk();

  blosc2_schunk* h1 = blosc2_schunk_open(urlpath);
  mu_assert("ERROR: cannot open the schunk", h1 != NULL);

  // Warm the cached view of the offsets
  uint8_t* chunk;
  bool needs_free;
  int old_size = blosc2_schunk_get_chunk(h1, 0, &chunk, &needs_free);
  mu_assert("ERROR: cannot read chunk 0 before the truncation", old_size > 0);
  if (needs_free) {
    free(chunk);
  }

  // Truncate the file for chunk 0 behind the library's back
  char chunkfile[1024];
  snprintf(chunkfile, sizeof(chunkfile), "%s/%08X.chunk", urlpath, 0);
  FILE* fp = fopen(chunkfile, "wb");
  mu_assert("ERROR: cannot truncate the chunkfile", fp != NULL);
  fclose(fp);

  int gsize = blosc2_schunk_get_chunk(h1, 0, &chunk, &needs_free);
  mu_assert("ERROR: get_chunk should fail with BLOSC2_ERROR_FILE_READ",
            gsize == BLOSC2_ERROR_FILE_READ);
  int lsize = blosc2_schunk_get_lazychunk(h1, 0, &chunk, &needs_free);
  mu_assert("ERROR: get_lazychunk should fail with BLOSC2_ERROR_FILE_READ",
            lsize == BLOSC2_ERROR_FILE_READ);

  blosc2_schunk_free(h1);
  blosc2_remove_urlpath(urlpath);
  return EXIT_SUCCESS;
}


static char *all_tests(void) {
  // Sparse frame (the scenario from the report)
  urlpath = "test_stale_sframe.b2frame";
  contiguous = false;
  mu_run_test(test_stale_reader);
  mu_run_test(test_stale_writer);
  mu_run_test(test_truncated_chunkfile);

  // Contiguous frame on disk (same coherence guarantee, shared code path)
  urlpath = "test_stale_cframe.b2frame";
  contiguous = true;
  mu_run_test(test_stale_reader);
  mu_run_test(test_stale_writer);

  return EXIT_SUCCESS;
}


int main(void) {
  char* result;

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
