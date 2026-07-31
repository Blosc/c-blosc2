/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  Reads from an on-disk frame with several worker threads: the decompressor
  fetches lazy chunks and then pulls each block straight from the file, so all
  of them hammer the frame's shared (cached) read handle concurrently.

  Then replaces the frame file underneath the open schunk, which the cached
  handle must notice rather than go on serving the inode it pinned.
*/

#include <stdio.h>
#include <stdlib.h>
#include "test_common.h"

#define NCHUNKS 4
#define CHUNKSIZE (500 * 1000)  /* int32 items: big enough for many blocks */
#define NTHREADS 4
#define NSLICES 200
#define SLICE_LEN 1000

#define REPLACED_BASE 1000000

static const char* URLPATH = "test_frame_shared_reader.b2frame";
static const char* URLPATH2 = "test_frame_shared_reader2.b2frame";

int main(void) {
  blosc2_init();
  blosc2_remove_urlpath(URLPATH);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = NTHREADS;
  dparams.nthreads = NTHREADS;
  blosc2_storage storage = {.contiguous=true, .urlpath=(char*)URLPATH,
                            .cparams=&cparams, .dparams=&dparams};

  blosc2_schunk* schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    fprintf(stderr, "Error creating schunk\n");
    return 1;
  }
  int32_t* data = malloc(CHUNKSIZE * sizeof(int32_t));
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = nchunk * CHUNKSIZE + i;
    }
    if (blosc2_schunk_append_buffer(schunk, data, CHUNKSIZE * sizeof(int32_t)) <= 0) {
      fprintf(stderr, "Error appending chunk %d\n", nchunk);
      return 1;
    }
  }
  blosc2_schunk_free(schunk);

  /* Reopen: reads now go through the frame's cached "rb" handle */
  schunk = blosc2_schunk_open(URLPATH);
  if (schunk == NULL) {
    fprintf(stderr, "Error opening schunk\n");
    return 1;
  }
  /* An opened schunk defaults to a single decompression thread */
  blosc2_dparams dparams_open = BLOSC2_DPARAMS_DEFAULTS;
  dparams_open.nthreads = NTHREADS;
  dparams_open.schunk = schunk;
  blosc2_free_ctx(schunk->dctx);
  schunk->dctx = blosc2_create_dctx(dparams_open);
  if (schunk->dctx == NULL) {
    fprintf(stderr, "Error creating a threaded decompression context\n");
    return 1;
  }

  /* Whole chunks: the block reads of one chunk run in parallel */
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    int32_t nbytes = blosc2_schunk_decompress_chunk(schunk, nchunk, data,
                                                    CHUNKSIZE * sizeof(int32_t));
    if (nbytes != (int32_t)(CHUNKSIZE * sizeof(int32_t))) {
      fprintf(stderr, "Error decompressing chunk %d: %d\n", nchunk, nbytes);
      return 1;
    }
    for (int i = 0; i < CHUNKSIZE; i++) {
      if (data[i] != nchunk * CHUNKSIZE + i) {
        fprintf(stderr, "Bad data in chunk %d at %d\n", nchunk, i);
        return 1;
      }
    }
  }

  /* Scattered slices: these go through the lazy-chunk path */
  int64_t nitems = (int64_t)NCHUNKS * CHUNKSIZE;
  int32_t* slice = malloc(SLICE_LEN * sizeof(int32_t));
  for (int s = 0; s < NSLICES; s++) {
    int64_t start = ((int64_t)s * 7919) % (nitems - SLICE_LEN);
    if (blosc2_schunk_get_slice_buffer(schunk, start, start + SLICE_LEN, slice) < 0) {
      fprintf(stderr, "Error getting slice at %lld\n", (long long)start);
      return 1;
    }
    for (int i = 0; i < SLICE_LEN; i++) {
      if (slice[i] != (int32_t)(start + i)) {
        fprintf(stderr, "Bad data in slice at %lld, item %d\n", (long long)start, i);
        return 1;
      }
    }
  }

  /* Replace the frame file underneath the still-open schunk: unlink, then move
     a different frame into its place -- what python-blosc2 does for mode="w"
     and for its os.replace() flows.  The cached read handle pins the old inode,
     so without revalidation the reads below keep serving pre-replacement data */
  blosc2_storage storage2 = {.contiguous=true, .urlpath=(char*)URLPATH2,
                             .cparams=&cparams, .dparams=&dparams};
  blosc2_remove_urlpath(URLPATH2);
  blosc2_schunk* replacement = blosc2_schunk_new(&storage2);
  if (replacement == NULL) {
    fprintf(stderr, "Error creating the replacement schunk\n");
    return 1;
  }
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = REPLACED_BASE + nchunk * CHUNKSIZE + i;
    }
    if (blosc2_schunk_append_buffer(replacement, data, CHUNKSIZE * sizeof(int32_t)) <= 0) {
      fprintf(stderr, "Error appending replacement chunk %d\n", nchunk);
      return 1;
    }
  }
  blosc2_schunk_free(replacement);
  blosc2_remove_urlpath(URLPATH);
  if (rename(URLPATH2, URLPATH) != 0) {
    fprintf(stderr, "Error moving %s over %s\n", URLPATH2, URLPATH);
    return 1;
  }

  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    int32_t nbytes = blosc2_schunk_decompress_chunk(schunk, nchunk, data,
                                                    CHUNKSIZE * sizeof(int32_t));
    if (nbytes != (int32_t)(CHUNKSIZE * sizeof(int32_t))) {
      fprintf(stderr, "Error decompressing replaced chunk %d: %d\n", nchunk, nbytes);
      return 1;
    }
    for (int i = 0; i < CHUNKSIZE; i++) {
      if (data[i] != REPLACED_BASE + nchunk * CHUNKSIZE + i) {
        fprintf(stderr, "Stale data after replacing the frame, chunk %d at item %d\n",
                nchunk, i);
        return 1;
      }
    }
  }

  free(slice);
  free(data);
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(URLPATH);
  blosc2_destroy();

  printf("Successful roundtrip!\n");
  return 0;
}
