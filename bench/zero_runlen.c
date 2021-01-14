/*
  Copyright (C) 2020  The Blosc Developers
  http://blosc.org
  License: BSD (see LICENSE.txt)

  Benchmark showing Blosc zero detection capabilities via run-length.

*/

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

#include "blosc2.h"
#include "frame.h"


#define KB  1024
#define MB  (1024*KB)
#define GB  (1024*MB)

#define NCHUNKS 2000
#define CHUNKSIZE (500 * 1000)  // > NCHUNKS for the bench purposes
#define NTHREADS 4

#define ACTIVATE_ZERO_DETECTION false
#define CHECK_NAN false
#define CHECK_VALUE true
#define REPEATED_VALUE 1


void fill_buffer(int32_t *buffer) {
  for (int i = 0; i < CHUNKSIZE; i++) {
    if (CHECK_VALUE) {
      buffer[i] = REPEATED_VALUE;
    }
    else if (CHECK_NAN) {
      buffer[i] = nanf("");
    }
    else {
      buffer[i] = 0;
    }
//    if (i == CHUNKSIZE - 1) {
//      buffer[i] = 1;
//    }
  }
}


int main(void) {
  blosc2_schunk *schunk;
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  int64_t nbytes, frame_len;
  int nchunk, nchunks = 0;
  blosc_timestamp_t last, current;
  double totaltime;
  double totalsize = (double)(isize * NCHUNKS);
  int32_t *data_buffer = malloc(CHUNKSIZE * sizeof(int32_t));
  int32_t *rec_buffer = malloc(CHUNKSIZE * sizeof(int32_t));

  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  /* Initialize the Blosc compressor */
  blosc_init();

  /* Create a super-chunk container */
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .sequential=true};
  schunk = blosc2_schunk_new(storage);

  /* Append the chunks */
  blosc_set_timestamp(&last);
  void* chunk = malloc(BLOSC_EXTENDED_HEADER_LENGTH + sizeof(int32_t));
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    if (ACTIVATE_ZERO_DETECTION) {
      fill_buffer(data_buffer);
      nchunks = blosc2_schunk_append_buffer(schunk, data_buffer, isize);
    }
    else {
      int csize;
      if (CHECK_VALUE) {
        int32_t value = REPEATED_VALUE;
        csize = blosc2_chunk_repeatval(isize, sizeof(int32_t), chunk,
                                       BLOSC_EXTENDED_HEADER_LENGTH + sizeof(int32_t), &value);
      }
      else if (CHECK_NAN) {
          csize = blosc2_chunk_nans(isize, sizeof(float), chunk, BLOSC_EXTENDED_HEADER_LENGTH);
      } else {
        csize = blosc2_chunk_zeros(isize, sizeof(int32_t), chunk, BLOSC_EXTENDED_HEADER_LENGTH);
      }
      if (csize < 0) {
        printf("Error creating chunk: %d\n", csize);
      }
      nchunks = blosc2_schunk_append_chunk(schunk, chunk, false);
    }
    if (nchunks < 0) {
      printf("Error appending chunk: %d\n", nchunks);
      return -1;
    }
  }
  blosc_set_timestamp(&current);
  free(chunk);
  totaltime = blosc_elapsed_secs(last, current);
  printf("[Compr] Elapsed time:\t %6.3f s."
                 "  Processed data: %.3f GB (%.3f GB/s)\n",
         totaltime, totalsize / GB, totalsize / (GB * totaltime));

  /* Gather some info */
  nbytes = schunk->nbytes;
  frame_len = schunk->frame->len;
  printf("Compression super-chunk: %ld -> %ld (%.1fx)\n",
         (long)nbytes, (long)frame_len, (1. * nbytes) / frame_len);

  /* Retrieve and decompress the chunks */
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, rec_buffer, isize);
    if (dsize < 0) {
      printf("Decompression error.  Error code: %d\n", dsize);
      return dsize;
    }
    assert (dsize == (int)isize);
  }
  blosc_set_timestamp(&current);
  totaltime = blosc_elapsed_secs(last, current);
  totalsize = (double)(isize * nchunks);
  printf("[Decompr] Elapsed time:\t %6.3f s."
         "  Processed data: %.3f GB (%.3f GB/s)\n",
         totaltime, totalsize / GB, totalsize / (GB * totaltime));

  /* Exercise the getitem */
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    bool needs_free;
    uint8_t* chunk_;
    int csize = blosc2_schunk_get_chunk(schunk, nchunk, &chunk_, &needs_free);
    if (csize < 0) {
      printf("blosc2_schunk_get_chunk error.  Error code: %d\n", dsize);
      return csize;
    }
    int expected_csize = CHECK_VALUE ? BLOSC_EXTENDED_HEADER_LENGTH + sizeof(int32_t) : BLOSC_EXTENDED_HEADER_LENGTH;
    assert (csize == expected_csize);
    if (CHECK_VALUE) {
      int32_t value;
      int rc = blosc_getitem(chunk_, nchunk, 1, &value);
      if (rc < 0) {
        printf("Error in getitem of a special value\n");
        return rc;
      }
      assert (value == REPEATED_VALUE);
    }
    else if (CHECK_NAN) {
      float value;
      int rc = blosc_getitem(chunk_, nchunk, 1, &value);
      if (rc < 0) {
        printf("Error in getitem of a special value\n");
        return rc;
      }
      assert (isnan(value));
    }
    else {
      int32_t value;
      blosc_getitem(chunk_, nchunk, 1, &value);
      assert (value == 0);
    }
    if (needs_free) {
      free(chunk_);
    }
  }
  blosc_set_timestamp(&current);
  totaltime = blosc_elapsed_secs(last, current);
  printf("[getitem] Elapsed time:\t %6.3f s.\n", totaltime);

  /* Check that all the values have a good roundtrip */
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) rec_buffer, isize);
    if (dsize < 0) {
      printf("Decompression error.  Error code: %d\n", dsize);
      return dsize;
    }
    assert (dsize == (int)isize);
    if (CHECK_VALUE) {
      int32_t* buffer = (int32_t*)rec_buffer;
      for (int i = 0; i < CHUNKSIZE; i++) {
        if (buffer[i] != REPEATED_VALUE) {
          printf("Value is not correct in chunk %d, position: %d\n", nchunk, i);
          return -1;
        }
      }
    }
    else if (CHECK_NAN) {
      float* buffer = (float*)rec_buffer;
      for (int i = 0; i < CHUNKSIZE; i++) {
        if (!isnan(buffer[i])) {
          printf("Value is not correct in chunk %d, position: %d\n", nchunk, i);
          return -1;
        }
      }
    }
    else {
      int32_t* buffer = (int32_t*)rec_buffer;
      for (int i = 0; i < CHUNKSIZE; i++) {
        if (buffer[i] != 0) {
          printf("Value is not correct in chunk %d, position: %d\n", nchunk, i);
          return -1;
        }
      }
    }
  }
  printf("All data did a good roundtrip!\n");

  /* Free resources */
  free(data_buffer);
  free(rec_buffer);
  /* Destroy the super-chunk */
  blosc2_schunk_free(schunk);
  /* Destroy the Blosc environment */
  blosc_destroy();

  return 0;
}
