/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include "test_common.h"
#include "sframe.h"

int tests_run = 0;

#define SFRAME_DIR "test_sframe_eviction.b2frame"
#define CHUNK_NITEMS (2000)
#define NUM_CHUNKS (5)

static bool chunk_file_exists(const char *dir, int64_t chunk_id) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%08" PRIX32 ".chunk", dir, (uint32_t)chunk_id);
  FILE *f = fopen(path, "rb");
  if (f != NULL) {
    fclose(f);
    return true;
  }
  return false;
}

static int count_chunk_files(const char *dir, int max_id) {
  int count = 0;
  for (int i = 0; i <= max_id; ++i) {
    if (chunk_file_exists(dir, i)) {
      count++;
    }
  }
  return count;
}

static void fill_data(int32_t *buf, int chunk_idx, int variation) {
  for (int i = 0; i < CHUNK_NITEMS; ++i) {
    buf[i] = chunk_idx * 10000 + variation * 100 + i;
  }
}

static char *test_sframe_eviction_and_refill(void) {
  blosc2_remove_dir(SFRAME_DIR);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;

  blosc2_storage storage = {
      .cparams = &cparams,
      .dparams = &dparams,
      .urlpath = SFRAME_DIR,
      .contiguous = false,
  };

  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: blosc2_schunk_new failed", schunk != NULL);

  int32_t chunk_data[CHUNK_NITEMS];
  int32_t buf_bytes = CHUNK_NITEMS * sizeof(int32_t);

  for (int i = 0; i < NUM_CHUNKS; ++i) {
    fill_data(chunk_data, i, 0);
    int64_t nchunks = blosc2_schunk_append_buffer(schunk, chunk_data, buf_bytes);
    mu_assert("ERROR: append_buffer failed", nchunks == i + 1);
  }

  /* Verify all chunk files exist initially */
  for (int i = 0; i < NUM_CHUNKS; ++i) {
    mu_assert("ERROR: initial chunk file missing", chunk_file_exists(SFRAME_DIR, i));
  }
  mu_assert("ERROR: expected 5 chunk files", count_chunk_files(SFRAME_DIR, 100) == 5);

  uint8_t special_buf[BLOSC_EXTENDED_HEADER_LENGTH];
  int32_t decomp[CHUNK_NITEMS];

  /* 1. Update chunk 1 to UNINIT: 00000001.chunk must be deleted */
  int ret = blosc2_chunk_uninit(cparams, buf_bytes, special_buf, sizeof(special_buf));
  mu_assert("ERROR: chunk_uninit failed", ret == BLOSC_EXTENDED_HEADER_LENGTH);
  int64_t nch = blosc2_schunk_update_chunk(schunk, 1, special_buf, true);
  mu_assert("ERROR: update chunk 1 to uninit failed", nch == NUM_CHUNKS);

  mu_assert("ERROR: chunk 1 file must be removed after eviction to uninit", !chunk_file_exists(SFRAME_DIR, 1));
  mu_assert("ERROR: chunk 0 file must still exist", chunk_file_exists(SFRAME_DIR, 0));
  mu_assert("ERROR: chunk 2 file must still exist", chunk_file_exists(SFRAME_DIR, 2));
  mu_assert("ERROR: chunk 3 file must still exist", chunk_file_exists(SFRAME_DIR, 3));
  mu_assert("ERROR: chunk 4 file must still exist", chunk_file_exists(SFRAME_DIR, 4));
  mu_assert("ERROR: expected 4 chunk files remaining", count_chunk_files(SFRAME_DIR, 100) == 4);

  /* 2. Update chunk 2 to ZERO: 00000002.chunk must be deleted */
  ret = blosc2_chunk_zeros(cparams, buf_bytes, special_buf, sizeof(special_buf));
  mu_assert("ERROR: chunk_zeros failed", ret == BLOSC_EXTENDED_HEADER_LENGTH);
  nch = blosc2_schunk_update_chunk(schunk, 2, special_buf, true);
  mu_assert("ERROR: update chunk 2 to zero failed", nch == NUM_CHUNKS);

  mu_assert("ERROR: chunk 2 file must be removed after eviction to zero", !chunk_file_exists(SFRAME_DIR, 2));
  mu_assert("ERROR: expected 3 chunk files remaining", count_chunk_files(SFRAME_DIR, 100) == 3);

  /* 3. Update chunk 3 to NAN: 00000003.chunk must be deleted */
  ret = blosc2_chunk_nans(cparams, buf_bytes, special_buf, sizeof(special_buf));
  mu_assert("ERROR: chunk_nans failed", ret == BLOSC_EXTENDED_HEADER_LENGTH);
  nch = blosc2_schunk_update_chunk(schunk, 3, special_buf, true);
  mu_assert("ERROR: update chunk 3 to nan failed", nch == NUM_CHUNKS);

  mu_assert("ERROR: chunk 3 file must be removed after eviction to nan", !chunk_file_exists(SFRAME_DIR, 3));
  mu_assert("ERROR: expected 2 chunk files remaining", count_chunk_files(SFRAME_DIR, 100) == 2);

  /* 4. Special-to-special update: update chunk 1 (UNINIT) to ZERO */
  nch = blosc2_schunk_update_chunk(schunk, 1, special_buf, true);
  mu_assert("ERROR: special-to-special update failed", nch == NUM_CHUNKS);
  mu_assert("ERROR: special-to-special must not create files", count_chunk_files(SFRAME_DIR, 100) == 2);

  /* 5. Refill (special-to-regular): update chunk 1 back to regular */
  fill_data(chunk_data, 1, 77);
  uint8_t *reg_chunk = malloc(buf_bytes + BLOSC2_MAX_OVERHEAD);
  int csize = blosc2_compress_ctx(schunk->cctx, chunk_data, buf_bytes, reg_chunk, buf_bytes + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: compress failed", csize > 0);
  nch = blosc2_schunk_update_chunk(schunk, 1, reg_chunk, true);
  free(reg_chunk);
  mu_assert("ERROR: refill update failed", nch == NUM_CHUNKS);

  /* A chunk file must have been created */
  mu_assert("ERROR: expected 3 chunk files after refill", count_chunk_files(SFRAME_DIR, 100) == 3);

  /* Decompress and verify refill data */
  int dsize = blosc2_schunk_decompress_chunk(schunk, 1, decomp, buf_bytes);
  mu_assert("ERROR: refilled chunk decompress failed", dsize == buf_bytes);
  mu_assert("ERROR: refilled chunk data mismatch", memcmp(decomp, chunk_data, buf_bytes) == 0);

  /* Verify unaffected regular chunk 0 */
  int32_t exp0[CHUNK_NITEMS];
  fill_data(exp0, 0, 0);
  dsize = blosc2_schunk_decompress_chunk(schunk, 0, decomp, buf_bytes);
  mu_assert("ERROR: chunk 0 decompress failed", dsize == buf_bytes);
  mu_assert("ERROR: chunk 0 data mismatch", memcmp(decomp, exp0, buf_bytes) == 0);

  /* 6. Non-removal control: update chunk 0 to repeated value (BLOSC2_SPECIAL_VALUE) */
  int32_t rep_val = 1234567;
  uint8_t rep_chunk[BLOSC_EXTENDED_HEADER_LENGTH + sizeof(int32_t)];
  int rep_len = blosc2_chunk_repeatval(cparams, buf_bytes, rep_chunk, sizeof(rep_chunk), &rep_val);
  mu_assert("ERROR: chunk_repeatval failed", rep_len > 0);

  nch = blosc2_schunk_update_chunk(schunk, 0, rep_chunk, true);
  mu_assert("ERROR: update to repeatval failed", nch == NUM_CHUNKS);

  /* Chunk file 0 MUST NOT be deleted because it carries payload! */
  mu_assert("ERROR: VALUE chunk file must NOT be deleted", chunk_file_exists(SFRAME_DIR, 0));
  mu_assert("ERROR: expected 3 chunk files still", count_chunk_files(SFRAME_DIR, 100) == 3);

  /* Decompress and check repeated values */
  dsize = blosc2_schunk_decompress_chunk(schunk, 0, decomp, buf_bytes);
  mu_assert("ERROR: VALUE chunk decompress failed", dsize == buf_bytes);
  for (int i = 0; i < CHUNK_NITEMS; ++i) {
    mu_assert("ERROR: VALUE chunk data mismatch", decomp[i] == rep_val);
  }

  /* 7. Reopen sframe from disk and verify persistence */
  blosc2_schunk_free(schunk);
  schunk = blosc2_schunk_open(SFRAME_DIR);
  mu_assert("ERROR: reopen failed", schunk != NULL);
  mu_assert("ERROR: nchunks changed on reopen", schunk->nchunks == NUM_CHUNKS);

  dsize = blosc2_schunk_decompress_chunk(schunk, 0, decomp, buf_bytes);
  mu_assert("ERROR: VALUE chunk on reopen failed", dsize == buf_bytes);
  for (int i = 0; i < CHUNK_NITEMS; ++i) {
    mu_assert("ERROR: VALUE chunk data mismatch on reopen", decomp[i] == rep_val);
  }

  /* 8. Evict ALL chunks to UNINIT: directory must have zero chunk files remaining */
  ret = blosc2_chunk_uninit(cparams, buf_bytes, special_buf, sizeof(special_buf));
  for (int i = 0; i < NUM_CHUNKS; ++i) {
    blosc2_schunk_update_chunk(schunk, i, special_buf, true);
  }
  mu_assert("ERROR: all chunk files must be deleted when all chunks are special",
            count_chunk_files(SFRAME_DIR, 100) == 0);

  /* 9. Refill all chunks: verify files are created cleanly */
  for (int i = 0; i < NUM_CHUNKS; ++i) {
    fill_data(chunk_data, i, 42);
    reg_chunk = malloc(buf_bytes + BLOSC2_MAX_OVERHEAD);
    blosc2_compress_ctx(schunk->cctx, chunk_data, buf_bytes, reg_chunk, buf_bytes + BLOSC2_MAX_OVERHEAD);
    blosc2_schunk_update_chunk(schunk, i, reg_chunk, true);
    free(reg_chunk);
  }
  mu_assert("ERROR: expected 5 chunk files after refilling all",
            count_chunk_files(SFRAME_DIR, 100) == 5);

  /* Verify all decompressed accurately */
  for (int i = 0; i < NUM_CHUNKS; ++i) {
    int32_t exp[CHUNK_NITEMS];
    fill_data(exp, i, 42);
    dsize = blosc2_schunk_decompress_chunk(schunk, i, decomp, buf_bytes);
    mu_assert("ERROR: decompress failed", dsize == buf_bytes);
    mu_assert("ERROR: data mismatch after all refilled", memcmp(decomp, exp, buf_bytes) == 0);
  }

  blosc2_schunk_free(schunk);
  blosc2_remove_dir(SFRAME_DIR);
  return EXIT_SUCCESS;
}

static char *test_sframe_churn_no_orphan_growth(void) {
  blosc2_remove_dir(SFRAME_DIR);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;

  blosc2_storage storage = {
      .cparams = &cparams,
      .dparams = &dparams,
      .urlpath = SFRAME_DIR,
      .contiguous = false,
  };

  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: blosc2_schunk_new failed", schunk != NULL);

  int32_t chunk_data[CHUNK_NITEMS];
  int32_t buf_bytes = CHUNK_NITEMS * sizeof(int32_t);

  /* Append 2 initial chunks */
  fill_data(chunk_data, 0, 1);
  blosc2_schunk_append_buffer(schunk, chunk_data, buf_bytes);
  fill_data(chunk_data, 1, 1);
  blosc2_schunk_append_buffer(schunk, chunk_data, buf_bytes);

  uint8_t special_buf[BLOSC_EXTENDED_HEADER_LENGTH];
  blosc2_chunk_uninit(cparams, buf_bytes, special_buf, sizeof(special_buf));

  /* Repeated eviction and refill loop on chunk 1 */
  for (int iter = 0; iter < 20; ++iter) {
    /* Evict to UNINIT */
    blosc2_schunk_update_chunk(schunk, 1, special_buf, true);
    /* At this point only chunk 0 file should exist (1 total file) */
    int cnt = count_chunk_files(SFRAME_DIR, 100);
    mu_assert("ERROR: orphan file left behind during eviction churn", cnt == 1);

    /* Refill with regular data */
    fill_data(chunk_data, 1, iter + 10);
    uint8_t *reg = malloc(buf_bytes + BLOSC2_MAX_OVERHEAD);
    blosc2_compress_ctx(schunk->cctx, chunk_data, buf_bytes, reg, buf_bytes + BLOSC2_MAX_OVERHEAD);
    blosc2_schunk_update_chunk(schunk, 1, reg, true);
    free(reg);

    /* At this point exactly 2 chunk files should exist */
    cnt = count_chunk_files(SFRAME_DIR, 100);
    mu_assert("ERROR: extra file created during refill churn", cnt == 2);
  }

  blosc2_schunk_free(schunk);
  blosc2_remove_dir(SFRAME_DIR);
  return EXIT_SUCCESS;
}

static char *test_sframe_reordered_physical_id_eviction(void) {
  blosc2_remove_dir(SFRAME_DIR);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;

  blosc2_storage storage = {
      .cparams = &cparams,
      .dparams = &dparams,
      .urlpath = SFRAME_DIR,
      .contiguous = false,
  };

  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: blosc2_schunk_new failed", schunk != NULL);

  int32_t chunk_data[CHUNK_NITEMS];
  int32_t buf_bytes = CHUNK_NITEMS * sizeof(int32_t);

  /* Append 3 chunks: physical IDs 0, 1, 2 */
  for (int i = 0; i < 3; ++i) {
    fill_data(chunk_data, i, 0);
    blosc2_schunk_append_buffer(schunk, chunk_data, buf_bytes);
  }

  /* Reorder: logical chunk 0 -> physical 2, logical chunk 1 -> physical 0, logical chunk 2 -> physical 1 */
  int64_t order[3] = {2, 0, 1};
  int rc = blosc2_schunk_reorder_offsets(schunk, order);
  mu_assert("ERROR: reorder failed", rc == 0);

  /* Now evict logical chunk 0 to UNINIT.
     Logical chunk 0 has physical ID 2 (00000002.chunk).
     We must remove 00000002.chunk, NOT 00000000.chunk! */
  uint8_t special_buf[BLOSC_EXTENDED_HEADER_LENGTH];
  blosc2_chunk_uninit(cparams, buf_bytes, special_buf, sizeof(special_buf));
  int64_t nch = blosc2_schunk_update_chunk(schunk, 0, special_buf, true);
  mu_assert("ERROR: update failed", nch == 3);

  /* Verify physical ID 2 was removed */
  mu_assert("ERROR: physical chunk 2 file must be deleted", !chunk_file_exists(SFRAME_DIR, 2));
  /* Physical IDs 0 and 1 must STILL EXIST */
  mu_assert("ERROR: physical chunk 0 must still exist (not nchunk=0!)", chunk_file_exists(SFRAME_DIR, 0));
  mu_assert("ERROR: physical chunk 1 must still exist", chunk_file_exists(SFRAME_DIR, 1));

  /* Verify logical chunk 1 (physical 0) and logical chunk 2 (physical 1) decompress correctly */
  int32_t decomp[CHUNK_NITEMS];
  int32_t exp0[CHUNK_NITEMS];
  fill_data(exp0, 0, 0);
  int dsize = blosc2_schunk_decompress_chunk(schunk, 1, decomp, buf_bytes);
  mu_assert("ERROR: logical chunk 1 decompress failed", dsize == buf_bytes);
  mu_assert("ERROR: logical chunk 1 data mismatch", memcmp(decomp, exp0, buf_bytes) == 0);

  int32_t exp1[CHUNK_NITEMS];
  fill_data(exp1, 1, 0);
  dsize = blosc2_schunk_decompress_chunk(schunk, 2, decomp, buf_bytes);
  mu_assert("ERROR: logical chunk 2 decompress failed", dsize == buf_bytes);
  mu_assert("ERROR: logical chunk 2 data mismatch", memcmp(decomp, exp1, buf_bytes) == 0);

  blosc2_schunk_free(schunk);
  blosc2_remove_dir(SFRAME_DIR);
  return EXIT_SUCCESS;
}

#include "blosc2/blosc2-stdio.h"

#define MAPPED_IO_ID 246

static char* map_test_path(const char *urlpath) {
  if (strstr(urlpath, ".chunk") != NULL) {
    size_t len = strlen(urlpath) + 16;
    char *mapped = malloc(len);
    snprintf(mapped, len, "%s.mapped", urlpath);
    return mapped;
  }
  size_t len = strlen(urlpath) + 1;
  char *res = malloc(len);
  memcpy(res, urlpath, len);
  return res;
}

static void* mapped_io_open(const char *urlpath, const char *mode, void *params) {
  char *mapped = map_test_path(urlpath);
  void *fp = blosc2_stdio_open(mapped, mode, params);
  free(mapped);
  return fp;
}

static int mapped_io_close(void *stream) {
  return blosc2_stdio_close(stream);
}

static int64_t mapped_io_size(void *stream) {
  return blosc2_stdio_size(stream);
}

static int64_t mapped_io_write(const void *ptr, int64_t size, int64_t nitems, int64_t position, void *stream) {
  return blosc2_stdio_write(ptr, size, nitems, position, stream);
}

static int64_t mapped_io_read(void **ptr, int64_t size, int64_t nitems, int64_t position, void *stream) {
  return blosc2_stdio_read(ptr, size, nitems, position, stream);
}

static int mapped_io_truncate(void *stream, int64_t size) {
  return blosc2_stdio_truncate(stream, size);
}

static int mapped_io_destroy(void *params) {
  BLOSC_UNUSED_PARAM(params);
  return 0;
}

static char *test_sframe_custom_io_mapped_filenames_eviction(void) {
  blosc2_remove_dir(SFRAME_DIR);

  blosc2_io_cb io_cb;
  memset(&io_cb, 0, sizeof(io_cb));
  io_cb.id = MAPPED_IO_ID;
  io_cb.name = "mapped_test_io";
  io_cb.is_allocation_necessary = true;
  io_cb.open = (blosc2_open_cb) mapped_io_open;
  io_cb.close = (blosc2_close_cb) mapped_io_close;
  io_cb.read = (blosc2_read_cb) mapped_io_read;
  io_cb.size = (blosc2_size_cb) mapped_io_size;
  io_cb.write = (blosc2_write_cb) mapped_io_write;
  io_cb.truncate = (blosc2_truncate_cb) mapped_io_truncate;
  io_cb.destroy = (blosc2_destroy_cb) mapped_io_destroy;

  int rc = blosc2_register_io_cb(&io_cb);
  mu_assert("ERROR: register custom io failed", rc >= 0);

  blosc2_io io = {.id = MAPPED_IO_ID, .name = "mapped_test_io", .params = NULL};

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;

  blosc2_storage storage = {
      .cparams = &cparams,
      .dparams = &dparams,
      .io = &io,
      .urlpath = SFRAME_DIR,
      .contiguous = false,
  };

  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: blosc2_schunk_new failed for custom mapped io", schunk != NULL);

  int32_t chunk_data[CHUNK_NITEMS];
  int32_t buf_bytes = CHUNK_NITEMS * sizeof(int32_t);

  for (int i = 0; i < 3; ++i) {
    fill_data(chunk_data, i, 1);
    int64_t nchunks = blosc2_schunk_append_buffer(schunk, chunk_data, buf_bytes);
    mu_assert("ERROR: append_buffer failed", nchunks == i + 1);
  }

  /* Create an unrelated decoy file at the unmapped path (SFRAME_DIR/00000001.chunk).
     Eviction in custom I/O backend must use backend callbacks exclusively and NOT delete this file. */
  char decoy_path[512];
  snprintf(decoy_path, sizeof(decoy_path), "%s/%08" PRIX32 ".chunk", SFRAME_DIR, (uint32_t)1);
  FILE *fdecoy = fopen(decoy_path, "wb");
  mu_assert("ERROR: cannot create decoy file", fdecoy != NULL);
  fputs("unrelated", fdecoy);
  fclose(fdecoy);

  /* Update chunk 1 to ZERO: old chunk was stored at 00000001.chunk.mapped.
     Custom I/O backend callbacks must be used exclusively.
     The decoy file at SFRAME_DIR/00000001.chunk must not be deleted.
     The mapped chunk file at SFRAME_DIR/00000001.chunk.mapped must be truncated to 0 bytes. */
  uint8_t special_buf[BLOSC_EXTENDED_HEADER_LENGTH];
  int ret = blosc2_chunk_zeros(cparams, buf_bytes, special_buf, sizeof(special_buf));
  mu_assert("ERROR: chunk_zeros failed", ret == BLOSC_EXTENDED_HEADER_LENGTH);

  int64_t nch = blosc2_schunk_update_chunk(schunk, 1, special_buf, true);
  mu_assert("ERROR: update chunk 1 to zero on custom I/O must succeed (not -21)", nch == 3);

  /* Assert the unrelated decoy file at the unmapped path was NOT deleted */
  FILE *fcheck = fopen(decoy_path, "rb");
  mu_assert("ERROR: custom I/O eviction must NOT delete unrelated file at unmapped path", fcheck != NULL);
  if (fcheck != NULL) fclose(fcheck);

  /* Assert the mapped chunk file was truncated to 0 bytes */
  char mapped_path[512];
  snprintf(mapped_path, sizeof(mapped_path), "%s/%08" PRIX32 ".chunk.mapped", SFRAME_DIR, (uint32_t)1);
  FILE *fmap = fopen(mapped_path, "rb");
  mu_assert("ERROR: mapped chunk file should exist", fmap != NULL);
  if (fmap != NULL) {
    fseek(fmap, 0, SEEK_END);
    long map_sz = ftell(fmap);
    fclose(fmap);
    mu_assert("ERROR: mapped chunk file must be truncated to 0 bytes upon eviction", map_sz == 0);
  }

  /* Verify reading chunk 1 returns zeros */
  int32_t decomp[CHUNK_NITEMS];
  int dsize = blosc2_schunk_decompress_chunk(schunk, 1, decomp, buf_bytes);
  mu_assert("ERROR: decompress ZERO chunk failed", dsize == buf_bytes);
  for (int i = 0; i < CHUNK_NITEMS; ++i) {
    mu_assert("ERROR: chunk 1 data should be zero", decomp[i] == 0);
  }

  /* Verify unaffected chunk 0 and chunk 2 */
  int32_t exp[CHUNK_NITEMS];
  fill_data(exp, 0, 1);
  dsize = blosc2_schunk_decompress_chunk(schunk, 0, decomp, buf_bytes);
  mu_assert("ERROR: decompress chunk 0 failed", dsize == buf_bytes);
  mu_assert("ERROR: chunk 0 mismatch", memcmp(decomp, exp, buf_bytes) == 0);

  fill_data(exp, 2, 1);
  dsize = blosc2_schunk_decompress_chunk(schunk, 2, decomp, buf_bytes);
  mu_assert("ERROR: decompress chunk 2 failed", dsize == buf_bytes);
  mu_assert("ERROR: chunk 2 mismatch", memcmp(decomp, exp, buf_bytes) == 0);

  /* Reopen sframe using custom I/O */
  blosc2_schunk_free(schunk);
  schunk = blosc2_schunk_open_udio(SFRAME_DIR, &io);
  mu_assert("ERROR: reopen with custom I/O failed", schunk != NULL);
  mu_assert("ERROR: nchunks changed on reopen", schunk->nchunks == 3);

  dsize = blosc2_schunk_decompress_chunk(schunk, 1, decomp, buf_bytes);
  mu_assert("ERROR: decompress ZERO chunk on reopen failed", dsize == buf_bytes);
  for (int i = 0; i < CHUNK_NITEMS; ++i) {
    mu_assert("ERROR: chunk 1 data on reopen should be zero", decomp[i] == 0);
  }

  blosc2_schunk_free(schunk);
  blosc2_remove_dir(SFRAME_DIR);
  return EXIT_SUCCESS;
}

static char *all_tests(void) {
  mu_run_test(test_sframe_eviction_and_refill);
  mu_run_test(test_sframe_churn_no_orphan_growth);
  mu_run_test(test_sframe_reordered_physical_id_eviction);
  mu_run_test(test_sframe_custom_io_mapped_filenames_eviction);
  return EXIT_SUCCESS;
}

int main(void) {
  blosc2_init();
  char *result = all_tests();
  blosc2_destroy();
  if (result != 0) {
    printf("FAILED: %s\n", result);
  } else {
    printf("ALL TESTS PASSED (%d tests)\n", tests_run);
  }
  return result != 0;
}
