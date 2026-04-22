/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include <stdint.h>
#include <stdio.h>

#include "blosc-private.h"
#include "frame.h"
#include "test_common.h"

#define CHUNKSIZE (256)

/* Global vars */
int tests_run = 0;

static char* test_reject_oversized_trailer_len_in_fileframe(void) {
  int32_t data[CHUNKSIZE];

  blosc2_init();

  blosc2_storage storage = {.contiguous = true};
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  mu_assert("Cannot create schunk", schunk != NULL);

  for (int i = 0; i < CHUNKSIZE; ++i) {
    data[i] = i;
  }

  int64_t n = blosc2_schunk_append_buffer(schunk, data, (int32_t)sizeof(data));
  mu_assert("Cannot append chunk", n >= 0);

  uint8_t *cframe = NULL;
  bool cframe_needs_free = false;
  int64_t cframe_len = blosc2_schunk_to_buffer(schunk, &cframe, &cframe_needs_free);
  mu_assert("Cannot export cframe", cframe_len > FRAME_TRAILER_MINLEN && cframe != NULL);

  uint32_t bad_trailer_len = UINT32_MAX;
  to_big(cframe + cframe_len - FRAME_TRAILER_LEN_OFFSET, &bad_trailer_len, sizeof(bad_trailer_len));

  const char *fname = "malformed_trailer_len.b2frame";
  FILE *fp = fopen(fname, "wb");
  mu_assert("Cannot create malformed frame file", fp != NULL);
  size_t written = fwrite(cframe, 1, (size_t)cframe_len, fp);
  fclose(fp);
  mu_assert("Cannot write malformed frame file", written == (size_t)cframe_len);

  blosc2_frame_s *frame = frame_from_file_offset(fname, &BLOSC2_IO_DEFAULTS, 0);
  mu_assert("Malformed trailer length must be rejected", frame == NULL);

  remove(fname);
  blosc2_schunk_free(schunk);
  if (cframe_needs_free) {
    free(cframe);
  }
  blosc2_destroy();

  return EXIT_SUCCESS;
}


static char *all_tests(void) {
  mu_run_test(test_reject_oversized_trailer_len_in_fileframe);
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
