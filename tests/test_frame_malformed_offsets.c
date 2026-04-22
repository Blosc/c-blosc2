/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "blosc-private.h"
#include "frame.h"
#include "test_common.h"

#define CHUNKSIZE (256)
#define NCHUNKS (32)

/* Global vars */
int tests_run = 0;

static int32_t build_malicious_offsets_chunk(const int64_t *offsets, int32_t offsets_nbytes,
                                             uint8_t *dest, int32_t dest_capacity) {
  int8_t clevel_candidates[] = {9, 5, 1, 0};
  for (size_t i = 0; i < sizeof(clevel_candidates) / sizeof(clevel_candidates[0]); ++i) {
    blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
    cparams.typesize = sizeof(int64_t);
    cparams.clevel = clevel_candidates[i];
    blosc2_context *cctx = blosc2_create_cctx(cparams);
    if (cctx == NULL) {
      continue;
    }

    int32_t cbytes = blosc2_compress_ctx(cctx, offsets, offsets_nbytes, dest, dest_capacity);
    blosc2_free_ctx(cctx);
    if (cbytes > 0 && cbytes <= dest_capacity) {
      return cbytes;
    }
  }

  return BLOSC2_ERROR_FAILURE;
}

static char* test_reject_malformed_frame_offsets(void) {
  int32_t data[CHUNKSIZE];

  blosc2_init();

  blosc2_storage storage = {.contiguous = true};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("Cannot create schunk", schunk != NULL);

  for (int i = 0; i < CHUNKSIZE; ++i) {
    data[i] = i;
  }

  for (int nchunk = 0; nchunk < NCHUNKS; ++nchunk) {
    int64_t n = blosc2_schunk_append_buffer(schunk, data, (int32_t)sizeof(data));
    mu_assert("Cannot append chunk", n >= 0);
  }

  uint8_t *cframe = NULL;
  bool cframe_needs_free = false;
  int64_t cframe_len = blosc2_schunk_to_buffer(schunk, &cframe, &cframe_needs_free);
  mu_assert("Cannot export cframe", cframe_len > 0 && cframe != NULL);

  int32_t header_len;
  int64_t cbytes;
  from_big(&header_len, cframe + FRAME_HEADER_LEN, sizeof(header_len));
  from_big(&cbytes, cframe + FRAME_CBYTES, sizeof(cbytes));

  mu_assert("Invalid header_len", header_len >= FRAME_HEADER_MINLEN);
  mu_assert("Invalid cbytes", cbytes > 0);
  mu_assert("Invalid data area", header_len + cbytes < cframe_len);

  uint8_t *off_chunk = cframe + header_len + cbytes;
  int32_t offsets_nbytes;
  int32_t off_chunk_cbytes;
  int rc = blosc2_cbuffer_sizes(off_chunk, &offsets_nbytes, &off_chunk_cbytes, NULL);
  mu_assert("Cannot parse offsets chunk", rc >= 0);

  int32_t expected_offsets_nbytes;
  bool ok = blosc2_nchunks_to_offsets_nbytes(NCHUNKS, &expected_offsets_nbytes);
  mu_assert("Cannot compute expected offsets size", ok);
  mu_assert("Unexpected offsets chunk size", offsets_nbytes == expected_offsets_nbytes);

  int64_t *malicious_offsets = malloc((size_t)offsets_nbytes);
  mu_assert("Cannot allocate malicious offsets", malicious_offsets != NULL);
  for (int i = 0; i < NCHUNKS; ++i) {
    malicious_offsets[i] = INT64_MAX / 4;
  }

  uint8_t *malicious_off_chunk = malloc((size_t)off_chunk_cbytes);
  mu_assert("Cannot allocate malicious offsets chunk", malicious_off_chunk != NULL);

  int32_t malicious_cbytes = build_malicious_offsets_chunk(malicious_offsets, offsets_nbytes,
                                                           malicious_off_chunk, off_chunk_cbytes);
  mu_assert("Cannot build malformed offsets chunk in-place", malicious_cbytes > 0);

  memset(off_chunk, 0, (size_t)off_chunk_cbytes);
  memcpy(off_chunk, malicious_off_chunk, (size_t)malicious_cbytes);

  blosc2_schunk *decoded = blosc2_schunk_from_buffer(cframe, cframe_len, true);
  mu_assert("Malformed offsets must be rejected", decoded == NULL);

  free(malicious_off_chunk);
  free(malicious_offsets);
  blosc2_schunk_free(schunk);
  if (cframe_needs_free) {
    free(cframe);
  }
  blosc2_destroy();

  return EXIT_SUCCESS;
}

static char* test_open_rejects_malformed_frame_offsets(void) {
  int32_t data[CHUNKSIZE];
  const char* path = "test_frame_malformed_offsets_open.b2frame";
  blosc2_storage storage = {.contiguous = true};
  blosc2_schunk *schunk = NULL;
  FILE *fp = NULL;
  uint8_t *filebuf = NULL;

  blosc2_remove_urlpath(path);
  blosc2_init();

  schunk = blosc2_schunk_new(&storage);
  mu_assert("Cannot create schunk", schunk != NULL);

  for (int i = 0; i < CHUNKSIZE; ++i) {
    data[i] = i;
  }

  for (int nchunk = 0; nchunk < NCHUNKS; ++nchunk) {
    int64_t n = blosc2_schunk_append_buffer(schunk, data, (int32_t)sizeof(data));
    mu_assert("Cannot append chunk", n >= 0);
  }

  int64_t written = blosc2_schunk_to_file(schunk, path);
  mu_assert("Cannot write frame", written > 0);

  fp = fopen(path, "rb+");
  mu_assert("Cannot open malformed test file", fp != NULL);
  mu_assert("Cannot seek to end", fseek(fp, 0, SEEK_END) == 0);
  long flen = ftell(fp);
  mu_assert("Cannot get file size", flen > 0);
  rewind(fp);

  filebuf = malloc((size_t)flen);
  mu_assert("Cannot allocate file buffer", filebuf != NULL);
  mu_assert("Cannot read frame file", fread(filebuf, 1, (size_t)flen, fp) == (size_t)flen);

  int32_t header_len;
  int64_t cbytes;
  from_big(&header_len, filebuf + FRAME_HEADER_LEN, sizeof(header_len));
  from_big(&cbytes, filebuf + FRAME_CBYTES, sizeof(cbytes));
  mu_assert("Invalid header length", header_len >= FRAME_HEADER_MINLEN);
  mu_assert("Invalid compressed bytes", cbytes > 0);

  int64_t off_pos = header_len + cbytes;
  mu_assert("Offset chunk position out of bounds",
            off_pos >= 0 && off_pos + BLOSC_EXTENDED_HEADER_LENGTH <= flen);

  int32_t off_nbytes;
  int32_t off_cbytes;
  int rc = blosc2_cbuffer_sizes(filebuf + off_pos, &off_nbytes, &off_cbytes, NULL);
  mu_assert("Cannot parse on-disk offsets chunk", rc >= 0);
  mu_assert("Invalid offsets cbytes",
            off_cbytes >= BLOSC_EXTENDED_HEADER_LENGTH && off_pos + off_cbytes <= flen);
  (void)off_nbytes;

  /* Corrupt the offsets chunk payload so frame_to_schunk fails on open. */
  memset(filebuf + off_pos + BLOSC_EXTENDED_HEADER_LENGTH, 0xFF,
         (size_t)(off_cbytes - BLOSC_EXTENDED_HEADER_LENGTH));

  rewind(fp);
  mu_assert("Cannot write malformed frame", fwrite(filebuf, 1, (size_t)flen, fp) == (size_t)flen);
  fclose(fp);
  fp = NULL;

  blosc2_schunk *decoded = blosc2_schunk_open(path);
  mu_assert("Malformed frame must be rejected", decoded == NULL);

  if (fp != NULL) {
    fclose(fp);
  }
  if (filebuf != NULL) {
    free(filebuf);
  }
  if (schunk != NULL) {
    blosc2_schunk_free(schunk);
  }
  blosc2_remove_urlpath(path);
  blosc2_destroy();

  return EXIT_SUCCESS;
}


static char *all_tests(void) {
  mu_run_test(test_reject_malformed_frame_offsets);
  mu_run_test(test_open_rejects_malformed_frame_offsets);
  return EXIT_SUCCESS;
}


int main(void) {
  char *result;

  result = all_tests();
  if (result != EXIT_SUCCESS) {
    printf(" (%s)\n", result);
  }
  else {
    printf(" ALL TESTS PASSED");
  }
  printf("\tTests run: %d\n", tests_run);

  return result != EXIT_SUCCESS;
}
