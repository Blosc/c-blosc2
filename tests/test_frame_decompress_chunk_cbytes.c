/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Regression test for sparse (directory) frames whose chunkfile is shorter
  than the `cbytes` advertised in its chunk header.

  sframe_get_chunk sizes the chunk buffer from the chunkfile, but
  frame_decompress_chunk used to replace that size with the header `cbytes`
  and pass it as srcsize to blosc2_decompress_ctx, so a truncated chunkfile
  made the decompressor read past the end of the buffer.  Both the getter
  and the decompression path must now reject such a chunk.
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "test_common.h"

#define CHUNKSIZE (50 * 1000)
#define NCHUNKS (2)

/* Global vars */
int tests_run = 0;
static const char *urlpath = "test_frame_decompress_chunk_cbytes.b2frame";
static const char *chunkpath = "test_frame_decompress_chunk_cbytes.b2frame/00000001.chunk";
static int32_t data[CHUNKSIZE];
static int32_t data_dest[CHUNKSIZE];


/* Write a sparse frame and cut the chunkfile of chunk 1 to half its size,
 * leaving its header (and hence its `cbytes`) untouched. */
static int write_truncated_sframe(void) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.clevel = 5;
  blosc2_storage storage = {.contiguous = false, .urlpath = (char *)urlpath, .cparams = &cparams};
  blosc2_remove_urlpath(urlpath);

  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    return -1;
  }
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = nchunk * CHUNKSIZE + i;
    }
    if (blosc2_schunk_append_buffer(schunk, data, sizeof(data)) != nchunk + 1) {
      blosc2_schunk_free(schunk);
      return -1;
    }
  }
  blosc2_schunk_free(schunk);

  FILE *fp = fopen(chunkpath, "rb");
  if (fp == NULL) {
    return -1;
  }
  uint8_t *chunk = malloc(sizeof(data) + BLOSC2_MAX_OVERHEAD);
  if (chunk == NULL) {
    fclose(fp);
    return -1;
  }
  size_t cbytes = fread(chunk, 1, sizeof(data) + BLOSC2_MAX_OVERHEAD, fp);
  fclose(fp);

  int32_t header_cbytes;
  if (blosc2_cbuffer_sizes(chunk, NULL, &header_cbytes, NULL) < 0 ||
      (size_t)header_cbytes != cbytes || cbytes / 2 < BLOSC_EXTENDED_HEADER_LENGTH) {
    free(chunk);
    return -1;
  }

  fp = fopen(chunkpath, "wb");
  if (fp == NULL) {
    free(chunk);
    return -1;
  }
  size_t wbytes = fwrite(chunk, 1, cbytes / 2, fp);
  fclose(fp);
  free(chunk);

  return wbytes == cbytes / 2 ? 0 : -1;
}


static char* test_decompress_chunk_rejects_truncated_chunkfile(void) {
  mu_assert("ERROR: cannot write the truncated sparse frame", write_truncated_sframe() == 0);

  blosc2_schunk *schunk = blosc2_schunk_open(urlpath);
  mu_assert("ERROR: cannot open the sparse frame", schunk != NULL);

  // An untouched chunk still round-trips
  int dsize = blosc2_schunk_decompress_chunk(schunk, 0, data_dest, sizeof(data_dest));
  mu_assert("ERROR: cannot decompress the untouched chunk", dsize == (int)sizeof(data_dest));

  // The truncated chunk must be rejected, not read past its end
  dsize = blosc2_schunk_decompress_chunk(schunk, 1, data_dest, sizeof(data_dest));
  mu_assert("ERROR: a chunk shorter than its header cbytes must not decompress", dsize < 0);

  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(urlpath);
  return EXIT_SUCCESS;
}


static char* test_get_chunk_rejects_truncated_chunkfile(void) {
  mu_assert("ERROR: cannot write the truncated sparse frame", write_truncated_sframe() == 0);

  blosc2_schunk *schunk = blosc2_schunk_open(urlpath);
  mu_assert("ERROR: cannot open the sparse frame", schunk != NULL);

  uint8_t *chunk;
  bool needs_free;
  int cbytes = blosc2_schunk_get_chunk(schunk, 1, &chunk, &needs_free);
  if (cbytes >= 0 && needs_free) {
    free(chunk);
  }
  mu_assert("ERROR: a chunk shorter than its header cbytes must not be returned", cbytes < 0);

  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(urlpath);
  return EXIT_SUCCESS;
}


/* Write a sparse frame and append padding to chunk 1, so the chunkfile
 * is larger than the header cbytes. */
static int write_padded_sframe(int32_t *expected_header_cbytes) {
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.clevel = 5;
  blosc2_storage storage = {.contiguous = false, .urlpath = (char *)urlpath, .cparams = &cparams};
  blosc2_remove_urlpath(urlpath);

  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    return -1;
  }
  for (int nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = nchunk * CHUNKSIZE + i;
    }
    if (blosc2_schunk_append_buffer(schunk, data, sizeof(data)) != nchunk + 1) {
      blosc2_schunk_free(schunk);
      return -1;
    }
  }
  blosc2_schunk_free(schunk);

  FILE *fp = fopen(chunkpath, "ab");
  if (fp == NULL) {
    return -1;
  }
  uint8_t padding[64] = {0};
  size_t wbytes = fwrite(padding, 1, sizeof(padding), fp);
  fclose(fp);
  if (wbytes != sizeof(padding)) {
    return -1;
  }

  fp = fopen(chunkpath, "rb");
  if (fp == NULL) {
    return -1;
  }
  uint8_t header[BLOSC_EXTENDED_HEADER_LENGTH];
  size_t rbytes = fread(header, 1, sizeof(header), fp);
  fclose(fp);
  if (rbytes != sizeof(header)) {
    return -1;
  }

  int32_t header_cbytes;
  if (blosc2_cbuffer_sizes(header, NULL, &header_cbytes, NULL) < 0) {
    return -1;
  }
  if (expected_header_cbytes) {
    *expected_header_cbytes = header_cbytes;
  }
  return 0;
}


static char* test_padded_chunkfile(void) {
  int32_t expected_header_cbytes = 0;
  mu_assert("ERROR: cannot write padded sparse frame", write_padded_sframe(&expected_header_cbytes) == 0);

  blosc2_schunk *schunk = blosc2_schunk_open(urlpath);
  mu_assert("ERROR: cannot open the sparse frame", schunk != NULL);

  // Getting the chunk returns the header cbytes, not the padded file size
  uint8_t *chunk;
  bool needs_free;
  int cbytes = blosc2_schunk_get_chunk(schunk, 1, &chunk, &needs_free);
  mu_assert("ERROR: failed to get padded chunk", cbytes == expected_header_cbytes);
  if (cbytes >= 0 && needs_free) {
    free(chunk);
  }

  // Decompressing the padded chunk succeeds and produces correct data
  int dsize = blosc2_schunk_decompress_chunk(schunk, 1, data_dest, sizeof(data_dest));
  mu_assert("ERROR: cannot decompress padded chunk", dsize == (int)sizeof(data_dest));
  for (int i = 0; i < CHUNKSIZE; i++) {
    mu_assert("ERROR: decompressed data mismatch in padded chunk", data_dest[i] == 1 * CHUNKSIZE + i);
  }

  blosc2_schunk_free(schunk);
  blosc2_remove_urlpath(urlpath);
  return EXIT_SUCCESS;
}


static char *all_tests(void) {
  mu_run_test(test_decompress_chunk_rejects_truncated_chunkfile);
  mu_run_test(test_get_chunk_rejects_truncated_chunkfile);
  mu_run_test(test_padded_chunkfile);
  return EXIT_SUCCESS;
}


int main(void) {
  char *result;

  blosc2_init();
  result = all_tests();
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
