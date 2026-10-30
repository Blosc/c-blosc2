/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
*/

/* Check blosc2_getitem_ctx() for any start, with numbers of items around the
   block size, so that the items may or may not start at a block boundary and
   span one or more blocks (including a leftover block). */

#include "config.h"
#include "test_common.h"

#define NITEMS 250       // 15 full blocks and a leftover block
#define BLOCKITEMS 16

int tests_run = 0;


static char* check_getitem_ranges(int compcode, int clevel) {
  int32_t data[NITEMS];
  for (int i = 0; i < NITEMS; i++) {
    data[i] = i * 3 + 1;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = (uint8_t) compcode;
  cparams.clevel = (uint8_t) clevel;
  cparams.blocksize = BLOCKITEMS * sizeof(int32_t);
  cparams.nthreads = 1;
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  uint8_t chunk[sizeof(data) + BLOSC2_MAX_OVERHEAD];
  int cbytes = blosc2_compress_ctx(cctx, data, sizeof(data), chunk, sizeof(chunk));
  blosc2_free_ctx(cctx);
  mu_assert("ERROR: cannot compress", cbytes > 0);

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  blosc2_context *dctx = blosc2_create_dctx(dparams);
  const int counts[] = {1, BLOCKITEMS - 1, BLOCKITEMS, BLOCKITEMS + 1, 2 * BLOCKITEMS, 2 * BLOCKITEMS + 1};
  int32_t items[NITEMS];
  for (int start = 0; start < NITEMS; start++) {
    for (size_t c = 0; c < sizeof(counts) / sizeof(counts[0]); c++) {
      int count = counts[c];
      if (start + count > NITEMS) {
        continue;
      }
      memset(items, 0, sizeof(items));
      int nbytes = blosc2_getitem_ctx(dctx, chunk, cbytes, start, count, items, (int32_t) sizeof(items));
      if (nbytes != count * (int) sizeof(int32_t) ||
          memcmp(items, data + start, (size_t) count * sizeof(int32_t)) != 0) {
        printf("(compcode %d, clevel %d, start %d, nitems %d, returned %d) ",
               compcode, clevel, start, count, nbytes);
        blosc2_free_ctx(dctx);
        return "ERROR: getitem returned wrong items";
      }
    }
  }
  blosc2_free_ctx(dctx);
  return 0;
}


static char* test_getitem_ranges(void) {
  const int compcodes[] = {
      BLOSC_BLOSCLZ, BLOSC_LZ4,
#ifdef HAVE_ZSTD
      BLOSC_ZSTD,
#endif
  };
  char *msg;
  for (size_t i = 0; i < sizeof(compcodes) / sizeof(compcodes[0]); i++) {
    // clevel 0 gives a memcpyed chunk
    for (int clevel = 0; clevel <= 5; clevel += 5) {
      msg = check_getitem_ranges(compcodes[i], clevel);
      if (msg != NULL) {
        return msg;
      }
    }
  }
  return 0;
}


static char *all_tests(void) {
  mu_run_test(test_getitem_ranges);
  return 0;
}


int main(void) {
  char *result;

  install_blosc_callback_test(); /* optionally install callback test */
  blosc2_init();

  /* Run all the suite */
  result = all_tests();
  if (result != 0) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  blosc2_destroy();

  return result != 0;
}
