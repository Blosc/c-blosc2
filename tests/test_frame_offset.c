/*
  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

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
}test_fill_special_backend;

CUTEST_TEST_DATA(fill_special) {
  blosc2_cparams cparams;
  blosc2_dparams dparams;
  int32_t data1[CHUNKSIZE];
  int32_t data2[CHUNKSIZE];
  blosc2_storage storage;
  blosc2_schunk* schunk0w;
  blosc2_schunk* schunk1a;
};

CUTEST_TEST_SETUP(fill_special) {
  blosc_init();
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
  data->schunk0w = blosc2_schunk_new(&storage);
  data->schunk1a = blosc2_schunk_new(&storage);
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int i, nchunk;
  int64_t nchunks;

  // Add some data
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      data->data1[i] = i * nchunk;
      data->data2[i] = 2 * i * nchunk;
    }
    nchunks = blosc2_schunk_append_buffer(data->schunk0w, data->data1, isize);
    assert(nchunks == nchunk + 1);
    blosc2_schunk_append_buffer(data->schunk1a, data->data2, isize);
  }
}


CUTEST_TEST_TEST(fill_special) {
  blosc2_schunk* schunk0w = data->schunk0w;
  blosc2_schunk* schunk1a = data->schunk1a;

  blosc_timestamp_t last, current;
  double ttotal;
  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  // Start different conversions between schunks, frames and fileframes

  // super-chunk -> cframe (contiguous frame, or buffer)
  uint8_t* cframe, *cframe1;
  bool cframe_needs_free, cframe_needs_free1;
  int64_t frame_len = blosc2_schunk_to_buffer(schunk0w, &cframe, &cframe_needs_free);
  if (frame_len < 0) {
    return (int)frame_len;
  }
  int64_t frame_len1 = blosc2_schunk_to_buffer(schunk1a, &cframe1, &cframe_needs_free1);
  if (frame_len1 < 0) {
    return (int)frame_len1;
  }

  // super-chunk -> fileframe (contiguous frame, on-disk)
  remove("frame_simple.b2frame");
  blosc_set_timestamp(&last);
  frame_len = blosc2_schunk_to_file(schunk0w, "frame_simple.b2frame");
  if (frame_len < 0) {
    return (int)frame_len;
  }
  printf("Frame length on disk: %ld bytes\n", (long)frame_len);
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for frame -> fileframe (frame_simple.b2frame): %.3g s, %.1f GB/s\n",
         ttotal, (double)schunk0w->nbytes / (ttotal * GB));

  blosc_set_timestamp(&last);
  int64_t offset = blosc2_schunk_append_file(schunk1a, "frame_simple.b2frame");
  if (offset < 0) {
    return (int)offset;
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for frame1 -> fileframe (frame_simple.b2frame) + offset: %.3g s, %.1f GB/s\n",
         ttotal, (double)schunk1a->nbytes / (ttotal * GB));

  // fileframe (file) -> schunk2 (on-disk contiguous, super-chunk)
  blosc_set_timestamp(&last);
  blosc2_schunk* schunk2r = blosc2_schunk_open("file:///frame_simple.b2frame");
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for fileframe (%s) -> frame2 : %.3g s, %.1f GB/s\n",
         schunk2r->storage->urlpath, ttotal, (double)schunk2r->nbytes / (ttotal * GB));

  // fileframe (file) -> schunk3 (on-disk contiguous, super-chunk)
  blosc_set_timestamp(&last);
  blosc2_schunk* schunk3o = blosc2_schunk_open_offset("file:///frame_simple.b2frame", offset);
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Time for fileframe (%s) + offset %ld -> frame3 : %.3g s, %.1f GB/s\n",
         schunk3o->storage->urlpath, offset, ttotal, (double)schunk3o->nbytes / (ttotal * GB));

  uint8_t* cframe2, *cframe3;
  bool cframe_needs_free2, cframe_needs_free3;
  int64_t frame_len2 = blosc2_schunk_to_buffer(schunk2r, &cframe2, &cframe_needs_free2);
  if (frame_len2 != frame_len) {
    return (int)frame_len2;
  }
  for (int j = 0; j < frame_len; ++j) {
    if (cframe[j] != cframe2[j]) {
      printf("schunk != schunk2 in index %d: %u, %u", j, cframe[j], cframe2[j]);
      return -1;
    }
  }
  int64_t frame_len3 = blosc2_schunk_to_buffer(schunk3o, &cframe3, &cframe_needs_free3);
  if (frame_len3 != frame_len1) {
    return (int)frame_len3;
  }
  for (int j = 0; j < frame_len1; ++j) {
    if (cframe1[j] != cframe3[j]) {
      printf("schunk1 != schunk3 in index %d: %u, %u", j, cframe1[j], cframe3[j]);
      return -1;
    }
  }

  printf("Successful roundtrip schunk <-> frame <-> fileframe\n"
         "                     schunk1 <-> frame1 <-> fileframe + offset");

  /* Free resources */
  blosc2_schunk_free(schunk0w);
  blosc2_schunk_free(schunk1a);
  blosc2_schunk_free(schunk2r);
  blosc2_schunk_free(schunk3o);
  if (cframe_needs_free) {
    free(cframe);
  }
  if (cframe_needs_free1) {
    free(cframe1);
  }
  if (cframe_needs_free2) {
    free(cframe2);
  }
  if (cframe_needs_free3) {
    free(cframe3);
  }

  return 0;
}

CUTEST_TEST_TEARDOWN(fill_special) {
  BLOSC_UNUSED_PARAM(data);
  blosc_destroy();
}


int main() {
  CUTEST_TEST_RUN(fill_special)
}
