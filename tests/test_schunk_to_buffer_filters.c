/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Serializing a super-chunk that has a prefilter or a postfilter gives a frame
  with the chunks as they are stored, for every kind of storage, whereas
  blosc2_schunk_copy() applies the postfilter (the copy holds the data as read).
  Setting the postfilter again on the deserialized super-chunk reads the same
  data as the original.
*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blosc2.h"
#include "cutest.h"

#define CHUNK_NITEMS (20 * 1000)
#define NCHUNKS 5
#define ZERO_CHUNK 1  // a special chunk of zeros
#define POSTFILTER_ADD 1000000
#define VLMETA_NAME "vlmeta"
#define VLMETA_CONTENT "vlmetalayer content"

typedef struct {
  bool contiguous;
  char *urlpath;
  bool postfilter;  // else a prefilter
} filters_params;

CUTEST_TEST_DATA(schunk_to_buffer_filters) {
  int32_t *data;
};

static int add_postfilter(blosc2_postfilter_params *params) {
  const int32_t *in = (const int32_t *)params->input;
  int32_t *out = (int32_t *)params->output;
  for (int32_t i = 0; i < params->size / params->typesize; i++) {
    out[i] = in[i] + POSTFILTER_ADD;
  }
  return 0;
}

static int double_prefilter(blosc2_prefilter_params *params) {
  const int32_t *in = (const int32_t *)params->input;
  int32_t *out = (int32_t *)params->output;
  for (int32_t i = 0; i < params->output_size / params->output_typesize; i++) {
    out[i] = in[i] * 2;
  }
  return 0;
}

/* The last chunk is shorter */
static int32_t chunk_nitems(int nchunk) {
  return nchunk == NCHUNKS - 1 ? CHUNK_NITEMS / 3 : CHUNK_NITEMS;
}

/* The values stored in `nchunk` (that is, after the prefilter, if any) */
static int32_t stored_value(int nchunk, int32_t i, bool prefilter) {
  if (nchunk == ZERO_CHUNK) {
    return 0;
  }
  int32_t value = nchunk * CHUNK_NITEMS + i;
  return prefilter ? 2 * value : value;
}

static int special_value(const uint8_t *chunk) {
  return (chunk[BLOSC2_CHUNK_BLOSC2_FLAGS] >> 4) & BLOSC2_SPECIAL_MASK;
}

/* Whether `other` is `chunk`.  A frame keeps a special chunk as a special
   value in its offsets and makes up its header (with the blocksize of the
   frame) when it is read, so special chunks only need the same value and size. */
static bool same_chunk(const uint8_t *chunk, int cbytes, const uint8_t *other, int other_cbytes) {
  if (other_cbytes != cbytes) {
    return false;
  }
  if (special_value(chunk) != 0) {
    int32_t nbytes, other_nbytes;
    blosc2_cbuffer_sizes(chunk, &nbytes, NULL, NULL);
    blosc2_cbuffer_sizes(other, &other_nbytes, NULL, NULL);
    return special_value(other) == special_value(chunk) && other_nbytes == nbytes;
  }
  return memcmp(chunk, other, (size_t)cbytes) == 0;
}

/* `other` has the chunks of `schunk` as they are stored */
static int check_same_chunks(blosc2_schunk *schunk, blosc2_schunk *other) {
  CUTEST_ASSERT("nchunks differ", other->nchunks == schunk->nchunks);
  for (int nchunk = 0; nchunk < schunk->nchunks; nchunk++) {
    uint8_t *chunk, *other_chunk;
    bool needs_free, other_needs_free;
    int cbytes = blosc2_schunk_get_chunk(schunk, nchunk, &chunk, &needs_free);
    int other_cbytes = blosc2_schunk_get_chunk(other, nchunk, &other_chunk, &other_needs_free);
    CUTEST_ASSERT("Error getting a chunk", cbytes > 0 && other_cbytes > 0);
    bool same = same_chunk(chunk, cbytes, other_chunk, other_cbytes);
    if (needs_free) {
      free(chunk);
    }
    if (other_needs_free) {
      free(other_chunk);
    }
    CUTEST_ASSERT("A chunk differs from the stored one", same);
  }
  return 0;
}

/* `schunk` reads `stored_value() + add` */
static int check_values(blosc2_schunk *schunk, bool prefilter, int32_t add, int32_t *buffer) {
  CUTEST_ASSERT("nchunks differ", schunk->nchunks == NCHUNKS);
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    int nbytes = blosc2_schunk_decompress_chunk(schunk, nchunk, buffer, CHUNK_NITEMS * sizeof(int32_t));
    CUTEST_ASSERT("Error decompressing", nbytes == chunk_nitems(nchunk) * (int)sizeof(int32_t));
    for (int32_t i = 0; i < chunk_nitems(nchunk); i++) {
      CUTEST_ASSERT("Wrong value", buffer[i] == stored_value(nchunk, i, prefilter) + add);
    }
  }
  return 0;
}

/* `schunk` has the vlmetalayer of the original, unchanged */
static int check_vlmeta(blosc2_schunk *schunk) {
  uint8_t *content;
  int32_t content_len;
  CUTEST_ASSERT("Error getting the vlmetalayer",
                blosc2_vlmeta_get(schunk, VLMETA_NAME, &content, &content_len) >= 0);
  bool same = content_len == (int32_t)sizeof(VLMETA_CONTENT) &&
              memcmp(content, VLMETA_CONTENT, sizeof(VLMETA_CONTENT)) == 0;
  free(content);
  CUTEST_ASSERT("The vlmetalayer differs", same);
  return 0;
}

/* Replace the decompression context of `schunk` with one that has `dparams` */
static void set_dparams(blosc2_schunk *schunk, blosc2_dparams dparams) {
  dparams.schunk = schunk;
  blosc2_free_ctx(schunk->dctx);
  schunk->dctx = blosc2_create_dctx(dparams);
}

static uint8_t *record_alloc(int64_t len, void *user_data) {
  uint8_t **buf = user_data;
  *buf = malloc((size_t)len);
  return *buf;
}

/* The contents of the file at `urlpath` are `len` bytes equal to `cframe` */
static int check_file(const char *urlpath, const uint8_t *cframe, int64_t len) {
  FILE *f = fopen(urlpath, "rb");
  CUTEST_ASSERT("Error opening the file", f != NULL);
  uint8_t *buf = malloc((size_t)len + 1);
  size_t nread = fread(buf, 1, (size_t)len + 1, f);
  fclose(f);
  bool same = nread == (size_t)len && memcmp(buf, cframe, (size_t)len) == 0;
  free(buf);
  CUTEST_ASSERT("The file differs from the buffer frame", same);
  return 0;
}

CUTEST_TEST_SETUP(schunk_to_buffer_filters) {
  blosc2_init();
  data->data = malloc(CHUNK_NITEMS * sizeof(int32_t));

  CUTEST_PARAMETRIZE(params, filters_params, CUTEST_DATA(
      {true, NULL, true},
      {false, NULL, true},
      {true, "test_to_buffer_filters.b2frame", true},
      {false, "test_to_buffer_filters_s.b2frame", true},
      {true, NULL, false},
      {false, NULL, false},
      {true, "test_to_buffer_filters.b2frame", false},
      {false, "test_to_buffer_filters_s.b2frame", false},
  ));
}

CUTEST_TEST_TEST(schunk_to_buffer_filters) {
  CUTEST_GET_PARAMETER(params, filters_params);
  bool prefilter = !params.postfilter;
  int32_t *buffer = data->data;

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  blosc2_prefilter_params preparams = {0};
  if (prefilter) {
    cparams.prefilter = double_prefilter;
    cparams.preparams = &preparams;
  }
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_postfilter_params postparams = {0};
  if (params.postfilter) {
    dparams.postfilter = add_postfilter;
    dparams.postparams = &postparams;
  }
  blosc2_remove_urlpath(params.urlpath);
  blosc2_storage storage = {.contiguous = params.contiguous, .urlpath = params.urlpath,
                            .cparams = &cparams, .dparams = &dparams};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  CUTEST_ASSERT("Error creating the super-chunk", schunk != NULL);
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    int32_t nbytes = chunk_nitems(nchunk) * (int32_t)sizeof(int32_t);
    int64_t rc;
    if (nchunk == ZERO_CHUNK) {
      uint8_t zeros[BLOSC_EXTENDED_HEADER_LENGTH];
      CUTEST_ASSERT("Error creating a chunk of zeros",
                    blosc2_chunk_zeros(cparams, nbytes, zeros, sizeof(zeros)) > 0);
      rc = blosc2_schunk_append_chunk(schunk, zeros, true);
    }
    else {
      for (int32_t i = 0; i < chunk_nitems(nchunk); i++) {
        buffer[i] = nchunk * CHUNK_NITEMS + i;
      }
      rc = blosc2_schunk_append_buffer(schunk, buffer, nbytes);
    }
    CUTEST_ASSERT("Error appending", rc == nchunk + 1);
  }
  CUTEST_ASSERT("Error adding a vlmetalayer",
                blosc2_vlmeta_add(schunk, VLMETA_NAME, (uint8_t *)VLMETA_CONTENT,
                                  sizeof(VLMETA_CONTENT), NULL) >= 0);
  // What the super-chunk reads
  int32_t add = params.postfilter ? POSTFILTER_ADD : 0;
  CUTEST_ASSERT("The filter is not applied", check_values(schunk, prefilter, add, buffer) == 0);

  uint8_t *cframe;
  bool needs_free;
  int64_t len = blosc2_schunk_to_buffer(schunk, &cframe, &needs_free);
  CUTEST_ASSERT("Error serializing", len > 0);
  blosc2_schunk *back = blosc2_schunk_from_buffer(cframe, len, true);
  CUTEST_ASSERT("Error deserializing", back != NULL);
  CUTEST_ASSERT("The frame must hold the chunks as stored", check_same_chunks(schunk, back) == 0);
  CUTEST_ASSERT("The frame must hold the stored values", check_values(back, prefilter, 0, buffer) == 0);
  CUTEST_ASSERT("The frame must hold the vlmetalayer", check_vlmeta(back) == 0);
  // With the postfilter set again, it reads what the original reads
  set_dparams(back, dparams);
  CUTEST_ASSERT("The postfilter must apply once", check_values(back, prefilter, add, buffer) == 0);
  blosc2_schunk_free(back);

  // The other ways of serializing give the same frame
  uint8_t *alloc_buf = NULL;
  int64_t alloc_len = blosc2_schunk_to_buffer_alloc(schunk, record_alloc, &alloc_buf);
  bool same = alloc_len == len && memcmp(alloc_buf, cframe, (size_t)len) == 0;
  free(alloc_buf);
  CUTEST_ASSERT("blosc2_schunk_to_buffer_alloc() gives another frame", same);
  char *fname = "test_to_buffer_filters_out.b2frame";
  blosc2_remove_urlpath(fname);
  CUTEST_ASSERT("Error writing the file", blosc2_schunk_to_file(schunk, fname) == len);
  CUTEST_ASSERT("blosc2_schunk_to_file() gives another frame", check_file(fname, cframe, len) == 0);
  blosc2_remove_urlpath(fname);
  CUTEST_ASSERT("Error appending to a file", blosc2_schunk_append_file(schunk, fname) == 0);
  CUTEST_ASSERT("blosc2_schunk_append_file() gives another frame", check_file(fname, cframe, len) == 0);
  blosc2_remove_urlpath(fname);
  if (needs_free) {
    free(cframe);
  }

  // A copy holds the values as read
  blosc2_storage copy_storage = {.contiguous = true};
  blosc2_schunk *copy = blosc2_schunk_copy(schunk, &copy_storage);
  CUTEST_ASSERT("Error copying", copy != NULL);
  CUTEST_ASSERT("A copy must hold the values as read", check_values(copy, prefilter, add, buffer) == 0);
  CUTEST_ASSERT("A copy must hold the vlmetalayer", check_vlmeta(copy) == 0);
  blosc2_schunk_free(copy);

  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(params.urlpath);
  return 0;
}

CUTEST_TEST_TEARDOWN(schunk_to_buffer_filters) {
  free(data->data);
  blosc2_destroy();
}

int main(void) {
  CUTEST_TEST_RUN(schunk_to_buffer_filters);
}
