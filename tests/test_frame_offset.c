/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdio.h>

#include "blosc2.h"
#include "cutest.h"


#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE (200 * 1000)
#define NCHUNKS 100
#define NTHREADS 4

enum {
  CHECK_ZEROS = 1,
};

typedef struct {
  bool contiguous;
  char *urlpath;
} test_fill_special_backend;

CUTEST_TEST_DATA(fill_special) {
  blosc2_cparams cparams;
  blosc2_dparams dparams;
  int32_t data1[CHUNKSIZE];
  int32_t data2[CHUNKSIZE];
  blosc2_storage storage;
  blosc2_schunk* schunk_write_start;
  blosc2_schunk* schunk_write_append;
};

CUTEST_TEST_SETUP(fill_special) {
  blosc2_init();
  data->cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_cparams* cparams = &data->cparams;
  cparams->typesize = sizeof(int32_t);
  cparams->compcode = BLOSC_LZ4;
  cparams->clevel = 9;
  cparams->nthreads = NTHREADS;
  data->dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_dparams* dparams = &data->dparams;
  dparams->nthreads = NTHREADS;
  data->storage.cparams = cparams;
  data->storage.dparams = dparams;
  blosc2_storage storage = data->storage;
  data->schunk_write_start = blosc2_schunk_new(&storage);
  data->schunk_write_append = blosc2_schunk_new(&storage);
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int i, nchunk;

  // Add some data
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      data->data1[i] = i * nchunk;
      data->data2[i] = 2 * i * nchunk;
    }
    blosc2_schunk_append_buffer(data->schunk_write_start, data->data1, isize);
    blosc2_schunk_append_buffer(data->schunk_write_append, data->data2, isize);
  }
}


CUTEST_TEST_TEST(fill_special) {
  blosc2_schunk* schunk_write_start = data->schunk_write_start;
  blosc2_schunk* schunk_write_append = data->schunk_write_append;

  blosc_timestamp_t last, current;
  double ttotal;
  printf("Blosc version info: %s (%s)\n",
         BLOSC2_VERSION_STRING, BLOSC2_VERSION_DATE);

  // Start different conversions between schunks, frames and fileframes

  // super-chunk -> cframe_write_start (contiguous frame, or buffer)
  uint8_t* cframe_write_start, *cframe_write_append;
  bool cframe_needs_free, cframe_needs_free1;
  int64_t frame_len = blosc2_schunk_to_buffer(schunk_write_start, &cframe_write_start, &cframe_needs_free);
  if (frame_len < 0) {
    return (int)frame_len;
  }
  int64_t frame_len1 = blosc2_schunk_to_buffer(schunk_write_append, &cframe_write_append, &cframe_needs_free1);
  if (frame_len1 < 0) {
    return (int)frame_len1;
  }

  // super-chunk -> fileframe (contiguous frame, on-disk)
  remove("frame_simple.b2frame");
  blosc_set_timestamp(&last);
  frame_len = blosc2_schunk_to_file(schunk_write_start, "frame_simple.b2frame");
  if (frame_len < 0) {
    return (int)frame_len;
  }
  printf("Frame length on disk: %ld bytes\n", (long)frame_len);
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for frame_start -> fileframe (frame_simple.b2frame): %.3g s, %.1f GB/s\n",
         ttotal, (double)schunk_write_start->nbytes / (ttotal * GB));

  // super-chunk -> fileframe (contiguous frame, on-disk) + offset
  blosc_set_timestamp(&last);
  int64_t offset = blosc2_schunk_append_file(schunk_write_append, "frame_simple.b2frame");
  if (offset < 0) {
    return (int)offset;
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for frame_append -> fileframe (frame_simple.b2frame) + offset: %.3g s, %.1f GB/s\n",
         ttotal, (double)schunk_write_append->nbytes / (ttotal * GB));

  // fileframe (file) -> schunk (on-disk contiguous, super-chunk)
  blosc_set_timestamp(&last);
  blosc2_schunk* schunk_read_start = blosc2_schunk_open("file:///frame_simple.b2frame");
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for fileframe (%s) -> frame_start : %.3g s, %.1f GB/s\n",
         schunk_read_start->storage->urlpath, ttotal, (double)schunk_read_start->nbytes / (ttotal * GB));

  // fileframe (file) + offset -> schunk (on-disk contiguous, super-chunk)
  blosc_set_timestamp(&last);
  blosc2_schunk* schunk_read_offset = blosc2_schunk_open_offset("file:///frame_simple.b2frame", offset);
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for fileframe (%s) + offset %ld -> frame_offset : %.3g s, %.1f GB/s\n",
         schunk_read_offset->storage->urlpath, (long)offset, ttotal, (double)schunk_read_offset->nbytes / (ttotal * GB));

  uint8_t* cframe_read_start, *cframe_read_offset;
  bool cframe_needs_free2, cframe_needs_free3;
  int64_t frame_len2 = blosc2_schunk_to_buffer(schunk_read_start, &cframe_read_start, &cframe_needs_free2);
  if (frame_len2 != frame_len) {
    return (int)frame_len2;
  }
  for (int j = 0; j < frame_len; ++j) {
    if (cframe_write_start[j] != cframe_read_start[j]) {
      printf("schunk != schunk2 in index %d: %u, %u", j, cframe_write_start[j], cframe_read_start[j]);
      return -1;
    }
  }
  int64_t frame_len3 = blosc2_schunk_to_buffer(schunk_read_offset, &cframe_read_offset, &cframe_needs_free3);
  if (frame_len3 != frame_len1) {
    return (int)frame_len3;
  }
  for (int j = 0; j < frame_len1; ++j) {
    if (cframe_write_append[j] != cframe_read_offset[j]) {
      printf("schunk1 != schunk3 in index %d: %u, %u", j, cframe_write_append[j], cframe_read_offset[j]);
      return -1;
    }
  }

  printf("Successful roundtrip schunk <-> frame <-> fileframe\n"
         "                     schunk1 <-> frame1 <-> fileframe + offset");

  /* Free resources */
  blosc2_schunk_free(schunk_write_start);
  blosc2_schunk_free(schunk_write_append);
  blosc2_schunk_free(schunk_read_start);
  blosc2_schunk_free(schunk_read_offset);
  if (cframe_needs_free) {
    free(cframe_write_start);
  }
  if (cframe_needs_free1) {
    free(cframe_write_append);
  }
  if (cframe_needs_free2) {
    free(cframe_read_start);
  }
  if (cframe_needs_free3) {
    free(cframe_read_offset);
  }

  return 0;
}

CUTEST_TEST_TEARDOWN(fill_special) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_destroy();
}


int main() {
  CUTEST_TEST_RUN(fill_special);
}
