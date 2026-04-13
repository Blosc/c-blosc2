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

#define DICT_VL_NBLOCKS 64
#define DICT_VL_BUF 512

static int32_t total_nbytes(void) {
  return srcsizes[0] + srcsizes[1] + srcsizes[2];
}

static int build_dict_vl_inputs(char raw[DICT_VL_NBLOCKS][DICT_VL_BUF],
                                const void* vsrcs[DICT_VL_NBLOCKS],
                                int32_t vsizes[DICT_VL_NBLOCKS],
                                int32_t* total) {
  *total = 0;
  for (int i = 0; i < DICT_VL_NBLOCKS; i++) {
    vsizes[i] = (int32_t)snprintf(raw[i], DICT_VL_BUF,
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
    if (vsizes[i] <= 0 || vsizes[i] >= DICT_VL_BUF) {
      return -1;
    }
    vsrcs[i] = raw[i];
    *total += vsizes[i];
  }
  return 0;
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

/* ---- blosc2_vlchunk_get_nblocks ------------------------------------------ */

static char *test_vlchunk_get_nblocks(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  mu_assert("ERROR: cannot create cctx", cctx != NULL);

  int32_t destsize = total_nbytes() + BLOSC2_MAX_OVERHEAD + 64;
  uint8_t *chunk = malloc((size_t)destsize);
  mu_assert("ERROR: cannot allocate chunk", chunk != NULL);

  int32_t cbytes = blosc2_vlcompress_ctx(cctx, (const void * const *)srcs, srcsizes, 3, chunk, destsize);
  mu_assert("ERROR: cannot create VL-block chunk", cbytes > 0);

  /* Happy path: should report 3 blocks. */
  int32_t nblocks = -1;
  int rc = blosc2_vlchunk_get_nblocks(chunk, cbytes, &nblocks);
  mu_assert("ERROR: blosc2_vlchunk_get_nblocks failed", rc == 0);
  mu_assert("ERROR: wrong nblocks", nblocks == 3);

  /* Reject NULL nblocks pointer. */
  rc = blosc2_vlchunk_get_nblocks(chunk, cbytes, NULL);
  mu_assert("ERROR: expected error for NULL nblocks", rc < 0);

  /* Reject a regular (non-VL) chunk. */
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_context *dctx = blosc2_create_dctx(dparams);
  mu_assert("ERROR: cannot create dctx", dctx != NULL);
  int32_t regular_destsize = srcsizes[0] + BLOSC2_MAX_OVERHEAD;
  uint8_t *regular_chunk = malloc((size_t)regular_destsize);
  mu_assert("ERROR: cannot allocate regular chunk", regular_chunk != NULL);
  int32_t regular_cbytes = blosc2_compress_ctx(cctx, srcs[0], srcsizes[0],
                                               regular_chunk, regular_destsize);
  mu_assert("ERROR: cannot create regular chunk", regular_cbytes > 0);
  nblocks = -1;
  rc = blosc2_vlchunk_get_nblocks(regular_chunk, regular_cbytes, &nblocks);
  mu_assert("ERROR: expected error for non-VL chunk", rc < 0);

  free(regular_chunk);
  free(chunk);
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  return EXIT_SUCCESS;
}


/* ---- blosc2_vldecompress_block_ctx --------------------------------------- */

static char *test_vldecompress_block_ctx(void) {
  blosc2_remove_urlpath(tdata.urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  mu_assert("ERROR: cannot create cctx", cctx != NULL);

  int32_t destsize = total_nbytes() + BLOSC2_MAX_OVERHEAD + 64;
  uint8_t *chunk = malloc((size_t)destsize);
  mu_assert("ERROR: cannot allocate chunk", chunk != NULL);

  int32_t cbytes = blosc2_vlcompress_ctx(cctx, (const void * const *)srcs, srcsizes, 3, chunk, destsize);
  mu_assert("ERROR: cannot create VL-block chunk", cbytes > 0);

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_context *dctx = blosc2_create_dctx(dparams);
  mu_assert("ERROR: cannot create dctx", dctx != NULL);

  /* Decompress block 0, middle (1), and last (2) one by one. */
  for (int i = 0; i < 3; ++i) {
    uint8_t *blk = NULL;
    int32_t blksize = -1;
    int rc = blosc2_vldecompress_block_ctx(dctx, chunk, cbytes, i, &blk, &blksize);
    mu_assert("ERROR: blosc2_vldecompress_block_ctx failed", rc == srcsizes[i]);
    mu_assert("ERROR: returned size mismatch", blksize == srcsizes[i]);
    mu_assert("ERROR: content mismatch",
              memcmp(blk, srcs[i], (size_t)srcsizes[i]) == 0);
    free(blk);
  }

  free(chunk);
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  blosc2_remove_urlpath(tdata.urlpath);
  return EXIT_SUCCESS;
}


static char *test_vldecompress_block_ctx_errors(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  blosc2_context *cctx = blosc2_create_cctx(cparams);
  mu_assert("ERROR: cannot create cctx", cctx != NULL);

  int32_t destsize = total_nbytes() + BLOSC2_MAX_OVERHEAD + 64;
  uint8_t *chunk = malloc((size_t)destsize);
  mu_assert("ERROR: cannot allocate chunk", chunk != NULL);

  int32_t cbytes = blosc2_vlcompress_ctx(cctx, (const void * const *)srcs, srcsizes, 3, chunk, destsize);
  mu_assert("ERROR: cannot create VL-block chunk", cbytes > 0);

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_context *dctx = blosc2_create_dctx(dparams);
  mu_assert("ERROR: cannot create dctx", dctx != NULL);

  uint8_t *blk = NULL;
  int32_t blksize = -1;

  /* Negative nblock. */
  int rc = blosc2_vldecompress_block_ctx(dctx, chunk, cbytes, -1, &blk, &blksize);
  mu_assert("ERROR: expected error for negative nblock", rc < 0);

  /* Out-of-range nblock. */
  rc = blosc2_vldecompress_block_ctx(dctx, chunk, cbytes, 3, &blk, &blksize);
  mu_assert("ERROR: expected error for out-of-range nblock", rc < 0);

  /* NULL dest. */
  rc = blosc2_vldecompress_block_ctx(dctx, chunk, cbytes, 0, NULL, &blksize);
  mu_assert("ERROR: expected error for NULL dest", rc < 0);

  /* NULL destsize. */
  rc = blosc2_vldecompress_block_ctx(dctx, chunk, cbytes, 0, &blk, NULL);
  mu_assert("ERROR: expected error for NULL destsize", rc < 0);

  /* Regular (non-VL) chunk must be rejected. */
  int32_t regular_destsize = srcsizes[0] + BLOSC2_MAX_OVERHEAD;
  uint8_t *regular_chunk = malloc((size_t)regular_destsize);
  mu_assert("ERROR: cannot allocate regular chunk", regular_chunk != NULL);
  int32_t regular_cbytes = blosc2_compress_ctx(cctx, srcs[0], srcsizes[0],
                                               regular_chunk, regular_destsize);
  mu_assert("ERROR: cannot create regular chunk", regular_cbytes > 0);
  rc = blosc2_vldecompress_block_ctx(dctx, regular_chunk, regular_cbytes, 0, &blk, &blksize);
  mu_assert("ERROR: expected error for non-VL chunk in block_ctx", rc < 0);

  free(regular_chunk);
  free(chunk);
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  return EXIT_SUCCESS;
}


/* ---- blosc2_schunk_get_vlblock ------------------------------------------- */

static char *test_schunk_get_vlblock(void) {
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

  blosc2_context *cctx = blosc2_create_cctx(cparams);
  mu_assert("ERROR: cannot create cctx", cctx != NULL);

  int32_t destsize = total_nbytes() + BLOSC2_MAX_OVERHEAD + 64;
  uint8_t *chunk = malloc((size_t)destsize);
  mu_assert("ERROR: cannot allocate chunk", chunk != NULL);
  int32_t cbytes = blosc2_vlcompress_ctx(cctx, (const void * const *)srcs, srcsizes, 3, chunk, destsize);
  mu_assert("ERROR: cannot create VL-block chunk", cbytes > 0);

  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: cannot create schunk", schunk != NULL);
  mu_assert("ERROR: cannot append chunk", blosc2_schunk_append_chunk(schunk, chunk, true) == 1);
  free(chunk);
  blosc2_free_ctx(cctx);

  /* Read each block individually and verify. */
  for (int i = 0; i < 3; ++i) {
    uint8_t *blk = NULL;
    int32_t blksize = -1;
    int rc = blosc2_schunk_get_vlblock(schunk, 0, i, &blk, &blksize);
    mu_assert("ERROR: blosc2_schunk_get_vlblock failed", rc == srcsizes[i]);
    mu_assert("ERROR: returned size mismatch (schunk)", blksize == srcsizes[i]);
    mu_assert("ERROR: content mismatch (schunk)",
              memcmp(blk, srcs[i], (size_t)srcsizes[i]) == 0);
    free(blk);
  }

  blosc2_schunk_free(schunk);

  /* Reopen persistent schunks and verify again. */
  if (tdata.urlpath != NULL) {
    schunk = blosc2_schunk_open(tdata.urlpath);
    mu_assert("ERROR: cannot reopen schunk", schunk != NULL);
    for (int i = 0; i < 3; ++i) {
      uint8_t *blk = NULL;
      int32_t blksize = -1;
      int rc = blosc2_schunk_get_vlblock(schunk, 0, i, &blk, &blksize);
      mu_assert("ERROR: reopened blosc2_schunk_get_vlblock failed", rc == srcsizes[i]);
      mu_assert("ERROR: reopened size mismatch", blksize == srcsizes[i]);
      mu_assert("ERROR: reopened content mismatch",
                memcmp(blk, srcs[i], (size_t)srcsizes[i]) == 0);
      free(blk);
    }
    blosc2_schunk_free(schunk);
  }

  blosc2_remove_urlpath(tdata.urlpath);
  return EXIT_SUCCESS;
}


#ifdef HAVE_ZSTD
/* Selective single-block decompression should work on dict-enabled VL chunks.
 * Uses the same block count and data shape as test_vlblocks_dict_roundtrip
 * (64 JSON-like blocks, ~350 B each) to ensure ZSTD dict training fires. */
static char *test_vldecompress_block_ctx_dict(void) {
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

  int32_t destsize = total + BLOSC2_MAX_OVERHEAD + DICT_NBLOCKS * 64 + BLOSC2_MAXDICTSIZE;
  uint8_t *chunk = malloc((size_t)destsize);
  mu_assert("ERROR: cannot allocate dict chunk", chunk != NULL);

  int32_t cbytes = blosc2_vlcompress_ctx(cctx, vsrcs, vsizes, DICT_NBLOCKS, chunk, destsize);
  mu_assert("ERROR: dict VL-block compression failed", cbytes > 0);
  mu_assert("ERROR: BLOSC2_USEDICT flag not set",
            (chunk[BLOSC2_CHUNK_BLOSC2_FLAGS] & BLOSC2_USEDICT) != 0);

  /* Read first, middle, and last block selectively. */
  int targets[] = {0, DICT_NBLOCKS / 2, DICT_NBLOCKS - 1};
  for (int t = 0; t < 3; ++t) {
    int i = targets[t];
    uint8_t *blk = NULL;
    int32_t blksize = -1;
    int rc = blosc2_vldecompress_block_ctx(dctx, chunk, cbytes, i, &blk, &blksize);
    mu_assert("ERROR: dict blosc2_vldecompress_block_ctx failed", rc == vsizes[i]);
    mu_assert("ERROR: dict returned size mismatch", blksize == vsizes[i]);
    mu_assert("ERROR: dict content mismatch",
              memcmp(blk, vsrcs[i], (size_t)vsizes[i]) == 0);
    free(blk);
  }

  free(chunk);
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  return EXIT_SUCCESS;
}
#endif  /* HAVE_ZSTD */


static blosc2_schunk* make_lazy_dict_vl_schunk(const char* urlpath, bool contiguous, int compcode,
                                               char raw[DICT_VL_NBLOCKS][DICT_VL_BUF],
                                               const void* vsrcs[DICT_VL_NBLOCKS],
                                               int32_t vsizes[DICT_VL_NBLOCKS],
                                               int32_t* total) {
  blosc2_remove_urlpath(urlpath);
  if (build_dict_vl_inputs(raw, vsrcs, vsizes, total) != 0) {
    return NULL;
  }

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  cparams.compcode = compcode;
  cparams.use_dict = 1;
  cparams.nthreads = 1;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_storage storage = {
      .contiguous = contiguous,
      .urlpath = (char *)urlpath,
      .cparams = &cparams,
      .dparams = &dparams,
  };

  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    return NULL;
  }

  blosc2_context* cctx = blosc2_create_cctx(cparams);
  if (cctx == NULL) {
    blosc2_schunk_free(schunk);
    blosc2_remove_urlpath(urlpath);
    return NULL;
  }

  int32_t destsize = *total + BLOSC2_MAX_OVERHEAD + DICT_VL_NBLOCKS * 64 + BLOSC2_MAXDICTSIZE;
  uint8_t* chunk = malloc((size_t)destsize);
  if (chunk == NULL) {
    blosc2_free_ctx(cctx);
    blosc2_schunk_free(schunk);
    blosc2_remove_urlpath(urlpath);
    return NULL;
  }

  int32_t cbytes = blosc2_vlcompress_ctx(cctx, vsrcs, vsizes, DICT_VL_NBLOCKS, chunk, destsize);
  if (cbytes <= 0 || (chunk[BLOSC2_CHUNK_BLOSC2_FLAGS] & BLOSC2_USEDICT) == 0 ||
      blosc2_schunk_append_chunk(schunk, chunk, true) < 0) {
    free(chunk);
    blosc2_free_ctx(cctx);
    blosc2_schunk_free(schunk);
    blosc2_remove_urlpath(urlpath);
    return NULL;
  }

  cbytes = blosc2_vlcompress_ctx(cctx, vsrcs, vsizes, DICT_VL_NBLOCKS, chunk, destsize);
  if (cbytes <= 0 || (chunk[BLOSC2_CHUNK_BLOSC2_FLAGS] & BLOSC2_USEDICT) == 0 ||
      blosc2_schunk_append_chunk(schunk, chunk, true) < 0) {
    free(chunk);
    blosc2_free_ctx(cctx);
    blosc2_schunk_free(schunk);
    blosc2_remove_urlpath(urlpath);
    return NULL;
  }

  free(chunk);
  blosc2_free_ctx(cctx);
  return schunk;
}


static char* run_lazy_vldecompress_block_ctx_dict_test(int compcode, const char* urlpath) {
  static char raw[DICT_VL_NBLOCKS][DICT_VL_BUF];
  const void* vsrcs[DICT_VL_NBLOCKS];
  int32_t vsizes[DICT_VL_NBLOCKS];
  int32_t total = 0;

  blosc2_schunk* schunk = make_lazy_dict_vl_schunk(urlpath, true, compcode, raw, vsrcs, vsizes, &total);
  mu_assert("ERROR: cannot create lazy dict VL schunk", schunk != NULL);

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.schunk = schunk;
  blosc2_context* dctx = blosc2_create_dctx(dparams);
  mu_assert("ERROR: cannot create fresh dctx for lazy dict test", dctx != NULL);

  int targets[] = {0, DICT_VL_NBLOCKS / 2, DICT_VL_NBLOCKS - 1};
  for (int64_t nchunk = 0; nchunk < schunk->nchunks; ++nchunk) {
    uint8_t* lazy_chunk = NULL;
    bool needs_free = false;
    int cbytes = blosc2_schunk_get_lazychunk(schunk, nchunk, &lazy_chunk, &needs_free);
    mu_assert("ERROR: cannot get lazy dict chunk", cbytes > 0);

    for (int t = 0; t < 3; ++t) {
      int i = targets[t];
      uint8_t* blk = NULL;
      int32_t blksize = -1;
      int rc = blosc2_vldecompress_block_ctx(dctx, lazy_chunk, cbytes, i, &blk, &blksize);
      mu_assert("ERROR: lazy dict blosc2_vldecompress_block_ctx failed", rc == vsizes[i]);
      mu_assert("ERROR: lazy dict returned size mismatch", blksize == vsizes[i]);
      mu_assert("ERROR: lazy dict content mismatch",
                memcmp(blk, vsrcs[i], (size_t)vsizes[i]) == 0);
      free(blk);
    }

    if (needs_free) {
      free(lazy_chunk);
    }
  }

  blosc2_free_ctx(dctx);
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(urlpath);
  return EXIT_SUCCESS;
}


static char* test_lazy_vldecompress_block_ctx_dict_lz4(void) {
  return run_lazy_vldecompress_block_ctx_dict_test(BLOSC_LZ4, "test_lazy_vldecomp_dict_lz4.b2frame");
}


#ifdef HAVE_ZSTD
static char* test_vlblocks_dict_fallback_is_per_chunk(void) {
  static char raw[DICT_VL_NBLOCKS][DICT_VL_BUF];
  const void* vsrcs[DICT_VL_NBLOCKS];
  int32_t vsizes[DICT_VL_NBLOCKS];
  int32_t total = 0;
  uint8_t tiny = 'x';
  const void* tiny_srcs[1] = {&tiny};
  int32_t tiny_sizes[1] = {1};
  uint8_t tiny_chunk[256];
  uint8_t* chunk = NULL;

  mu_assert("ERROR: cannot build dict VL inputs", build_dict_vl_inputs(raw, vsrcs, vsizes, &total) == 0);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  cparams.compcode = BLOSC_ZSTD;
  cparams.use_dict = 1;
  cparams.nthreads = 1;
  blosc2_context* cctx = blosc2_create_cctx(cparams);
  mu_assert("ERROR: cannot create VL dict cctx", cctx != NULL);

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_context* dctx = blosc2_create_dctx(dparams);
  mu_assert("ERROR: cannot create VL dict dctx", dctx != NULL);

  int32_t tiny_cbytes = blosc2_vlcompress_ctx(cctx, tiny_srcs, tiny_sizes, 1, tiny_chunk, (int32_t)sizeof(tiny_chunk));
  mu_assert("ERROR: cannot create tiny fallback VL chunk", tiny_cbytes > 0);

  int32_t destsize = total + BLOSC2_MAX_OVERHEAD + DICT_VL_NBLOCKS * 64 + BLOSC2_MAXDICTSIZE;
  chunk = malloc((size_t)destsize);
  mu_assert("ERROR: cannot allocate dict VL chunk", chunk != NULL);

  int32_t cbytes = blosc2_vlcompress_ctx(cctx, vsrcs, vsizes, DICT_VL_NBLOCKS, chunk, destsize);
  mu_assert("ERROR: cannot create dict VL chunk after fallback", cbytes > 0);
  mu_assert("ERROR: later VL chunk lost BLOSC2_USEDICT after fallback",
            (chunk[BLOSC2_CHUNK_BLOSC2_FLAGS] & BLOSC2_USEDICT) != 0);

  void* buffers[DICT_VL_NBLOCKS];
  int32_t sizes[DICT_VL_NBLOCKS];
  memset(buffers, 0, sizeof(buffers));
  int32_t nblocks = blosc2_vldecompress_ctx(dctx, chunk, cbytes, buffers, sizes, DICT_VL_NBLOCKS);
  mu_assert("ERROR: cannot decompress dict VL chunk after fallback", nblocks == DICT_VL_NBLOCKS);
  for (int i = 0; i < DICT_VL_NBLOCKS; ++i) {
    mu_assert("ERROR: dict VL size mismatch after fallback", sizes[i] == vsizes[i]);
    mu_assert("ERROR: dict VL content mismatch after fallback",
              memcmp(buffers[i], vsrcs[i], (size_t)vsizes[i]) == 0);
    free(buffers[i]);
  }

  free(chunk);
  blosc2_free_ctx(cctx);
  blosc2_free_ctx(dctx);
  return EXIT_SUCCESS;
}


static char* test_lazy_vldecompress_block_ctx_dict_zstd(void) {
  return run_lazy_vldecompress_block_ctx_dict_test(BLOSC_ZSTD, "test_lazy_vldecomp_dict_zstd.b2frame");
}
#endif


/* ---- Lazy VL chunk tests ------------------------------------------------- */

/* Helper: build a 2-chunk file-backed schunk with 3 VL blocks per chunk.
 * Returns the schunk (caller must blosc2_schunk_free it), or NULL on error. */
static blosc2_schunk* make_lazy_vl_schunk(const char* urlpath, bool contiguous) {
  blosc2_remove_urlpath(urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = 1;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_storage storage = {
      .contiguous = contiguous,
      .urlpath = (char *)urlpath,
      .cparams = &cparams,
      .dparams = &dparams,
  };

  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) return NULL;

  blosc2_context* cctx = blosc2_create_cctx(cparams);
  if (cctx == NULL) { blosc2_schunk_free(schunk); return NULL; }

  int32_t destsize = total_nbytes() + BLOSC2_MAX_OVERHEAD + 64;
  uint8_t* chunk = malloc((size_t)destsize);
  if (chunk == NULL) { blosc2_free_ctx(cctx); blosc2_schunk_free(schunk); return NULL; }

  int32_t cbytes = blosc2_vlcompress_ctx(cctx, (const void * const *)srcs, srcsizes, 3,
                                         chunk, destsize);
  if (cbytes <= 0 || blosc2_schunk_append_chunk(schunk, chunk, true) < 0) {
    free(chunk); blosc2_free_ctx(cctx); blosc2_schunk_free(schunk); return NULL;
  }
  /* Append a second chunk (same data) so nchunks > 1. */
  cbytes = blosc2_vlcompress_ctx(cctx, (const void * const *)srcs, srcsizes, 3,
                                 chunk, destsize);
  if (cbytes <= 0 || blosc2_schunk_append_chunk(schunk, chunk, true) < 0) {
    free(chunk); blosc2_free_ctx(cctx); blosc2_schunk_free(schunk); return NULL;
  }
  free(chunk);
  blosc2_free_ctx(cctx);
  return schunk;
}

static void put_le32(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xffu);
  p[1] = (uint8_t)((v >> 8) & 0xffu);
  p[2] = (uint8_t)((v >> 16) & 0xffu);
  p[3] = (uint8_t)((v >> 24) & 0xffu);
}

static char *test_lazy_vlchunk_get_nblocks(void) {
  const char* urlpath = "test_lazy_vlnblocks.b2frame";
  blosc2_schunk* schunk = make_lazy_vl_schunk(urlpath, true);
  mu_assert("ERROR: cannot create lazy VL schunk", schunk != NULL);

  for (int64_t nchunk = 0; nchunk < schunk->nchunks; ++nchunk) {
    uint8_t* lazy_chunk = NULL;
    bool needs_free = false;
    int cbytes = blosc2_schunk_get_lazychunk(schunk, nchunk, &lazy_chunk, &needs_free);
    mu_assert("ERROR: blosc2_schunk_get_lazychunk failed", cbytes > 0);

    int32_t nblocks = -1;
    int rc = blosc2_vlchunk_get_nblocks(lazy_chunk, cbytes, &nblocks);
    mu_assert("ERROR: blosc2_vlchunk_get_nblocks failed on lazy chunk", rc == 0);
    mu_assert("ERROR: wrong nblocks from lazy chunk", nblocks == 3);

    if (needs_free) free(lazy_chunk);
  }

  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(urlpath);
  return EXIT_SUCCESS;
}

static char *test_lazy_vlchunk_rejects_huge_nblocks(void) {
  const char* urlpath = "test_lazy_vlnblocks_corrupt.b2frame";
  blosc2_schunk* schunk = make_lazy_vl_schunk(urlpath, false);
  mu_assert("ERROR: cannot create lazy VL schunk", schunk != NULL);

  char chunk_path[256];
  int chunk_path_len = snprintf(chunk_path, sizeof(chunk_path), "%s/%08X.chunk", urlpath, 0U);
  mu_assert("ERROR: cannot build chunk path", chunk_path_len > 0 && chunk_path_len < (int)sizeof(chunk_path));

  FILE* fp = fopen(chunk_path, "rb+");
  mu_assert("ERROR: cannot open first chunk file", fp != NULL);

  uint8_t header[BLOSC_EXTENDED_HEADER_LENGTH];
  size_t rbytes = fread(header, 1, sizeof(header), fp);
  mu_assert("ERROR: cannot read first chunk header", rbytes == sizeof(header));

  header[BLOSC2_CHUNK_BLOSC2_FLAGS2] |= BLOSC2_VL_BLOCKS;
  put_le32(header + BLOSC2_CHUNK_BLOCKSIZE, UINT32_MAX);

  mu_assert("ERROR: cannot rewind chunk file", fseek(fp, 0, SEEK_SET) == 0);
  size_t wbytes = fwrite(header, 1, sizeof(header), fp);
  mu_assert("ERROR: cannot write corrupted chunk header", wbytes == sizeof(header));
  fclose(fp);

  uint8_t* lazy_chunk = NULL;
  bool needs_free = false;
  int cbytes = blosc2_schunk_get_lazychunk(schunk, 0, &lazy_chunk, &needs_free);
  mu_assert("ERROR: malformed lazy VL chunk should be rejected", cbytes < 0);
  if (needs_free) {
    free(lazy_chunk);
  }

  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(urlpath);
  return EXIT_SUCCESS;
}


static char *test_lazy_vlchunk_rejects_truncated_trailer(void) {
  const char* urlpath = "test_lazy_vl_truncated_trailer.b2frame";
  blosc2_schunk* schunk = make_lazy_vl_schunk(urlpath, true);
  mu_assert("ERROR: cannot create lazy VL schunk", schunk != NULL);

  uint8_t* dest = malloc((size_t)total_nbytes());
  mu_assert("ERROR: cannot allocate destination buffer", dest != NULL);

  for (int64_t nchunk = 0; nchunk < schunk->nchunks; ++nchunk) {
    uint8_t* lazy_chunk = NULL;
    bool needs_free = false;
    int cbytes = blosc2_schunk_get_lazychunk(schunk, nchunk, &lazy_chunk, &needs_free);
    mu_assert("ERROR: blosc2_schunk_get_lazychunk failed", cbytes > 0);

    int32_t nblocks = -1;
    int rc = blosc2_vlchunk_get_nblocks(lazy_chunk, cbytes, &nblocks);
    mu_assert("ERROR: cannot inspect lazy VL nblocks", rc == 0 && nblocks > 0);

    int64_t trailer_offset = BLOSC_EXTENDED_HEADER_LENGTH + (int64_t)nblocks * (int64_t)sizeof(int32_t);
    int64_t trailer_nbytes = (int64_t)sizeof(int32_t) + (int64_t)sizeof(int64_t) +
                             (int64_t)nblocks * (int64_t)sizeof(int32_t);
    int64_t truncated_srcsize = trailer_offset + trailer_nbytes - 1;
    mu_assert("ERROR: malformed srcsize should be positive", truncated_srcsize > 0);
    mu_assert("ERROR: malformed srcsize should be smaller than cbytes", truncated_srcsize < cbytes);

    memset(dest, 0, (size_t)total_nbytes());
    rc = blosc2_decompress_ctx(schunk->dctx, lazy_chunk, (int32_t)truncated_srcsize,
                               dest, total_nbytes());
    mu_assert("ERROR: truncated lazy VL trailer must be rejected", rc < 0);

    if (needs_free) {
      free(lazy_chunk);
    }
  }

  free(dest);
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(urlpath);
  return EXIT_SUCCESS;
}


static char *test_lazy_vldecompress_block_ctx(void) {
  const char* urlpath = "test_lazy_vldecomp.b2frame";
  blosc2_schunk* schunk = make_lazy_vl_schunk(urlpath, true);
  mu_assert("ERROR: cannot create lazy VL schunk", schunk != NULL);

  /* Use the schunk's own dctx, which already has schunk->dctx->schunk set. */
  blosc2_context* dctx = schunk->dctx;

  for (int64_t nchunk = 0; nchunk < schunk->nchunks; ++nchunk) {
    uint8_t* lazy_chunk = NULL;
    bool needs_free = false;
    int cbytes = blosc2_schunk_get_lazychunk(schunk, nchunk, &lazy_chunk, &needs_free);
    mu_assert("ERROR: blosc2_schunk_get_lazychunk failed", cbytes > 0);

    for (int i = 0; i < 3; ++i) {
      uint8_t* blk = NULL;
      int32_t blksize = -1;
      int rc = blosc2_vldecompress_block_ctx(dctx, lazy_chunk, cbytes, i, &blk, &blksize);
      mu_assert("ERROR: lazy blosc2_vldecompress_block_ctx failed", rc == srcsizes[i]);
      mu_assert("ERROR: lazy returned size mismatch", blksize == srcsizes[i]);
      mu_assert("ERROR: lazy content mismatch",
                memcmp(blk, srcs[i], (size_t)srcsizes[i]) == 0);
      free(blk);
    }
    if (needs_free) free(lazy_chunk);
  }

  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(urlpath);
  return EXIT_SUCCESS;
}


static char *test_lazy_vldecompress_block_ctx_sframe(void) {
  const char* urlpath = "test_lazy_vldecomp_s.b2frame";
  blosc2_schunk* schunk = make_lazy_vl_schunk(urlpath, false);
  mu_assert("ERROR: cannot create lazy VL sframe schunk", schunk != NULL);

  /* Use the schunk's own dctx. */
  blosc2_context* dctx = schunk->dctx;

  for (int64_t nchunk = 0; nchunk < schunk->nchunks; ++nchunk) {
    uint8_t* lazy_chunk = NULL;
    bool needs_free = false;
    int cbytes = blosc2_schunk_get_lazychunk(schunk, nchunk, &lazy_chunk, &needs_free);
    mu_assert("ERROR: sframe blosc2_schunk_get_lazychunk failed", cbytes > 0);

    for (int i = 0; i < 3; ++i) {
      uint8_t* blk = NULL;
      int32_t blksize = -1;
      int rc = blosc2_vldecompress_block_ctx(dctx, lazy_chunk, cbytes, i, &blk, &blksize);
      mu_assert("ERROR: sframe lazy blosc2_vldecompress_block_ctx failed", rc == srcsizes[i]);
      mu_assert("ERROR: sframe lazy returned size mismatch", blksize == srcsizes[i]);
      mu_assert("ERROR: sframe lazy content mismatch",
                memcmp(blk, srcs[i], (size_t)srcsizes[i]) == 0);
      free(blk);
    }
    if (needs_free) free(lazy_chunk);
  }

  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(urlpath);
  return EXIT_SUCCESS;
}


static char *all_tests(void) {
  for (int i = 0; i < (int)ARRAY_SIZE(backends); ++i) {
    tdata = backends[i];
    mu_run_test(test_vlblocks_roundtrip);
    mu_run_test(test_no_mix_regular_and_vlblocks);
    mu_run_test(test_vlchunk_get_nblocks);
    mu_run_test(test_vldecompress_block_ctx);
    mu_run_test(test_vldecompress_block_ctx_errors);
    mu_run_test(test_schunk_get_vlblock);
  }
  mu_run_test(test_vlblocks_mt_roundtrip);
  mu_run_test(test_lazy_vlchunk_get_nblocks);
  mu_run_test(test_lazy_vlchunk_rejects_huge_nblocks);
  mu_run_test(test_lazy_vlchunk_rejects_truncated_trailer);
  mu_run_test(test_lazy_vldecompress_block_ctx);
  mu_run_test(test_lazy_vldecompress_block_ctx_sframe);
  mu_run_test(test_lazy_vldecompress_block_ctx_dict_lz4);
#ifdef HAVE_ZSTD
  mu_run_test(test_vlblocks_dict_roundtrip);
  mu_run_test(test_vlblocks_dict_fallback_is_per_chunk);
  mu_run_test(test_vldecompress_block_ctx_dict);
  mu_run_test(test_lazy_vldecompress_block_ctx_dict_zstd);
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
