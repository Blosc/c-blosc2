/*********************************************************************
  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Benchmark for testing sframe vs frame.
  For usage instructions of this benchmark, please see:
    https://www.blosc.org/pages/synthetic-benchmarks/
  We are collecting speeds for different machines, so the output of your
  benchmarks and your processor specifications are welcome!

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <blosc2.h>
#include <assert.h>
#include <inttypes.h>

#define KB  1024
#define MB  (1024*KB)
#define GB  (1024*MB)

#define NCHUNKS 1000       /* number of chunks */
#define CHUNKSIZE (200 * 1000)

int nchunks = NCHUNKS;
int iterations = 5;

#if defined(_WIN32) && !defined(__MINGW32__)
#include <time.h>
  #include "win32/stdint-windows.h"
#else
#include <stdint.h>
#include <sys/time.h>
#endif

#if defined(_WIN32)
#include <malloc.h>

#endif  /* defined(_WIN32) && !defined(__MINGW32__) */


void test_update(blosc2_schunk* schunk_sframe, blosc2_schunk* schunk_cframe) {
  blosc_timestamp_t last, current;
  double cframe_update_time, sframe_update_time;

  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int32_t* data = malloc(isize);

  // Random update list
  int32_t* update_chunks = malloc(sizeof(int32_t) * iterations);
  srand(time(NULL));
  for (int i = 0; i < iterations; i++) {
    update_chunks[i] = rand() % schunk_sframe->nchunks;
  }

  printf("*******************************************************\n");
  printf("******************* Updating %d chunks ******************\n", iterations);
  printf("*******************************************************\n");

  // Update the sframe chunks
  sframe_update_time = 0.0;
  cframe_update_time = 0.0;
  int32_t datasize = sizeof(int32_t) * CHUNKSIZE;
  int32_t chunksize = sizeof(int32_t) * CHUNKSIZE + BLOSC_MAX_OVERHEAD;
  uint8_t* chunk;
  int csize;
  int _nchunks;
  for (int i = 0; i < iterations; i++) {
    // Generate data
    for (int j = 0; j < CHUNKSIZE; j++) {
      data[j] = i * CHUNKSIZE;
    }
    chunk = malloc(chunksize);
    csize = blosc2_compress_ctx(schunk_sframe->cctx, data, datasize, chunk, chunksize);
    if (csize < 0) {
      printf("ERROR: chunk cannot be compressed\n");
    }
    // Sframe
    blosc_set_timestamp(&current);
    _nchunks = blosc2_schunk_update_chunk(schunk_sframe, update_chunks[i], chunk, true);
    blosc_set_timestamp(&last);
    if (_nchunks < 0){
      printf("ERROR: chunk cannot be updated correctly\n");
    }
    sframe_update_time += blosc_elapsed_secs(current, last);

    // Frame
    blosc_set_timestamp(&current);
    _nchunks = blosc2_schunk_update_chunk(schunk_cframe, update_chunks[i], chunk, true);
    blosc_set_timestamp(&last);
    free(chunk);
    if (_nchunks <= 0){
      printf("ERROR: chunk cannot be updated correctly\n");
    }
    cframe_update_time += blosc_elapsed_secs(current, last);
  }

  printf("[Sframe Update] Elapsed time:\t %6.3f s. Total sframe size: %.3" PRId64 " bytes\n",
         sframe_update_time, schunk_sframe->cbytes);
  printf("[Cframe Update] Elapsed time:\t %6.3f s. Total cframe size: %.3" PRId64 " bytes\n",
         cframe_update_time, schunk_cframe->cbytes);

  /* Free resources */
  free(update_chunks);
  free(data);
}

void test_insert(blosc2_schunk* schunk_sframe, blosc2_schunk* schunk_cframe) {
  blosc_timestamp_t last, current;
  double cframe_insert_time, sframe_insert_time;

  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int32_t* data = malloc(isize);

  // Random insert list
  int32_t* insert_chunks = malloc(sizeof(int32_t) * iterations);
  srand(time(NULL));
  for (int i = 0; i < iterations; i++) {
    insert_chunks[i] = rand() % schunk_sframe->nchunks;
  }

  printf("*******************************************************\n");
  printf("****************** Inserting %d chunks *****************\n", iterations);
  printf("*******************************************************\n");

  // Update the sframe chunks
  sframe_insert_time = 0.0;
  cframe_insert_time = 0.0;
  int32_t datasize = sizeof(int32_t) * CHUNKSIZE;
  int32_t chunksize = sizeof(int32_t) * CHUNKSIZE + BLOSC_MAX_OVERHEAD;
  uint8_t* chunk;
  int csize;
  int _nchunks;
  for (int i = 0; i < iterations; i++) {
    // Generate data
    for (int j = 0; j < CHUNKSIZE; j++) {
      data[j] = j + i * CHUNKSIZE;
    }
    chunk = malloc(chunksize);
    csize = blosc2_compress_ctx(schunk_sframe->cctx, data, datasize, chunk, chunksize);
    if (csize < 0) {
      printf("ERROR: chunk cannot be compressed\n");
    }
    // Sframe
    blosc_set_timestamp(&current);
    _nchunks = blosc2_schunk_insert_chunk(schunk_sframe, insert_chunks[i], chunk, true);
    blosc_set_timestamp(&last);
    if (_nchunks < 0){
      printf("ERROR: chunk cannot be updated correctly\n");
    }
    sframe_insert_time += blosc_elapsed_secs(current, last);

    // Frame
    blosc_set_timestamp(&current);
    _nchunks = blosc2_schunk_update_chunk(schunk_cframe, insert_chunks[i], chunk, true);
    blosc_set_timestamp(&last);
    free(chunk);
    if (_nchunks <= 0){
      printf("ERROR: chunk cannot be updated correctly\n");
    }
    cframe_insert_time += blosc_elapsed_secs(current, last);
  }

  printf("[Sframe Insert] Elapsed time:\t %6.3f s.  Total sframe size: %.3" PRId64 " bytes \n",
         sframe_insert_time, schunk_sframe->cbytes);
  printf("[Cframe Insert] Elapsed time:\t %6.3f s.  Total cframe size: %.3" PRId64 " bytes \n",
         cframe_insert_time, schunk_cframe->cbytes);


  /* Free resources */
  free(insert_chunks);
  free(data);
}

void test_reorder(blosc2_schunk* schunk_sframe, blosc2_schunk* schunk_cframe) {
  blosc_timestamp_t last, current;
  double cframe_reorder_time, sframe_reorder_time;

  // Reorder list
  int *offsets_order = malloc(sizeof(int) * schunk_sframe->nchunks);
  for (int i = 0; i < schunk_sframe->nchunks; ++i) {
    offsets_order[i] = (i + 3) % schunk_sframe->nchunks;
  }

  printf("*******************************************************\n");
  printf("****************** Reordering chunks ******************\n");
  printf("*******************************************************\n");

  // Reorder sframe
  blosc_set_timestamp(&current);
  int err = blosc2_schunk_reorder_offsets(schunk_sframe, offsets_order);
  blosc_set_timestamp(&last);
  if (err < 0) {
    printf("ERROR: cannot reorder chunks\n");
  }
  sframe_reorder_time = blosc_elapsed_secs(current, last);

  // Reorder frame
  blosc_set_timestamp(&current);
  err = blosc2_schunk_reorder_offsets(schunk_cframe, offsets_order);
  blosc_set_timestamp(&last);
  if (err < 0) {
    printf("ERROR: cannot reorder chunks\n");
  }
  cframe_reorder_time = blosc_elapsed_secs(current, last);

  printf("[Sframe Update] Elapsed time:\t %f s.  Total sframe size: %.3" PRId64 " bytes \n",
         sframe_reorder_time, schunk_sframe->cbytes);
  printf("[Cframe Update] Elapsed time:\t %f s.  Total cframe size: %.3" PRId64 " bytes \n",
         cframe_reorder_time, schunk_cframe->cbytes);

  /* Free resources */
  free(offsets_order);
}

void test_create_sframe_frame(char* operation) {
  blosc_timestamp_t last, current;
  double cframe_append_time, sframe_append_time, cframe_decompress_time, sframe_decompress_time;

  int64_t nbytes, cbytes;
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize;
  float totalsize = (float)(isize * nchunks);
  int32_t* data = malloc(isize);
  int32_t* data_dest = malloc(isize);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;

  blosc2_schunk* schunk_sframe;
  blosc2_schunk* schunk_cframe;

  /* Initialize the Blosc compressor */
  blosc_init();
  printf("*******************************************************\n");
  printf("***** Creating the frame and sframe with %d chunks ****\n", nchunks);
  printf("*******************************************************\n");
  /* Create a frame container */
  cparams.typesize = sizeof(int32_t);

  cparams.nthreads = 2;
  dparams.nthreads = 2;

  blosc2_storage storage = {.contiguous=false, .urlpath="dir.b2frame", .cparams=&cparams, .dparams=&dparams};
  schunk_sframe = blosc2_schunk_new(&storage);


  blosc2_storage storage2 = {.contiguous=true, .urlpath="test_cframe.b2frame", .cparams=&cparams, .dparams=&dparams};
  schunk_cframe = blosc2_schunk_new(&storage2);

  printf("Test comparation frame vs sframe with %d chunks.\n", nchunks);

  // Feed it with data
  sframe_append_time=0.0;
  cframe_append_time=0.0;
  blosc_set_timestamp(&current);
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = i + nchunk * CHUNKSIZE;
    }
    blosc_set_timestamp(&current);
    blosc2_schunk_append_buffer(schunk_sframe, data, isize);
    blosc_set_timestamp(&last);
    sframe_append_time += blosc_elapsed_secs(current, last);

    blosc_set_timestamp(&current);
    blosc2_schunk_append_buffer(schunk_cframe, data, isize);
    blosc_set_timestamp(&last);
    cframe_append_time += blosc_elapsed_secs(current, last);
  }
  printf("[Sframe Compr] Elapsed time:\t %6.3f s.  Processed data: %.3f GB (%.3f GB/s)\n",
         sframe_append_time, totalsize / GB, totalsize / (GB * sframe_append_time));
  printf("[Cframe Compr] Elapsed time:\t %6.3f s.  Processed data: %.3f GB (%.3f GB/s)\n",
         cframe_append_time, totalsize / GB, totalsize / (GB * cframe_append_time));

  /* Gather some info */
  nbytes = schunk_sframe->nbytes;
  cbytes = schunk_sframe->cbytes;
  printf("Compression super-chunk-sframe: %ld -> %ld (%.1fx)\n",
         (long)nbytes, (long)cbytes, (1. * nbytes) / cbytes);
  nbytes = schunk_cframe->nbytes;
  cbytes = schunk_cframe->cbytes;
  printf("Compression super-chunk-cframe: %ld -> %ld (%.1fx)\n",
         (long)nbytes, (long)cbytes, (1. * nbytes) / cbytes);

  // Decompress the data
  sframe_decompress_time = 0;
  cframe_decompress_time = 0;
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    // Sframe
    blosc_set_timestamp(&current);
    dsize = blosc2_schunk_decompress_chunk(schunk_sframe, nchunk, (void *) data_dest, isize);
    blosc_set_timestamp(&last);
    if (dsize < 0) {
      printf("Decompression error sframe.  Error code: %d\n", dsize);
    }
    assert (dsize == (int)isize);
    sframe_decompress_time += blosc_elapsed_secs(current, last);
    // Frame
    blosc_set_timestamp(&current);
    dsize = blosc2_schunk_decompress_chunk(schunk_cframe, nchunk, (void *) data_dest, isize);
    blosc_set_timestamp(&last);
    if (dsize < 0) {
      printf("Decompression error cframe.  Error code: %d\n", dsize);
    }
    assert (dsize == (int)isize);
    cframe_decompress_time += blosc_elapsed_secs(current, last);
  }

  printf("[Sframe Decompr] Elapsed time:\t %6.3f s.  Processed data: %.3f GB (%.3f GB/s)\n",
         sframe_decompress_time, totalsize / GB, totalsize / (GB * sframe_decompress_time));
  printf("[Cframe Decompr] Elapsed time:\t %6.3f s.  Processed data: %.3f GB (%.3f GB/s)\n",
         cframe_decompress_time, totalsize / GB, totalsize / (GB * cframe_decompress_time));

  printf("Decompression successful!\n");

  printf("Successful roundtrip!\n");

  /* Free resources */
  free(data_dest);
  free(data);

  if (strcmp(operation, "insert") == 0) {
    test_insert(schunk_sframe, schunk_cframe);
  }
  else if (strcmp(operation, "update") == 0) {
    test_update(schunk_sframe, schunk_cframe);
  }
  else if (strcmp(operation, "reorder") == 0) {
    test_reorder(schunk_sframe, schunk_cframe);
  }


  blosc2_remove_dir(schunk_sframe->storage->urlpath);
  blosc2_schunk_free(schunk_sframe);
  blosc2_schunk_free(schunk_cframe);
  /* Destroy the Blosc environment */
  blosc_destroy();

}

int main(int argc, char* argv[]) {
  char* operation;

  if (argc >= 5) {
    printf("Usage: ./sframe_bench [nchunks] [insert | update | reorder] [num operations]\n");
    exit(1);
  }
  else if (argc >= 2) {
    nchunks = (int)strtol(argv[1], NULL, 10);
  }
  if (argc >= 3) {
    operation = argv[2];
  }
  if (argc == 4) {
    iterations = (int)strtol(argv[3], NULL, 10);
  }

  test_create_sframe_frame(operation);

  return 0;
}
