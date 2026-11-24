/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  blosc2_schunk_copy() must leave the storage it is given alone, so that it can
  be used for another copy, and a recompressing copy keeps the size of a shorter
  last chunk.  Copies of super-chunks and b2nd arrays that fail halfway (the
  destination exists already, a chunk or a vlmetalayer of the source cannot be
  read) must return an error, hand out no array and free whatever they had built
  (run under LeakSanitizer to see the leaks).
*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blosc2.h"
#include "b2nd.h"
#include "cutest.h"

#define CHUNK_NITEMS 1000
#define NCHUNKS 4

static char *src_path = "test_copy_errors_src.b2frame";
static char *dst_path = "test_copy_errors_dst.b2frame";

/* A super-chunk of NCHUNKS chunks, the last one shorter */
static blosc2_schunk *new_schunk(blosc2_storage *storage) {
  blosc2_schunk *schunk = blosc2_schunk_new(storage);
  if (schunk == NULL) {
    return NULL;
  }
  int32_t data[CHUNK_NITEMS];
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (int i = 0; i < CHUNK_NITEMS; i++) {
      data[i] = nchunk * CHUNK_NITEMS + i;
    }
    int32_t nitems = (nchunk == NCHUNKS - 1) ? CHUNK_NITEMS / 2 : CHUNK_NITEMS;
    if (blosc2_schunk_append_buffer(schunk, data, nitems * (int32_t)sizeof(int32_t)) != nchunk + 1) {
      blosc2_schunk_free(schunk);
      return NULL;
    }
  }
  return schunk;
}

/* Make chunk 2 of a sparse (directory) frame unreadable */
static void remove_chunk_file(const char *urlpath) {
  char path[256];
  snprintf(path, sizeof(path), "%s/%08X.chunk", urlpath, 2);
  remove(path);
}

CUTEST_TEST_DATA(copy_errors) {
  int unused;
};

CUTEST_TEST_SETUP(copy_errors) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_init();
}

CUTEST_TEST_TEST(copy_errors) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = 1;
  blosc2_storage mem_storage = {.contiguous = false, .cparams = &cparams};
  blosc2_schunk *schunk = new_schunk(&mem_storage);
  CUTEST_ASSERT("Error creating the source", schunk != NULL);

  // A copy leaves the storage it is given alone (it used to store a pointer to a
  // local variable in it), so the same storage can be used again
  blosc2_remove_urlpath(dst_path);
  blosc2_storage dir_storage = {.contiguous = false, .urlpath = dst_path};
  blosc2_schunk *dir = blosc2_schunk_copy(schunk, &dir_storage);
  CUTEST_ASSERT("Error copying to a directory", dir != NULL);
  CUTEST_ASSERT("The copy must not change the storage it was given", dir_storage.cparams == NULL);
  blosc2_schunk_free(dir);
  blosc2_remove_urlpath(dst_path);
  dir = blosc2_schunk_copy(schunk, &dir_storage);
  CUTEST_ASSERT("Error copying to a directory again", dir != NULL);
  CUTEST_ASSERT("The copy must have the cparams of the source", dir->clevel == schunk->clevel);
  blosc2_schunk_free(dir);

  // The destination exists already, as a directory and as a file
  CUTEST_ASSERT("Copying over an existing directory must fail",
                blosc2_schunk_copy(schunk, &dir_storage) == NULL);
  blosc2_remove_urlpath(dst_path);
  FILE *f = fopen(dst_path, "wb");
  fclose(f);
  blosc2_storage file_storage = {.contiguous = true, .urlpath = dst_path};
  CUTEST_ASSERT("Copying over an existing file must fail", blosc2_schunk_copy(schunk, &file_storage) == NULL);
  blosc2_remove_urlpath(dst_path);

  // A recompressing copy keeps the shorter last chunk as it is
  blosc2_cparams cparams2 = cparams;
  cparams2.clevel = 1;
  blosc2_storage recompress_storage = {.contiguous = false, .cparams = &cparams2};
  blosc2_schunk *recompressed = blosc2_schunk_copy(schunk, &recompress_storage);
  CUTEST_ASSERT("Error recompressing", recompressed != NULL);
  CUTEST_ASSERT("Recompressing must keep nbytes", recompressed->nbytes == schunk->nbytes);
  CUTEST_ASSERT("Recompressing must keep nchunks", recompressed->nchunks == schunk->nchunks);
  int32_t last[CHUNK_NITEMS];
  int dsize = blosc2_schunk_decompress_chunk(recompressed, NCHUNKS - 1, last, sizeof(last));
  CUTEST_ASSERT("The last chunk must keep its size", dsize == CHUNK_NITEMS / 2 * (int)sizeof(int32_t));
  CUTEST_ASSERT("The last chunk must keep its data",
                last[CHUNK_NITEMS / 2 - 1] == (NCHUNKS - 1) * CHUNK_NITEMS + CHUNK_NITEMS / 2 - 1);
  blosc2_schunk_free(recompressed);

  // A vlmetalayer of the source that cannot be decompressed fails the copy after the chunks
  CUTEST_ASSERT("Error adding a vlmetalayer",
                blosc2_vlmeta_add(schunk, "vlmeta", (uint8_t *)"some content", 12, NULL) >= 0);
  uint8_t *content = schunk->vlmetalayers[0]->content;
  memset(content + BLOSC_MIN_HEADER_LENGTH, 0xff, schunk->vlmetalayers[0]->content_len - BLOSC_MIN_HEADER_LENGTH);
  content[0] = 0xff;  // an unknown format version
  blosc2_storage mem_copy_storage = {.contiguous = false};
  CUTEST_ASSERT("A broken vlmetalayer must fail the copy", blosc2_schunk_copy(schunk, &mem_copy_storage) == NULL);
  blosc2_storage frame_copy_storage = {.contiguous = true};
  CUTEST_ASSERT("A broken vlmetalayer must fail the copy", blosc2_schunk_copy(schunk, &frame_copy_storage) == NULL);
  blosc2_schunk_free(schunk);

  // A source chunk that cannot be read, for every kind of destination
  blosc2_remove_urlpath(src_path);
  blosc2_storage src_storage = {.contiguous = false, .urlpath = src_path, .cparams = &cparams};
  schunk = new_schunk(&src_storage);
  CUTEST_ASSERT("Error creating the directory source", schunk != NULL);
  remove_chunk_file(src_path);
  blosc2_storage storages[] = {
      {.contiguous = false},
      {.contiguous = true},
      {.contiguous = false, .urlpath = dst_path},
      {.contiguous = true, .urlpath = dst_path},
      {.contiguous = false, .cparams = &cparams2},
      {.contiguous = true, .urlpath = dst_path, .cparams = &cparams2},
  };
  for (int i = 0; i < (int)(sizeof(storages) / sizeof(storages[0])); i++) {
    blosc2_remove_urlpath(dst_path);
    CUTEST_ASSERT("A missing source chunk must fail the copy", blosc2_schunk_copy(schunk, &storages[i]) == NULL);
  }
  blosc2_remove_urlpath(dst_path);
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(src_path);

  // b2nd: creating or copying an array over an existing file
  int64_t shape[] = {100, 100};
  int32_t chunkshape[] = {50, 50};
  int32_t blockshape[] = {25, 25};
  blosc2_storage b2_storage = {.cparams = &cparams};
  cparams.typesize = 8;
  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, 2, shape, chunkshape, blockshape, NULL, 0, NULL, 0);
  b2nd_array_t *array;
  CUTEST_ASSERT("Error creating an array", b2nd_zeros(ctx, &array) == BLOSC2_ERROR_SUCCESS);
  b2nd_free_ctx(ctx);
  f = fopen(dst_path, "wb");
  fclose(f);
  blosc2_storage b2_file_storage = {.contiguous = true, .urlpath = dst_path, .cparams = &cparams};
  ctx = b2nd_create_ctx(&b2_file_storage, 2, shape, chunkshape, blockshape, NULL, 0, NULL, 0);
  b2nd_array_t *other = NULL;
  CUTEST_ASSERT("Creating an array over an existing file must fail", b2nd_zeros(ctx, &other) < 0);
  CUTEST_ASSERT("A failed creation must not hand out an array", other == NULL);
  CUTEST_ASSERT("Copying an array over an existing file must fail", b2nd_copy(ctx, array, &other) < 0);
  CUTEST_ASSERT("A failed copy must not hand out an array", other == NULL);
  b2nd_free_ctx(ctx);
  b2nd_free(array);
  blosc2_remove_urlpath(dst_path);

  return 0;
}

CUTEST_TEST_TEARDOWN(copy_errors) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_destroy();
}

int main(void) {
  CUTEST_TEST_RUN(copy_errors);
}
