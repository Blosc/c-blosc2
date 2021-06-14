/*
  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include "test_common.h"
#include "cutest.h"

CUTEST_TEST_DATA(small_chunks) {
    bool fix_windows_compilation;
};

CUTEST_TEST_SETUP(small_chunks) {
  blosc_init();
}


CUTEST_TEST_TEST(small_chunks) {
    int rc;

    int8_t itemsize = 8;
    int32_t nitems = 50 * 1000;
    int32_t chunksize = 10 * itemsize;
    int32_t blocksize = 10 * itemsize;
    int64_t nchunks = nitems * itemsize / chunksize;

    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.blocksize = blocksize;
    blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
    storage.cparams = &cparams;
    storage.urlpath = "ex_update.caterva";
    blosc2_remove_dir("ex_update.caterva");

    storage.contiguous = false;
    blosc2_schunk *sc = blosc2_schunk_new(&storage);

    int32_t chunk_nbytes = itemsize + BLOSC_MAX_OVERHEAD;
    uint8_t *chunk = malloc(chunk_nbytes);
    int64_t rep_val = 8;
    blosc2_chunk_repeatval(cparams, chunksize, chunk, chunk_nbytes, &rep_val);

    for (int i = 0; i < nchunks; ++i) {
      rc = blosc2_schunk_append_chunk(sc, chunk, true);
      CUTEST_ASSERT("Can not append chunk", rc == i + 1);
    }
    free(chunk);

    bool needs_free;
    rc = blosc2_schunk_get_chunk(sc, 999, &chunk, &needs_free);
    if (rc < 0) {
      return BLOSC2_ERROR_FAILURE;
    }
    CUTEST_ASSERT("Can not get chunk", rc >= 0);
    
    if (needs_free) {
      free(chunk);
    }

    blosc2_schunk_free(sc);

    blosc2_remove_dir("ex_update.caterva");

    blosc_destroy();

    return 0;

}

CUTEST_TEST_TEARDOWN(small_chunks) {
  blosc_destroy();
}


int main() {
  CUTEST_TEST_RUN(small_chunks)
}
