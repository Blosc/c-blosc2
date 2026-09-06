/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Benchmark for bounded-buffer contiguous-frame tail updates.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "blosc2.h"
#include "frame.h"

#define BENCH_FRAME "bench_compact_tail.b2frame"
#define CHUNK_ITEMS (250 * 1000) /* 1 MB of int32 per chunk */
#define NUM_CHUNKS (20)           /* 20 MB total uncompressed */

static void bench_move_range_caps(void) {
  printf("=== Microbenchmark: frame_move_range throughput across buffer caps ===\n");
  const char *tmpfile = "bench_move_raw.bin";

  FILE *f = fopen(tmpfile, "w+b");
  if (!f) return;
  uint8_t *dummy = malloc(1024 * 1024);
  memset(dummy, 0x5A, 1024 * 1024);
  for (int i = 0; i < 50; ++i) {
    fwrite(dummy, 1, 1024 * 1024, f);
  }
  free(dummy);
  fflush(f);

  blosc2_io_cb *io_cb = blosc2_get_io_cb(BLOSC2_IO_FILESYSTEM);
  void *fp = io_cb->open(tmpfile, "rb+", NULL);

  int64_t move_len = 30 * 1024 * 1024; /* 30 MB tail */
  int64_t caps[] = {64 * 1024, 256 * 1024, 1024 * 1024, 4 * 1024 * 1024};
  int ncaps = (int)(sizeof(caps) / sizeof(caps[0]));

  for (int i = 0; i < ncaps; ++i) {
    int64_t cap = caps[i];

    /* Test shrink (forward) */
    blosc_timestamp_t t0, t1;
    blosc_set_timestamp(&t0);
    int rc = frame_move_range(io_cb, fp, 15 * 1024 * 1024, 10 * 1024 * 1024, move_len, cap);
    blosc_set_timestamp(&t1);
    double dt_shrink = blosc_elapsed_secs(t0, t1);
    double speed_shrink = ((double)move_len / (1024.0 * 1024.0)) / dt_shrink;

    /* Test growth (backward) */
    blosc_set_timestamp(&t0);
    rc = frame_move_range(io_cb, fp, 10 * 1024 * 1024, 15 * 1024 * 1024, move_len, cap);
    blosc_set_timestamp(&t1);
    double dt_growth = blosc_elapsed_secs(t0, t1);
    double speed_growth = ((double)move_len / (1024.0 * 1024.0)) / dt_growth;

    printf("  Cap: %6" PRId64 " KB | Shrink (fwd): %6.2f MB/s (%6.4f s) | Growth (bwd): %6.2f MB/s (%6.4f s) [rc=%d]\n",
           cap / 1024, speed_shrink, dt_shrink, speed_growth, dt_growth, rc);
  }

  io_cb->close(fp);
  remove(tmpfile);
}

static void bench_frame_update_positions(void) {
  printf("\n=== Macrobenchmark: contiguous frame_update_chunk early/mid/late ===\n");
  blosc2_remove_urlpath(BENCH_FRAME);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 1; /* Fast compression so tail I/O is dominant */

  blosc2_storage storage = {
      .cparams = &cparams,
      .dparams = &dparams,
      .urlpath = BENCH_FRAME,
      .contiguous = true,
  };

  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  int32_t *data = malloc(CHUNK_ITEMS * sizeof(int32_t));
  int32_t buf_bytes = CHUNK_ITEMS * sizeof(int32_t);

  for (int i = 0; i < NUM_CHUNKS; ++i) {
    for (int j = 0; j < CHUNK_ITEMS; ++j) {
      data[j] = i * 10000 + j;
    }
    blosc2_schunk_append_buffer(schunk, data, buf_bytes);
  }

  int64_t positions[] = {0, NUM_CHUNKS / 2, NUM_CHUNKS - 1};
  const char *pos_names[] = {"Early (chunk 0)", "Middle (chunk 10)", "Late (chunk 19)"};

  for (int p = 0; p < 3; ++p) {
    int64_t pos = positions[p];

    /* Growth update: less compressible data */
    for (int j = 0; j < CHUNK_ITEMS; ++j) {
      data[j] = rand();
    }
    uint8_t *chunk_grow = malloc(buf_bytes + BLOSC2_MAX_OVERHEAD);
    int csize_grow = blosc2_compress_ctx(schunk->cctx, data, buf_bytes, chunk_grow, buf_bytes + BLOSC2_MAX_OVERHEAD);

    blosc_timestamp_t t0, t1;
    blosc_set_timestamp(&t0);
    blosc2_schunk_update_chunk(schunk, pos, chunk_grow, true);
    blosc_set_timestamp(&t1);
    double dt_grow = blosc_elapsed_secs(t0, t1);
    free(chunk_grow);

    /* Shrink update: highly compressible data */
    for (int j = 0; j < CHUNK_ITEMS; ++j) {
      data[j] = 42;
    }
    uint8_t *chunk_shrink = malloc(buf_bytes + BLOSC2_MAX_OVERHEAD);
    int csize_shrink = blosc2_compress_ctx(schunk->cctx, data, buf_bytes, chunk_shrink, buf_bytes + BLOSC2_MAX_OVERHEAD);

    blosc_set_timestamp(&t0);
    blosc2_schunk_update_chunk(schunk, pos, chunk_shrink, true);
    blosc_set_timestamp(&t1);
    double dt_shrink = blosc_elapsed_secs(t0, t1);
    free(chunk_shrink);

    printf("  Position: %-20s | Growth delta (%d -> %d): %6.4f s | Shrink delta (%d -> %d): %6.4f s\n",
           pos_names[p], csize_shrink, csize_grow, dt_grow, csize_grow, csize_shrink, dt_shrink);
  }

  free(data);
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(BENCH_FRAME);
}

int main(void) {
  blosc2_init();
  bench_move_range_caps();
  bench_frame_update_positions();
  blosc2_destroy();
  return 0;
}
