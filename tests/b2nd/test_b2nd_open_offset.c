/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "test_common.h"

#include "blosc2.h"
#include "b2nd.h"
#include "cutest.h"

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE (200 * 1000)
#define NCHUNKS 100
#define NTHREADS 4


CUTEST_TEST_SETUP(open_offset) {
  blosc2_init();
}


CUTEST_TEST_TEST(open_offset) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  int32_t *data1 = malloc(CHUNKSIZE * sizeof(int32_t));
  int32_t *data2 = malloc(CHUNKSIZE * sizeof(int32_t));

  blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
  blosc2_schunk* schunk_write_start;
  blosc2_schunk* schunk_write_append;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_LZ4;
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  storage.cparams = &cparams;
  storage.dparams = &dparams;
  int64_t shape[1] = {NCHUNKS * CHUNKSIZE};
  int32_t chunkshape[1] = {CHUNKSIZE};
  int32_t blockshape[1] = {CHUNKSIZE};
  b2nd_context_t *b2nd_params = b2nd_create_ctx(&storage, 1, shape, chunkshape, blockshape,
                                                NULL, 0, NULL, 0);
  b2nd_array_t* arr_write_start;
  BLOSC_ERROR(b2nd_empty(b2nd_params, &arr_write_start));
  b2nd_array_t* arr_write_offset;
  BLOSC_ERROR(b2nd_empty(b2nd_params, &arr_write_offset));
  schunk_write_start = arr_write_start->sc;
  schunk_write_append = arr_write_offset->sc;
  schunk_write_start->nchunks = 0;
  schunk_write_append->nchunks = 0;
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int i, nchunk;
  int64_t nchunks;

  // Add some data
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      data1[i] = i * nchunk;
      data2[i] = 2 * i * nchunk;
    }
    nchunks = blosc2_schunk_append_buffer(schunk_write_start, data1, isize);
    if (nchunks != nchunk + 1) {
      printf("Unexpected nchunks: %ld, %d\n", (long)nchunks, nchunk + 1);
      return -1;
    }
    blosc2_schunk_append_buffer(schunk_write_append, data2, isize);
  }

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
  b2nd_array_t* arr_read_start;
  BLOSC_ERROR(b2nd_open("file:///frame_simple.b2frame", &arr_read_start));
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for fileframe (%s) -> frame_start : %.3g s, %.1f GB/s\n",
         arr_read_start->sc->storage->urlpath, ttotal, (double)arr_read_start->sc->nbytes / (ttotal * GB));

  // fileframe (file) + offset -> schunk (on-disk contiguous, super-chunk)
  blosc_set_timestamp(&last);
  b2nd_array_t* arr_read_offset;
  BLOSC_ERROR(b2nd_open_offset("file:///frame_simple.b2frame", &arr_read_offset, offset));
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for fileframe (%s) + offset %ld -> open_offset : %.3g s, %.1f GB/s\n",
         arr_read_offset->sc->storage->urlpath, (long)offset, ttotal, (double)arr_read_offset->sc->nbytes / (ttotal * GB));

  uint8_t* cframe_read_start, *cframe_read_offset;
  bool cframe_needs_free2, cframe_needs_free3;
  int64_t frame_len2 = b2nd_to_cframe(arr_read_start, &cframe_read_start, &frame_len,
                                      &cframe_needs_free2);
  if (frame_len2 != frame_len) {
    return (int)frame_len2;
  }
  for (int j = 0; j < frame_len; ++j) {
    if (cframe_write_start[j] != cframe_read_start[j]) {
      printf("schunk != schunk2 in index %d: %u, %u", j, cframe_write_start[j], cframe_read_start[j]);
      return -1;
    }
  }
  int64_t frame_len3 = b2nd_to_cframe(arr_read_offset, &cframe_read_offset, &frame_len1,
                                      &cframe_needs_free3);
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
  b2nd_free(arr_read_start);
  b2nd_free(arr_read_offset);
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
  free(data1);
  free(data2);

  return 0;
}

CUTEST_TEST_TEARDOWN(open_offset) {
  blosc2_destroy();
}


int main() {
  CUTEST_TEST_RUN(open_offset);
}
