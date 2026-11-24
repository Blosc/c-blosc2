/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.

  A chunk read from a frame and written back into the same file with
  copy=true.  Such a chunk can point into storage that the write moves or
  overwrites, so it must be copied first: growing an in-memory frame or a
  memory-mapped file moves its storage, and an update shifts the chunks behind
  the updated one, also under the mapping of another handle of the file.  Run
  under AddressSanitizer for the use-after-free.
*/

#include <stdio.h>
#include "test_common.h"

#define CHUNKSIZE (10 * 1000)  /* int32 items */
#define NCHUNKS 3
#define URLPATH "test_frame_self_chunk.b2frame"

int tests_run = 0;
static int32_t data[CHUNKSIZE];
static int32_t result[CHUNKSIZE];
static blosc2_stdio_mmap mmap_file;


/* Chunk n holds values that compress to different sizes for each n */
static void fill_chunk(int n) {
  uint32_t x = 12345u + (uint32_t) n;
  for (int i = 0; i < CHUNKSIZE; i++) {
    x = x * 1103515245u + 12345u;
    data[i] = (int32_t) (i * (n + 1)) + (int32_t) ((x >> 16) % (uint32_t) (1 + 1000 * n));
  }
}


static blosc2_schunk* new_schunk(blosc2_storage* storage) {
  blosc2_remove_urlpath(URLPATH);
  if (storage->io != NULL && storage->io->id == BLOSC2_IO_FILESYSTEM_MMAP) {
    // A mapping is only good for one file
    mmap_file = BLOSC2_STDIO_MMAP_DEFAULTS;
    mmap_file.mode = "w+";
    // Small, so that growing the file remaps it
    mmap_file.initial_mapping_size = 4096;
  }
  blosc2_schunk* schunk = blosc2_schunk_new(storage);
  if (schunk == NULL) {
    return NULL;
  }
  for (int n = 0; n < NCHUNKS; n++) {
    fill_chunk(n);
    if (blosc2_schunk_append_buffer(schunk, data, sizeof(data)) != n + 1) {
      blosc2_schunk_free(schunk);
      return NULL;
    }
  }
  return schunk;
}


/* Whether chunk `nchunk` of `schunk` holds the data of chunk `n` of new_schunk() */
static bool chunk_holds(blosc2_schunk* schunk, int64_t nchunk, int n) {
  fill_chunk(n);
  int nbytes = blosc2_schunk_decompress_chunk(schunk, nchunk, result, sizeof(result));
  return nbytes == (int) sizeof(result) && memcmp(data, result, sizeof(data)) == 0;
}


/* Read chunk `src` as the library hands it out and write it back as chunk `dst` */
static int64_t write_back(blosc2_schunk* schunk, char op, int64_t src, int64_t dst) {
  uint8_t* chunk;
  bool needs_free;
  int cbytes = blosc2_schunk_get_lazychunk(schunk, src, &chunk, &needs_free);
  if (cbytes < 0) {
    return cbytes;
  }
  if (needs_free) {
    // A lazy chunk cannot be written; read the whole chunk then
    free(chunk);
    cbytes = blosc2_schunk_get_chunk(schunk, src, &chunk, &needs_free);
    if (cbytes < 0) {
      return cbytes;
    }
  }
  int64_t rc;
  switch (op) {
    case 'a':
      rc = blosc2_schunk_append_chunk(schunk, chunk, true);
      break;
    case 'i':
      rc = blosc2_schunk_insert_chunk(schunk, dst, chunk, true);
      break;
    default:
      rc = blosc2_schunk_update_chunk(schunk, dst, chunk, true);
  }
  if (needs_free) {
    free(chunk);
  }
  return rc;
}


static char* check_storage(blosc2_storage* storage, char* name) {
  blosc2_schunk* schunk = new_schunk(storage);
  mu_assert("Cannot create the super-chunk", schunk != NULL);

  // Update a chunk with one behind it, which the update shifts
  mu_assert("Cannot update", write_back(schunk, 'u', 2, 0) == NCHUNKS);
  mu_assert(name, chunk_holds(schunk, 0, 2) && chunk_holds(schunk, 1, 1) && chunk_holds(schunk, 2, 2));
  // And with a larger one in front of it, which makes the frame grow
  mu_assert("Cannot update", write_back(schunk, 'u', 0, 1) == NCHUNKS);
  mu_assert(name, chunk_holds(schunk, 0, 2) && chunk_holds(schunk, 1, 2) && chunk_holds(schunk, 2, 2));
  blosc2_schunk_free(schunk);

  schunk = new_schunk(storage);
  mu_assert("Cannot create the super-chunk", schunk != NULL);
  mu_assert("Cannot append", write_back(schunk, 'a', 1, 0) == NCHUNKS + 1);
  mu_assert(name, chunk_holds(schunk, 3, 1) && chunk_holds(schunk, 1, 1));
  mu_assert("Cannot insert", write_back(schunk, 'i', 2, 0) == NCHUNKS + 2);
  mu_assert(name, chunk_holds(schunk, 0, 2) && chunk_holds(schunk, 1, 0) && chunk_holds(schunk, 3, 2) &&
                  chunk_holds(schunk, 4, 1));
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(URLPATH);
  return EXIT_SUCCESS;
}


static char* test_in_memory_frame(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = 1;
  blosc2_storage storage = {.contiguous=true, .cparams=&cparams};
  return check_storage(&storage, "Wrong data after writing back a chunk of an in-memory frame");
}


static char* test_mmap_frame(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = 1;
  blosc2_io io = {.id = BLOSC2_IO_FILESYSTEM_MMAP, .name = "filesystem_mmap", .params = &mmap_file};
  blosc2_storage storage = {.contiguous=true, .urlpath=URLPATH, .cparams=&cparams, .io=&io};
  return check_storage(&storage, "Wrong data after writing back a chunk of a memory-mapped frame");
}


static char* test_file_frame(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = 1;
  blosc2_storage storage = {.contiguous=true, .urlpath=URLPATH, .cparams=&cparams};
  return check_storage(&storage, "Wrong data after writing back a chunk of a file frame");
}


/* A chunk of another handle's mapping of the file, written through a handle
   that does not map it, by an update that grows the chunk in front of it */
static char* test_other_handle_mapping(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = 1;
  blosc2_storage storage = {.contiguous=true, .urlpath=URLPATH, .cparams=&cparams};
  blosc2_schunk* schunk = new_schunk(&storage);
  mu_assert("Cannot create the super-chunk", schunk != NULL);
  blosc2_schunk_free(schunk);

  mmap_file = BLOSC2_STDIO_MMAP_DEFAULTS;
  mmap_file.mode = "r";
  blosc2_io io = {.id = BLOSC2_IO_FILESYSTEM_MMAP, .name = "filesystem_mmap", .params = &mmap_file};
  blosc2_schunk* reader = blosc2_schunk_open_udio(URLPATH, &io);
  blosc2_schunk* writer = blosc2_schunk_open(URLPATH);
  mu_assert("Cannot open the frame twice", reader != NULL && writer != NULL);
  uint8_t* chunk;
  bool needs_free;
  int cbytes = blosc2_schunk_get_chunk(reader, NCHUNKS - 1, &chunk, &needs_free);
  mu_assert("Cannot get the chunk of the mapping", cbytes > 0 && !needs_free);
  int64_t rc = blosc2_schunk_update_chunk(writer, 0, chunk, true);
  blosc2_schunk_free(writer);
  blosc2_schunk_free(reader);
  mu_assert("Cannot update", rc == NCHUNKS);

  schunk = blosc2_schunk_open(URLPATH);
  mu_assert("The update broke the frame", schunk != NULL);
  bool ok = chunk_holds(schunk, 0, NCHUNKS - 1) && chunk_holds(schunk, 1, 1) &&
            chunk_holds(schunk, NCHUNKS - 1, NCHUNKS - 1);
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(URLPATH);
  mu_assert("Wrong data after writing a chunk of another handle's mapping", ok);
  return EXIT_SUCCESS;
}


static char* all_tests(void) {
  mu_run_test(test_in_memory_frame);
  mu_run_test(test_file_frame);
  mu_run_test(test_mmap_frame);
  mu_run_test(test_other_handle_mapping);
  return EXIT_SUCCESS;
}


int main(void) {
  blosc2_init();
  char* result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);
  blosc2_destroy();
  return result != EXIT_SUCCESS;
}
