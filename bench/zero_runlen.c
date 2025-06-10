/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Benchmark showing Blosc zero detection capabilities via run-length.

*/

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <inttypes.h>

#include "blosc2.h"


#define KB  1024
#define MB  (1024*KB)
#define GB  (1024*MB)

#define NCHUNKS (2000)
#define CHUNKSIZE (500 * 1000)  // > NCHUNKS for the bench purposes
#define NTHREADS 8

enum {
  ZERO_DETECTION = 0,
  CHECK_ZEROS = 1,
  CHECK_NANS = 2,
  CHECK_VALUES = 3,
  CHECK_UNINIT = 4,
};
#define REPEATED_VALUE 1


int check_special_values(int svalue) {
  blosc2_schunk *schunk;
  int32_t isize = CHUNKSIZE * sizeof(int32_t);
  int32_t osize = CHUNKSIZE * sizeof(int32_t) + BLOSC2_MAX_OVERHEAD;
  int dsize, csize;
  int64_t nbytes, frame_len;
  int nchunk;
  int64_t nchunks = 0;
  int rc;
  int32_t value = REPEATED_VALUE;
  float fvalue;
  blosc_timestamp_t last, current;
  double totaltime;
  double totalsize = (double)isize * NCHUNKS;
  int32_t *data_buffer = malloc(CHUNKSIZE * sizeof(int32_t));
  int32_t *rec_buffer = malloc(CHUNKSIZE * sizeof(int32_t));

  /* Initialize the Blosc compressor */
  blosc2_init();

  /* Create a super-chunk container */
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_BLOSCLZ;
  cparams.clevel = 9;
  cparams.nthreads = NTHREADS;
  blosc2_storage storage = {.cparams=&cparams, .contiguous=false};
  schunk = blosc2_schunk_new(&storage);

  void* chunk = malloc(BLOSC_EXTENDED_HEADER_LENGTH + isize);

  // Cache the special chunks
  switch (svalue) {
    case ZERO_DETECTION:
      memset(data_buffer, 0, isize);
      csize = blosc2_compress(5, 1, sizeof(int32_t), data_buffer, isize, chunk, osize);
      break;
    case CHECK_ZEROS:
      csize = blosc2_chunk_zeros(cparams, isize, chunk, BLOSC_EXTENDED_HEADER_LENGTH);
      break;
    case CHECK_UNINIT:
      csize = blosc2_chunk_uninit(cparams, isize, chunk, BLOSC_EXTENDED_HEADER_LENGTH);
      break;
    case CHECK_NANS:
      csize = blosc2_chunk_nans(cparams, isize, chunk, BLOSC_EXTENDED_HEADER_LENGTH);
      break;
    case CHECK_VALUES:
      csize = blosc2_chunk_repeatval(cparams, isize, chunk,
                                     BLOSC_EXTENDED_HEADER_LENGTH + sizeof(int32_t), &value);
      break;
    default:
      printf("Unknown case\n");
      exit(1);
  }
  if (csize < 0) {
    printf("Error creating chunk: %d\n", csize);
    exit(1);
  }

  /* Append the chunks */
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    nchunks = blosc2_schunk_append_chunk(schunk, chunk, true);
    if (nchunks < 0) {
      printf("Error appending chunk: %" PRId64 "\n", nchunks);
      exit(1);
    }
  }
  blosc_set_timestamp(&current);
  free(chunk);
  totaltime = blosc_elapsed_secs(last, current);
  printf("\n[Compr] Elapsed time:\t %6.3f s."
                 "  Processed data: %.3f GB (%.3f GB/s)\n",
         totaltime, totalsize / GB, totalsize / (GB * totaltime));

  /* Gather some info */
  nbytes = schunk->nbytes;
  frame_len = blosc2_schunk_frame_len(schunk);
  printf("Compression super-chunk: %ld -> %ld (%.1fx)\n",
         (long)nbytes, (long)frame_len, (1. * (double)nbytes) / (double)frame_len);

  /* Retrieve and decompress the chunks */
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, rec_buffer, isize);
    if (dsize < 0) {
      printf("Decompression error.  Error code: %d\n", dsize);
      exit(dsize);
    }
    assert (dsize == (int)isize);
  }
  blosc_set_timestamp(&current);
  totaltime = blosc_elapsed_secs(last, current);
  totalsize = (double)(isize) * (double)nchunks;
  printf("[Decompr] Elapsed time:\t %6.3f s."
         "  Processed data: %.3f GB (%.3f GB/s)\n",
         totaltime, totalsize / GB, totalsize / (GB * totaltime));

  /* Exercise the getitem */
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    bool needs_free;
    uint8_t* chunk_;
    csize = blosc2_schunk_get_chunk(schunk, nchunk, &chunk_, &needs_free);
    if (csize < 0) {
      printf("blosc2_schunk_get_chunk error.  Error code: %d\n", dsize);
      return csize;
    }
    switch (svalue) {
      case CHECK_VALUES:
        rc = blosc1_getitem(chunk_, nchunk, 1, &value);
        if (rc < 0) {
          printf("Error in getitem of a special value\n");
          return rc;
        }
        if (value != REPEATED_VALUE) {
          printf("Wrong value!");
          exit(1);
        }
        break;
      case CHECK_NANS:
        rc = blosc1_getitem(chunk_, nchunk, 1, &fvalue);
        if (rc < 0) {
          printf("Error in getitem of a special value\n");
          return rc;
        }
        if (!isnan(fvalue)) {
          printf("Wrong value!");
          exit(1);
        }
        break;
      case CHECK_ZEROS:
        rc = blosc1_getitem(chunk_, nchunk, 1, &value);
        if (rc < 0) {
          printf("Error in getitem of zeros value\n");
          return rc;
        }
        if (value != 0) {
          printf("Wrong value!");
          exit(1);
        }
        break;
      default:
        // It can only be non-initialized
        rc = blosc1_getitem(chunk_, nchunk, 1, &value);
        if (rc < 0) {
          printf("Error in getitem of an non-initialized value\n");
          return rc;
        }
    }
    if (needs_free) {
      free(chunk_);
    }
  }
  blosc_set_timestamp(&current);
  totaltime = blosc_elapsed_secs(last, current);
  printf("[getitem] Elapsed time:\t %6.3f s.\n", totaltime);

//  /* Check that all the values have a good roundtrip */
//  blosc_set_timestamp(&last);
//  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
//    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, (void *) rec_buffer, isize);
//    if (dsize < 0) {
//      printf("Decompression error.  Error code: %d\n", dsize);
//      return dsize;
//    }
//    assert (dsize == (int)isize);
//    if (CHECK_VALUE) {
//      int32_t* buffer = (int32_t*)rec_buffer;
//      for (int i = 0; i < CHUNKSIZE; i++) {
//        if (buffer[i] != REPEATED_VALUE) {
//          printf("Value is not correct in chunk %d, position: %d\n", nchunk, i);
//          return -1;
//        }
//      }
//    }
//    else if (CHECK_NAN) {
//      float* buffer = (float*)rec_buffer;
//      for (int i = 0; i < CHUNKSIZE; i++) {
//        if (!isnan(buffer[i])) {
//          printf("Value is not correct in chunk %d, position: %d\n", nchunk, i);
//          return -1;
//        }
//      }
//    }
//    else {
//      int32_t* buffer = (int32_t*)rec_buffer;
//      for (int i = 0; i < CHUNKSIZE; i++) {
//        if (buffer[i] != 0) {
//          printf("Value is not correct in chunk %d, position: %d\n", nchunk, i);
//          return -1;
//        }
//      }
//    }
//  }
//  printf("All data did a good roundtrip!\n");

  /* Free resources */
  free(data_buffer);
  free(rec_buffer);
  /* Destroy the super-chunk */
  blosc2_schunk_free(schunk);
  /* Destroy the Blosc environment */
  blosc2_destroy();

  return 0;
}


int main(void) {
  int rc;
  printf("*** Testing special zeros...");
  rc = check_special_values(CHECK_ZEROS);
  if (rc < 0) {
    return rc;
  }
  printf("*** Testing NaNs...");
  rc = check_special_values(CHECK_NANS);
  if (rc < 0) {
    return rc;
  }
  printf("*** Testing repeated values...");
  rc = check_special_values(CHECK_VALUES);
  if (rc < 0) {
    return rc;
  }
  printf("*** Testing non-initialized values...");
  rc = check_special_values(CHECK_UNINIT);
  if (rc < 0) {
    return rc;
  }
  printf("Testing zero detection...");
  rc = check_special_values(ZERO_DETECTION);
  if (rc < 0) {
    return rc;
  }
}
