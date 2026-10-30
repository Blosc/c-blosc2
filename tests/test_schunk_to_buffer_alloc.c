/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Serializing super-chunks to contiguous frames: blosc2_schunk_to_buffer(),
  blosc2_schunk_to_buffer_alloc() and blosc2_schunk_to_file() for every kind of
  storage (also a memory-mapped file), with special chunks (coded in the frame
  offsets), a shorter last chunk, metalayers and variable-length metalayers, and
  without any chunk (also when the file of an empty frame cannot be read).
*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blosc2.h"
#include "cutest.h"

#define CHUNK_NITEMS (50 * 1000)
#define NCHUNKS 7

typedef struct {
  bool contiguous;
  char *urlpath;
  bool empty;
  bool mmap;
} to_buffer_params;

CUTEST_TEST_DATA(schunk_to_buffer_alloc) {
  blosc2_cparams cparams;
};

typedef struct {
  uint8_t *buf;
  int64_t len;
  int ncalls;
  bool fail;
} alloc_state;

static uint8_t *test_alloc(int64_t len, void *user_data) {
  alloc_state *state = user_data;
  state->ncalls++;
  if (state->fail) {
    return NULL;
  }
  state->len = len;
  state->buf = malloc((size_t)len);
  return state->buf;
}

/* Chunk contents: regular, zeros, NaN, uninitialized, regular, ..., and a shorter last one */
static int32_t fill_chunk(int nchunk, float *data) {
  int32_t nitems = (nchunk == NCHUNKS - 1) ? CHUNK_NITEMS / 3 : CHUNK_NITEMS;
  for (int i = 0; i < nitems; i++) {
    data[i] = (float)(nchunk * CHUNK_NITEMS + i);
  }
  return nitems;
}

static int append_chunks(blosc2_schunk *schunk, blosc2_cparams *cparams) {
  float *data = malloc(CHUNK_NITEMS * sizeof(float));
  int32_t nbytes = CHUNK_NITEMS * (int32_t)sizeof(float);
  uint8_t *chunk = malloc(nbytes + BLOSC_EXTENDED_HEADER_LENGTH);
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    int csize;
    switch (nchunk) {
      case 1:
        csize = blosc2_chunk_zeros(*cparams, nbytes, chunk, BLOSC_EXTENDED_HEADER_LENGTH);
        break;
      case 2:
        csize = blosc2_chunk_nans(*cparams, nbytes, chunk, BLOSC_EXTENDED_HEADER_LENGTH);
        break;
      case 4:
        csize = blosc2_chunk_uninit(*cparams, nbytes, chunk, BLOSC_EXTENDED_HEADER_LENGTH);
        break;
      default: {
        int32_t nitems = fill_chunk(nchunk, data);
        int64_t rc = blosc2_schunk_append_buffer(schunk, data, nitems * (int32_t)sizeof(float));
        CUTEST_ASSERT("Error appending a buffer", rc == nchunk + 1);
        continue;
      }
    }
    CUTEST_ASSERT("Error creating a special chunk", csize == BLOSC_EXTENDED_HEADER_LENGTH);
    int64_t rc = blosc2_schunk_append_chunk(schunk, chunk, true);
    CUTEST_ASSERT("Error appending a special chunk", rc == nchunk + 1);
  }
  free(chunk);
  free(data);
  return 0;
}

/* `copy` has the counters, chunks, metalayers and vlmetalayers of `schunk` */
static int check_same(blosc2_schunk *schunk, blosc2_schunk *copy) {
  CUTEST_ASSERT("nchunks differ", copy->nchunks == schunk->nchunks);
  CUTEST_ASSERT("nbytes differ", copy->nbytes == schunk->nbytes);
  CUTEST_ASSERT("chunksize differs", copy->chunksize == schunk->chunksize);
  CUTEST_ASSERT("metalayers differ", copy->nmetalayers == schunk->nmetalayers);
  CUTEST_ASSERT("vlmetalayers differ", copy->nvlmetalayers == schunk->nvlmetalayers);
  float *a = malloc(CHUNK_NITEMS * sizeof(float));
  float *b = malloc(CHUNK_NITEMS * sizeof(float));
  for (int nchunk = 0; nchunk < schunk->nchunks; nchunk++) {
    int sa = blosc2_schunk_decompress_chunk(schunk, nchunk, a, CHUNK_NITEMS * sizeof(float));
    int sb = blosc2_schunk_decompress_chunk(copy, nchunk, b, CHUNK_NITEMS * sizeof(float));
    CUTEST_ASSERT("Error decompressing", sa > 0 && sa == sb);
    // An uninitialized chunk has no defined contents
    if (nchunk != 4) {
      CUTEST_ASSERT("Chunk contents differ", memcmp(a, b, sa) == 0);
    }
    uint8_t *chunk;
    bool needs_free;
    int cbytes = blosc2_schunk_get_chunk(copy, nchunk, &chunk, &needs_free);
    bool special = nchunk == 1 || nchunk == 2 || nchunk == 4;
    CUTEST_ASSERT("A special chunk should stay special",
                  (cbytes == BLOSC_EXTENDED_HEADER_LENGTH) == special);
    if (needs_free) {
      free(chunk);
    }
  }
  free(a);
  free(b);
  uint8_t *content;
  int32_t content_len;
  CUTEST_ASSERT("Missing metalayer", blosc2_meta_get(copy, "meta1", &content, &content_len) >= 0);
  CUTEST_ASSERT("Wrong metalayer", content_len == 5 && memcmp(content, "abcde", 5) == 0);
  free(content);
  CUTEST_ASSERT("Missing vlmetalayer", blosc2_vlmeta_get(copy, "vlmeta1", &content, &content_len) >= 0);
  CUTEST_ASSERT("Wrong vlmetalayer", content_len == 1000 && content[999] == 7);
  free(content);
  return 0;
}

CUTEST_TEST_SETUP(schunk_to_buffer_alloc) {
  blosc2_init();
  data->cparams = BLOSC2_CPARAMS_DEFAULTS;
  data->cparams.typesize = sizeof(float);
  data->cparams.clevel = 5;
  data->cparams.nthreads = 2;

  CUTEST_PARAMETRIZE(params, to_buffer_params, CUTEST_DATA(
      {false, NULL, false, false},
      {true, NULL, false, false},
      {true, "test_to_buffer_alloc.b2frame", false, false},
      {false, "test_to_buffer_alloc_s.b2frame", false, false},
      {false, NULL, true, false},
      {true, NULL, true, false},
      {true, "test_to_buffer_alloc.b2frame", true, false},
      {true, "test_to_buffer_alloc_m.b2frame", false, true},
  ));
}

CUTEST_TEST_TEST(schunk_to_buffer_alloc) {
  CUTEST_GET_PARAMETER(params, to_buffer_params);
  blosc2_remove_urlpath(params.urlpath);
  blosc2_storage storage = {.contiguous = params.contiguous, .urlpath = params.urlpath,
                            .cparams = &data->cparams};
  blosc2_stdio_mmap mmap_file = BLOSC2_STDIO_MMAP_DEFAULTS;
  mmap_file.mode = "w+";
  blosc2_io mmap_io = {.id = BLOSC2_IO_FILESYSTEM_MMAP, .name = "filesystem_mmap", .params = &mmap_file};
  if (params.mmap) {
    storage.io = &mmap_io;
  }
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  CUTEST_ASSERT("Error creating schunk", schunk != NULL);
  CUTEST_ASSERT("Error adding a metalayer",
                blosc2_meta_add(schunk, "meta1", (uint8_t *)"abcde", 5) >= 0);
  if (!params.empty) {
    CUTEST_ASSERT("Error appending chunks", append_chunks(schunk, &data->cparams) == 0);
  }
  uint8_t vlmeta[1000];
  memset(vlmeta, 7, sizeof(vlmeta));
  CUTEST_ASSERT("Error adding a vlmetalayer",
                blosc2_vlmeta_add(schunk, "vlmeta1", vlmeta, sizeof(vlmeta), NULL) >= 0);

  uint8_t *cframe;
  bool needs_free;
  int64_t len = blosc2_schunk_to_buffer(schunk, &cframe, &needs_free);
  CUTEST_ASSERT("Error serializing", len > 0);

  // The same frame, in memory from the caller
  alloc_state state = {0};
  int64_t len2 = blosc2_schunk_to_buffer_alloc(schunk, test_alloc, &state);
  CUTEST_ASSERT("Error serializing into a buffer", len2 == len);
  CUTEST_ASSERT("alloc must be called once, with the frame length", state.ncalls == 1 && state.len == len);
  CUTEST_ASSERT("Frames differ", memcmp(cframe, state.buf, (size_t)len) == 0);
  free(state.buf);

  alloc_state failing = {.fail = true};
  CUTEST_ASSERT("A failed alloc must fail the call",
                blosc2_schunk_to_buffer_alloc(schunk, test_alloc, &failing) < 0 && failing.ncalls == 1);

  // ...and in a file
  char *fname = "test_to_buffer_alloc_out.b2frame";
  blosc2_remove_urlpath(fname);
  int64_t flen = blosc2_schunk_to_file(schunk, fname);
  CUTEST_ASSERT("Error writing the file", flen == len);
  FILE *f = fopen(fname, "rb");
  uint8_t *fbuf = malloc((size_t)flen);
  CUTEST_ASSERT("Error reading the file", fread(fbuf, 1, (size_t)flen, f) == (size_t)flen);
  fclose(f);
  blosc2_remove_urlpath(fname);
  CUTEST_ASSERT("File and buffer frames differ", memcmp(fbuf, cframe, (size_t)len) == 0);
  free(fbuf);

  for (int copy = 0; copy < 2; copy++) {
    blosc2_schunk *back = blosc2_schunk_from_buffer(cframe, len, copy);
    CUTEST_ASSERT("Error deserializing", back != NULL);
    CUTEST_ASSERT("Round trip differs", check_same(schunk, back) == 0);
    blosc2_schunk_free(back);
  }
  if (needs_free) {
    free(cframe);
  }

  // A copy into a new file frame
  blosc2_storage file_storage = {.contiguous = true, .urlpath = fname};
  blosc2_schunk *copied = blosc2_schunk_copy(schunk, &file_storage);
  CUTEST_ASSERT("Error copying to a file", copied != NULL);
  CUTEST_ASSERT("Copy differs", check_same(schunk, copied) == 0);
  blosc2_schunk_free(copied);
  copied = blosc2_schunk_open(fname);
  CUTEST_ASSERT("Reopened copy differs", check_same(schunk, copied) == 0);
  blosc2_schunk_free(copied);
  blosc2_remove_urlpath(fname);

  if (params.empty && params.urlpath != NULL) {
    // Without chunks there is nothing to read from the frame file, which may
    // not even hold the frame yet (python-blosc2's TreeStore has such frames)
    alloc_state before = {0};
    int64_t len3 = blosc2_schunk_to_buffer_alloc(schunk, test_alloc, &before);
    FILE *emptied = fopen(params.urlpath, "wb");
    CUTEST_ASSERT("Error emptying the file", emptied != NULL);
    fclose(emptied);
    alloc_state after = {0};
    CUTEST_ASSERT("A super-chunk without chunks must serialize without its file",
                  len3 > 0 && blosc2_schunk_to_buffer_alloc(schunk, test_alloc, &after) == len3 &&
                  memcmp(before.buf, after.buf, (size_t)len3) == 0);
    free(before.buf);
    free(after.buf);
  }

  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(params.urlpath);
  return 0;
}

CUTEST_TEST_TEARDOWN(schunk_to_buffer_alloc) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_destroy();
}

int main(void) {
  CUTEST_TEST_RUN(schunk_to_buffer_alloc);
}
