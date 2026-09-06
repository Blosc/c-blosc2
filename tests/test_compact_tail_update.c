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
#include "test_common.h"
#include "frame.h"

int tests_run = 0;

#define TEST_FILE "test_compact_tail.bin"
#define TEST_FRAME "test_compact_tail.b2frame"

/* ----------------------------------------------------------------------
   Custom recording I/O callback fixture to deterministically verify:
   1. Transfers are bounded by the cap
   2. Transfer direction is strictly forward for shrink (dst < src)
   3. Transfer direction is strictly backward for growth (dst > src)
   4. Total bytes transferred match expected length
   5. Fault injection (simulated short read/write)
   ---------------------------------------------------------------------- */

typedef struct {
  FILE *file;
  int read_calls;
  int write_calls;
  int64_t max_read_transfer;
  int64_t max_write_transfer;
  int64_t total_read_bytes;
  int64_t total_written_bytes;
  int64_t last_read_pos;
  int64_t last_write_pos;
  bool read_dir_forward;
  bool read_dir_backward;
  bool direction_error;
  bool inject_read_fail;
  int fail_at_read_call;
  bool inject_write_fail;
  int fail_at_write_call;
} recording_file_t;

static int64_t recording_read(void **ptr, int64_t size, int64_t nitems, int64_t position, void *stream) {
  recording_file_t *rec = (recording_file_t *)stream;
  rec->read_calls++;
  int64_t nbytes = size * nitems;

  if (rec->inject_read_fail && rec->read_calls == rec->fail_at_read_call) {
    return 0; /* Simulated read error */
  }

  if (nbytes > rec->max_read_transfer) {
    rec->max_read_transfer = nbytes;
  }
  if (rec->read_calls > 1) {
    if (rec->read_dir_forward && position <= rec->last_read_pos) {
      rec->direction_error = true;
    }
    if (rec->read_dir_backward && position >= rec->last_read_pos) {
      rec->direction_error = true;
    }
  }
  rec->last_read_pos = position;
  rec->total_read_bytes += nbytes;

  if (fseek(rec->file, (long)position, SEEK_SET) != 0) {
    return 0;
  }
  size_t r = fread(*ptr, (size_t)size, (size_t)nitems, rec->file);
  return (int64_t)r;
}

static int64_t recording_write(const void *ptr, int64_t size, int64_t nitems, int64_t position, void *stream) {
  recording_file_t *rec = (recording_file_t *)stream;
  rec->write_calls++;
  int64_t nbytes = size * nitems;

  if (rec->inject_write_fail && rec->write_calls == rec->fail_at_write_call) {
    return 0; /* Simulated write error */
  }

  if (nbytes > rec->max_write_transfer) {
    rec->max_write_transfer = nbytes;
  }
  rec->last_write_pos = position;
  rec->total_written_bytes += nbytes;

  if (fseek(rec->file, (long)position, SEEK_SET) != 0) {
    return 0;
  }
  size_t w = fwrite(ptr, (size_t)size, (size_t)nitems, rec->file);
  return (int64_t)w;
}

static blosc2_io_cb create_recording_cb(void) {
  blosc2_io_cb cb;
  memset(&cb, 0, sizeof(cb));
  cb.id = 250;
  cb.name = "recording_io";
  cb.is_allocation_necessary = true;
  cb.read = recording_read;
  cb.write = recording_write;
  return cb;
}

/* ----------------------------------------------------------------------
   Unit tests for frame_move_range
   ---------------------------------------------------------------------- */

static char *test_move_range_invalid_and_noop(void) {
  blosc2_io_cb cb = create_recording_cb();
  recording_file_t rec;
  memset(&rec, 0, sizeof(rec));

  /* Null arguments */
  mu_assert("ERROR: null io_cb should fail",
            frame_move_range(NULL, &rec, 0, 10, 100, 0) == BLOSC2_ERROR_NULL_POINTER);
  mu_assert("ERROR: null fp should fail",
            frame_move_range(&cb, NULL, 0, 10, 100, 0) == BLOSC2_ERROR_NULL_POINTER);

  /* Negative parameters */
  mu_assert("ERROR: negative length should fail",
            frame_move_range(&cb, &rec, 0, 10, -5, 0) == BLOSC2_ERROR_INVALID_PARAM);
  mu_assert("ERROR: negative src_pos should fail",
            frame_move_range(&cb, &rec, -1, 10, 5, 0) == BLOSC2_ERROR_INVALID_PARAM);
  mu_assert("ERROR: negative dst_pos should fail",
            frame_move_range(&cb, &rec, 10, -1, 5, 0) == BLOSC2_ERROR_INVALID_PARAM);

  /* Position overflow */
  mu_assert("ERROR: src overflow should fail",
            frame_move_range(&cb, &rec, INT64_MAX - 5, 10, 10, 0) == BLOSC2_ERROR_INVALID_PARAM);
  mu_assert("ERROR: dst overflow should fail",
            frame_move_range(&cb, &rec, 10, INT64_MAX - 5, 10, 0) == BLOSC2_ERROR_INVALID_PARAM);

  /* No-op cases: length == 0 or src_pos == dst_pos */
  mu_assert("ERROR: zero length should succeed as no-op",
            frame_move_range(&cb, &rec, 10, 20, 0, 0) == BLOSC2_ERROR_SUCCESS);
  mu_assert("ERROR: equal src and dst should succeed as no-op",
            frame_move_range(&cb, &rec, 50, 50, 100, 0) == BLOSC2_ERROR_SUCCESS);
  mu_assert("ERROR: no-op must not perform any transfers",
            rec.read_calls == 0 && rec.write_calls == 0);

  return EXIT_SUCCESS;
}

static char *test_move_range_shrink_direction_and_cap(void) {
  /* Set up a file with 10,000 deterministic bytes */
  const int file_size = 10000;
  uint8_t original[10000];
  for (int i = 0; i < file_size; ++i) {
    original[i] = (uint8_t)(i % 251);
  }

  /* Test shrink (dst < src): tail moves forward */
  int64_t src = 3000;
  int64_t dst = 1000;
  int64_t length = 4500;
  int64_t cap = 512; /* Smaller than displacement and length */

  FILE *f = fopen(TEST_FILE, "w+b");
  mu_assert("ERROR: cannot open test file", f != NULL);
  fwrite(original, 1, file_size, f);
  fflush(f);

  recording_file_t rec;
  memset(&rec, 0, sizeof(rec));
  rec.file = f;
  rec.read_dir_forward = true;

  blosc2_io_cb cb = create_recording_cb();
  int rc = frame_move_range(&cb, &rec, src, dst, length, cap);
  mu_assert("ERROR: frame_move_range shrink failed", rc == BLOSC2_ERROR_SUCCESS);

  /* Verify recording invariants */
  mu_assert("ERROR: max transfer must not exceed cap", rec.max_read_transfer <= cap);
  mu_assert("ERROR: max write transfer must not exceed cap", rec.max_write_transfer <= cap);
  mu_assert("ERROR: total read bytes mismatch", rec.total_read_bytes == length);
  mu_assert("ERROR: total written bytes mismatch", rec.total_written_bytes == length);
  mu_assert("ERROR: shrink transfer must be monotonically forward", !rec.direction_error);

  /* Verify data at destination */
  uint8_t readback[10000];
  fseek(f, 0, SEEK_SET);
  size_t n = fread(readback, 1, file_size, f);
  fclose(f);
  mu_assert("ERROR: fread failed", (int)n == file_size);

  /* Bytes before dst should be untouched */
  mu_assert("ERROR: pre-dst corrupted", memcmp(readback, original, (size_t)dst) == 0);
  /* Bytes moved to dst must match original[src .. src+length] */
  mu_assert("ERROR: moved data corrupted", memcmp(readback + dst, original + src, (size_t)length) == 0);

  remove(TEST_FILE);
  return EXIT_SUCCESS;
}

static char *test_move_range_growth_direction_and_cap(void) {
  /* Set up a file with 10,000 deterministic bytes */
  const int file_size = 10000;
  uint8_t original[10000];
  for (int i = 0; i < file_size; ++i) {
    original[i] = (uint8_t)(i % 251);
  }

  /* Test growth (dst > src): tail moves backward */
  int64_t src = 1000;
  int64_t dst = 2500;
  int64_t length = 4000;
  int64_t cap = 300; /* Smaller than displacement and length */

  FILE *f = fopen(TEST_FILE, "w+b");
  mu_assert("ERROR: cannot open test file", f != NULL);
  fwrite(original, 1, file_size, f);
  fflush(f);

  recording_file_t rec;
  memset(&rec, 0, sizeof(rec));
  rec.file = f;
  rec.read_dir_backward = true;

  blosc2_io_cb cb = create_recording_cb();
  int rc = frame_move_range(&cb, &rec, src, dst, length, cap);
  mu_assert("ERROR: frame_move_range growth failed", rc == BLOSC2_ERROR_SUCCESS);

  /* Verify recording invariants */
  mu_assert("ERROR: max transfer must not exceed cap", rec.max_read_transfer <= cap);
  mu_assert("ERROR: max write transfer must not exceed cap", rec.max_write_transfer <= cap);
  mu_assert("ERROR: total read bytes mismatch", rec.total_read_bytes == length);
  mu_assert("ERROR: total written bytes mismatch", rec.total_written_bytes == length);
  mu_assert("ERROR: growth transfer must be monotonically backward", !rec.direction_error);

  /* Verify data at destination */
  uint8_t readback[10000];
  fseek(f, 0, SEEK_SET);
  size_t n = fread(readback, 1, file_size, f);
  fclose(f);
  mu_assert("ERROR: fread failed", (int)n == file_size);

  /* Bytes before src should be untouched */
  mu_assert("ERROR: pre-src corrupted", memcmp(readback, original, (size_t)src) == 0);
  /* Bytes moved to dst must match original[src .. src+length] */
  mu_assert("ERROR: moved data corrupted", memcmp(readback + dst, original + src, (size_t)length) == 0);

  remove(TEST_FILE);
  return EXIT_SUCCESS;
}

static char *test_move_range_small_overlap(void) {
  /* Test when displacement is smaller than the cap (overlap smaller than cap) */
  const int file_size = 5000;
  uint8_t original[5000];
  for (int i = 0; i < file_size; ++i) {
    original[i] = (uint8_t)(i % 179);
  }

  /* Shrink with 7-byte displacement, cap = 256 */
  {
    FILE *f = fopen(TEST_FILE, "w+b");
    fwrite(original, 1, file_size, f);
    fflush(f);
    recording_file_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.file = f;
    rec.read_dir_forward = true;
    blosc2_io_cb cb = create_recording_cb();

    int rc = frame_move_range(&cb, &rec, 507, 500, 2000, 256);
    mu_assert("ERROR: small overlap shrink failed", rc == BLOSC2_ERROR_SUCCESS);
    mu_assert("ERROR: direction violation in shrink", !rec.direction_error);

    uint8_t readback[5000];
    fseek(f, 0, SEEK_SET);
    fread(readback, 1, file_size, f);
    fclose(f);
    mu_assert("ERROR: moved data corrupted", memcmp(readback + 500, original + 507, 2000) == 0);
    remove(TEST_FILE);
  }

  /* Growth with 7-byte displacement, cap = 256 */
  {
    FILE *f = fopen(TEST_FILE, "w+b");
    fwrite(original, 1, file_size, f);
    fflush(f);
    recording_file_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.file = f;
    rec.read_dir_backward = true;
    blosc2_io_cb cb = create_recording_cb();

    int rc = frame_move_range(&cb, &rec, 500, 507, 2000, 256);
    mu_assert("ERROR: small overlap growth failed", rc == BLOSC2_ERROR_SUCCESS);
    mu_assert("ERROR: direction violation in growth", !rec.direction_error);

    uint8_t readback[5000];
    fseek(f, 0, SEEK_SET);
    fread(readback, 1, file_size, f);
    fclose(f);
    mu_assert("ERROR: moved data corrupted", memcmp(readback + 507, original + 500, 2000) == 0);
    remove(TEST_FILE);
  }

  return EXIT_SUCCESS;
}

static char *test_move_range_fault_injection(void) {
  const int file_size = 2000;
  uint8_t original[2000];
  memset(original, 0xAA, file_size);

  /* Read failure injection */
  {
    FILE *f = fopen(TEST_FILE, "w+b");
    fwrite(original, 1, file_size, f);
    fflush(f);

    recording_file_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.file = f;
    rec.inject_read_fail = true;
    rec.fail_at_read_call = 2;

    blosc2_io_cb cb = create_recording_cb();
    int rc = frame_move_range(&cb, &rec, 500, 200, 1000, 200);
    fclose(f);
    remove(TEST_FILE);
    mu_assert("ERROR: expected read failure error code", rc == BLOSC2_ERROR_FILE_READ);
  }

  /* Write failure injection */
  {
    FILE *f = fopen(TEST_FILE, "w+b");
    fwrite(original, 1, file_size, f);
    fflush(f);

    recording_file_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.file = f;
    rec.inject_write_fail = true;
    rec.fail_at_write_call = 2;

    blosc2_io_cb cb = create_recording_cb();
    int rc = frame_move_range(&cb, &rec, 500, 200, 1000, 200);
    fclose(f);
    remove(TEST_FILE);
    mu_assert("ERROR: expected write failure error code", rc == BLOSC2_ERROR_FILE_WRITE);
  }

  return EXIT_SUCCESS;
}

/* ----------------------------------------------------------------------
   Integration tests for contiguous frame_update_chunk
   ---------------------------------------------------------------------- */

#define CHUNK_NITEMS (5000)
#define NUM_CHUNKS (5)

static void fill_chunk_data(int64_t *buf, int chunk_idx, int variation) {
  for (int i = 0; i < CHUNK_NITEMS; ++i) {
    buf[i] = (int64_t)(chunk_idx * 1000000 + variation * 10000 + i);
  }
}

static char *test_contiguous_tail_update_positions(void) {
  blosc2_remove_urlpath(TEST_FRAME);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int64_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 5;

  blosc2_storage storage = {
      .cparams = &cparams,
      .dparams = &dparams,
      .urlpath = TEST_FRAME,
      .contiguous = true,
  };

  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: blosc2_schunk_new failed", schunk != NULL);

  int64_t chunk_data[CHUNK_NITEMS];
  int32_t buf_bytes = CHUNK_NITEMS * sizeof(int64_t);

  for (int i = 0; i < NUM_CHUNKS; ++i) {
    fill_chunk_data(chunk_data, i, 0);
    int64_t nchunks = blosc2_schunk_append_buffer(schunk, chunk_data, buf_bytes);
    mu_assert("ERROR: append_buffer failed", nchunks == i + 1);
  }

  int64_t decomp[CHUNK_NITEMS];

  /* 1. Growth at first position (nchunk = 0): tail chunks 1..4 shift backward */
  {
    /* Fill with non-repetitive pseudorandom data to make compressed size larger */
    for (int i = 0; i < CHUNK_NITEMS; ++i) {
      chunk_data[i] = (int64_t)rand() * 1000 + i;
    }
    int32_t chunk_alloc = buf_bytes + BLOSC2_MAX_OVERHEAD;
    uint8_t *chunk = malloc(chunk_alloc);
    int csize = blosc2_compress_ctx(schunk->cctx, chunk_data, buf_bytes, chunk, chunk_alloc);
    mu_assert("ERROR: compress failed", csize > 0);

    int64_t nchunks_ = blosc2_schunk_update_chunk(schunk, 0, chunk, true);
    free(chunk);
    mu_assert("ERROR: update chunk 0 failed", nchunks_ == NUM_CHUNKS);

    /* Verify chunk 0 */
    int dsize = blosc2_schunk_decompress_chunk(schunk, 0, decomp, buf_bytes);
    mu_assert("ERROR: chunk 0 decompress failed", dsize == buf_bytes);
    mu_assert("ERROR: chunk 0 data mismatch", memcmp(decomp, chunk_data, buf_bytes) == 0);

    /* Verify chunks 1..4 are 100% unchanged */
    for (int i = 1; i < NUM_CHUNKS; ++i) {
      int64_t expected[CHUNK_NITEMS];
      fill_chunk_data(expected, i, 0);
      dsize = blosc2_schunk_decompress_chunk(schunk, i, decomp, buf_bytes);
      mu_assert("ERROR: chunk decompress failed", dsize == buf_bytes);
      mu_assert("ERROR: unaffected chunk corrupted after growth", memcmp(decomp, expected, buf_bytes) == 0);
    }
  }

  /* 2. Shrink at middle position (nchunk = 2): tail chunks 3..4 shift forward */
  {
    /* Fill with constant to make compressed size tiny (shrink) */
    for (int i = 0; i < CHUNK_NITEMS; ++i) {
      chunk_data[i] = 42;
    }
    int32_t chunk_alloc = buf_bytes + BLOSC2_MAX_OVERHEAD;
    uint8_t *chunk = malloc(chunk_alloc);
    int csize = blosc2_compress_ctx(schunk->cctx, chunk_data, buf_bytes, chunk, chunk_alloc);
    mu_assert("ERROR: compress failed", csize > 0);

    int64_t nchunks_ = blosc2_schunk_update_chunk(schunk, 2, chunk, true);
    free(chunk);
    mu_assert("ERROR: update chunk 2 failed", nchunks_ == NUM_CHUNKS);

    /* Verify chunk 2 */
    int dsize = blosc2_schunk_decompress_chunk(schunk, 2, decomp, buf_bytes);
    mu_assert("ERROR: chunk 2 decompress failed", dsize == buf_bytes);
    mu_assert("ERROR: chunk 2 data mismatch", memcmp(decomp, chunk_data, buf_bytes) == 0);

    /* Verify chunks 1, 3, 4 unchanged */
    for (int i = 1; i < NUM_CHUNKS; ++i) {
      if (i == 2) continue;
      int64_t expected[CHUNK_NITEMS];
      fill_chunk_data(expected, i, 0);
      dsize = blosc2_schunk_decompress_chunk(schunk, i, decomp, buf_bytes);
      mu_assert("ERROR: chunk decompress failed", dsize == buf_bytes);
      mu_assert("ERROR: unaffected chunk corrupted after shrink", memcmp(decomp, expected, buf_bytes) == 0);
    }
  }

  /* 3. Update last position (nchunk = 4): empty tail, no tail movement */
  {
    fill_chunk_data(chunk_data, 4, 99);
    int32_t chunk_alloc = buf_bytes + BLOSC2_MAX_OVERHEAD;
    uint8_t *chunk = malloc(chunk_alloc);
    int csize = blosc2_compress_ctx(schunk->cctx, chunk_data, buf_bytes, chunk, chunk_alloc);
    mu_assert("ERROR: compress failed", csize > 0);

    int64_t nchunks_ = blosc2_schunk_update_chunk(schunk, 4, chunk, true);
    free(chunk);
    mu_assert("ERROR: update chunk 4 failed", nchunks_ == NUM_CHUNKS);

    int dsize = blosc2_schunk_decompress_chunk(schunk, 4, decomp, buf_bytes);
    mu_assert("ERROR: chunk 4 decompress failed", dsize == buf_bytes);
    mu_assert("ERROR: chunk 4 data mismatch", memcmp(decomp, chunk_data, buf_bytes) == 0);
  }

  /* 4. Equal size replacement */
  {
    fill_chunk_data(chunk_data, 1, 77);
    int32_t chunk_alloc = buf_bytes + BLOSC2_MAX_OVERHEAD;
    uint8_t *chunk = malloc(chunk_alloc);
    int csize = blosc2_compress_ctx(schunk->cctx, chunk_data, buf_bytes, chunk, chunk_alloc);
    mu_assert("ERROR: compress failed", csize > 0);

    int64_t nchunks_ = blosc2_schunk_update_chunk(schunk, 1, chunk, true);
    free(chunk);
    mu_assert("ERROR: update chunk 1 failed", nchunks_ == NUM_CHUNKS);

    int dsize = blosc2_schunk_decompress_chunk(schunk, 1, decomp, buf_bytes);
    mu_assert("ERROR: chunk 1 decompress failed", dsize == buf_bytes);
    mu_assert("ERROR: chunk 1 data mismatch", memcmp(decomp, chunk_data, buf_bytes) == 0);
  }

  /* 5. Reopen the frame from disk and verify consistency */
  blosc2_schunk_free(schunk);
  schunk = blosc2_schunk_open(TEST_FRAME);
  mu_assert("ERROR: blosc2_schunk_open failed", schunk != NULL);
  mu_assert("ERROR: nchunks changed on reopen", schunk->nchunks == NUM_CHUNKS);

  int dsize = blosc2_schunk_decompress_chunk(schunk, 1, decomp, buf_bytes);
  mu_assert("ERROR: chunk 1 decompress after reopen failed", dsize == buf_bytes);
  mu_assert("ERROR: chunk 1 mismatch after reopen", memcmp(decomp, chunk_data, buf_bytes) == 0);

  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(TEST_FRAME);
  return EXIT_SUCCESS;
}

static char *test_contiguous_special_transitions(void) {
  blosc2_remove_urlpath(TEST_FRAME);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int64_t);
  cparams.compcode = BLOSC_BLOSCLZ;

  blosc2_storage storage = {
      .cparams = &cparams,
      .dparams = &dparams,
      .urlpath = TEST_FRAME,
      .contiguous = true,
  };

  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: blosc2_schunk_new failed", schunk != NULL);

  int64_t chunk_data[CHUNK_NITEMS];
  int32_t buf_bytes = CHUNK_NITEMS * sizeof(int64_t);

  for (int i = 0; i < 4; ++i) {
    fill_chunk_data(chunk_data, i, 0);
    blosc2_schunk_append_buffer(schunk, chunk_data, buf_bytes);
  }

  uint8_t special_buf[BLOSC_EXTENDED_HEADER_LENGTH];
  int64_t decomp[CHUNK_NITEMS];

  /* 1. Regular -> UNINIT on chunk 1 */
  int ret = blosc2_chunk_uninit(cparams, buf_bytes, special_buf, sizeof(special_buf));
  mu_assert("ERROR: chunk_uninit failed", ret == BLOSC_EXTENDED_HEADER_LENGTH);
  int64_t nch = blosc2_schunk_update_chunk(schunk, 1, special_buf, true);
  mu_assert("ERROR: update to uninit failed", nch == 4);

  /* Verify chunk 0, 2, 3 intact */
  for (int i = 0; i < 4; ++i) {
    if (i == 1) continue;
    int64_t exp[CHUNK_NITEMS];
    fill_chunk_data(exp, i, 0);
    int dsize = blosc2_schunk_decompress_chunk(schunk, i, decomp, buf_bytes);
    mu_assert("ERROR: decompress failed", dsize == buf_bytes);
    mu_assert("ERROR: chunk data changed after regular->UNINIT", memcmp(decomp, exp, buf_bytes) == 0);
  }

  /* 2. UNINIT -> Regular on chunk 1 */
  fill_chunk_data(chunk_data, 1, 555);
  uint8_t *reg_chunk = malloc(buf_bytes + BLOSC2_MAX_OVERHEAD);
  int csize = blosc2_compress_ctx(schunk->cctx, chunk_data, buf_bytes, reg_chunk, buf_bytes + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: compress failed", csize > 0);
  nch = blosc2_schunk_update_chunk(schunk, 1, reg_chunk, true);
  free(reg_chunk);
  mu_assert("ERROR: update to regular failed", nch == 4);

  int dsize = blosc2_schunk_decompress_chunk(schunk, 1, decomp, buf_bytes);
  mu_assert("ERROR: chunk 1 decompress failed", dsize == buf_bytes);
  mu_assert("ERROR: chunk 1 data mismatch after UNINIT->regular", memcmp(decomp, chunk_data, buf_bytes) == 0);

  /* 3. Regular -> ZERO on chunk 2 */
  ret = blosc2_chunk_zeros(cparams, buf_bytes, special_buf, sizeof(special_buf));
  mu_assert("ERROR: chunk_zeros failed", ret == BLOSC_EXTENDED_HEADER_LENGTH);
  nch = blosc2_schunk_update_chunk(schunk, 2, special_buf, true);
  mu_assert("ERROR: update to zero failed", nch == 4);

  dsize = blosc2_schunk_decompress_chunk(schunk, 2, decomp, buf_bytes);
  mu_assert("ERROR: chunk 2 decompress failed", dsize == buf_bytes);
  for (int i = 0; i < CHUNK_NITEMS; ++i) {
    mu_assert("ERROR: expected zero chunk", decomp[i] == 0);
  }

  /* 4. ZERO -> NAN on chunk 2 */
  ret = blosc2_chunk_nans(cparams, buf_bytes, special_buf, sizeof(special_buf));
  mu_assert("ERROR: chunk_nans failed", ret == BLOSC_EXTENDED_HEADER_LENGTH);
  nch = blosc2_schunk_update_chunk(schunk, 2, special_buf, true);
  mu_assert("ERROR: update to nan failed", nch == 4);

  /* Verify reopen */
  blosc2_schunk_free(schunk);
  schunk = blosc2_schunk_open(TEST_FRAME);
  mu_assert("ERROR: reopen failed", schunk != NULL);
  mu_assert("ERROR: nchunks changed", schunk->nchunks == 4);

  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(TEST_FRAME);
  return EXIT_SUCCESS;
}

static char *test_contiguous_reordered_tail_update(void) {
  blosc2_remove_urlpath(TEST_FRAME);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int64_t);
  cparams.compcode = BLOSC_BLOSCLZ;

  blosc2_storage storage = {
      .cparams = &cparams,
      .dparams = &dparams,
      .urlpath = TEST_FRAME,
      .contiguous = true,
  };

  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: blosc2_schunk_new failed", schunk != NULL);

  int64_t chunk_data[CHUNK_NITEMS];
  int32_t buf_bytes = CHUNK_NITEMS * sizeof(int64_t);

  for (int i = 0; i < 4; ++i) {
    fill_chunk_data(chunk_data, i, 0);
    blosc2_schunk_append_buffer(schunk, chunk_data, buf_bytes);
  }

  /* Reorder offsets logically: 3, 2, 1, 0 */
  int64_t new_order[4] = {3, 2, 1, 0};
  int rc = blosc2_schunk_reorder_offsets(schunk, new_order);
  mu_assert("ERROR: reorder offsets failed", rc == 0);

  /* Update logical chunk 0 (which maps to physical chunk 3) */
  fill_chunk_data(chunk_data, 9, 999);
  uint8_t *chunk = malloc(buf_bytes + BLOSC2_MAX_OVERHEAD);
  int csize = blosc2_compress_ctx(schunk->cctx, chunk_data, buf_bytes, chunk, buf_bytes + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: compress failed", csize > 0);

  int64_t nch = blosc2_schunk_update_chunk(schunk, 0, chunk, true);
  free(chunk);
  mu_assert("ERROR: update reordered chunk failed", nch == 4);

  int64_t decomp[CHUNK_NITEMS];
  int dsize = blosc2_schunk_decompress_chunk(schunk, 0, decomp, buf_bytes);
  mu_assert("ERROR: decompress reordered failed", dsize == buf_bytes);
  mu_assert("ERROR: data mismatch in reordered update", memcmp(decomp, chunk_data, buf_bytes) == 0);

  /* Verify logical chunk 3 (which was original chunk 0) */
  int64_t exp0[CHUNK_NITEMS];
  fill_chunk_data(exp0, 0, 0);
  dsize = blosc2_schunk_decompress_chunk(schunk, 3, decomp, buf_bytes);
  mu_assert("ERROR: decompress logical chunk 3 failed", dsize == buf_bytes);
  mu_assert("ERROR: data mismatch in logical chunk 3", memcmp(decomp, exp0, buf_bytes) == 0);

  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(TEST_FRAME);
  return EXIT_SUCCESS;
}

#include "blosc2/blosc2-stdio.h"

#define MMAP_CHUNK_ITEMS (75000) /* 300 KB of int32 per chunk */
#define MMAP_NUM_CHUNKS (6)       /* 4 random chunks in tail = ~1.2 MB > 1 MiB cap */

static char *test_contiguous_mmap_update(void) {
  blosc2_remove_urlpath(TEST_FRAME);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 1;

  blosc2_stdio_mmap mmap_file = BLOSC2_STDIO_MMAP_DEFAULTS;
  mmap_file.mode = "w+";
  mmap_file.initial_mapping_size = 128 * 1024; /* 128 KB to force remapping on writes */
  blosc2_io io = {.id = BLOSC2_IO_FILESYSTEM_MMAP, .name = "filesystem_mmap", .params = &mmap_file};

  blosc2_storage storage = {
      .cparams = &cparams,
      .dparams = &dparams,
      .io = &io,
      .urlpath = TEST_FRAME,
      .contiguous = true,
  };

  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("ERROR: blosc2_schunk_new failed for mmap", schunk != NULL);

  int32_t chunk_bytes = MMAP_CHUNK_ITEMS * (int32_t)sizeof(int32_t);
  int32_t *chunks_data[MMAP_NUM_CHUNKS];
  for (int i = 0; i < MMAP_NUM_CHUNKS; ++i) {
    chunks_data[i] = malloc(chunk_bytes);
  }

  /* Chunk 0: compressible sequential values */
  for (int j = 0; j < MMAP_CHUNK_ITEMS; ++j) chunks_data[0][j] = j * 3;
  /* Chunk 1: initially compressible zeros */
  for (int j = 0; j < MMAP_CHUNK_ITEMS; ++j) chunks_data[1][j] = 0;

  /* Chunks 2..5: pseudo-random data that do not compress much (~300 KB each) */
  uint32_t lcg = 0x12345678;
  for (int i = 2; i < MMAP_NUM_CHUNKS; ++i) {
    for (int j = 0; j < MMAP_CHUNK_ITEMS; ++j) {
      lcg = lcg * 1664525u + 1013904223u;
      chunks_data[i][j] = (int32_t)lcg;
    }
  }

  for (int i = 0; i < MMAP_NUM_CHUNKS; ++i) {
    int64_t nchunks = blosc2_schunk_append_buffer(schunk, chunks_data[i], chunk_bytes);
    mu_assert("ERROR: append_buffer failed", nchunks == i + 1);
  }

  /* 1. Mmap shrink: update chunk 0 with highly compressible constant */
  for (int j = 0; j < MMAP_CHUNK_ITEMS; ++j) chunks_data[0][j] = 42;
  uint8_t *c0_chunk = malloc(chunk_bytes + BLOSC2_MAX_OVERHEAD);
  int csize0 = blosc2_compress_ctx(schunk->cctx, chunks_data[0], chunk_bytes, c0_chunk, chunk_bytes + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: c0 compress failed", csize0 > 0);
  int64_t nch = blosc2_schunk_update_chunk(schunk, 0, c0_chunk, true);
  free(c0_chunk);
  mu_assert("ERROR: mmap shrink update chunk 0 failed", nch == MMAP_NUM_CHUNKS);

  /* 2. Mmap growth: update chunk 1 from ~100 bytes to ~300 KB.
     The tail following chunk 1 consists of 4 chunks of ~300 KB each + trailer,
     totalling ~1.2 MB, which is > 1 MiB (FRAME_TAIL_COPY_BUFFER_CAP).
     This forces backward movement with multiple segments AND forces mmap remapping. */
  for (int j = 0; j < MMAP_CHUNK_ITEMS; ++j) {
    lcg = lcg * 1664525u + 1013904223u;
    chunks_data[1][j] = (int32_t)lcg;
  }
  uint8_t *c1_chunk = malloc(chunk_bytes + BLOSC2_MAX_OVERHEAD);
  int csize1 = blosc2_compress_ctx(schunk->cctx, chunks_data[1], chunk_bytes, c1_chunk, chunk_bytes + BLOSC2_MAX_OVERHEAD);
  mu_assert("ERROR: c1 compress failed", csize1 > 0);
  nch = blosc2_schunk_update_chunk(schunk, 1, c1_chunk, true);
  free(c1_chunk);
  mu_assert("ERROR: mmap growth update chunk 1 failed", nch == MMAP_NUM_CHUNKS);

  /* Verify all chunks in active schunk */
  int32_t *decomp = malloc(chunk_bytes);
  for (int i = 0; i < MMAP_NUM_CHUNKS; ++i) {
    int dsize = blosc2_schunk_decompress_chunk(schunk, i, decomp, chunk_bytes);
    mu_assert("ERROR: active decompress failed", dsize == chunk_bytes);
    mu_assert("ERROR: active chunk data mismatch", memcmp(decomp, chunks_data[i], chunk_bytes) == 0);
  }
  blosc2_schunk_free(schunk);

  /* 3. Reopen from disk with standard I/O and verify every chunk */
  blosc2_schunk *schunk_reopen = blosc2_schunk_open(TEST_FRAME);
  mu_assert("ERROR: reopen from disk failed", schunk_reopen != NULL);
  mu_assert("ERROR: reopen nchunks mismatch", schunk_reopen->nchunks == MMAP_NUM_CHUNKS);
  for (int i = 0; i < MMAP_NUM_CHUNKS; ++i) {
    int dsize = blosc2_schunk_decompress_chunk(schunk_reopen, i, decomp, chunk_bytes);
    mu_assert("ERROR: reopen decompress failed", dsize == chunk_bytes);
    mu_assert("ERROR: reopen chunk data mismatch", memcmp(decomp, chunks_data[i], chunk_bytes) == 0);
  }
  blosc2_schunk_free(schunk_reopen);

  /* 4. Reopen from disk with mmap I/O and verify every chunk */
  blosc2_stdio_mmap mmap_read = BLOSC2_STDIO_MMAP_DEFAULTS;
  mmap_read.mode = "r";
  blosc2_io io_read = {.id = BLOSC2_IO_FILESYSTEM_MMAP, .name = "filesystem_mmap", .params = &mmap_read};
  blosc2_schunk *schunk_mmap = blosc2_schunk_open_udio(TEST_FRAME, &io_read);
  mu_assert("ERROR: reopen with mmap failed", schunk_mmap != NULL);
  mu_assert("ERROR: mmap reopen nchunks mismatch", schunk_mmap->nchunks == MMAP_NUM_CHUNKS);
  for (int i = 0; i < MMAP_NUM_CHUNKS; ++i) {
    int dsize = blosc2_schunk_decompress_chunk(schunk_mmap, i, decomp, chunk_bytes);
    mu_assert("ERROR: mmap reopen decompress failed", dsize == chunk_bytes);
    mu_assert("ERROR: mmap reopen chunk data mismatch", memcmp(decomp, chunks_data[i], chunk_bytes) == 0);
  }
  blosc2_schunk_free(schunk_mmap);

  free(decomp);
  for (int i = 0; i < MMAP_NUM_CHUNKS; ++i) {
    free(chunks_data[i]);
  }
  blosc2_remove_urlpath(TEST_FRAME);
  return EXIT_SUCCESS;
}

static char *all_tests(void) {
  mu_run_test(test_move_range_invalid_and_noop);
  mu_run_test(test_move_range_shrink_direction_and_cap);
  mu_run_test(test_move_range_growth_direction_and_cap);
  mu_run_test(test_move_range_small_overlap);
  mu_run_test(test_move_range_fault_injection);
  mu_run_test(test_contiguous_tail_update_positions);
  mu_run_test(test_contiguous_special_transitions);
  mu_run_test(test_contiguous_reordered_tail_update);
  mu_run_test(test_contiguous_mmap_update);
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
