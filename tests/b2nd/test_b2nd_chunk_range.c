/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/* blosc2_schunk_get_chunk_range() gives where each chunk of a frame on disk is in its file: exactly the chunk's bytes
   for chunks written in order, size 0 for chunks stored as special values, and an error for chunks that do not exist
   and for super-chunks without a frame. */

#include "test_common.h"

#define URLPATH "test_b2nd_chunk_range.b2nd"
#define NDIM 3


/* An int32 array on disk whose chunks in the first 30 rows of axis 1 hold data and the others stay zeros (special
   values in the chunk index) */
static int create_array(int cthreads) {
  const int64_t shape[NDIM] = {40, 50, 61};
  const int32_t chunkshape[NDIM] = {10, 20, 16};
  const int32_t blockshape[NDIM] = {5, 8, 8};
  blosc2_remove_urlpath(URLPATH);
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = (int16_t) cthreads;
  blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
  storage.cparams = &cparams;
  storage.contiguous = true;
  storage.urlpath = URLPATH;
  b2nd_context_t *ctx = b2nd_create_ctx(&storage, NDIM, shape, chunkshape, blockshape, NULL, 0, NULL, 0);
  b2nd_array_t *array;
  if (ctx == NULL || b2nd_zeros(ctx, &array) < 0) {
    return -1;
  }
  int64_t box_start[NDIM] = {0, 0, 0};
  int64_t box_stop[NDIM] = {40, 30, 61};
  int64_t n = 40 * 30 * 61;
  int32_t *data = malloc(n * sizeof(int32_t));
  for (int64_t i = 0; i < n; ++i) {
    data[i] = (int32_t) ((i * 2654435761u) >> 7);  // varied, so that chunks compress to different sizes
  }
  int rc = b2nd_set_slice_cbuffer(data, box_stop, n * (int64_t) sizeof(int32_t), box_start, box_stop, array);
  free(data);
  b2nd_free(array);
  b2nd_free_ctx(ctx);
  return rc;
}


/* A completed mutation through another handle must invalidate the cached index,
   even when reordering leaves the frame length unchanged. */
static int check_stale_ranges(int cthreads) {
  blosc2_stdio_params params = {.locking = true};
  blosc2_io io = {.id = BLOSC2_IO_FILESYSTEM, .name = "filesystem", .params = &params};
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.nthreads = (int16_t) cthreads;
  blosc2_storage storage = {.contiguous = true, .urlpath = URLPATH, .cparams = &cparams, .io = &io};
  blosc2_remove_urlpath(URLPATH);
  blosc2_schunk *writer = blosc2_schunk_new(&storage);
  CUTEST_ASSERT("Cannot create range refresh frame", writer != NULL);
  int32_t data[1024];
  for (int i = 0; i < 1024; ++i) {
    data[i] = i * 17 + 1;
  }
  CUTEST_ASSERT("Cannot append first chunk", blosc2_schunk_append_buffer(writer, data, sizeof(data)) == 1);
  data[0] = -12345;
  CUTEST_ASSERT("Cannot append second chunk", blosc2_schunk_append_buffer(writer, data, sizeof(data)) == 2);
  blosc2_schunk *reader = blosc2_schunk_open_udio(URLPATH, &io);
  CUTEST_ASSERT("Cannot open range reader", reader != NULL);
  int64_t old_offset, expected_offset, offset, nbytes;
  CUTEST_ASSERT("Cannot cache first range",
                blosc2_schunk_get_chunk_range(reader, 0, &old_offset, &nbytes) == 0);
  CUTEST_ASSERT("Cannot get second range",
                blosc2_schunk_get_chunk_range(reader, 1, &expected_offset, &nbytes) == 0);
  CUTEST_ASSERT("Chunks must occupy different offsets", old_offset != expected_offset);
  int64_t order[2] = {1, 0};
  CUTEST_ASSERT("Cannot reorder chunks", blosc2_schunk_reorder_offsets(writer, order) == 0);
  CUTEST_ASSERT("Cannot query reordered range",
                blosc2_schunk_get_chunk_range(reader, 0, &offset, &nbytes) == 0);
  if (offset != expected_offset) {
    fprintf(stderr, "stale range: old=%lld expected=%lld actual=%lld\n",
            (long long) old_offset, (long long) expected_offset, (long long) offset);
  }
  CUTEST_ASSERT("Range retained the old chunk offset after reorder", offset == expected_offset);

  // Growing the index must also refresh its cached size and the frame length.
  CUTEST_ASSERT("Cannot append third chunk", blosc2_schunk_append_buffer(writer, data, sizeof(data)) == 3);
  CUTEST_ASSERT("Cannot get appended range from writer",
                blosc2_schunk_get_chunk_range(writer, 2, &expected_offset, &nbytes) == 0);
  CUTEST_ASSERT("Cannot get appended range from reader",
                blosc2_schunk_get_chunk_range(reader, 2, &offset, &nbytes) == 0);
  CUTEST_ASSERT("Appended range differs between handles", offset == expected_offset && nbytes > 0);
  CUTEST_ASSERT("Reader counters were not refreshed", reader->nchunks == 3 && reader->nbytes == writer->nbytes &&
                reader->cbytes == writer->cbytes);
  blosc2_schunk_free(reader);
  blosc2_schunk_free(writer);
  blosc2_remove_urlpath(URLPATH);
  return 0;
}


CUTEST_TEST_SETUP(chunk_range) {
  blosc2_init();
  CUTEST_PARAMETRIZE(cthreads, int, CUTEST_DATA(1, 4));
}


CUTEST_TEST_TEST(chunk_range) {
  CUTEST_GET_PARAMETER(cthreads, int);
  CUTEST_ASSERT("Stale range regression", check_stale_ranges(cthreads) == 0);
  CUTEST_ASSERT("Cannot create the array", create_array(cthreads) == 0);
  b2nd_array_t *array;
  CUTEST_ASSERT("Cannot open the array", b2nd_open(URLPATH, &array) == 0);
  FILE *f = fopen(URLPATH, "rb");
  CUTEST_ASSERT("Cannot open the file", f != NULL);

  int nspecial = 0;
  for (int64_t nchunk = 0; nchunk < array->sc->nchunks; ++nchunk) {
    int64_t offset;
    int64_t nbytes;
    CUTEST_ASSERT("No chunk range", blosc2_schunk_get_chunk_range(array->sc, nchunk, &offset, &nbytes) == 0);
    uint8_t *chunk;
    bool needs_free;
    int cbytes = blosc2_schunk_get_chunk(array->sc, nchunk, &chunk, &needs_free);
    CUTEST_ASSERT("Cannot get the chunk", cbytes > 0);
    if (nbytes == 0) {
      nspecial++;
    }
    else {
      // Written in order: the range is exactly the chunk
      CUTEST_ASSERT("Range size differs from the chunk's", nbytes == cbytes);
      uint8_t *file_bytes = malloc(nbytes);
      CUTEST_ASSERT("Cannot read the range",
                    fseek(f, (long) offset, SEEK_SET) == 0 && fread(file_bytes, 1, nbytes, f) == (size_t) nbytes);
      CUTEST_ASSERT("Range bytes differ from the chunk", memcmp(file_bytes, chunk, nbytes) == 0);
      free(file_bytes);
    }
    if (needs_free) {
      free(chunk);
    }
  }
  CUTEST_ASSERT("Expected chunks stored as special values", nspecial > 0);
  int64_t offset;
  int64_t nbytes;
  CUTEST_ASSERT("Range for a chunk that does not exist",
                blosc2_schunk_get_chunk_range(array->sc, array->sc->nchunks, &offset, &nbytes) < 0);
  fclose(f);
  b2nd_free(array);

  blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;  // in memory, not contiguous: no frame
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  CUTEST_ASSERT("Range in a super-chunk without a frame",
                schunk != NULL && blosc2_schunk_get_chunk_range(schunk, 0, &offset, &nbytes) < 0);
  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(URLPATH);
  return 0;
}


CUTEST_TEST_TEARDOWN(chunk_range) {
  blosc2_destroy();
}


int main(void) {
  CUTEST_TEST_RUN(chunk_range);
}
