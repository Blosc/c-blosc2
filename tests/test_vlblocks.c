/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "frame.h"
#include "test_common.h"

int tests_run = 0;

typedef struct {
  bool contiguous;
  const char *urlpath;
} test_data;

static test_data tdata;

static test_data backends[] = {
    {false, NULL},
    {true, "test_vlblocks.b2frame"},
    {false, "test_vlblocks_s.b2frame"},
};

static const char block0[] = "red";
static const char block1[] = "green-green";
static const char block2[] = "blue-blue-blue-blue";

static const uint8_t *srcs[] = {
    (const uint8_t *)block0,
    (const uint8_t *)block1,
    (const uint8_t *)block2,
};

static const int32_t srcsizes[] = {
    (int32_t)sizeof(block0),
    (int32_t)sizeof(block1),
    (int32_t)sizeof(block2),
};

static int32_t total_nbytes(void) {
  return srcsizes[0] + srcsizes[1] + srcsizes[2];
}

static int check_concatenated(const uint8_t *buffer, int32_t nbytes) {
  int32_t offset = 0;
  if (nbytes != total_nbytes()) {
    return -1;
  }
  if (memcmp(buffer + offset, srcs[0], (size_t)srcsizes[0]) != 0) {
    return -2;
  }
  offset += srcsizes[0];
  if (memcmp(buffer + offset, srcs[1], (size_t)srcsizes[1]) != 0) {
    return -3;
  }
  offset += srcsizes[1];
  if (memcmp(buffer + offset, srcs[2], (size_t)srcsizes[2]) != 0) {
    return -4;
  }
  return 0;
}

static int check_split_buffers(void **buffers, const int32_t *sizes, int32_t nblocks) {
  if (nblocks != 3) {
    return -1;
  }
  for (int i = 0; i < nblocks; ++i) {
    if (sizes[i] != srcsizes[i]) {
      return -2;
    }
    if (memcmp(buffers[i], srcs[i], (size_t)srcsizes[i]) != 0) {
      return -3;
    }
  }
  return 0;
}

static int assert_frame_vlblocks_flag(blosc2_schunk *schunk, bool expected) {
  uint8_t *cframe = NULL;
  bool needs_free = false;
  int64_t cframe_len = blosc2_schunk_to_buffer(schunk, &cframe, &needs_free);
  if (cframe_len < FRAME_HEADER_MINLEN || cframe == NULL) {
    return -1;
  }

  bool actual = (cframe[FRAME_FLAGS] & FRAME_VL_BLOCKS) != 0;
  if (needs_free) {
    free(cframe);
  }
  return actual == expected ? 0 : -2;
}

static int assert_frame_version(blosc2_schunk *schunk, uint8_t expected) {
  uint8_t *cframe = NULL;
  bool needs_free = false;
  int64_t cframe_len = blosc2_schunk_to_buffer(schunk, &cframe, &needs_free);
  if (cframe_len < FRAME_HEADER_MINLEN || cframe == NULL) {
    return -1;
  }

  uint8_t actual = cframe[FRAME_FLAGS] & 0x0fu;
  if (needs_free) {
    free(cframe);
  }
  return actual == expected ? 0 : -2;
}

static char *test_vlblocks_roundtrip(void) {
  blosc2_remove_urlpath(tdata.urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  mu_assert("ERROR: cannot create cctx", cctx != NULL);

  int32_t destsize = total_nbytes() + BLOSC2_MAX_OVERHEAD + 64;
  uint8_t *chunk = malloc((size_t)destsize);
  mu_assert("ERROR: cannot allocate chunk buffer", chunk != NULL);

  int32_t cbytes = blosc2_vlcompress_ctx(cctx, (const void * const *)srcs, srcsizes, 3, chunk, destsize);
  mu_assert("ERROR: cannot create VL-block chunk", cbytes > 0);
  mu_assert("ERROR: VL-block chunk should use format version 6",
            chunk[BLOSC2_CHUNK_VERSION] == BLOSC2_VERSION_FORMAT_VL_BLOCKS);
  mu_assert("ERROR: VL-block flag missing in chunk", (chunk[BLOSC2_CHUNK_BLOSC2_FLAGS2] & BLOSC2_VL_BLOCKS) != 0);

  int32_t header_blocksize;
  mu_assert("ERROR: cannot inspect VL-block chunk", blosc2_cbuffer_sizes(chunk, NULL, NULL, &header_blocksize) >= 0);
  mu_assert("ERROR: VL-block chunk should store nblocks in blocksize", header_blocksize == 3);

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_context *dctx = blosc2_create_dctx(dparams);
  mu_assert("ERROR: cannot create dctx", dctx != NULL);

  uint8_t *buffer = malloc((size_t)total_nbytes());
  mu_assert("ERROR: cannot allocate destination buffer", buffer != NULL);
  int32_t nbytes = blosc2_decompress_ctx(dctx, chunk, cbytes, buffer, total_nbytes());
  mu_assert("ERROR: cannot decompress VL-block chunk contiguously", nbytes == total_nbytes());
  mu_assert("ERROR: contiguous VL-block output mismatch", check_concatenated(buffer, nbytes) == 0);

  void *buffers[3] = {NULL};
  int32_t sizes[3] = {0};
  int32_t nblocks = blosc2_vldecompress_ctx(dctx, chunk, cbytes, buffers, sizes, 3);
  mu_assert("ERROR: cannot vldecompress chunk", nblocks == 3);
  mu_assert("ERROR: split VL-block output mismatch", check_split_buffers(buffers, sizes, nblocks) == 0);
  uint8_t item[4];
  mu_assert("ERROR: getitem should reject VL-block chunks",
            blosc2_getitem_ctx(dctx, chunk, cbytes, 0, 1, item, (int32_t)sizeof(item)) < 0);

  blosc2_storage storage = {
      .contiguous = tdata.contiguous,
      .urlpath = (char *)tdata.urlpath,
      .cparams = &cparams,
      .dparams = &dparams,
  };
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: cannot create schunk", schunk != NULL);
  mu_assert("ERROR: cannot append VL-block chunk", blosc2_schunk_append_chunk(schunk, chunk, true) == 1);
  mu_assert("ERROR: VL-block frame should use cframe version 3",
            assert_frame_version(schunk, BLOSC2_VERSION_FRAME_FORMAT_VL_BLOCKS) == 0);
  mu_assert("ERROR: frame should signal VL-block chunks", assert_frame_vlblocks_flag(schunk, true) == 0);

  memset(buffer, 0, (size_t)total_nbytes());
  nbytes = blosc2_schunk_decompress_chunk(schunk, 0, buffer, total_nbytes());
  mu_assert("ERROR: cannot decompress VL-block schunk chunk", nbytes == total_nbytes());
  mu_assert("ERROR: schunk output mismatch", check_concatenated(buffer, nbytes) == 0);

  blosc2_schunk_free(schunk);
  if (tdata.urlpath != NULL) {
    schunk = blosc2_schunk_open(tdata.urlpath);
    mu_assert("ERROR: cannot reopen VL-block schunk", schunk != NULL);
    mu_assert("ERROR: reopened VL-block frame should use cframe version 3",
              assert_frame_version(schunk, BLOSC2_VERSION_FRAME_FORMAT_VL_BLOCKS) == 0);
    mu_assert("ERROR: reopened frame should signal VL-block chunks", assert_frame_vlblocks_flag(schunk, true) == 0);
    memset(buffer, 0, (size_t)total_nbytes());
    nbytes = blosc2_schunk_decompress_chunk(schunk, 0, buffer, total_nbytes());
    mu_assert("ERROR: cannot decompress reopened VL-block schunk chunk", nbytes == total_nbytes());
    mu_assert("ERROR: reopened schunk output mismatch", check_concatenated(buffer, nbytes) == 0);
    blosc2_schunk_free(schunk);
  }

  for (int i = 0; i < 3; ++i) {
    free(buffers[i]);
  }
  free(buffer);
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  free(chunk);
  blosc2_remove_urlpath(tdata.urlpath);

  return EXIT_SUCCESS;
}

static char *test_no_mix_regular_and_vlblocks(void) {
  blosc2_remove_urlpath(tdata.urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_storage storage = {
      .contiguous = tdata.contiguous,
      .urlpath = (char *)tdata.urlpath,
      .cparams = &cparams,
      .dparams = &dparams,
  };
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: cannot create schunk", schunk != NULL);

  uint8_t regular[128];
  int regular_cbytes = blosc2_compress_ctx(schunk->cctx, block0, (int32_t)sizeof(block0), regular, (int32_t)sizeof(regular));
  mu_assert("ERROR: cannot create regular chunk", regular_cbytes > 0);
  mu_assert("ERROR: cannot append regular chunk", blosc2_schunk_append_chunk(schunk, regular, true) == 1);

  uint8_t vlchunk[256];
  int vl_cbytes = blosc2_vlcompress_ctx(schunk->cctx, (const void * const *)srcs, srcsizes, 3, vlchunk, (int32_t)sizeof(vlchunk));
  mu_assert("ERROR: cannot create VL-block chunk", vl_cbytes > 0);
  mu_assert("ERROR: mixing regular and VL-block chunks should fail", blosc2_schunk_append_chunk(schunk, vlchunk, true) < 0);

  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(tdata.urlpath);
  return EXIT_SUCCESS;
}

/* Verify that multi-threaded VL-block compression produces a chunk that can
 * be decompressed correctly.  This is a regression test for the bug where
 * parallel threads wrote compressed blocks in finish order, leaving bstarts[]
 * non-monotonically increasing and causing decompression to fail with -11. */
static char *test_vlblocks_mt_roundtrip(void) {
  /* Use enough blocks so that multiple threads actually race */
  enum { NBLOCKS = 16 };
  static const char *data[NBLOCKS] = {
    "alpha", "bravo", "charlie", "delta",
    "echo",  "foxtrot", "golf", "hotel",
    "india", "juliet", "kilo",  "lima",
    "mike",  "november", "oscar", "papa",
  };
  const void *vsrcs[NBLOCKS];
  int32_t vsizes[NBLOCKS];
  int32_t total = 0;
  for (int i = 0; i < NBLOCKS; i++) {
    vsrcs[i] = data[i];
    vsizes[i] = (int32_t)strlen(data[i]);
    total += vsizes[i];
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  cparams.nthreads = 8;
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  mu_assert("ERROR: cannot create cctx", cctx != NULL);

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_context *dctx = blosc2_create_dctx(dparams);
  mu_assert("ERROR: cannot create dctx", dctx != NULL);

  int32_t destsize = total + BLOSC2_MAX_OVERHEAD + NBLOCKS * 64;
  uint8_t *chunk = malloc((size_t)destsize);
  mu_assert("ERROR: cannot allocate chunk buffer", chunk != NULL);

  int32_t cbytes = blosc2_vlcompress_ctx(cctx, vsrcs, vsizes, NBLOCKS, chunk, destsize);
  mu_assert("ERROR: MT VL-block compression failed", cbytes > 0);

  void *buffers[NBLOCKS];
  int32_t sizes[NBLOCKS];
  memset(buffers, 0, sizeof(buffers));
  int32_t nblocks = blosc2_vldecompress_ctx(dctx, chunk, cbytes, buffers, sizes, NBLOCKS);
  mu_assert("ERROR: MT VL-block decompression failed", nblocks == NBLOCKS);

  for (int i = 0; i < NBLOCKS; i++) {
    mu_assert("ERROR: decompressed size mismatch", sizes[i] == vsizes[i]);
    mu_assert("ERROR: decompressed content mismatch",
              memcmp(buffers[i], vsrcs[i], (size_t)vsizes[i]) == 0);
    free(buffers[i]);
  }

  free(chunk);
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  return EXIT_SUCCESS;
}

#ifdef HAVE_ZSTD
/* Regression test for the VL-block dict bug: blosc2_vlcompress_ctx was
 * missing the dict-training pass, so compressing with use_dict=1 produced a
 * malformed chunk that caused an abort on decompression.
 *
 * Uses 64 JSON-like blocks (~350 B each, ~22 KB total) to give ZDICT enough
 * training data.  Verifies that BLOSC2_USEDICT is set in the chunk header and
 * that both split and concatenated decompression return the original data.
 * The multi-threaded compression path is checked too. */
static char *test_vlblocks_dict_roundtrip(void) {
  enum { DICT_NBLOCKS = 64, DICT_BUF = 512 };

  static char raw[DICT_NBLOCKS][DICT_BUF];
  const void *vsrcs[DICT_NBLOCKS];
  int32_t vsizes[DICT_NBLOCKS];
  int32_t total = 0;
  for (int i = 0; i < DICT_NBLOCKS; i++) {
    vsizes[i] = (int32_t)snprintf(raw[i], DICT_BUF,
        "{\"id\":\"en:ingredient-%03d\",\"vegan\":\"%s\","
        "\"vegetarian\":\"%s\",\"percent_estimate\":%.2f,"
        "\"is_in_taxonomy\":%d,\"text\":\"INGREDIENT NUMBER %03d\","
        "\"from_palm_oil\":null,\"processing\":null,"
        "\"labels\":null,\"origins\":null,\"quantity\":null,"
        "\"quantity_g\":null,\"ciqual_food_code\":null,"
        "\"additives_n\":null,\"nova_group\":null,"
        "\"pnns_groups_1\":null,\"pnns_groups_2\":null,"
        "\"carbon_footprint_percent_of_known_ingredients\":null}",
        i,
        (i % 3 == 0) ? "maybe" : "yes",
        (i % 5 == 0) ? "no" : "yes",
        (double)(i % 20) * 2.5 + 0.1,
        i % 2,
        i);
    vsrcs[i] = raw[i];
    total += vsizes[i];
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  cparams.compcode = BLOSC_ZSTD;
  cparams.use_dict = 1;
  cparams.nthreads = 1;
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  mu_assert("ERROR: cannot create dict cctx", cctx != NULL);

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_context *dctx = blosc2_create_dctx(dparams);
  mu_assert("ERROR: cannot create dict dctx", dctx != NULL);

  /* Extra headroom for the dict itself (up to BLOSC2_MAXDICTSIZE) */
  int32_t destsize = total + BLOSC2_MAX_OVERHEAD + DICT_NBLOCKS * 64 + BLOSC2_MAXDICTSIZE;
  uint8_t *chunk = malloc((size_t)destsize);
  mu_assert("ERROR: cannot allocate dict chunk buffer", chunk != NULL);

  int32_t cbytes = blosc2_vlcompress_ctx(cctx, vsrcs, vsizes, DICT_NBLOCKS, chunk, destsize);
  mu_assert("ERROR: dict VL-block compression failed", cbytes > 0);
  mu_assert("ERROR: BLOSC2_USEDICT flag not set",
            (chunk[BLOSC2_CHUNK_BLOSC2_FLAGS] & BLOSC2_USEDICT) != 0);

  /* Verify split decompression */
  void *buffers[DICT_NBLOCKS];
  int32_t sizes[DICT_NBLOCKS];
  memset(buffers, 0, sizeof(buffers));
  int32_t nblocks = blosc2_vldecompress_ctx(dctx, chunk, cbytes, buffers, sizes, DICT_NBLOCKS);
  mu_assert("ERROR: dict VL-block split decompression failed", nblocks == DICT_NBLOCKS);
  for (int i = 0; i < DICT_NBLOCKS; i++) {
    mu_assert("ERROR: dict split size mismatch", sizes[i] == vsizes[i]);
    mu_assert("ERROR: dict split content mismatch",
              memcmp(buffers[i], vsrcs[i], (size_t)vsizes[i]) == 0);
    free(buffers[i]);
  }

  /* Verify concatenated decompression */
  uint8_t *concat = malloc((size_t)total);
  mu_assert("ERROR: cannot allocate concat buffer", concat != NULL);
  int32_t nbytes = blosc2_decompress_ctx(dctx, chunk, cbytes, concat, total);
  mu_assert("ERROR: dict VL-block concat decompression failed", nbytes == total);
  int32_t off = 0;
  for (int i = 0; i < DICT_NBLOCKS; i++) {
    mu_assert("ERROR: dict concat content mismatch",
              memcmp(concat + off, vsrcs[i], (size_t)vsizes[i]) == 0);
    off += vsizes[i];
  }
  free(concat);

  /* Verify the multi-threaded compression path */
  blosc2_free_ctx(cctx);
  cparams.nthreads = 4;
  cctx = blosc2_create_cctx(cparams);
  mu_assert("ERROR: cannot create MT dict cctx", cctx != NULL);

  int32_t cbytes_mt = blosc2_vlcompress_ctx(cctx, vsrcs, vsizes, DICT_NBLOCKS, chunk, destsize);
  mu_assert("ERROR: MT dict VL-block compression failed", cbytes_mt > 0);
  mu_assert("ERROR: MT BLOSC2_USEDICT flag not set",
            (chunk[BLOSC2_CHUNK_BLOSC2_FLAGS] & BLOSC2_USEDICT) != 0);

  memset(buffers, 0, sizeof(buffers));
  nblocks = blosc2_vldecompress_ctx(dctx, chunk, cbytes_mt, buffers, sizes, DICT_NBLOCKS);
  mu_assert("ERROR: MT dict VL-block decompression failed", nblocks == DICT_NBLOCKS);
  for (int i = 0; i < DICT_NBLOCKS; i++) {
    mu_assert("ERROR: MT dict size mismatch", sizes[i] == vsizes[i]);
    mu_assert("ERROR: MT dict content mismatch",
              memcmp(buffers[i], vsrcs[i], (size_t)vsizes[i]) == 0);
    free(buffers[i]);
  }

  free(chunk);
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  return EXIT_SUCCESS;
}
#endif  /* HAVE_ZSTD */

static char *all_tests(void) {
  for (int i = 0; i < (int)ARRAY_SIZE(backends); ++i) {
    tdata = backends[i];
    mu_run_test(test_vlblocks_roundtrip);
    mu_run_test(test_no_mix_regular_and_vlblocks);
  }
  mu_run_test(test_vlblocks_mt_roundtrip);
#ifdef HAVE_ZSTD
  mu_run_test(test_vlblocks_dict_roundtrip);
#endif
  return EXIT_SUCCESS;
}

int main(void) {
  char *result;
  blosc2_init();
  result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf(" Tests run: %d\n", tests_run);
  blosc2_destroy();
  return result != EXIT_SUCCESS;
}
